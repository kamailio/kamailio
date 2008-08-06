#!/usr/bin/python
#
# Copyright 2008 Google Inc. All Rights Reserved.

"""SQL-like access layer for dbtext.

This module provides the glue for kamctl to interact with dbtext files
using basic SQL syntax thus avoiding special case handling of dbtext.

"""

__author__ = 'herman@google.com (Herman Sheremetyev)'

import fcntl
import os
import shutil
import sys
import tempfile
import time

if 'DBTEXTDB_DEBUG' in os.environ:
  DEBUG = os.environ['DBTEXTDB_DEBUG']
else:
  DEBUG = 0


def Debug(msg):
  """Debug print method."""
  if DEBUG:
    print msg


class DBText(object):
  """Provides connection to a dbtext database."""

  RESERVED_WORDS = ['SELECT', 'DELETE', 'UPDATE', 'INSERT', 'SET',
                    'VALUES', 'INTO', 'FROM', 'ORDER', 'BY', 'WHERE',
                    'COUNT', 'CONCAT', 'AND', 'AS']
  ALL_COMMANDS = ['SELECT', 'DELETE', 'UPDATE', 'INSERT']
  WHERE_COMMANDS = ['SELECT', 'DELETE', 'UPDATE']

  def __init__(self, location):
    self.location = location  # location of dbtext tables
    self.tokens = []          # query broken up into tokens
    self.conditions = {}      # args to the WHERE clause
    self.columns = []         # columns requested by SELECT
    self.table = ''           # name of the table being queried
    self.header = {}          # table header
    self.orig_data = []       # original table data used to diff after updates
    self.data = []            # table data as a list of dicts
    self.count = False        # where or not using COUNT()
    self.aliases = {}         # column aliases (SELECT AS)
    self.targets = {}         # target columns-value pairs for INSERT/UPDATE
    self.args = ''            # query arguments preceeding the ;
    self.command = ''         # which command are we executing
    self.strings = []         # list of string literals parsed from the query
    self.parens = []          # list of parentheses parsed from the query
    self._str_placeholder = '__DBTEXTDB_PARSED_OUT_STRING__'
    self._paren_placeholder = '__DBTEXTDB_PARSED_OUT_PARENS__'
    if not os.path.isdir(location):
      raise ParseError(location + ' is not a directory')

  def _ParseOrderBy(self):
    """Parse out the column name to be used for ordering the dataset.

    Raises:
      ParseError: Invalid ORDER BY clause
    """
    self.order_by = ''
    if 'ORDER' in self.tokens:
      order_index = self.tokens.index('ORDER')
      if order_index != len(self.tokens) - 3:
        raise ParseError('ORDER must be followed with BY and column name')
      if self.tokens[order_index + 1] != 'BY':
        raise ParseError('ORDER must be followed with BY')
      self.order_by = self.tokens[order_index + 2]

      # strip off the order by stuff
      self.tokens.pop()  # column name
      self.tokens.pop()  # BY
      self.tokens.pop()  # ORDER

    elif 'BY' in self.tokens:
      raise ParseError('BY must be preceeded by ORDER')

    Debug('Order by: ' + self.order_by)

  def _ParseConditions(self):
    """Parse out WHERE clause.

    Take everything after the WHERE keyword and convert it to a dict of
    name value pairs corresponding to the columns and their values that
    should be matched.

    Raises:
      ParseError: Invalid WHERE clause
      NotSupportedError: Unsupported syntax
    """
    self.conditions = {}
    Debug('self.tokens = %s' % self.tokens)
    if 'WHERE' not in self.tokens:
      return

    if self.command not in self.WHERE_COMMANDS:
      raise ParseError(self.command + ' cannot have a WHERE clause')
    if 'OR' in self.tokens:
      raise NotSupportedError('WHERE clause does not support OR operator')

    where_clause = self.tokens[self.tokens.index('WHERE') + 1:]
    self.conditions = self._ParsePairs(' '.join(where_clause), 'AND')
    for cond in self.conditions:
      self.conditions[cond] = self._EscapeChars(self.conditions[cond])
    Debug('Conditions are [%s]' % self.conditions)

    # pop off where clause
    a = self.tokens.pop()
    while a != 'WHERE':
      a = self.tokens.pop()

    Debug('self.tokens: %s' % self.tokens)

  def _ParseColumns(self):
    """Parse out the columns that need to be selected.

    Raises:
      ParseError: Invalid SELECT syntax
    """
    self.columns = []
    self.count = False
    self.aliases = {}
    col_end = 0
    # this is only valid for SELECT
    if self.command != 'SELECT':
      return

    if 'FROM' not in self.tokens:
      raise ParseError('SELECT must be followed by FROM')

    col_end = self.tokens.index('FROM')
    if not col_end:  # col_end == 0
      raise ParseError('SELECT must be followed by column name[s]')

    cols_str = ' '.join(self.tokens[0:col_end])
    # check if there is a function modifier on the columns
    if self.tokens[0] == 'COUNT':
      self.count = True
      if col_end == 1:
        raise ParseError('COUNT must be followed by column name[s]')
      if not self.tokens[1].startswith(self._paren_placeholder):
        raise ParseError('COUNT must be followed by ()')
      cols_str = self._ReplaceParens(self.tokens[1])

    cols = cols_str.split(',')
    for col in cols:
      if not col.strip():
        raise ParseError('Extra comma in columns')
      col_split = col.split()
      if col_split[0] == 'CONCAT':
        # found a concat statement, do the same overall steps for those cols
        self._ParseColumnsConcatHelper(col_split)
      else:
        col_split = col.split()
        if len(col_split) > 2 and col_split[1] != 'AS':
          raise ParseError('multiple columns must be separated by a comma')
        elif len(col_split) == 3:
          if col_split[1] != 'AS':
            raise ParseError('Invalid column alias, use AS')
          my_key = self._ReplaceStringLiterals(col_split[2], quotes=True)
          my_val = self._ReplaceStringLiterals(col_split[0], quotes=True)
          self.aliases[my_key] = [my_val]
          self.columns.append(my_key)
        elif len(col_split) > 3:
          raise ParseError('multiple columns must be separated by a comma')
        elif len(col_split) == 2:  # alias
          my_key = self._ReplaceStringLiterals(col_split[1], quotes=True)
          my_val = self._ReplaceStringLiterals(col_split[0], quotes=True)
          self.aliases[my_key] = [my_val]
          self.columns.append(my_key)
        else:
          col = self._ReplaceStringLiterals(col, quotes=True).strip()
          if not col:  # col == ''
            raise ParseError('empty column name not allowed')

          self.columns.append(col)

    # pop off all the columns related junk
    self.tokens = self.tokens[col_end + 1:]

    Debug('Columns: %s' % self.columns)
    Debug('Aliases: %s' % self.aliases)
    Debug('self.tokens: %s' % self.tokens)

  def _ParseColumnsConcatHelper(self, col_split):
    """Handles the columns being CONCAT'd together.

    Args:
      col_split: ['column', 'column']

    Raises:
      ParseError: invalid CONCAT()
    """
    concat_placeholder = '_'
    split_len = len(col_split)
    if split_len == 1:
      raise ParseError('CONCAT() must be followed by column name[s]')
    if not col_split[1].startswith(self._paren_placeholder):
      raise ParseError('CONCAT must be followed by ()')
    if split_len > 2:
      if split_len == 4 and col_split[2] != 'AS':
        raise ParseError('CONCAT() must be followed by an AS clause')
      if split_len > 5:
        raise ParseError('CONCAT() AS clause takes exactly 1 arg. '
                         'Extra args: [%s]' % (col_split[4:]))
      else:
        concat_placeholder = self._ReplaceStringLiterals(col_split[-1],
                                                         quotes=True)

    # make sure this place hodler is unique
    while concat_placeholder in self.aliases:
      concat_placeholder += '_'
    concat_cols_str = self._ReplaceParens(col_split[1])
    concat_cols = concat_cols_str.split(',')
    concat_col_list = []
    for concat_col in concat_cols:
      if ' ' in concat_col.strip():
        raise ParseError('multiple columns must be separated by a'
                         ' comma inside CONCAT()')
      concat_col = self._ReplaceStringLiterals(concat_col, quotes=True).strip()
      if not concat_col:
        raise ParseError('Attempting to CONCAT empty set')
      concat_col_list.append(concat_col)

    self.aliases[concat_placeholder] = concat_col_list
    self.columns.append(concat_placeholder)

  def _ParseTable(self):
    """Parse out the table name (multiple table names not supported).

    Raises:
      ParseError: Unable to parse table name
    """
    table_name = ''
    if (not self.tokens or  # len == 0
        (self.tokens[0] in self.RESERVED_WORDS and
         self.tokens[0] not in ['FROM', 'INTO'])):
      raise ParseError('Missing table name')

    # SELECT
    if self.command == 'SELECT':
      table_name = self.tokens.pop(0)

    # INSERT
    elif self.command == 'INSERT':
      table_name = self.tokens.pop(0)
      if table_name == 'INTO':
        table_name = self.tokens.pop(0)

    # DELETE
    elif self.command == 'DELETE':
      if self.tokens[0] != 'FROM':
        raise ParseError('DELETE command must be followed by FROM')

      self.tokens.pop(0)  # FROM
      table_name = self.tokens.pop(0)

    # UPDATE
    elif self.command == 'UPDATE':
      table_name = self.tokens.pop(0)

    if not self.table:
      self.table = table_name

    else:  # multiple queries detected, make sure they're against same table
      if self.table != table_name:
        raise ParseError('Table changed between queries! %s -> %s' %
                         (self.table, table_name))
    Debug('Table is [%s]' % self.table)
    Debug('self.tokens is %s' % self.tokens)

  def _ParseTargets(self):
    """Parse out name value pairs of columns and their values.

    Raises:
      ParseError: Unable to parse targets
    """
    self.targets = {}
    # UPDATE
    if self.command == 'UPDATE':
      if self.tokens.pop(0) != 'SET':
        raise ParseError('UPDATE command must be followed by SET')

      self.targets = self._ParsePairs(' '.join(self.tokens), ',')

    # INSERT
    if self.command == 'INSERT':
      if self.tokens[0] == 'SET':
        self.targets = self._ParsePairs(' '.join(self.tokens[1:]), ',')

      elif len(self.tokens) == 3 and self.tokens[1] == 'VALUES':
        if not self.tokens[0].startswith(self._paren_placeholder):
          raise ParseError('INSERT column names must be inside parens')
        if not self.tokens[2].startswith(self._paren_placeholder):
          raise ParseError('INSERT values must be inside parens')

        cols = self._ReplaceParens(self.tokens[0]).split(',')
        vals = self._ReplaceParens(self.tokens[2]).split(',')

        if len(cols) != len(vals):
          raise ParseError('INSERT column and value numbers must match')
        if not cols:  # len == 0
          raise ParseError('INSERT column number must be greater than 0')

        i = 0
        while i < len(cols):
          val = vals[i].strip()
          if not val:  # val == ''
            raise ParseError('INSERT values cannot be empty')
          if ' ' in val:
            raise ParseError('INSERT values must be comma separated')
          self.targets[cols[i].strip()] = self._ReplaceStringLiterals(val)
          i += 1

      else:
        raise ParseError('Unable to parse INSERT targets')

    for target in self.targets:
      self.targets[target] = self._EscapeChars(self.targets[target])

    Debug('Targets are [%s]' % self.targets)

  def _EscapeChars(self, value):
    """Escape necessary chars before inserting into dbtext.

    Args:
      value: 'string'

    Returns:
      escaped: 'string' with chars escaped appropriately
    """
    # test that the value is string, if not return it as is
    try:
      value.find('a')
    except:
      return value

    escaped = value
    escaped = escaped.replace('\\', '\\\\').replace('\0', '\\0')
    escaped = escaped.replace(':', '\\:').replace('\n', '\\n')
    escaped = escaped.replace('\r', '\\r').replace('\t', '\\t')
    return escaped

  def _UnEscapeChars(self, value):
    """Un-escape necessary chars before returning to user.

    Args:
      value: 'string'

    Returns:
      escaped: 'string' with chars escaped appropriately
    """
    # test that the value is string, if not return it as is
    try:
      value.find('a')
    except:
      return value

    escaped = value
    escaped = escaped.replace('\\:', ':').replace('\\n', '\n')
    escaped = escaped.replace('\\r', '\r').replace('\\t', '\t')
    escaped = escaped.replace('\\0', '\0').replace('\\\\', '\\')
    return escaped

  def Execute(self, query, writethru=True):
    """Parse and execute the query.

    Args:
      query: e.g. 'select * from table;'
      writethru: bool

    Returns:
      dataset: [{col: val, col: val}, {col: val}, {col: val}]

    Raises:
      ExecuteError: unable to execute query
    """
    # parse the query
    self.ParseQuery(query)

    # get lock and execute the query
    self.OpenTable()
    Debug('Running ' + self.command)
    dataset = []
    if self.command == 'SELECT':
      dataset = self._RunSelect()
    elif self.command == 'UPDATE':
      dataset = self._RunUpdate()
    elif self.command == 'INSERT':
      dataset = self._RunInsert()
    elif self.command == 'DELETE':
      dataset = self._RunDelete()

    if self.command != 'SELECT' and writethru:
      self.WriteTempTable()
      self.MoveTableIntoPlace()

    Debug(dataset)
    return dataset

  def CleanUp(self):
    """Reset the internal variables (for multiple queries)."""
    self.tokens = []          # query broken up into tokens
    self.conditions = {}      # args to the WHERE clause
    self.columns = []         # columns requested by SELECT
    self.table = ''           # name of the table being queried
    self.header = {}          # table header
    self.orig_data = []       # original table data used to diff after updates
    self.data = []            # table data as a list of dicts
    self.count = False        # where or not using COUNT()
    self.aliases = {}         # column aliases (SELECT AS)
    self.targets = {}         # target columns-value pairs for INSERT/UPDATE
    self.args = ''            # query arguments preceeding the ;
    self.command = ''         # which command are we executing
    self.strings = []         # list of string literals parsed from the query
    self.parens = []          # list of parentheses parsed from the query

  def ParseQuery(self, query):
    """External wrapper for the query parsing routines.

    Args:
      query: string

    Raises:
      ParseError: Unable to parse query
    """
    self.args = query.split(';')[0]
    self._Tokenize()
    self._ParseCommand()
    self._ParseOrderBy()
    self._ParseConditions()
    self._ParseColumns()
    self._ParseTable()
    self._ParseTargets()

  def _ParseCommand(self):
    """Determine the command: SELECT, UPDATE, DELETE or INSERT.

    Raises:
      ParseError: unable to parse command
    """
    self.command = self.tokens[0]
    # Check that command is valid
    if self.command not in self.ALL_COMMANDS:
      raise ParseError('Unsupported command: ' + self.command)

    self.tokens.pop(0)
    Debug('Command is: %s' % self.command)
    Debug('self.tokens: %s' % self.tokens)

  def _Tokenize(self):
    """Turn the string query into a list of tokens.

    Split on '(', ')', ' ', ';', '=' and ','.
    In addition capitalize any SQL keywords found.
    """
    # horrible hack to handle now()
    time_now = '%s' % int(time.time())
    time_now = time_now[0:-2] + '00'  # round off the seconds for unittesting
    while 'now()' in self.args.lower():
      start = self.args.lower().find('now()')
      self.args = ('%s%s%s' % (self.args[0:start], time_now,
                               self.args[start + 5:]))
    # pad token separators with spaces
    pad = self.args.replace('(', ' ( ').replace(')', ' ) ')
    pad = pad.replace(',', ' , ').replace(';', ' ; ').replace('=', ' = ')
    self.args = pad
    # parse out all the blocks (string literals and parens)
    self._ParseOutBlocks()
    # split remaining into tokens
    self.tokens = self.args.split()

    # now capitalize
    i = 0
    while i < len(self.tokens):
      if self.tokens[i].upper() in self.RESERVED_WORDS:
        self.tokens[i] = self.tokens[i].upper()

      i += 1

    Debug('Tokens: %s' % self.tokens)

  def _ParseOutBlocks(self):
    """Parse out string literals and parenthesized values."""
    self.strings = []
    self.parens = []

    # set str placeholder to a value that's not present in the string
    while self._str_placeholder in self.args:
      self._str_placeholder = '%s_' % self._str_placeholder

    # set paren placeholder to a value that's not present in the string
    while self._paren_placeholder in self.args:
      self._paren_placeholder = '%s_' % self._paren_placeholder

    self.strings = self._ParseOutHelper(self._str_placeholder, ["'", '"'],
                                        'quotes')
    self.parens = self._ParseOutHelper(self._paren_placeholder, ['(', ')'],
                                       'parens')
    Debug('Strings: %s' % self.strings)
    Debug('Parens: %s' % self.parens)

  def _ParseOutHelper(self, placeholder, delims, mode):
    """Replace all text within delims with placeholders.

    Args:
      placeholder: string
      delims: list of strings
      mode: string
          'parens': if there are 2 delims treat the first as opening
                    and second as closing, such as with ( and )
          'quotes': treat each delim as either opening or
                    closing and require the same one to terminate the block,
                    such as with ' and "

    Returns:
      list: [value1, value2, ...]

    Raises:
      ParseError: unable to parse out delims
      ExecuteError: Invalid usage
    """
    if mode not in ['quotes', 'parens']:
      raise ExecuteError('_ParseOutHelper: invalid mode ' + mode)
    if mode == 'parens' and len(delims) != 2:
      raise ExecuteError('_ParseOutHelper: delims must have 2 values '
                         'in "parens" mode')
    values = []
    started = 0
    new_args = ''
    string = ''
    my_id = 0
    delim = ''
    for c in self.args:
      if c in delims:
        if not started:
          if mode == 'parens' and c != delims[0]:
            raise ParseError('Found closing delimeter %s before '
                             'corresponding %s' % (c, delims[0]))
          started += 1
          delim = c
        else:
          if ((mode == 'parens' and c == delim) or
              (mode == 'quotes' and c != delim)):
            string = '%s%s' % (string, c)
            continue  # wait for matching delim

          started -= 1
          if not started:
            values.append(string)
            new_args = '%s %s' % (new_args, '%s%d' % (placeholder, my_id))
            my_id += 1
            string = ''

      else:
        if not started:
          new_args = '%s%s' % (new_args, c)
        else:
          string = '%s%s' % (string, c)

    if started:
      if mode == 'parens':
        waiting_for = delims[1]
      else:
        waiting_for = delim
      raise ParseError('Unterminated block, waiting for ' + waiting_for)

    self.args = new_args
    Debug('Values: %s' % values)
    return values

  def _ReplaceStringLiterals(self, s, quotes=False):
    """Replaces string placeholders with real values.

    If quotes is set to True surround the returned value with single quotes

    Args:
      s: string
      quotes: bool

    Returns:
      s: string
    """
    if s.strip().startswith(self._str_placeholder):
      str_index = int(s.split(self._str_placeholder)[1])
      s = self.strings[str_index]
      if quotes:
        s = "'" + s + "'"

    return s

  def _ReplaceParens(self, s):
    """Replaces paren placeholders with real values.

    Args:
      s: string

    Returns:
      s: string
    """
    if s.strip().startswith(self._paren_placeholder):
      str_index = int(s.split(self._paren_placeholder)[1])
      s = self.parens[str_index].strip()

    return s

  def _RunDelete(self):
    """Run the DELETE command.

    Go through the rows in self.data matching them
    against the conditions, if they fit delete the row leaving a placeholder
    value (in order to keep the iteration process sane).  Afterward clean up
    any empty values.

    Returns:
      dataset: [number of affected rows]
    """
    i = 0
    length = len(self.data)
    affected = 0
    while i < length:
      if self._MatchRow(self.data[i]):
        self.data[i] = None
        affected += 1

      i += 1

    # clean out the placeholders
    while None in self.data:
      self.data.remove(None)

    return [affected]

  def _RunUpdate(self):
    """Run the UPDATE command.

    Find the matching rows and update based on self.targets

    Returns:
      affected: [int]
    Raises:
      ExecuteError: failed to run UPDATE
    """
    i = 0
    length = len(self.data)
    affected = 0
    while i < length:
      if self._MatchRow(self.data[i]):
        for target in self.targets:
          if target not in self.header:
            raise ExecuteError(target + ' is an invalid column name')
          if self.header[target]['auto']:
            raise ExecuteError(target + ' is type auto and connot be updated')

          self.data[i][target] = self._TypeCheck(self.targets[target], target)
        affected += 1

      i += 1

    return [affected]

  def _RunInsert(self):
    """Run the INSERT command.

    Build up the row based on self.targets and table defaults, then append to
    self.data

    Returns:
      affected: [int]
    Raises:
      ExecuteError: failed to run INSERT
    """
    new_row = {}
    cols = self._SortHeaderColumns()
    for col in cols:
      if col in self.targets:
        if self.header[col]['auto']:
          raise ExecuteError(col + ' is type auto: cannot be modified')
        new_row[col] = self.targets[col]

      elif self.header[col]['null']:
        new_row[col] = ''

      elif self.header[col]['auto']:
        new_row[col] = self._GetNextAuto(col)

      else:
        raise ExecuteError(col + ' cannot be empty or null')

    self.data.append(new_row)
    return [1]

  def _GetNextAuto(self, col):
    """Figure out the next value for col based on existing values.

    Scan all the current values and return the highest one + 1.

    Args:
      col: string

    Returns:
      next: int

    Raises:
      ExecuteError: Failed to get auto inc
    """
    highest = 0
    seen = []
    for row in self.data:
      if row[col] > highest:
        highest = row[col]

      if row[col] not in seen:
        seen.append(row[col])
      else:
        raise ExecuteError('duplicate value %s in %s' % (row[col], col))

    return highest + 1

  def _RunSelect(self):
    """Run the SELECT command.

    Returns:
      dataset: []

    Raises:
      ExecuteError: failed to run SELECT
    """
    dataset = []
    if ['*'] == self.columns:
      self.columns = self._SortHeaderColumns()

    for row in self.data:
      if self._MatchRow(row):
        match = []
        for col in self.columns:
          if col in self.aliases:
            concat = ''
            for concat_col in self.aliases[col]:
              if concat_col.startswith("'") and concat_col.endswith("'"):
                concat += concat_col.strip("'")
              elif concat_col not in self.header.keys():
                raise ExecuteError('Table %s does not have a column %s' %
                                   (self.table, concat_col))
              else:
                concat = '%s%s' % (concat, row[concat_col])

            if not concat.strip():
              raise ExecuteError('Empty CONCAT statement')

            my_match = concat

          elif col.startswith("'") and col.endswith("'"):
            my_match = col.strip("'")
          elif col not in self.header.keys():
            raise ExecuteError('Table %s does not have a column %s' %
                               (self.table, col))
          else:
            my_match = row[col]

          match.append(self._UnEscapeChars(my_match))

        dataset.append(match)

    if self.count:
      Debug('Dataset: %s' % dataset)
      dataset = [len(dataset)]

    if self.order_by:
      if self.order_by not in self.header.keys():
        raise ExecuteError('Unknown column %s in ORDER BY clause' %
                           self.order_by)
      pos = self._PositionByCol(self.order_by)
      dataset = self._SortMatrixByCol(dataset, pos)

    return dataset

  def _SortMatrixByCol(self, dataset, pos):
    """Sorts the matrix (array or arrays) based on a given column value.

    That is, if given matrix that looks like:

    [[1, 2, 3], [6, 5, 4], [3, 2, 1]]

    given pos = 0 produce:

    [[1, 2, 3], [3, 2, 1], [6, 5, 4]]

    given pos = 1 produce:

    [[1, 2, 3], [3, 2, 1], [6, 5, 4]]

    given pos = 2 produce:

    [[3, 2, 1], [1, 2, 3], [6, 5, 4]]

    Works for both integer and string values of column.

    Args:
      dataset: [[], [], ...]
      pos: int

    Returns:
      sorted: [[], [], ...]
    """
    # prepend value in pos to the beginning of every row
    i = 0
    while i < len(dataset):
      dataset[i].insert(0, dataset[i][pos])
      i += 1

    # sort the matrix, which is done on the row we just prepended
    dataset.sort()

    # strip away the first value
    i = 0
    while i < len(dataset):
      dataset[i].pop(0)
      i += 1

    return dataset

  def _MatchRow(self, row):
    """Matches the row against self.conditions.

    Args:
      row: ['val', 'val']

    Returns:
      Bool
    """
    match = True
    # when there are no conditions we match everything
    if not self.conditions:
      return match

    for condition in self.conditions:
      cond_val = self.conditions[condition]
      if condition not in self.header.keys():
        match = False
        break
      else:
        if cond_val != row[condition]:
          match = False
          break

    return match

  def _ProcessHeader(self):
    """Parse out the header information.

    Returns:
      {col_name: {'type': string, 'null': string, 'auto': string, 'pos': int}}
    """
    header = self.fd.readline().strip()
    cols = {}
    pos = 0
    for col in header.split():
      col_name = col.split('(')[0]
      col_type = col.split('(')[1].split(')')[0].split(',')[0]
      col_null = False
      col_auto = False
      if ',' in col.split('(')[1].split(')')[0]:
        if col.split('(')[1].split(')')[0].split(',')[1].lower() == 'null':
          col_null = True
        if col.split('(')[1].split(')')[0].split(',')[1].lower() == 'auto':
          col_auto = True

      cols[col_name] = {}
      cols[col_name]['type'] = col_type
      cols[col_name]['null'] = col_null
      cols[col_name]['auto'] = col_auto
      cols[col_name]['pos'] = pos
      pos += 1

    return cols

  def _GetData(self):
    """Reads table data into memory as a list of dicts keyed on column names.

    Returns:
      data: [{row}, {row}, ...]
    Raises:
      ExecuteError: failed to get data
    """
    data = []
    row_num = 0
    for row in self.fd:
      row = row.rstrip('\n')
      row_dict = {}
      i = 0
      field_start = 0
      field_num = 0
      while i < len(row):
        if row[i] == ':':
          # the following block is executed again after the while is done
          val = row[field_start:i]
          col = self._ColByPosition(field_num)
          val = self._TypeCheck(val, col)
          row_dict[col] = val

          field_start = i + 1  # skip the colon itself
          field_num += 1
        if row[i] == '\\':
          i += 2  # skip the next char since it's escaped
        else:
          i += 1

      # handle the last field since we won't hit a : at the end
      # sucks to duplicate the code outside the loop but I can't think
      # of a better way :(

      val = row[field_start:i]
      col = self._ColByPosition(field_num)
      val = self._TypeCheck(val, col)
      row_dict[col] = val

      # verify that all columns were created
      for col in self.header:
        if col not in row_dict:
          raise ExecuteError('%s is missing from row %d in %s' %
                             (col, row_num, self.table))

      row_num += 1
      data.append(row_dict)

    return data

  def _TypeCheck(self, val, col):
    """Verify type of val based on the header.

    Make sure the value is returned in quotes if it's a string
    and as '' when it's empty and Null

    Args:
      val: string
      col: string

    Returns:
      val: string

    Raises:
      ExecuteError: invalid value or column
    """
    if not val and not self.header[col]['null']:
      raise ExecuteError(col + ' cannot be empty or null')

    if (self.header[col]['type'].lower() == 'int' or
        self.header[col]['type'].lower() == 'double'):
      try:
        if val:
          val = eval(val)
      except NameError, e:
        raise ExecuteError('Failed to parse %s in %s '
                           '(unable to convert to type %s): %s' %
                           (col, self.table, self.header[col]['type'], e))
      except SyntaxError, e:
        raise ExecuteError('Failed to parse %s in %s '
                           '(unable to convert to type %s): %s' %
                           (col, self.table, self.header[col]['type'], e))

    return val

  def _ColByPosition(self, pos):
    """Returns column name based on position.

    Args:
      pos: int

    Returns:
      column: string

    Raises:
      ExecuteError: invalid column
    """
    for col in self.header:
      if self.header[col]['pos'] == pos:
        return col

    raise ExecuteError('Header does not contain column %d' % pos)

  def _PositionByCol(self, col):
    """Returns position of the column based on the name.

    Args:
      col: string

    Returns:
      pos: int

    Raises:
      ExecuteError: invalid column
    """
    if col not in self.header.keys():
      raise ExecuteError(col + ' is not a valid column name')

    return self.header[col]['pos']

  def _SortHeaderColumns(self):
    """Sort column names by position.

    Returns:
      sorted: [col1, col2, ...]

    Raises:
      ExecuteError: unable to sort header
    """
    cols = self.header.keys()
    sorted_cols = [''] * len(cols)
    for col in cols:
      pos = self.header[col]['pos']
      sorted_cols[pos] = col

    if '' in sorted_cols:
      raise ExecuteError('Unable to sort header columns: %s' % cols)

    return sorted_cols

  def OpenTable(self):
    """Opens the table file and places its content into memory.

    Raises:
      ExecuteError: unable to open table
    """
    # if we already have a header assume multiple queries on same table
    # (can't use self.data in case the table was empty to begin with)
    if self.header:
      return

    try:
      self.fd = open(os.path.join(self.location, self.table), 'r')
      self.header = self._ProcessHeader()

      if self.command in ['INSERT', 'DELETE', 'UPDATE']:
        fcntl.flock(self.fd, fcntl.LOCK_EX)

      self.data = self._GetData()
      self.orig_data = self.data[:]  # save a copy of the data before modifying

    except IOError, e:
      raise ExecuteError('Unable to open table %s: %s' % (self.table, e))

    Debug('Header is: %s' % self.header)

    # type check the conditions
    for cond in self.conditions:
      if cond not in self.header.keys():
        raise ExecuteError('unknown column %s in WHERE clause' % cond)
      self.conditions[cond] = self._TypeCheck(self.conditions[cond], cond)

    # type check the targets
    for target in self.targets:
      if target not in self.header.keys():
        raise ExecuteError('unknown column in targets:  %s' % target)
      self.targets[target] = self._TypeCheck(self.targets[target], target)

    Debug('Type checked conditions: %s' % self.conditions)

    Debug('Data is:')
    for row in self.data:
      Debug('=======================')
      Debug(row)
    Debug('=======================')

  def WriteTempTable(self):
    """Write table header and data.

    First write header and data to a temp file,
    then move the tmp file to replace the original table file.
    """
    self.temp_file = tempfile.NamedTemporaryFile()
    Debug('temp_file: ' + self.temp_file.name)
    # write header
    columns = self._SortHeaderColumns()
    header = ''
    for col in columns:
      header = '%s %s' % (header, col)
      header = '%s(%s' % (header, self.header[col]['type'])
      if self.header[col]['null']:
        header = '%s,null)' % header
      elif self.header[col]['auto']:
        header = '%s,auto)' % header
      else:
        header = '%s)' % header

    self.temp_file.write(header.strip() + '\n')

    # write data
    for row in self.data:
      row_str = ''
      for col in columns:
        row_str = '%s:%s' % (row_str, row[col])

      self.temp_file.write(row_str[1:] + '\n')

    self.temp_file.flush()

  def MoveTableIntoPlace(self):
    """Replace the real table with the temp one.

    Diff the new data against the original and replace the table when they are
    different.
    """
    if self.data != self.orig_data:
      temp_file = self.temp_file.name
      table_file = os.path.join(self.location, self.table)
      Debug('Copying %s to %s' % (temp_file, table_file))
      shutil.copy(self.temp_file.name, self.location + '/' + self.table)

  def _ParsePairs(self, s, delimeter):
    """Parses out name value pairs from a string.

    String contains name=value pairs
    separated by a delimiter (such as "and" or ",")

    Args:
      s: string
      delimeter: string

    Returns:
      my_dict: dictionary

    Raises:
      ParseError: unable to parse pairs
    """
    my_dict = {}
    Debug('parse pairs: [%s]' % s)
    pairs = s.split(delimeter)
    for pair in pairs:
      if '=' not in pair:
        raise ParseError('Invalid condition pair: ' + pair)

      split = pair.split('=')
      Debug('split: %s' % split)
      if len(split) != 2:
        raise ParseError('Invalid condition pair: ' + pair)

      col = split[0].strip()
      if not col or not split[1].strip() or ' ' in col:
        raise ParseError('Invalid condition pair: ' + pair)

      val = self._ReplaceStringLiterals(split[1].strip())
      my_dict[col] = val

    return my_dict


class Error(Exception):
  """DBText error."""


class ParseError(Error):
  """Parse error."""


class NotSupportedError(Error):
  """Not Supported error."""


class ExecuteError(Error):
  """Execute error."""


def main(argv):

  if len(argv) < 2:
    print 'Usage %s query' % argv[0]
    sys.exit(1)

  if 'DBTEXT_PATH' not in os.environ or not os.environ['DBTEXT_PATH']:
    print 'DBTEXT_PATH must be set'
    sys.exit(1)
  else:
    location = os.environ['DBTEXT_PATH']

  try:
    conn = DBText(location)
    dataset = conn.Execute(' '.join(argv[1:]))
    if dataset:
      for row in dataset:
        if conn.command != 'SELECT':
          print 'Updated %s, rows affected: %d' % (conn.table, row)
        else:
          print row
  except Error, e:
    print e
    sys.exit(1)


if __name__ == '__main__':
  main(sys.argv)
