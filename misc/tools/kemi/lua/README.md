# Tools For KEMI Lua #

Tools useful for using Kamailio with KEMI Lua scripts.

## kemiksrlib.py ##

Generates the file `KSR.lua` with skeleton functions in the `KSR` object. The
function parameters are prefixed with `p_` and the functions return:

  * empty string - in case of functions returning `xval`
  * `1` - in case of functions returning `int`
  * `true` - in case of functions returning `int`
  * nothing - in case of void functions
  * `nil` - in the other cases

Useful to use for syntax checking and functions prototypes.

Usage:

```
python3 kemiksrlib.py /path/to/src/kamailio
```

## kemiluacheck.py ##

Generates the file `KSR.luacheckrc` that can be used with `luacheck` to verify
if the KEMI Lua script uses undefined symbols (e.g., functions that are not
in the `KSR` library due to typos).

Usage:

```
python3 kemiluacheck.py /path/to/src/kamailio

luacheck --config KSR.luacheckrc kamailio-basic-kemi-lua.lua -d
```

Example output with `KSR` correct functions:

```
Checking kamailio-basic-kemi-lua.lua              5 warnings

    kamailio-basic-kemi-lua.lua:67:10: unused global variable ksr_request_route
    kamailio-basic-kemi-lua.lua:389:10: unused global variable ksr_branch_manage
    kamailio-basic-kemi-lua.lua:398:10: unused global variable ksr_onreply_manage
    kamailio-basic-kemi-lua.lua:409:10: unused global variable ksr_failure_manage
    kamailio-basic-kemi-lua.lua:420:10: unused global variable ksr_reply_route

Total: 5 warnings / 0 errors in 1 file
```

Example output with `KSR.is_CANCLE()` typo:

```
Checking kamailio-basic-kemi-lua.lua              6 warnings

    kamailio-basic-kemi-lua.lua:67:10: unused global variable ksr_request_route
    kamailio-basic-kemi-lua.lua:76:5: accessing undefined field is_CANCLE of global KSR
    kamailio-basic-kemi-lua.lua:389:10: unused global variable ksr_branch_manage
    kamailio-basic-kemi-lua.lua:398:10: unused global variable ksr_onreply_manage
    kamailio-basic-kemi-lua.lua:409:10: unused global variable ksr_failure_manage
    kamailio-basic-kemi-lua.lua:420:10: unused global variable ksr_reply_route

Total: 6 warnings / 0 errors in 1 file
```

## Other Tools ##

Other external tools useful to check the Lua scripts.

### luac ###

The `luac` (the Lua compiler) can be used with parameter `-p` to check for
syntax errors (e.g., missing closing quote or parenthesis).

Example - when there is a missing closing quote, like:

```
KSR.hdr.remove("Route);
```

the check results in:

```
luac -p kamailio-basic-kemi-lua.lua

luac: kamailio-basic-kemi-lua.lua:100: unfinished string near '"Route);'
```
