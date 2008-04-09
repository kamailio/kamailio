#include "orasel.h"

//-----------------------------------------------------------------------------
static void out_delim(const unsigned* pl, unsigned nc)
{
	unsigned i;

	for(i = 0; i < nc; i++) {
		unsigned j = pl[i] + 2;

		putchar('+');
		do putchar('-'); while(--j);
	}
	printf("+\n");
}

//-----------------------------------------------------------------------------
void out_res(const res_t* _r)
{
	unsigned* pl = NULL;
	unsigned  nc = _r->col_n, nr = _r->row_n, i, j;
	Str**     ps = _r->names;

	if(!outmode.raw) {
		pl = safe_malloc(nc * sizeof(unsigned));
		for(i = 0; i < nc; i++)
			pl[i] = ps[i]->len;
		for(j = 0; j < nr; j++) {
			ps = _r->rows[j];
			for(i = 0; i < nc; i++) 
				if(pl[i] < ps[i]->len) pl[i] = ps[i]->len;
		}

		out_delim(pl, nc);
	}
	if(!outmode.hdr) {
		ps = _r->names;
		for(i = 0; i < nc; i++) {
			if(!outmode.raw) {
				printf("| %-*.*s ", pl[i], ps[i]->len, ps[i]->s);
			} else {
				if(i) putchar('\t');
				printf("%.*s", ps[i]->len, ps[i]->s);
			}
		}
		if(outmode.raw) putchar('\n');
		else {
			printf("|\n");
			out_delim(pl, nc);
		}
	}
	for(j = 0; j < nr; j++) {
		ps = _r->rows[j];
		if(!outmode.raw) {
			for(i = 0; i < nc; i++)
				printf(_r->types[i] ? "| %-*.*s " : "| %*.*s ",
					pl[i], ps[i]->len, ps[i]->s);
			printf("|\n");
		} else {
			for(i = 0; i < nc; i++) {
				if(i) putchar('\t');
				printf("%.*s", ps[i]->len, ps[i]->s);
			}
			putchar('\n');
		}
	}
	if(!outmode.raw) out_delim(pl, nc);
}

//-----------------------------------------------------------------------------
