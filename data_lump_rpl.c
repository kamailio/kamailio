
#include <string.h>
#include "dprint.h"
#include "mem/mem.h"
#include "data_lump_rpl.h"


struct lump_rpl* build_lump_rpl( char* text, int len )
{
	struct lump_rpl *lump = 0;

	lump = (struct lump_rpl*) pkg_malloc(sizeof(struct lump_rpl));
	if (!lump)
	{
		LOG(L_ERR,"ERROR:build_lump_rpl : no free memory (struct)!\n");
		goto error;
	}

	lump->text.s = pkg_malloc( len );
	if (!lump->text.s)
	{
		LOG(L_ERR,"ERROR:build_lump_rpl : no free memory (%d)!\n", len );
		goto error;
	}

	memcpy(lump->text.s,text,len);
	lump->text.len = len;
	lump->next = 0;

	return lump;

error:
	if (lump) pkg_free(lump);
	return 0;
}



void add_lump_rpl(struct sip_msg * msg, struct lump_rpl* lump)
{
	struct lump_rpl *foo;

	if (!msg->reply_lump)
	{
		msg->reply_lump = lump;
	}else{
		for(foo=msg->reply_lump;foo->next;foo=foo->next);
		foo->next = lump;
	}
}



void free_lump_rpl(struct lump_rpl* lump)
{
	if (lump && lump->text.s)  pkg_free(lump->text.s);
	if (lump) pkg_free(lump);
}



