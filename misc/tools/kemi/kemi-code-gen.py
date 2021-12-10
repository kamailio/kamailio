# Python 3 helper program for KEMI
# - print IF conditions with param types for KEMI interpreters
# - print typedefs for functions

PRINTPARAMS=3
# - print mode: typedefs, js, lua, python, pythonparams, ruby, sqlang
PRINTMODE="lua"
# - two tabs for python params, three for the other cases
# PRINTTABS="\t\t"
PRINTTABS="\t\t\t"
PRINTELSE=""

def printCodeFuncTypedefs(prefix):
	sfunc = "typedef int (*sr_kemi_fm" + prefix + "_f)(sip_msg_t*"
	", str*, str*, str*, str*, str*);"
	for i, c in enumerate(prefix):
		if c == 's':
			sfunc += ", str*"
		else:
			sfunc += ", int"
	sfunc += ");"
	print(sfunc)


def printCodeIfEnd(sretfunc):
	print(PRINTTABS + "} else {")
	print(PRINTTABS + "\tLM_ERR(\"invalid parameters for: %.*s\\n\", fname->len, fname->s);")
	print(PRINTTABS + "\treturn " + sretfunc + ";")
	print(PRINTTABS + "}")


def printCodeIfParams(prefix):
	global PRINTELSE
	sparams = ""
	for i, c in enumerate(prefix):
		if i==0:
			if c == 's':
				print(PRINTTABS + PRINTELSE + "if(ket->ptypes[0]==SR_KEMIP_STR")
				sparams += "&vps[" + str(i) +"].s, "
			else:
				print(PRINTTABS + PRINTELSE + "if(ket->ptypes[0]==SR_KEMIP_INT")
				sparams += "vps[" + str(i) +"].n, "
			PRINTELSE = "} else "
		elif i==PRINTPARAMS-1:
			if c == 's':
				print(PRINTTABS + "\t\t&& ket->ptypes[" + str(i) + "]==SR_KEMIP_STR) {")
				sparams += "&vps[" + str(i) +"].s);"
			else:
				print(PRINTTABS + "\t\t&& ket->ptypes[" + str(i) + "]==SR_KEMIP_INT) {")
				sparams += "vps[" + str(i) +"].n);"
		else:
			if c == 's':
				print(PRINTTABS + "\t\t&& ket->ptypes[" + str(i) + "]==SR_KEMIP_STR")
				sparams += "&vps[" + str(i) +"].s, "
			else:
				print(PRINTTABS + "\t\t&& ket->ptypes[" + str(i) + "]==SR_KEMIP_INT")
				sparams += "vps[" + str(i) +"].n, "
	return sparams


def printCodeIfJS(prefix):
	global PRINTELSE
	sparams = printCodeIfParams(prefix)
	print("\t\t\t\tif(ket->rtype==SR_KEMIP_XVAL) {")
	sfunc = PRINTTABS + "\t\txret = ((sr_kemi_xfm" + prefix + "_f)(ket->func))(env_J->msg,\n" + PRINTTABS + "\t\t\t"
	print(sfunc + sparams)
	print(PRINTTABS + "\t\treturn sr_kemi_jsdt_return_xval(J, ket, xret);")
	print("\t\t\t\t} else {")
	sfunc = PRINTTABS + "\t\tret = ((sr_kemi_fm" + prefix + "_f)(ket->func))(env_J->msg,\n" + PRINTTABS + "\t\t\t"
	print(sfunc + sparams)
	print(PRINTTABS + "\t\treturn sr_kemi_jsdt_return_int(J, ket, ret);")
	print("\t\t\t\t}")


def printCodeIfLua(prefix):
	global PRINTELSE
	sparams = printCodeIfParams(prefix)
	print("\t\t\t\tif(ket->rtype==SR_KEMIP_XVAL) {")
	sfunc = PRINTTABS + "\t\txret = ((sr_kemi_xfm" + prefix + "_f)(ket->func))(env_L->msg,\n" + PRINTTABS + "\t\t\t"
	print(sfunc + sparams)
	print(PRINTTABS + "\t\treturn sr_kemi_lua_return_xval(L, ket, xret);")
	print("\t\t\t\t} else {")
	sfunc = PRINTTABS + "\t\tret = ((sr_kemi_fm" + prefix + "_f)(ket->func))(env_L->msg,\n" + PRINTTABS + "\t\t\t"
	print(sfunc + sparams)
	print(PRINTTABS + "\t\treturn sr_kemi_lua_return_int(L, ket, ret);")
	print("\t\t\t\t}")


def printCodeIfPython(prefix):
	global PRINTELSE
	sparams = printCodeIfParams(prefix)
	print("\t\t\t\tif(ket->rtype==SR_KEMIP_XVAL) {")
	sfunc = PRINTTABS + "\t\txret = ((sr_kemi_xfm" + prefix + "_f)(ket->func))(lmsg,\n" + PRINTTABS + "\t\t\t"
	print(sfunc + sparams)
	print(PRINTTABS + "\t\treturn sr_kemi_apy_return_xval(ket, xret);")
	print("\t\t\t\t} else {")
	sfunc = PRINTTABS + "\t\tret = ((sr_kemi_fm" + prefix + "_f)(ket->func))(lmsg,\n" + PRINTTABS + "\t\t\t"
	print(sfunc + sparams)
	print(PRINTTABS + "\t\treturn sr_kemi_apy_return_int(ket, ret);")
	print("\t\t\t\t}")


def printCodeIfPythonParams(prefix):
	global PRINTELSE
	sfunc = PRINTTABS + "\tif(!PyArg_ParseTuple(args, \"" + prefix + ":kemi-param-" + prefix + "\",\n" + PRINTTABS + "\t\t\t"
	slen = ""
	sdbg = PRINTTABS + "\tLM_DBG(\"params[%d] for: %.*s are:"
	sval = ""
	for i, c in enumerate(prefix):
		if i==0:
			if c == 's':
				print(PRINTTABS + PRINTELSE + "if(ket->ptypes[0]==SR_KEMIP_STR")
				sfunc += "&vps[" + str(i) +"].s.s, "
				slen += PRINTTABS + "\tvps[" + str(i) + "].s.len = strlen(vps[" + str(i) + "].s.s);\n"
				sdbg += " [%s]"
				sval += "vps[" + str(i) + "].s.s, "
			else:
				print(PRINTTABS + PRINTELSE + "if(ket->ptypes[0]==SR_KEMIP_INT")
				sfunc += "&vps[" + str(i) +"].n, "
				sdbg += " [%d]"
				sval += "vps[" + str(i) + "].n, "
			PRINTELSE = "} else "
		elif i==PRINTPARAMS-1:
			if c == 's':
				print(PRINTTABS + "\t\t&& ket->ptypes[" + str(i) + "]==SR_KEMIP_STR) {")
				sfunc += "&vps[" + str(i) +"].s.s)) {"
				slen += PRINTTABS + "\tvps[" + str(i) + "].s.len = strlen(vps[" + str(i) + "].s.s);\n"
				sdbg += " [%s]"
				sval += "vps[" + str(i) + "].s.s"
			else:
				print(PRINTTABS + "\t\t&& ket->ptypes[" + str(i) + "]==SR_KEMIP_INT) {")
				sfunc += "&vps[" + str(i) +"].n)) {"
				sdbg += " [%d]"
				sval += "vps[" + str(i) + "].n"
		else:
			if c == 's':
				print(PRINTTABS + "\t\t&& ket->ptypes[" + str(i) + "]==SR_KEMIP_STR")
				sfunc += "&vps[" + str(i) +"].s.s, "
				slen += PRINTTABS + "\tvps[" + str(i) + "].s.len = strlen(vps[" + str(i) + "].s.s);\n"
				sdbg += " [%s]"
				sval += "vps[" + str(i) + "].s.s, "
			else:
				print(PRINTTABS + "\t\t&& ket->ptypes[" + str(i) + "]==SR_KEMIP_INT")
				sfunc += "&vps[" + str(i) +"].n, "
				sdbg += " [%d]"
				sval += "vps[" + str(i) + "].n, "
	print(sfunc)
	print(PRINTTABS + "\t\tLM_ERR(\"unable to retrieve " + prefix + " params %d\\n\", i);")
	print(PRINTTABS + "\t\treturn sr_kemi_apy_return_false();")
	print(PRINTTABS + "\t}")
	print(slen)
	print(sdbg + "\\n\",\n" + PRINTTABS + "\t\t\t" + "i, fname.len, fname.s,")
	print(PRINTTABS + "\t\t\t" + sval + ");")


def printCodeIfRuby(prefix):
	global PRINTELSE
	sfunc = PRINTTABS + "\tret = ((sr_kemi_fm" + prefix + "_f)(ket->func))(env_R->msg,\n" + PRINTTABS + "\t\t\t"
	for i, c in enumerate(prefix):
		if i==0:
			if c == 's':
				print(PRINTTABS + PRINTELSE + "if(ket->ptypes[0]==SR_KEMIP_STR")
				sfunc += "&vps[" + str(i) +"].s, "
			else:
				print(PRINTTABS + PRINTELSE + "if(ket->ptypes[0]==SR_KEMIP_INT")
				sfunc += "vps[" + str(i) +"].n, "
			PRINTELSE = "} else "
		elif i==PRINTPARAMS-1:
			if c == 's':
				print(PRINTTABS + "\t\t&& ket->ptypes[" + str(i) + "]==SR_KEMIP_STR) {")
				sfunc += "&vps[" + str(i) +"].s);"
			else:
				print(PRINTTABS + "\t\t&& ket->ptypes[" + str(i) + "]==SR_KEMIP_INT) {")
				sfunc += "vps[" + str(i) +"].n);"
		else:
			if c == 's':
				print(PRINTTABS + "\t\t&& ket->ptypes[" + str(i) + "]==SR_KEMIP_STR")
				sfunc += "&vps[" + str(i) +"].s, "
			else:
				print(PRINTTABS + "\t\t&& ket->ptypes[" + str(i) + "]==SR_KEMIP_INT")
				sfunc += "vps[" + str(i) +"].n, "

	print(sfunc)
	print(PRINTTABS + "\treturn sr_kemi_ruby_return_int(ket, ret);")


def printCodeIfSQLang(prefix):
	global PRINTELSE
	sfunc = PRINTTABS + "\tret = ((sr_kemi_fm" + prefix + "_f)(ket->func))(env_J->msg,\n" + PRINTTABS + "\t\t\t"
	for i, c in enumerate(prefix):
		if i==0:
			if c == 's':
				print(PRINTTABS + PRINTELSE + "if(ket->ptypes[0]==SR_KEMIP_STR")
				sfunc += "&vps[" + str(i) +"].s, "
			else:
				print(PRINTTABS + PRINTELSE + "if(ket->ptypes[0]==SR_KEMIP_INT")
				sfunc += "vps[" + str(i) +"].n, "
			PRINTELSE = "} else "
		elif i==PRINTPARAMS-1:
			if c == 's':
				print(PRINTTABS + "\t\t&& ket->ptypes[" + str(i) + "]==SR_KEMIP_STR) {")
				sfunc += "&vps[" + str(i) +"].s);"
			else:
				print(PRINTTABS + "\t\t&& ket->ptypes[" + str(i) + "]==SR_KEMIP_INT) {")
				sfunc += "vps[" + str(i) +"].n);"
		else:
			if c == 's':
				print(PRINTTABS + "\t\t&& ket->ptypes[" + str(i) + "]==SR_KEMIP_STR")
				sfunc += "&vps[" + str(i) +"].s, "
			else:
				print(PRINTTABS + "\t\t&& ket->ptypes[" + str(i) + "]==SR_KEMIP_INT")
				sfunc += "vps[" + str(i) +"].n, "

	print(sfunc)
	print(PRINTTABS + "\treturn sr_kemi_sqlang_return_int(J, ket, ret);")


# generated possible strings of length k with chars from set.
def printAllKLength(cset, k):

	n = len(cset)
	printAllKLengthRec(cset, "", n, k)


def printAllKLengthRec(cset, prefix, n, k):

	if (k == 0) :
		# print(prefix)
		if PRINTMODE == "js":
			printCodeIfJS(prefix)
		elif PRINTMODE == "lua":
			printCodeIfLua(prefix)
		elif PRINTMODE == "python":
			printCodeIfPython(prefix)
		elif PRINTMODE == "pythonparams":
			printCodeIfPythonParams(prefix)
		elif PRINTMODE == "ruby":
			printCodeIfRuby(prefix)
		elif PRINTMODE == "sqlang":
			printCodeIfSQLang(prefix)
		else:
			printCodeFuncTypedefs(prefix)
		return

	for i in range(n):
		newPrefix = prefix + cset[i]
		printAllKLengthRec(cset, newPrefix, n, k - 1)


# main statements
if __name__ == "__main__":

	charset = ['s', 'n']
	k = PRINTPARAMS
	printAllKLength(charset, k)
	if PRINTMODE == "js":
		printCodeIfEnd("app_jsdt_return_false(J)")
	elif PRINTMODE == "lua":
		printCodeIfEnd("app_lua_return_false(L)")
	elif PRINTMODE == "python":
		printCodeIfEnd("sr_kemi_apy_return_false()")
	elif PRINTMODE == "pythonparams":
		printCodeIfEnd("sr_kemi_apy_return_false()")
	elif PRINTMODE == "ruby":
		printCodeIfEnd("Qfalse")
	elif PRINTMODE == "sqlang":
		printCodeIfEnd("app_sqlang_return_false(J)")


