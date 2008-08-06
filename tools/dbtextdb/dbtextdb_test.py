#!/usr/bin/python
#
# Copyright 2008 Google Inc. All Rights Reserved.

"""Test for dbtext_query."""

__author__ = 'herman@google.com (Herman Sheremetyev)'

import time
import unittest
from dbtextdb import *


class DBTextTest(unittest.TestCase):

  def setUp(self):
    self.time_now = '%s' % int(time.time())
    self.time_now = self.time_now[0:-2] + '00'

  def testParseQuery(self):
    db_conn = DBText('./tests')
    # bad command
    query_bad_command = 'selecta * from table;'
    self.assertRaises(ParseError, db_conn.ParseQuery, query_bad_command)
    #  normal query
    query_normal = 'select * from subscriber;'
    db_conn.ParseQuery(query_normal)
    self.assert_(db_conn.command == 'SELECT')
    self.assert_(db_conn.table == 'subscriber')
    self.assert_(db_conn.columns == ['*'])
    db_conn.CleanUp()
    # normal query with condition
    query_normal_cond = 'select * from subscriber where column="value";'
    db_conn.ParseQuery(query_normal_cond)
    self.assert_(db_conn.command == 'SELECT')
    self.assert_(db_conn.table == 'subscriber')
    self.assert_(db_conn.columns == ['*'])
    self.assert_(db_conn.strings == ['value'])
    self.assert_(not db_conn.count)
    self.assert_(db_conn.conditions == {'column': 'value'})
    db_conn.CleanUp()
    # normal query with multiple conditions
    query_normal_cond = ('select * from subscriber where column="value1" and '
                         'col2=" another value " and col3= foo and a="";')
    db_conn.ParseQuery(query_normal_cond)
    self.assert_(db_conn.command == 'SELECT')
    self.assert_(db_conn.table == 'subscriber')
    self.assert_(db_conn.columns == ['*'])
    self.assert_(db_conn.strings == ['value1', ' another value ', ''])
    self.assertEqual(db_conn.conditions, {'column': 'value1',
                                          'col2': ' another value ',
                                          'col3': 'foo', 'a': ''})
    db_conn.CleanUp()
    # normal query with count
    query_normal_count = 'select count(*) from subscriber;'
    db_conn.ParseQuery(query_normal_count)
    self.assert_(db_conn.command == 'SELECT')
    self.assert_(db_conn.table == 'subscriber')
    self.assert_(db_conn.columns == ['*'])
    self.assert_(db_conn.count == True)
    db_conn.CleanUp()
    # normal query with now()
    query_normal_count = 'select count(*) from subscriber where time=now();'
    db_conn.ParseQuery(query_normal_count)
    self.assert_(db_conn.command == 'SELECT')
    self.assert_(db_conn.table == 'subscriber')
    self.assert_(db_conn.columns == ['*'])
    self.assert_(db_conn.count == True)
    self.assertEqual(db_conn.conditions, {'time': self.time_now})
    db_conn.CleanUp()
    # normal delete query
    query_normal_delete = 'delete from subscriber where foo = 2;'
    db_conn.ParseQuery(query_normal_delete)
    self.assert_(db_conn.command == 'DELETE')
    self.assert_(db_conn.table == 'subscriber')
    self.assertEqual(db_conn.conditions, {'foo': '2'})
    db_conn.CleanUp()
    # normal insert values query with no into
    query_normal_insert_values = ('insert subscriber (col1, col2, col3) '
                                  'values (1, "foo", "");')
    db_conn.ParseQuery(query_normal_insert_values)
    self.assert_(db_conn.command == 'INSERT')
    self.assert_(db_conn.table == 'subscriber')
    self.assertEqual(db_conn.targets, {'col1': '1', 'col2': 'foo', 'col3': ''})
    db_conn.CleanUp()
    # normal insert values query with into
    query_normal_insert_into_values = ('insert into subscriber (col1, col2) '
                                       'values (1, "foo");')
    db_conn.ParseQuery(query_normal_insert_into_values)
    self.assert_(db_conn.command == 'INSERT')
    self.assert_(db_conn.table == 'subscriber')
    self.assertEqual(db_conn.targets, {'col1': '1', 'col2': 'foo'})
    db_conn.CleanUp()
    # normal insert values query with now()
    query_normal_insert_into_values = ('insert into subscriber (a, b, c) '
                                       'values (NOW(), "foo", now());')
    db_conn.ParseQuery(query_normal_insert_into_values)
    self.assert_(db_conn.command == 'INSERT')
    self.assert_(db_conn.table == 'subscriber')
    self.assertEqual(db_conn.targets, {'a': self.time_now, 'b': 'foo',
                                       'c': self.time_now})
    db_conn.CleanUp()
    # bad insert: missing table
    bad_insert_query_missing_table = ('insert into (col1, col2) '
                                      'values (1, "foo");')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_insert_query_missing_table)
    db_conn.CleanUp()
    # bad insert: missing parens
    bad_insert_query_missing_parens = ('insert into test col1, col2 '
                                       'values (1, "foo");')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_insert_query_missing_parens)
    db_conn.CleanUp()
    # bad insert: missing paren
    bad_insert_query_missing_paren = ('insert into test (col1, col2) '
                                      'values 1, "foo");')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_insert_query_missing_paren)
    db_conn.CleanUp()
    # bad insert: missing quote
    bad_insert_query_missing_quote = ('insert into test (col1, col2) '
                                      '(values 1, "foo);')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_insert_query_missing_quote)
    db_conn.CleanUp()
    # bad insert: missing values
    bad_insert_query_missing_values = ('insert into test (col1, col2) '
                                       '( 1, "foo");')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_insert_query_missing_values)
    db_conn.CleanUp()
    # bad insert: mislplaced values
    bad_insert_query_misplaced_values = ('insert into test values (col1, col2) '
                                         '( 1, "foo");')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_insert_query_misplaced_values)
    db_conn.CleanUp()
    # bad insert: extra values
    bad_insert_query_extra_values = ('insert into test values (col1, col2) '
                                     ' values values ( 1, "foo");')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_insert_query_extra_values)
    db_conn.CleanUp()
    # bad insert: extra paren set
    bad_insert_query_extra_paren_set = ('insert into test values (col1, col2) '
                                        ' values ( 1, "foo")();')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_insert_query_extra_paren_set)
    db_conn.CleanUp()
    # bad insert: mismatched value pairs
    bad_insert_query_mismatched_vals = ('insert into test values (col1, col2) '
                                        ' values ("foo");')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_insert_query_mismatched_vals)
    db_conn.CleanUp()
    # normal insert set  query with no into
    query_normal_insert_set = ('insert subscriber set col= 1, col2 ="\'f\'b";')
    db_conn.ParseQuery(query_normal_insert_set)
    self.assert_(db_conn.command == 'INSERT')
    self.assert_(db_conn.table == 'subscriber')
    self.assertEqual(db_conn.targets, {'col': '1', 'col2': '\'f\'b'})
    db_conn.CleanUp()
    # normal update
    query_normal_update = ('update subscriber set col1= 1, col2 ="foo";')
    db_conn.ParseQuery(query_normal_update)
    self.assert_(db_conn.command == 'UPDATE')
    self.assert_(db_conn.table == 'subscriber')
    self.assertEqual(db_conn.targets, {'col1': '1', 'col2': 'foo'})
    db_conn.CleanUp()
    # normal update with condition
    query_normal_update_cond = ('update subscriber set col1= 1, col2 ="foo" '
                                'where   foo = "bar" and id=1 and a="";')
    db_conn.ParseQuery(query_normal_update_cond)
    self.assert_(db_conn.command == 'UPDATE')
    self.assert_(db_conn.table == 'subscriber')
    self.assertEqual(db_conn.targets, {'col1': '1', 'col2': 'foo'})
    self.assertEqual(db_conn.conditions, {'foo': 'bar', 'id': '1', 'a': ''})
    db_conn.CleanUp()
    # bad update: extra parens
    bad_update_query_extra_paren = ('update test set (col1 = "foo");')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_update_query_extra_paren)
    db_conn.CleanUp()
    # bad update: missing table
    bad_update_query_missing_table = ('update SET col1 = "foo";')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_update_query_missing_table)
    db_conn.CleanUp()
    # bad update: missing set
    bad_update_query_missing_set = ('update test sett col1 = "foo";')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_update_query_missing_set)
    db_conn.CleanUp()
    # bad update: missing val
    bad_update_query_missing_val = ('update test set col1 =;')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_update_query_missing_val)
    db_conn.CleanUp()
    # bad update: missing comma
    bad_update_query_missing_comma = ('update test set col1 = "foo" crap =5;')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_update_query_missing_comma)
    db_conn.CleanUp()
    # bad update: missing equal
    bad_update_query_missing_equal = ('update test set col1 = "foo", and 5;')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_update_query_missing_equal)
    db_conn.CleanUp()
    # bad update: missing col
    bad_update_query_missing_col = ('update test set col1 = "foo", = 5;')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_update_query_missing_col)
    db_conn.CleanUp()
    # bad update: double col
    bad_update_query_double_col = ('update test set col1 = "foo", and a = 5;')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_update_query_double_col)
    db_conn.CleanUp()
    # normal query with multiple columns
    query_normal_count = 'select col1, "col 2",col3  , "col4" from subscriber;'
    db_conn.ParseQuery(query_normal_count)
    self.assert_(db_conn.command == 'SELECT')
    self.assert_(db_conn.table == 'subscriber')
    self.assert_(db_conn.strings == ['col 2', 'col4'])
    self.assert_(db_conn.columns == ['col1', "'col 2'", 'col3', "'col4'"])
    db_conn.CleanUp()
    # normal query with ORDER BY
    query_normal_order_by = ('select col1, col2 from test'
                             ' ORDER by col1;')
    db_conn.ParseQuery(query_normal_order_by)
    self.assert_(db_conn.command == 'SELECT')
    self.assert_(db_conn.table == 'test')
    self.assert_(db_conn.columns == ['col1', 'col2'])
    self.assert_(db_conn.order_by == 'col1')
    db_conn.CleanUp()
    # normal query with ORDER BY with conditions
    query_normal_order_by_cond = ('select col1, col2 from test where col="asdf"'
                                  ' and col2  = "foo" ORDER by col;')
    db_conn.ParseQuery(query_normal_order_by_cond)
    self.assert_(db_conn.command == 'SELECT')
    self.assert_(db_conn.table == 'test')
    self.assert_(db_conn.columns == ['col1', 'col2'])
    self.assert_(db_conn.conditions == {'col': 'asdf', 'col2': 'foo'})
    self.assert_(db_conn.order_by == 'col')
    db_conn.CleanUp()
    # normal query with CONCAT
    query_normal_concat = ('select concat(uname,"@", domain) as email_addr '
                           'from subscriber where id=3;')
    db_conn.ParseQuery(query_normal_concat)
    self.assert_(db_conn.command == 'SELECT')
    self.assert_(db_conn.table == 'subscriber')
    self.assert_(db_conn.columns == ['email_addr'])
    self.assert_(db_conn.conditions == {'id': '3'})
    self.assert_(db_conn.aliases == {'email_addr': ['uname', "'@'", 'domain']})
    db_conn.CleanUp()
    # normal query with multiple CONCAT
    query_normal_mult_concat = ('select concat(uname,"@", domain) as email,'
                                ' foo as "bar" from table where id=3;')
    db_conn.ParseQuery(query_normal_mult_concat)
    self.assert_(db_conn.command == 'SELECT')
    self.assert_(db_conn.table == 'table')
    self.assert_(db_conn.columns == ['email', "'bar'"])
    self.assert_(db_conn.conditions == {'id': '3'})
    self.assert_(db_conn.aliases == {"'bar'": ['foo'],
                                     'email': ['uname', "'@'", 'domain']})
    db_conn.CleanUp()
    # bad query with CONCAT missing AS
    bad_query_concat_no_as = ('select concat(col1,col2) from test'
                              ' ORDER by col1 col2;')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_query_concat_no_as)
    db_conn.CleanUp()
    # bad query with CONCAT missing AS arg
    bad_query_concat_no_as_arg = ('select concat(col1,col2) as from test'
                                  ' ORDER by col1 col2;')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_query_concat_no_as_arg)
    db_conn.CleanUp()
    # bad query with CONCAT missing paren
    bad_query_concat_no_paren = ('select concat(col1,col2  as foo from test'
                                 ' ORDER by col1 col2;')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_query_concat_no_paren)
    db_conn.CleanUp()
    # bad query with ORDER BY multiple columns
    bad_query_mult_order_by = ('select col1, col2 from test'
                               ' ORDER by col1 col2;')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_query_mult_order_by)
    db_conn.CleanUp()
    # bad select query: missing FROM
    bad_query_missing_from = 'select * subscriber;'
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_query_missing_from)
    db_conn.CleanUp()
    # bad select query: missing comma in columns
    bad_query_missing_comma = 'select col1 col2 col3 from subscriber;'
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_query_missing_comma)
    db_conn.CleanUp()
    # bad select query: extra comma in columns
    bad_query_extra_comma = 'select col1,col2, from subscriber;'
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_query_extra_comma)
    db_conn.CleanUp()
    bad_query_extra_comma = 'select col1,,col2 from subscriber;'
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_query_extra_comma)
    db_conn.CleanUp()
    bad_query_extra_comma = 'select ,col1,col2 from subscriber;'
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_query_extra_comma)
    db_conn.CleanUp()
    # bad conditions: missing AND
    bad_query_missing_and = ('select * from subscriber where column = asdf '
                             ' something=missing_and;')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_query_missing_and)
    db_conn.CleanUp()
    # bad conditions: missing value
    bad_query_missing_value = ('select * from subscriber where column = asdf'
                               ' and something=;')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_query_missing_value)
    db_conn.CleanUp()
    # bad query: unterminated string
    bad_query_unterm_str = ('select * from test where column ="asdf;')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_query_unterm_str)
    db_conn.CleanUp()
    # bad select query: missing table
    bad_select_query_missing_table = ('select * from where column ="asdf";')
    self.assertRaises(ParseError, db_conn.ParseQuery,
                      bad_select_query_missing_table)
    db_conn.CleanUp()

  def testOpenTable(self):
    # check that header is retrieved and parsed correctly
    query = ('select * from test;')
    db_conn = DBText('./tests')
    db_conn.ParseQuery(query)
    db_conn.OpenTable()
    self.assertEqual(db_conn.header, {'col2': {'auto': False, 'null': True,
                                               'type': 'string', 'pos': 2},
                                      'id': {'auto': True, 'null': False,
                                             'type': 'int', 'pos': 1},
                                      'col1': {'auto': False, 'null': False,
                                               'type': 'string', 'pos': 0}})

    # check that data is retrieved and parsed correctly
    query = ('select * from test;')
    db_conn = DBText('./tests')
    db_conn.ParseQuery(query)
    db_conn.OpenTable()
    self.assertEqual(db_conn.data,
                     [{'col1': 'item1\\:', 'id': 1, 'col2': 'item2'},
                      {'col1': 'it\\:em1\\\\', 'id': 2, 'col2': ''},
                      {'col1': '\\:item3', 'id': 3, 'col2': 'asdf\\:'}])

    # missing table
    query = ('select * from non_existent_table;')
    db_conn = DBText('./tests')
    db_conn.ParseQuery(query)
    self.assertRaises(ExecuteError, db_conn.OpenTable)

    # type string value in type int column
    query = ('select * from bad_table_wrong_type;')
    db_conn = DBText('./tests')
    db_conn.ParseQuery(query)
    self.assertRaises(ExecuteError, db_conn.OpenTable)

    # row has fewer fields than header
    query = ('select * from bad_table_short_row;')
    db_conn = DBText('./tests')
    db_conn.ParseQuery(query)
    self.assertRaises(ExecuteError, db_conn.OpenTable)

    # row has more fields than header
    query = ('select * from bad_table_long_row;')
    db_conn = DBText('./tests')
    db_conn.ParseQuery(query)
    self.assertRaises(ExecuteError, db_conn.OpenTable)

    # value mismatch: non-null column is null
    query = ('select * from bad_table_null;')
    db_conn = DBText('./tests')
    db_conn.ParseQuery(query)
    self.assertRaises(ExecuteError, db_conn.OpenTable)

    # value mismatch: int column is string
    query = ('select * from bad_table_int;')
    db_conn = DBText('./tests')
    db_conn.ParseQuery(query)
    self.assertRaises(ExecuteError, db_conn.OpenTable)

  def testExecute(self):
    db_conn = DBText('./tests')
    writethru = False

    # test count
    query = ("select count(*) from subscriber where username='monitor' and"
             " domain='test.com';")
    result = db_conn.Execute(query, writethru)
    self.assertEqual(result, [2])
    db_conn.CleanUp()

    query = ('select count(*) from subscriber where '
             "username='test2';")
    result = db_conn.Execute(query, writethru)
    self.assertEqual(result, [1])
    db_conn.CleanUp()

    query = ('select count(*) from subscriber where '
             "username='test1';")
    result = db_conn.Execute(query, writethru)
    self.assertEqual(result, [3])
    db_conn.CleanUp()

    # test concat
    query = ("select concat(username, '@', domain) as email_addr from "
             'subscriber where id = 3;')
    result = db_conn.Execute(query, writethru)
    self.assertEqual(result, [['test2@test.com']])
    db_conn.CleanUp()

    # test select
    query = ("select * from subscriber where username='test2' and"
             " domain='test.com';")
    expected_result = [[3, 'test2', 'test.com', 'password', '', '',
                        'test-team@test.com', 1202336327,
                        '9fe9bfa1315b8202838838c3807a0a32',
                        'fac1f260ebda200719de4aa29880ee05', '', '']]
    result = db_conn.Execute(query, writethru)
    self.assertEqual(result, expected_result)
    db_conn.CleanUp()

    query = ('select * from subscriber where id = 3;')
    expected_result = [[3, 'test2', 'test.com', 'password', '', '',
                        'test-team@test.com', 1202336327,
                        '9fe9bfa1315b8202838838c3807a0a32',
                        'fac1f260ebda200719de4aa29880ee05', '', '']]
    result = db_conn.Execute(query, writethru)
    self.assertEqual(result, expected_result)
    db_conn.CleanUp()

    # test order by
    query = ('select * from test order by non_existent_column;')
    self.assertRaises(ExecuteError, db_conn.Execute, query, writethru)
    db_conn.CleanUp()

    query = ('select * from unsorted_table order by id;')
    result = db_conn.Execute(query, writethru)
    self.assertEqual(result, [[1, 'fred', 'test.com', 2125551234],
                              [2, 'james', 'test4.com', 2125551231],
                              [3, 'mike', 'test2.com', 2125551239],
                              [4, 'alex', 'test1.com', 2125551237],
                              [5, 'john', 'test.com', 2125551240]])
    db_conn.CleanUp()

    query = ('select * from unsorted_table order by user;')
    result = db_conn.Execute(query, writethru)
    self.assertEqual(result, [[4, 'alex', 'test1.com', 2125551237],
                              [1, 'fred', 'test.com', 2125551234],
                              [2, 'james', 'test4.com', 2125551231],
                              [5, 'john', 'test.com', 2125551240],
                              [3, 'mike', 'test2.com', 2125551239]])
    db_conn.CleanUp()

    query = ('select * from unsorted_table order by domain;')
    result = db_conn.Execute(query, writethru)
    self.assertEqual(result, [[1, 'fred', 'test.com', 2125551234],
                              [5, 'john', 'test.com', 2125551240],
                              [4, 'alex', 'test1.com', 2125551237],
                              [3, 'mike', 'test2.com', 2125551239],
                              [2, 'james', 'test4.com', 2125551231]])
    db_conn.CleanUp()

    query = ('select * from unsorted_table order by number;')
    result = db_conn.Execute(query, writethru)
    self.assertEqual(result, [[2, 'james', 'test4.com', 2125551231],
                              [1, 'fred', 'test.com', 2125551234],
                              [4, 'alex', 'test1.com', 2125551237],
                              [3, 'mike', 'test2.com', 2125551239],
                              [5, 'john', 'test.com', 2125551240]])
    db_conn.CleanUp()

    # test delete
    query = ('delete from unsorted_table where id = 3;')
    result = db_conn.Execute(query, writethru)
    self.assertEqual(result, [1])
    self.assertEqual(db_conn.data, [{'id': 1, 'user': 'fred', 'domain':
                                     'test.com', 'number': 2125551234},
                                    {'id': 4, 'user': 'alex', 'domain':
                                     'test1.com', 'number': 2125551237},
                                    {'id': 2, 'user': 'james', 'domain':
                                     'test4.com', 'number': 2125551231},
                                    {'id': 5, 'user': 'john', 'domain':
                                     'test.com', 'number': 2125551240}])
    db_conn.CleanUp()

    query = ('delete from unsorted_table where id = 5;')
    result = db_conn.Execute(query, writethru)
    self.assertEqual(db_conn.data, [{'id': 1, 'user': 'fred', 'domain':
                                     'test.com', 'number': 2125551234},
                                    {'id': 4, 'user': 'alex', 'domain':
                                     'test1.com', 'number': 2125551237},
                                    {'id': 2, 'user': 'james', 'domain':
                                     'test4.com', 'number': 2125551231},
                                    {'id': 3, 'user': 'mike', 'domain':
                                     'test2.com', 'number': 2125551239}])
    db_conn.CleanUp()

    # test insert with auto increment
    query = ("insert into unsorted_table set user='jake', domain='test.com',"
             'number = 2125551456;')
    result = db_conn.Execute(query, writethru)
    self.assertEqual(db_conn.data, [{'id': 1, 'user': 'fred', 'domain':
                                     'test.com', 'number': 2125551234},
                                    {'id': 4, 'user': 'alex', 'domain':
                                     'test1.com', 'number': 2125551237},
                                    {'id': 2, 'user': 'james', 'domain':
                                     'test4.com', 'number': 2125551231},
                                    {'id': 3, 'user': 'mike', 'domain':
                                     'test2.com', 'number': 2125551239},
                                    {'id': 5, 'user': 'john', 'domain':
                                     'test.com', 'number': 2125551240},
                                    {'id': 6, 'user': 'jake', 'domain':
                                     'test.com', 'number': 2125551456}])
    db_conn.CleanUp()

    # test insert with null value
    query = ("insert into test set col1='asdf';")
    result = db_conn.Execute(query, writethru)
    self.assertEqual(db_conn.data, [{'col2': 'item2', 'id': 1, 'col1':
                                     'item1\\:'},
                                    {'col2': '', 'id': 2, 'col1':
                                     'it\\:em1\\\\'},
                                    {'col2': 'asdf\\:', 'id': 3, 'col1':
                                     '\\:item3'},
                                    {'col2': '', 'id': 4, 'col1': 'asdf'}])
    db_conn.CleanUp()

    # test insert with null value alternate syntax
    query = ("insert test ( col1) values ('asdf');")
    result = db_conn.Execute(query, writethru)
    self.assertEqual(db_conn.data, [{'col2': 'item2', 'id': 1, 'col1':
                                     'item1\\:'},
                                    {'col2': '', 'id': 2, 'col1':
                                     'it\\:em1\\\\'},
                                    {'col2': 'asdf\\:', 'id': 3, 'col1':
                                     '\\:item3'},
                                    {'col2': '', 'id': 4, 'col1': 'asdf'}])
    db_conn.CleanUp()

    # test insert with colon inside value
    query = ("insert into test set col1='as:df';")
    result = db_conn.Execute(query, writethru)
    self.assertEqual(db_conn.data, [{'col2': 'item2', 'id': 1, 'col1':
                                     'item1\\:'},
                                    {'col2': '', 'id': 2, 'col1':
                                     'it\\:em1\\\\'},
                                    {'col2': 'asdf\\:', 'id': 3, 'col1':
                                     '\\:item3'},
                                    {'col2': '', 'id': 4, 'col1': 'as\:df'}])
    db_conn.CleanUp()

    # test insert with escaped colon inside value
    query = ("insert into test set col1='as\:df';")
    result = db_conn.Execute(query, writethru)
    self.assertEqual(db_conn.data, [{'col2': 'item2', 'id': 1, 'col1':
                                     'item1\\:'},
                                    {'col2': '', 'id': 2, 'col1':
                                     'it\\:em1\\\\'},
                                    {'col2': 'asdf\\:', 'id': 3, 'col1':
                                     '\\:item3'},
                                    {'col2': '', 'id': 4, 'col1': 'as\\\\\\:df'}])
    db_conn.CleanUp()

    # bad insert with non-null column not provided
    query = ("insert test ( col2) values ('asdf');")
    self.assertRaises(ExecuteError, db_conn.Execute, query, writethru)
    db_conn.CleanUp()

    # bad insert with auto column forced
    query = ("insert test (col1, id) values ('asdf', 4);")
    self.assertRaises(ExecuteError, db_conn.Execute, query, writethru)
    db_conn.CleanUp()

    # test update with null value
    query = ("update test set col2='' where id = 3;")
    result = db_conn.Execute(query, writethru)
    self.assertEqual(db_conn.data, [{'col2': 'item2', 'id': 1, 'col1':
                                     'item1\\:'},
                                    {'col2': '', 'id': 2, 'col1':
                                     'it\\:em1\\\\'},
                                    {'col2': '', 'id': 3, 'col1':
                                     '\\:item3'}])
    db_conn.CleanUp()


if __name__ == '__main__':
  unittest.main()
