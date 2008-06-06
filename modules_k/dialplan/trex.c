/* see copyright notice in trex.h */
#include "trex.h"

static const TRexChar *g_nnames[] =
{
	_TREXC("NONE"),_TREXC("OP_GREEDY"),	_TREXC("OP_OR"),
	_TREXC("OP_EXPR"),_TREXC("OP_DOT"),	_TREXC("OP_CLASS"),
	_TREXC("OP_CCLASS"),_TREXC("OP_NCLASS"),_TREXC("OP_RANGE"),_TREXC("OP_CHAR"),
	_TREXC("OP_EOL"),_TREXC("OP_BOL")
};


static int trex_list(TRex *exp);

#define trex_error(express, err_msg)\
	do{\
		LM_ERR("TREX error %s \n", (err_msg));\
		if(express->_error)\
			*(express->_error) = (err_msg);\
		if((express)->_jmpbuf)\
			longjmp(*((jmp_buf*) (express)->_jmpbuf),-1);\
	}while(0);


#define verify_newnode(_exp, _c, _n) \
	do{\
		_n = trex_newnode((_exp), (_c));\
		if(_n<0)\
			trex_error(exp, "out of memory");\
	}while(0)

static int trex_newnode(TRex *exp, TRexNodeType type)
{
	TRexNode n;
	n.type = type;
	n.next = n.right = n.left = -1;
	if(type == OP_EXPR)
		n.right = exp->_nsubexpr++;
	if(exp->_nallocated < (exp->_nsize + 1)) {
		exp->_nallocated *= 2;
		exp->_nodes = (TRexNode *)trex_shm_realloc(exp->_nodes,exp->_nallocated * sizeof(TRexNode));
	}
	exp->_nodes[exp->_nsize++] = n;
	return (int)exp->_nsize - 1;
}

static void trex_expect(TRex *exp, int n){
	if((*exp->_p) != n) 
		trex_error(exp, _TREXC("expected paren"));
	exp->_p++;
}

static TRexBool trex_ischar(TRexChar c)
{
	switch(c) {
	case TREX_SYMBOL_BRANCH:case TREX_SYMBOL_GREEDY_ZERO_OR_MORE:
	case TREX_SYMBOL_GREEDY_ZERO_OR_ONE:case TREX_SYMBOL_GREEDY_ONE_OR_MORE:
	case TREX_SYMBOL_BEGINNING_OF_STRING:case TREX_SYMBOL_END_OF_STRING:
	case TREX_SYMBOL_ANY_CHAR:case TREX_SYMBOL_ESCAPE_CHAR:case '(':case ')':case '[':case '{': case '}':
		return TRex_False;
    }
	return TRex_True;
}

static TRexChar trex_escapechar(TRex *exp)
{
	if(*exp->_p == TREX_SYMBOL_ESCAPE_CHAR){
		exp->_p++;
		switch(*exp->_p) {
		case 'n': exp->_p++; return '\n';
		case 't': exp->_p++; return '\t';
		case 'r': exp->_p++; return '\r';
		case 'f': exp->_p++; return '\f';
		default: return (*exp->_p++);
		}
	} else if(!trex_ischar(*exp->_p)) trex_error(exp,_TREXC("letter expected"));
	return (*exp->_p++);
}

static int trex_charclass(TRex *exp,int classid)
{
	int n;
	verify_newnode(exp,OP_CCLASS, n);
	exp->_nodes[n].left = classid;
	return n;
}

static int trex_charnode(TRex *exp)
{
	TRexChar t;
	int n;
	if(*exp->_p == TREX_SYMBOL_ESCAPE_CHAR) {
		exp->_p++;
		switch(*exp->_p) {
			case 'n': exp->_p++; verify_newnode(exp,'\n', n); return n;
			case 't': exp->_p++; verify_newnode(exp,'\t', n); return n;
			case 'r': exp->_p++; verify_newnode(exp,'\r', n); return n;
			case 'f': exp->_p++; verify_newnode(exp,'\f', n); return n;
			case 'a': exp->_p++; return trex_charclass(exp,'a');
			case 'A': exp->_p++; return trex_charclass(exp,'A');
			case 'w': exp->_p++; return trex_charclass(exp,'w');
			case 'W': exp->_p++; return trex_charclass(exp,'W');
			case 's': exp->_p++; return trex_charclass(exp,'s');
			case 'S': exp->_p++; return trex_charclass(exp,'S');
			case 'd': exp->_p++; return trex_charclass(exp,'d');
			case 'D': exp->_p++; return trex_charclass(exp,'D');
			case 'x': exp->_p++; return trex_charclass(exp,'x');
			case 'X': exp->_p++; return trex_charclass(exp,'X');
			case 'c': exp->_p++; return trex_charclass(exp,'c');
			case 'C': exp->_p++; return trex_charclass(exp,'C');
			case 'p': exp->_p++; return trex_charclass(exp,'p');
			case 'P': exp->_p++; return trex_charclass(exp,'P');
			case 'l': exp->_p++; return trex_charclass(exp,'l');
			case 'u': exp->_p++; return trex_charclass(exp,'u');
			default: 
				t = *exp->_p; exp->_p++;
				verify_newnode(exp,t, n); return n;
		}
	}
	else if(!trex_ischar(*exp->_p)) {
		
		trex_error(exp,_TREXC("letter expected"));
	}
	verify_newnode(exp,*exp->_p++, n);
	return n;
}

static int trex_class(TRex *exp)
{
	int ret = -1;
	int first = -1,chain;
	if(*exp->_p == TREX_SYMBOL_BEGINNING_OF_STRING){
		verify_newnode(exp,OP_NCLASS, ret);
		exp->_p++;
	}else{
		verify_newnode(exp,OP_CLASS, ret);
	}
	
	if(*exp->_p == ']' || *exp->_p == '-'){
		first = *exp->_p;
		exp->_p++;
	}
	chain = ret;
	while(*exp->_p != ']' && exp->_p != exp->_eol) {
		if(*exp->_p == '-' && first != -1){ 
			int r, t;
			if(*exp->_p++ == ']') trex_error(exp,_TREXC("unfinished range"));
			verify_newnode(exp,OP_RANGE, r);
			if(first>*exp->_p) trex_error(exp,_TREXC("invalid range"));
			if(exp->_nodes[first].type == OP_CCLASS) trex_error(exp,_TREXC("cannot use character classes in ranges"));
			exp->_nodes[r].left = exp->_nodes[first].type;
			t = trex_escapechar(exp);
			exp->_nodes[r].right = t;
            exp->_nodes[chain].next = r;
			chain = r;
			first = -1;
		}
		else{
			if(first!=-1){
				int c = first;
				exp->_nodes[chain].next = c;
				chain = c;
				first = trex_charnode(exp);
			}
			else{
				first = trex_charnode(exp);
			}
		}
	}
	if(first!=-1){
		int c = first;
		exp->_nodes[chain].next = c;
		chain = c;
		first = -1;
	}
	/* hack? */
	exp->_nodes[ret].left = exp->_nodes[ret].next;
	exp->_nodes[ret].next = -1;
	return ret;
}

static int trex_parsenumber(TRex *exp)
{
	int ret = *exp->_p-'0';
	int positions = 10;
	exp->_p++;
	while(isdigit(*exp->_p)) {
		ret = ret*10+(*exp->_p++-'0');
		if(positions==1000000000) trex_error(exp,_TREXC("overflow in numeric constant"));
		positions *= 10;
	};
	return ret;
}

static int trex_element(TRex *exp)
{
	int ret;
	switch(*exp->_p)
	{
	case '(': {
		int expr, newn;
		exp->_p++;
		verify_newnode(exp,OP_EXPR, expr);
		newn = trex_list(exp);
		exp->_nodes[expr].left = newn;
		ret = expr;
		trex_expect(exp,')');
	}
		break;
	case '[':
		exp->_p++;
		ret = trex_class(exp);
		trex_expect(exp,']');
		break;
	case TREX_SYMBOL_END_OF_STRING: exp->_p++; verify_newnode(exp,OP_EOL, ret);break;
	case TREX_SYMBOL_ANY_CHAR: exp->_p++; verify_newnode(exp,OP_DOT, ret);break;
	default:
		ret = trex_charnode(exp);
		break;
	}
	/* scope block */
	{
		int op;
		unsigned short p0 = 0, p1 = 0;
		switch(*exp->_p){
		case TREX_SYMBOL_GREEDY_ZERO_OR_MORE: p0 = 0; p1 = 0xFFFF; exp->_p++; goto __end;
		case TREX_SYMBOL_GREEDY_ONE_OR_MORE: p0 = 1; p1 = 0xFFFF; exp->_p++; goto __end;
		case TREX_SYMBOL_GREEDY_ZERO_OR_ONE: p0 = 0; p1 = 1; exp->_p++; goto __end;
		case '{':{
			exp->_p++;
			if(!isdigit(*exp->_p)) trex_error(exp,_TREXC("number expected"));
			p0 = trex_parsenumber(exp);
			switch(*exp->_p) {
			case '}':
				p1 = p0; exp->_p++;
				goto __end;
			case ',':
				exp->_p++;
				p1 = 0xFFFF;
				if(isdigit(*exp->_p)){
					p1 = trex_parsenumber(exp);
				}
				trex_expect(exp,'}');
				goto __end;
			default:
				trex_error(exp,_TREXC(", or } expected"));
			}
		}
		__end: {
				int nnode;
				verify_newnode(exp,OP_GREEDY, nnode);
				op = OP_GREEDY;
				exp->_nodes[nnode].left = ret;
				exp->_nodes[nnode].right = ((p0)<<16)|p1;
				ret = nnode;
			}
		}
	}
	if((*exp->_p != TREX_SYMBOL_BRANCH) && (*exp->_p != ')') && (*exp->_p != TREX_SYMBOL_GREEDY_ZERO_OR_MORE) && (*exp->_p != TREX_SYMBOL_GREEDY_ONE_OR_MORE) && (*exp->_p != '\0')){
		int nnode = trex_element(exp);
		exp->_nodes[ret].next = nnode;
	}
	return ret;
}

static int trex_list(TRex *exp)
{
	int ret=-1,e;
	if(*exp->_p == TREX_SYMBOL_BEGINNING_OF_STRING) {
		exp->_p++;
		verify_newnode(exp,OP_BOL, ret);
	}
	e = trex_element(exp);
	if(ret != -1) {
		exp->_nodes[ret].next = e;
	}
	else ret = e;

	if(*exp->_p == TREX_SYMBOL_BRANCH) {
		int temp, tright;
		exp->_p++;
		verify_newnode(exp,OP_OR, temp);
		exp->_nodes[temp].left = ret;
		tright = trex_list(exp);
		exp->_nodes[temp].right = tright;
		ret = temp;
	}
	return ret;
}

static TRexBool trex_matchcclass(int cclass,TRexChar c)
{
	switch(cclass) {
	case 'a': return isalpha(c)?TRex_True:TRex_False;
	case 'A': return !isalpha(c)?TRex_True:TRex_False;
	case 'w': return isalnum(c)?TRex_True:TRex_False;
	case 'W': return !isalnum(c)?TRex_True:TRex_False;
	case 's': return isspace(c)?TRex_True:TRex_False;
	case 'S': return !isspace(c)?TRex_True:TRex_False;
	case 'd': return isdigit(c)?TRex_True:TRex_False;
	case 'D': return !isdigit(c)?TRex_True:TRex_False;
	case 'x': return isxdigit(c)?TRex_True:TRex_False;
	case 'X': return !isxdigit(c)?TRex_True:TRex_False;
	case 'c': return iscntrl(c)?TRex_True:TRex_False;
	case 'C': return !iscntrl(c)?TRex_True:TRex_False;
	case 'p': return ispunct(c)?TRex_True:TRex_False;
	case 'P': return !ispunct(c)?TRex_True:TRex_False;
	case 'l': return islower(c)?TRex_True:TRex_False;
	case 'u': return isupper(c)?TRex_True:TRex_False;
	}
	return TRex_False; /*cannot happen*/
}

static TRexBool trex_matchclass(TRex* exp,TRexNode *node,TRexChar c)
{
	do {
		switch(node->type) {
			case OP_RANGE:
				if(c >= node->left && c <= node->right) return TRex_True;
				break;
			case OP_CCLASS:
				if(trex_matchcclass(node->left,c)) return TRex_True;
				break;
			default:
				if(c == node->type)return TRex_True;
		}
	} while((node->next != -1) && (node = &exp->_nodes[node->next]));
	return TRex_False;
}

static const TRexChar *trex_matchnode(TRex* exp,TRexNode *node,const TRexChar *str)
{
	TRexNodeType type = node->type;
	switch(type) {
	case OP_GREEDY: {
		int p0 = (node->right >> 16)&0x0000FFFF, p1 = node->right&0x0000FFFF, nmaches = 0;
		const TRexChar *s=str, *good = str;
		while((nmaches == 0xFFFF || nmaches < p1) 
			&& (s = trex_matchnode(exp,&exp->_nodes[node->left],s))) {
			good=s;
			nmaches++;
			if(s >= exp->_eol)
				break;
		}
		if(p0 == p1 && p0 == nmaches) return good;
		else if(nmaches >= p0 && p1 == 0xFFFF) return good;
		else if(nmaches >= p0 && nmaches <= p1) return good;
		return NULL;
	}
	case OP_OR: {
			const TRexChar *asd = str;
			TRexNode *temp=&exp->_nodes[node->left];
			while((asd = trex_matchnode(exp,temp,asd))) {
				if(temp->next != -1)
					temp = &exp->_nodes[temp->next];
				else
					return asd;
			}
			asd = str;
			temp = &exp->_nodes[node->right];
			while((asd = trex_matchnode(exp,temp,asd))) {
				if(temp->next != -1)
					temp = &exp->_nodes[temp->next];
				else
					return asd;
			}
			return NULL;
			break;
	}
	case OP_EXPR: {
			TRexNode *n = &exp->_nodes[node->left];
			const TRexChar *cur = str;
			int capture = -1;
			if(node->right == exp->_currsubexp) {
				capture = exp->_currsubexp;
				exp->_matches[capture].begin = cur;
				exp->_currsubexp++;
			}

			do {
				if(!(cur = trex_matchnode(exp,n,cur))) {
					if(capture != -1){
						exp->_matches[capture].begin = 0;
						exp->_matches[capture].len = 0;
					}
					return NULL;
				}
			} while((n->next != -1) && (n = &exp->_nodes[n->next]));

			if(capture != -1) 
				exp->_matches[capture].len = cur - exp->_matches[capture].begin;
			return cur;
	}				 
	case OP_BOL:
		if(str == exp->_bol) return str;
		return NULL;
	case OP_EOL:
		
		if(str == exp->_eol) return str;
		return NULL;
	case OP_DOT:

		str++;
		return str;
	case OP_NCLASS:
	case OP_CLASS:
		
		if(trex_matchclass(exp,&exp->_nodes[node->left],*str)?(type == OP_CLASS?TRex_True:TRex_False):(type == OP_NCLASS?TRex_True:TRex_False)) {
			str++;
			return str;
		}
		return NULL;
	case OP_CCLASS:
		
		if(trex_matchcclass(node->left,*str)) {
			str++;
			return str;
		}
		return NULL;
	default: /* char */
		
		if(*str != node->type) return NULL;
		str++;
		return str;
	}
	return NULL;
}

/*public api */
TRex *trex_compile(const TRexChar *pattern,const TRexChar **error)
{
	int nsize, res;
	TRexNode *t;
	TRex *exp;

	exp = NULL;

	if(*pattern == '\0'){
		LM_ERR("invalid parameter pattern\n");
		return NULL;
	}

	exp = (TRex *)trex_shm_alloc(sizeof(TRex));
	if(!exp){
		LM_ERR("out of shm memory\n");
		return NULL;
	}

	exp->_jmpbuf = trex_pkg_alloc(sizeof(jmp_buf));
	if(!exp->_jmpbuf){
		LM_ERR("out of pkg memory");
		goto error;
	}
	
	exp->_nallocated = (int)trex_strlen(pattern) * sizeof(TRexChar);
	exp->_nodes = (TRexNode *)trex_shm_alloc(exp->_nallocated * NODE_SIZE);
	if(!exp->_nodes){
		LM_ERR("out of shm memory\n");
		goto error;
	}

	exp->_eol = exp->_bol = NULL;
	exp->_p = pattern;
	exp->_nsize = 0;
	exp->_matches = 0;
	exp->_nsubexpr = 0;
	if((exp->_first = trex_newnode(exp,OP_EXPR)) <0)
		goto error;

	exp->_error = error;
	
	if(setjmp(*((jmp_buf*)exp->_jmpbuf)) == 0) {

		res = trex_list(exp);
		exp->_nodes[exp->_first].left = res;
		if(*exp->_p!='\0'){
			trex_error(exp,"unexpected character");
		}

		nsize = exp->_nsize;
		t = &exp->_nodes[0];

		exp->_matches=(TRexMatch*)trex_shm_alloc(exp->_nsubexpr*MATCH_SIZE);
		if(!exp->_matches)
			trex_error(exp, "out of memory");
		memset(exp->_matches,0,exp->_nsubexpr * MATCH_SIZE);
		if(exp->_jmpbuf) {
			trex_pkg_free(exp->_jmpbuf);
			exp->_jmpbuf = 0;
		}
		//debug purposes
		{
			int nsize,i;
			TRexNode *t;
			nsize = exp->_nsize;
			t = &exp->_nodes[0];
			LM_DBG("\n");
			for(i = 0;i < nsize; i++) {
				if(exp->_nodes[i].type>MAX_CHAR)
					LM_DBG("[%02d] %10s ",i,
						g_nnames[exp->_nodes[i].type-MAX_CHAR]);
				else
					LM_DBG("[%02d] %10c ",i,exp->_nodes[i].type);
				LM_DBG("left %02d right %02d next %02d\n",
					(int)exp->_nodes[i].left,
					(int)exp->_nodes[i].right,(int)exp->_nodes[i].next);
			}
			LM_DBG("\n");
		}


	}
	else{
		LM_ERR("compilation error [%s]!\n",
			(*exp->_error)?(*exp->_error):"undefined");

error:
		trex_destroy(exp);
		return NULL;
	}
	return exp;
}

TRexBool trex_match(TRex* exp,const TRexChar* text)
{
	const TRexChar* res = NULL;
	exp->_bol = text;
	exp->_eol = text + trex_strlen(text);
	exp->_currsubexp = 0;
	res = trex_matchnode(exp,exp->_nodes,text);
	if(res == NULL || res != exp->_eol)
		return TRex_False;
	return TRex_True;
}

TRexBool trex_searchrange(TRex* exp,const TRexChar* text_begin,const TRexChar* text_end,const TRexChar** out_begin, const TRexChar** out_end)
{
	const TRexChar *cur = NULL;
	int node = exp->_first;
	exp->_bol = text_begin;
	exp->_eol = text_end;
	do {
		cur = text_begin;
		while(node != -1) {
			exp->_currsubexp = 0;
			cur = trex_matchnode(exp,&exp->_nodes[node],cur);
			if(!cur)
				break;
			node = exp->_nodes[node].next;
		}
		text_begin++;
	} while(cur == NULL && text_begin != text_end);

	if(cur == NULL)
		return TRex_False;

	--text_begin;

	if(out_begin) *out_begin = text_begin;
	if(out_end) *out_end = cur;
	return TRex_True;
}

TRexBool trex_search(TRex* exp,const TRexChar* text, const TRexChar** out_begin, const TRexChar** out_end)
{
	return trex_searchrange(exp,text,text + trex_strlen(text),out_begin,out_end);
}

int trex_getsubexpcount(TRex* exp)
{
	if(!exp)
		return 0;
	return exp->_nsubexpr;
}

TRexBool trex_getsubexp(TRex* exp, int n, TRexMatch *subexp)
{
	if( n<0 || n >= exp->_nsubexpr) return TRex_False;
	*subexp = exp->_matches[n];
	return TRex_True;
}

void trex_destroy(TRex *exp)
{
	if(exp)	{
		if(exp->_nodes) trex_shm_free(exp->_nodes);
		if(exp->_jmpbuf) {
			trex_pkg_free(exp->_jmpbuf);
			exp->_jmpbuf = 0;
		}
		if(exp->_matches) trex_shm_free(exp->_matches);
		trex_shm_free(exp);
		exp = 0;
	}
}


