#include "im_funcs.h"

#include "../../dprint.h"
#include "../../config.h"
#include "../../ut.h"



#define READ(val) \
	(*(val + 0) + (*(val + 1) << 8) + (*(val + 2) << 16) + (*(val + 3) << 24))
#define advance(_ptr,_n,_str,_error) \
	do{\
		if ((_ptr)+(_n)>(_str).s+(_str).len)\
			goto _error;\
		(_ptr) = (_ptr) + (_n);\
	}while(0);
#define one_of_16( _x , _t ) \
	(_x==_t[0]||_x==_t[15]||_x==_t[8]||_x==_t[2]||_x==_t[3]||_x==_t[4]\
	||_x==_t[5]||_x==_t[6]||_x==_t[7]||_x==_t[1]||_x==_t[9]||_x==_t[10]\
	||_x==_t[11]||_x==_t[12]||_x==_t[13]||_x==_t[14])
#define one_of_15( _x , _t ) \
	(_x==_t[15]||_x==_t[8]||_x==_t[2]||_x==_t[3]||_x==_t[4]\
	||_x==_t[5]||_x==_t[6]||_x==_t[7]||_x==_t[1]||_x==_t[9]||_x==_t[10]\
	||_x==_t[11]||_x==_t[12]||_x==_t[13]||_x==_t[14])
#define one_of_8( _x , _t ) \
	(_x==_t[0]||_x==_t[7]||_x==_t[1]||_x==_t[2]||_x==_t[3]||_x==_t[4]\
	||_x==_t[5]||_x==_t[6])




int inline im_get_body_len( struct sip_msg* msg)
{
	int x,err;
	str foo;

	if (!msg->content_length)
	{
		LOG(L_ERR,"ERROR: im_get_body_len: Content-Length header absent!\n");
		goto error;
	}
	/* if header is present, trim to get only the string containing numbers */
	trim_len( foo.len , foo.s , msg->content_length->body );
	/* convert from string to number */
	x = str2s( (unsigned char*)foo.s,foo.len,&err);
	if (err){
		LOG(L_ERR, "ERROR: im_get_body_len:"
			" unable to parse the Content_Length number !\n");
		goto error;
	}
	return x;
error:
	return -1;
}




int inline im_check_content_type(struct sip_msg *msg)
{
	static unsigned int text[16] = {
		0x74786574/*text*/,0x74786554/*texT*/,0x74784574/*teXt*/,
		0x74784554/*teXT*/,0x74586574/*tExt*/,0x74586554/*tExT*/,
		0x74584574/*tEXt*/,0x74584554/*tEXT*/,0x54786574/*Text*/,
		0x54786554/*TexT*/,0x54784574/*TeXt*/,0x54784554/*TeXT*/,
		0x54586574/*TExt*/,0x54586554/*TExT*/,0x54584574/*TEXt*/,
		0x54584554/*TEXT*/ };
	static unsigned int plai[16] = {
		0x69616c70/*plai*/,0x69616c50/*plaI*/,0x69614c70/*plAi*/,
		0x69614c50/*plAI*/,0x69416c70/*pLai*/,0x69416c50/*pLaI*/,
		0x69414c70/*pLAi*/,0x69414c50/*pLAI*/,0x49616c70/*Plai*/,
		0x49616c50/*PlaI*/,0x49614c70/*PlAi*/,0x49614c50/*PlAI*/,
		0x49416c70/*PLai*/,0x49416c50/*PLaI*/,0x49414c70/*PLAi*/,
		0x49414c50/*PLAI*/ };
	static unsigned int mess[16] = {
		0x7373656d/*mess*/,0x7373654d/*mesS*/,0x7373456d/*meSs*/,
		0x7373454d/*meSS*/,0x7353656d/*mEss*/,0x7353654d/*mEsS*/,
		0x7353456d/*mESs*/,0x7353454d/*mESS*/,0x5373656d/*Mess*/,
		0x5373654d/*MesS*/,0x5373456d/*MeSs*/,0x5373454d/*MeSS*/,
		0x5353656d/*MEss*/,0x5353654d/*MEsS*/,0x5353456d/*MESs*/,
		0x5353454d/*MESS*/ };
	static unsigned int age_[8] = {
		0x00656761/*age_*/,0x00656741/*agE_*/,0x00654761/*aGe_*/,
		0x00654741/*aGE_*/,0x00456761/*Age_*/,0x00456741/*AgE-*/,
		0x00454761/*AGe-*/,0x00454741/*AGE-*/ };
	static unsigned int cpim[16] = {
		0x6d697063/*cpim*/,0x6d697043/*cpiM*/,0x6d695063/*cpIm*/,
		0x6d695043/*cpIM*/,0x6d497063/*cPim*/,0x6d497043/*cPiM*/,
		0x6d495063/*cPIm*/,0x6d495043/*cPIM*/,0x4d697063/*Cpim*/,
		0x4d697043/*CpiM*/,0x4d695063/*CpIm*/,0x4d695043/*CpIM*/,
		0x4d497063/*CPim*/,0x4d497043/*CPiM*/,0x4d495063/*CPIm*/,
		0x4d495043/*CPIM*/ };
	str           str_type;
	unsigned int  i,x,mime;
	char          *p;

	if (!msg->content_type)
	{
		LOG(L_WARN,"WARNING: im_get_body_len: Content-TYPE header absent!"
			"let's assume the content is text/plain ;-)\n");
		return 1;
	}

	trim_len(str_type.len,str_type.s,msg->content_type->body);
	p = str_type.s;
	advance(p,4,str_type,error_1);
	x = READ(p-4);
	if (x==text[0])
		mime = 1 ;
	else if (x==mess[0]) {
		advance(p,3,str_type,error_1);
		x = READ(p-3);
		x = x&0x00ffffff;
		if ( one_of_8(x,age_) )
			mime = 2;
		else
			goto other;
	} else
	if ( one_of_15(x,text) )
		mime =1;
	else if ( one_of_15(x,mess) ) {
		advance(p,3,str_type,error_1);
		x = READ(p-3);
		x = x&0x00ffffff;
		if ( one_of_8(x,age_) )
			mime = 2;
		else
			goto other;
	} else
		goto other;

	/* skip spaces and tabs if any */
	while (*p==' ' || *p=='\t')
		advance(p,1,str_type,error_1);
	if (*p!='/')
	{
		LOG(L_ERR, "ERROR:im_check_content_type: parse error:"
			"no / found after primary type\n");
		goto error;
	}
	advance(p,1,str_type,error_1);
	while ((*p==' ' || *p=='\t') && p+1<str_type.s+str_type.len)
		advance(p,1,str_type,error_1);

	advance(p,4,str_type,error_1);
	x = READ(p-4);
	switch (mime) {
		case 1:
			if ( one_of_16(x,plai) ) {
				advance(p,1,str_type,error_1);
				if (*(p-1)=='n' || *(p-1)=='N')
					goto s_end;
			}
			goto other;
		case 2:
			if ( one_of_16(x,cpim) )
				goto s_end;
			goto other;
	}

error_1:
	LOG(L_ERR,"ERROR:im_check_content_type: parse error: body ended :-(!\n");
error:
	return -1;
other:
	LOG(L_ERR,"ERROR:im_check_content_type: invlaid type for a message\n");
	return -1;
s_end:
	if (*p==';'||*p==' '||*p=='\t'||*p=='\n'||*p=='\r'||*p==0) {
		DBG("DEBUG:im_check_content_type: type <%.*s> found valid\n",
			p-str_type.s,str_type.s);
		return 1;
	} else {
		LOG(L_ERR,"ERROR:im_check_content_type: bad end for type!\n");
		return -1;
	}
}




int im_extract_body(struct sip_msg *msg, str *body )
{
	int len;

	if ( parse_headers(msg,HDR_EOH)==-1 )
	{
		LOG(L_ERR,"ERROR: im_extract_body:unable to parse all headers!\n");
		goto error;
	}

	/*is the content type corect?*/
	if (im_check_content_type(msg)==-1)
	{
		LOG(L_ERR,"ERROR: im_extract_body: content type mismatching\n");
		goto error;
	}

	/* get the lenght from COntent-Lenght header */
	if ( (len = im_get_body_len(msg))<0 )
	{
		LOG(L_ERR,"ERROR: im_extract_body: cannot get body length\n");
		goto error;
	}

	if ( strncmp(CRLF,msg->unparsed,CRLF_LEN)!=0 )
	{
		LOG(L_ERR,"ERROR: im_extract_body:unable to detect the beginning"
			" of message body!\n ");
		goto error;
	}

	body->s = msg->unparsed + CRLF_LEN;
	body->len = len;
	DBG("DEBUG:im_extract_body:=|%.*s|\n",body->len,body->s);

	return 1;
error:
	return -1;
}




int im_get_user(struct sip_msg *msg, str *user, str *host)
{
	struct sip_uri uri;

	if (parse_uri(msg->first_line.u.request.uri.s,
		msg->first_line.u.request.uri.len, &uri) <0 )
	{
		LOG(L_ERR,"ERROR: im_get_user:unable to parse uri\n");
		return -1;
	}
	user->s   = uri.user.s;
	user->len = uri.user.len;
	host->s   = uri.host.s;
	host->len = uri.host.len;
	return 1;

}


