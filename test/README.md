Test directory
===============

Modules
-------
Note: Under development (2016-06-03 OEJ)

Modules found here test APIs in other modules or are just created for testing, not for production
use. In order to use the existing build system, each module needs a directory named mod_something
in test. 

Module testing
--------------
Each module needs a subdirectory called "test" with a test configuration and a Makefile.

A typical test script load the htable module and execute tests in the [event:htable_init]
event route when starting. Typically, if a test fails, it runs abort() from the cfg_utils
module to abort the process. 

Targets of the test makefile:
	- "test":	Test syntax with "kamailio -c"
	- "all":	Run full test


Ideas: We may need a way to exit kamailio from inside without dumping a core file, but simply
  stopping execution and returning different return values to the shell. That way a test
  config can run for a limited amount of time or until a test fails. We can also
  stop on an external action, like a RPC request calling the same function.


