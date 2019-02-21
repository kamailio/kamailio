/*-
 * Copyright (c) 2003, 2004, 2005, 2006, 2007 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include "asn_internal.h"
#include "constr_SEQUENCE.h"
#include "per_opentype.h"

/*
 * Number of bytes left for this structure.
 * (ctx->left) indicates the number of bytes _transferred_ for the structure.
 * (size) contains the number of bytes in the buffer passed.
 */
#define	LEFT	((size<(size_t)ctx->left)?size:(size_t)ctx->left)

/*
 * If the subprocessor function returns with an indication that it wants
 * more data, it may well be a fatal decoding problem, because the
 * size is constrained by the <TLV>'s L, even if the buffer size allows
 * reading more data.
 * For example, consider the buffer containing the following TLVs:
 * <T:5><L:1><V> <T:6>...
 * The TLV length clearly indicates that one byte is expected in V, but
 * if the V processor returns with "want more data" even if the buffer
 * contains way more data than the V processor have seen.
 */
#define	SIZE_VIOLATION	(ctx->left >= 0 && (size_t)ctx->left <= size)

/*
 * This macro "eats" the part of the buffer which is definitely "consumed",
 * i.e. was correctly converted into local representation or rightfully skipped.
 */
#undef	ADVANCE
#define	ADVANCE(num_bytes)	do {		\
		size_t num = num_bytes;		\
		ptr = ((const char *)ptr) + num; \
		size -= num;			\
		if(ctx->left >= 0)		\
			ctx->left -= num;	\
		consumed_myself += num;		\
	} while(0)

/*
 * Switch to the next phase of parsing.
 */
#undef	NEXT_PHASE
#undef	PHASE_OUT
#define	NEXT_PHASE(ctx)	do {			\
		ctx->phase++;			\
		ctx->step = 0;			\
	} while(0)
#define	PHASE_OUT(ctx)	do { ctx->phase = 10; } while(0)

/*
 * Return a standardized complex structure.
 */
#undef	RETURN
#define	RETURN(_code)	do {			\
		rval.code = _code;		\
		rval.consumed = consumed_myself;\
		return rval;			\
	} while(0)

/*
 * Check whether we are inside the extensions group.
 */
#define	IN_EXTENSION_GROUP(specs, memb_idx)	\
	( ((memb_idx) > (specs)->ext_after)	\
	&&((memb_idx) < (specs)->ext_before))


/*
 * Tags are canonically sorted in the tag2element map.
 */
static int
_t2e_cmp(const void *ap, const void *bp) {
	const asn_TYPE_tag2member_t *a = (const asn_TYPE_tag2member_t *)ap;
	const asn_TYPE_tag2member_t *b = (const asn_TYPE_tag2member_t *)bp;

	int a_class = BER_TAG_CLASS(a->el_tag);
	int b_class = BER_TAG_CLASS(b->el_tag);

	if(a_class == b_class) {
		ber_tlv_tag_t a_value = BER_TAG_VALUE(a->el_tag);
		ber_tlv_tag_t b_value = BER_TAG_VALUE(b->el_tag);

		if(a_value == b_value) {
			if(a->el_no > b->el_no)
				return 1;
			/*
			 * Important: we do not check
			 * for a->el_no <= b->el_no!
			 */
			return 0;
		} else if(a_value < b_value)
			return -1;
		else
			return 1;
	} else if(a_class < b_class) {
		return -1;
	} else {
		return 1;
	}
}


/*
 * The decoder of the SEQUENCE type.
 */
asn_dec_rval_t
SEQUENCE_decode_ber(asn_codec_ctx_t *opt_codec_ctx, asn_TYPE_descriptor_t *td,
	void **struct_ptr, const void *ptr, size_t size, int tag_mode) {
	/*
	 * Bring closer parts of structure description.
	 */
	asn_SEQUENCE_specifics_t *specs = (asn_SEQUENCE_specifics_t *)td->specifics;
	asn_TYPE_member_t *elements = td->elements;

	/*
	 * Parts of the structure being constructed.
	 */
	void *st = *struct_ptr;	/* Target structure. */
	asn_struct_ctx_t *ctx;	/* Decoder context */

	ber_tlv_tag_t tlv_tag;	/* T from TLV */
	asn_dec_rval_t rval;	/* Return code from subparsers */

	ssize_t consumed_myself = 0;	/* Consumed bytes from ptr */
	int edx;			/* SEQUENCE element's index */

	ASN_DEBUG("Decoding %s as SEQUENCE", td->name);
	
	/*
	 * Create the target structure if it is not present already.
	 */
	if(st == 0) {
		st = *struct_ptr = CALLOC(1, specs->struct_size);
		if(st == 0) {
			RETURN(RC_FAIL);
		}
	}

	/*
	 * Restore parsing context.
	 */
	ctx = (asn_struct_ctx_t *)((char *)st + specs->ctx_offset);
	
	/*
	 * Start to parse where left previously
	 */
	switch(ctx->phase) {
	case 0:
		/*
		 * PHASE 0.
		 * Check that the set of tags associated with given structure
		 * perfectly fits our expectations.
		 */

		rval = ber_check_tags(opt_codec_ctx, td, ctx, ptr, size,
			tag_mode, 1, &ctx->left, 0);
		if(rval.code != RC_OK) {
			ASN_DEBUG("%s tagging check failed: %d",
				td->name, rval.code);
			return rval;
		}

		if(ctx->left >= 0)
			ctx->left += rval.consumed; /* ?Substracted below! */
		ADVANCE(rval.consumed);

		NEXT_PHASE(ctx);

		ASN_DEBUG("Structure consumes %ld bytes, buffer %ld",
			(long)ctx->left, (long)size);

		/* Fall through */
	case 1:
		/*
		 * PHASE 1.
		 * From the place where we've left it previously,
		 * try to decode the next member from the list of
		 * this structure's elements.
		 * (ctx->step) stores the member being processed
		 * between invocations and the microphase {0,1} of parsing
		 * that member:
		 * 	step = (<member_number> * 2 + <microphase>).
		 */
	  for(edx = (ctx->step >> 1); edx < td->elements_count;
			edx++, ctx->step = (ctx->step & ~1) + 2) {
		void *memb_ptr;		/* Pointer to the member */
		void **memb_ptr2;	/* Pointer to that pointer */
		ssize_t tag_len;	/* Length of TLV's T */
		int opt_edx_end;	/* Next non-optional element */
		int use_bsearch;
		int n;

		if(ctx->step & 1)
			goto microphase2;

		/*
		 * MICROPHASE 1: Synchronize decoding.
		 */
		ASN_DEBUG("In %s SEQUENCE left %d, edx=%d flags=%d"
				" opt=%d ec=%d",
			td->name, (int)ctx->left, edx,
			elements[edx].flags, elements[edx].optional,
			td->elements_count);

		if(ctx->left == 0	/* No more stuff is expected */
		&& (
			/* Explicit OPTIONAL specification reaches the end */
			(edx + elements[edx].optional
					== td->elements_count)
			||
			/* All extensions are optional */
			(IN_EXTENSION_GROUP(specs, edx)
				&& specs->ext_before > td->elements_count)
		   )
		) {
			ASN_DEBUG("End of SEQUENCE %s", td->name);
			/*
			 * Found the legitimate end of the structure.
			 */
			PHASE_OUT(ctx);
			RETURN(RC_OK);
		}

		/*
		 * Fetch the T from TLV.
		 */
		tag_len = ber_fetch_tag(ptr, LEFT, &tlv_tag);
		ASN_DEBUG("Current tag in %s SEQUENCE for element %d "
			"(%s) is %s encoded in %d bytes, of frame %ld",
			td->name, edx, elements[edx].name,
			ber_tlv_tag_string(tlv_tag), (int)tag_len, (long)LEFT);
		switch(tag_len) {
		case 0: if(!SIZE_VIOLATION) RETURN(RC_WMORE);
			/* Fall through */
		case -1: RETURN(RC_FAIL);
		}

		if(ctx->left < 0 && ((const uint8_t *)ptr)[0] == 0) {
			if(LEFT < 2) {
				if(SIZE_VIOLATION)
					RETURN(RC_FAIL);
				else
					RETURN(RC_WMORE);
			} else if(((const uint8_t *)ptr)[1] == 0) {
			ASN_DEBUG("edx = %d, opt = %d, ec=%d",
				edx, elements[edx].optional,
				td->elements_count);
				if((edx + elements[edx].optional
					== td->elements_count)
				|| (IN_EXTENSION_GROUP(specs, edx)
					&& specs->ext_before
						> td->elements_count)) {
					/*
					 * Yeah, baby! Found the terminator
					 * of the indefinite length structure.
					 */
					/*
					 * Proceed to the canonical
					 * finalization function.
					 * No advancing is necessary.
					 */
					goto phase3;
				}
			}
		}

		/*
		 * Find the next available type with this tag.
		 */
		use_bsearch = 0;
		opt_edx_end = edx + elements[edx].optional + 1;
		if(opt_edx_end > td->elements_count)
			opt_edx_end = td->elements_count;	/* Cap */
		else if(opt_edx_end - edx > 8) {
			/* Limit the scope of linear search... */
			opt_edx_end = edx + 8;
			use_bsearch = 1;
			/* ... and resort to bsearch() */
		}
		for(n = edx; n < opt_edx_end; n++) {
			if(BER_TAGS_EQUAL(tlv_tag, elements[n].tag)) {
				/*
				 * Found element corresponding to the tag
				 * being looked at.
				 * Reposition over the right element.
				 */
				edx = n;
				ctx->step = 1 + 2 * edx;	/* Remember! */
				goto microphase2;
			} else if(elements[n].flags & ATF_OPEN_TYPE) {
				/*
				 * This is the ANY type, which may bear
				 * any flag whatsoever.
				 */
				edx = n;
				ctx->step = 1 + 2 * edx;	/* Remember! */
				goto microphase2;
			} else if(elements[n].tag == (ber_tlv_tag_t)-1) {
				use_bsearch = 1;
				break;
			}
		}
		if(use_bsearch) {
			/*
			 * Resort to a binary search over
			 * sorted array of tags.
			 */
			const asn_TYPE_tag2member_t *t2m;
			asn_TYPE_tag2member_t key;
			key.el_tag = tlv_tag;
			key.el_no = edx;
			t2m = (const asn_TYPE_tag2member_t *)bsearch(&key,
				specs->tag2el, specs->tag2el_count,
				sizeof(specs->tag2el[0]), _t2e_cmp);
			if(t2m) {
				const asn_TYPE_tag2member_t *best = 0;
				const asn_TYPE_tag2member_t *t2m_f, *t2m_l;
				int edx_max = edx + elements[edx].optional;
				/*
				 * Rewind to the first element with that tag,
				 * `cause bsearch() does not guarantee order.
				 */
				t2m_f = t2m + t2m->toff_first;
				t2m_l = t2m + t2m->toff_last;
				for(t2m = t2m_f; t2m <= t2m_l; t2m++) {
					if(t2m->el_no > edx_max) break;
					if(t2m->el_no < edx) continue;
					best = t2m;
				}
				if(best) {
					edx = best->el_no;
					ctx->step = 1 + 2 * edx;
					goto microphase2;
				}
			}
			n = opt_edx_end;
		}
		if(n == opt_edx_end) {
			/*
			 * If tag is unknown, it may be either
			 * an unknown (thus, incorrect) tag,
			 * or an extension (...),
			 * or an end of the indefinite-length structure.
			 */
			if(!IN_EXTENSION_GROUP(specs,
				edx + elements[edx].optional)) {
				ASN_DEBUG("Unexpected tag %s (at %d)",
					ber_tlv_tag_string(tlv_tag), edx);
				ASN_DEBUG("Expected tag %s (%s)%s",
					ber_tlv_tag_string(elements[edx].tag),
					elements[edx].name,
					elements[edx].optional
						?" or alternatives":"");
				RETURN(RC_FAIL);
			} else {
				/* Skip this tag */
				ssize_t skip;
				edx += elements[edx].optional;

				ASN_DEBUG("Skipping unexpected %s (at %d)",
					ber_tlv_tag_string(tlv_tag), edx);
				skip = ber_skip_length(opt_codec_ctx,
					BER_TLV_CONSTRUCTED(ptr),
					(const char *)ptr + tag_len,
					LEFT - tag_len);
				ASN_DEBUG("Skip length %d in %s",
					(int)skip, td->name);
				switch(skip) {
				case 0: if(!SIZE_VIOLATION) RETURN(RC_WMORE);
					/* Fall through */
				case -1: RETURN(RC_FAIL);
				}

				ADVANCE(skip + tag_len);
				ctx->step -= 2;
				edx--;
				continue;  /* Try again with the next tag */
			}
		}

		/*
		 * MICROPHASE 2: Invoke the member-specific decoder.
		 */
		ctx->step |= 1;		/* Confirm entering next microphase */
	microphase2:
		ASN_DEBUG("Inside SEQUENCE %s MF2", td->name);
		
		/*
		 * Compute the position of the member inside a structure,
		 * and also a type of containment (it may be contained
		 * as pointer or using inline inclusion).
		 */
		if(elements[edx].flags & ATF_POINTER) {
			/* Member is a pointer to another structure */
			memb_ptr2 = (void **)((char *)st + elements[edx].memb_offset);
		} else {
			/*
			 * A pointer to a pointer
			 * holding the start of the structure
			 */
			memb_ptr = (char *)st + elements[edx].memb_offset;
			memb_ptr2 = &memb_ptr;
		}
		/*
		 * Invoke the member fetch routine according to member's type
		 */
		rval = elements[edx].type->ber_decoder(opt_codec_ctx,
				elements[edx].type,
				memb_ptr2, ptr, LEFT,
				elements[edx].tag_mode);
		ASN_DEBUG("In %s SEQUENCE decoded %d %s of %d "
			"in %d bytes rval.code %d, size=%d",
			td->name, edx, elements[edx].type->name,
			(int)LEFT, (int)rval.consumed, rval.code, (int)size);
		switch(rval.code) {
		case RC_OK:
			break;
		case RC_WMORE: /* More data expected */
			if(!SIZE_VIOLATION) {
				ADVANCE(rval.consumed);
				RETURN(RC_WMORE);
			}
			ASN_DEBUG("Size violation (c->l=%ld <= s=%ld)",
				(long)ctx->left, (long)size);
			/* Fall through */
		case RC_FAIL: /* Fatal error */
			RETURN(RC_FAIL);
		} /* switch(rval) */
		
		ADVANCE(rval.consumed);
	  }	/* for(all structure members) */

	phase3:
		ctx->phase = 3;
	case 3:	/* 00 and other tags expected */
	case 4:	/* only 00's expected */

		ASN_DEBUG("SEQUENCE %s Leftover: %ld, size = %ld",
			td->name, (long)ctx->left, (long)size);

		/*
		 * Skip everything until the end of the SEQUENCE.
		 */
		while(ctx->left) {
			ssize_t tl, ll;

			tl = ber_fetch_tag(ptr, LEFT, &tlv_tag);
			switch(tl) {
			case 0: if(!SIZE_VIOLATION) RETURN(RC_WMORE);
				/* Fall through */
			case -1: RETURN(RC_FAIL);
			}

			/*
			 * If expected <0><0>...
			 */
			if(ctx->left < 0
				&& ((const uint8_t *)ptr)[0] == 0) {
				if(LEFT < 2) {
					if(SIZE_VIOLATION)
						RETURN(RC_FAIL);
					else
						RETURN(RC_WMORE);
				} else if(((const uint8_t *)ptr)[1] == 0) {
					/*
					 * Correctly finished with <0><0>.
					 */
					ADVANCE(2);
					ctx->left++;
					ctx->phase = 4;
					continue;
				}
			}

			if(!IN_EXTENSION_GROUP(specs, td->elements_count)
			|| ctx->phase == 4) {
				ASN_DEBUG("Unexpected continuation "
					"of a non-extensible type "
					"%s (SEQUENCE): %s",
					td->name,
					ber_tlv_tag_string(tlv_tag));
				RETURN(RC_FAIL);
			}

			ll = ber_skip_length(opt_codec_ctx,
				BER_TLV_CONSTRUCTED(ptr),
				(const char *)ptr + tl, LEFT - tl);
			switch(ll) {
			case 0: if(!SIZE_VIOLATION) RETURN(RC_WMORE);
				/* Fall through */
			case -1: RETURN(RC_FAIL);
			}

			ADVANCE(tl + ll);
		}

		PHASE_OUT(ctx);
	}
	
	RETURN(RC_OK);
}


/*
 * The DER encoder of the SEQUENCE type.
 */
asn_enc_rval_t
SEQUENCE_encode_der(asn_TYPE_descriptor_t *td,
	void *sptr, int tag_mode, ber_tlv_tag_t tag,
	asn_app_consume_bytes_f *cb, void *app_key) {
	size_t computed_size = 0;
	asn_enc_rval_t erval;
	ssize_t ret;
	int edx;

	ASN_DEBUG("%s %s as SEQUENCE",
		cb?"Encoding":"Estimating", td->name);

	/*
	 * Gather the length of the underlying members sequence.
	 */
	for(edx = 0; edx < td->elements_count; edx++) {
		asn_TYPE_member_t *elm = &td->elements[edx];
		void *memb_ptr;
		if(elm->flags & ATF_POINTER) {
			memb_ptr = *(void **)((char *)sptr + elm->memb_offset);
			if(!memb_ptr) {
				if(elm->optional) continue;
				/* Mandatory element is missing */
				ASN__ENCODE_FAILED;
			}
		} else {
			memb_ptr = (void *)((char *)sptr + elm->memb_offset);
		}
		erval = elm->type->der_encoder(elm->type, memb_ptr,
			elm->tag_mode, elm->tag,
			0, 0);
		if(erval.encoded == -1)
			return erval;
		computed_size += erval.encoded;
		ASN_DEBUG("Member %d %s estimated %ld bytes",
			edx, elm->name, (long)erval.encoded);
	}

	/*
	 * Encode the TLV for the sequence itself.
	 */
	ret = der_write_tags(td, computed_size, tag_mode, 1, tag, cb, app_key);
	ASN_DEBUG("Wrote tags: %ld (+%ld)", (long)ret, (long)computed_size);
	if(ret == -1)
		ASN__ENCODE_FAILED;
	erval.encoded = computed_size + ret;

	if(!cb) ASN__ENCODED_OK(erval);

	/*
	 * Encode all members.
	 */
	for(edx = 0; edx < td->elements_count; edx++) {
		asn_TYPE_member_t *elm = &td->elements[edx];
		asn_enc_rval_t tmperval;
		void *memb_ptr;

		if(elm->flags & ATF_POINTER) {
			memb_ptr = *(void **)((char *)sptr + elm->memb_offset);
			if(!memb_ptr) continue;
		} else {
			memb_ptr = (void *)((char *)sptr + elm->memb_offset);
		}
		tmperval = elm->type->der_encoder(elm->type, memb_ptr,
			elm->tag_mode, elm->tag,
			cb, app_key);
		if(tmperval.encoded == -1)
			return tmperval;
		computed_size -= tmperval.encoded;
		ASN_DEBUG("Member %d %s of SEQUENCE %s encoded in %ld bytes",
			edx, elm->name, td->name, (long)tmperval.encoded);
	}

	if(computed_size != 0)
		/*
		 * Encoded size is not equal to the computed size.
		 */
		ASN__ENCODE_FAILED;

	ASN__ENCODED_OK(erval);
}


#undef	XER_ADVANCE
#define	XER_ADVANCE(num_bytes)	do {			\
		size_t num = num_bytes;			\
		buf_ptr = ((const char *)buf_ptr) + num;\
		size -= num;				\
		consumed_myself += num;			\
	} while(0)

/*
 * Decode the XER (XML) data.
 */
asn_dec_rval_t
SEQUENCE_decode_xer(asn_codec_ctx_t *opt_codec_ctx, asn_TYPE_descriptor_t *td,
	void **struct_ptr, const char *opt_mname,
		const void *buf_ptr, size_t size) {
	/*
	 * Bring closer parts of structure description.
	 */
	asn_SEQUENCE_specifics_t *specs
		= (asn_SEQUENCE_specifics_t *)td->specifics;
	asn_TYPE_member_t *elements = td->elements;
	const char *xml_tag = opt_mname ? opt_mname : td->xml_tag;

	/*
	 * ... and parts of the structure being constructed.
	 */
	void *st = *struct_ptr;	/* Target structure. */
	asn_struct_ctx_t *ctx;	/* Decoder context */

	asn_dec_rval_t rval;		/* Return value from a decoder */
	ssize_t consumed_myself = 0;	/* Consumed bytes from ptr */
	int edx;			/* Element index */
	int edx_end;

	/*
	 * Create the target structure if it is not present already.
	 */
	if(st == 0) {
		st = *struct_ptr = CALLOC(1, specs->struct_size);
		if(st == 0) RETURN(RC_FAIL);
	}

	/*
	 * Restore parsing context.
	 */
	ctx = (asn_struct_ctx_t *)((char *)st + specs->ctx_offset);


	/*
	 * Phases of XER/XML processing:
	 * Phase 0: Check that the opening tag matches our expectations.
	 * Phase 1: Processing body and reacting on closing tag.
	 * Phase 2: Processing inner type.
	 * Phase 3: Skipping unknown extensions.
	 * Phase 4: PHASED OUT
	 */
	for(edx = ctx->step; ctx->phase <= 3;) {
		pxer_chunk_type_e ch_type;	/* XER chunk type */
		ssize_t ch_size;		/* Chunk size */
		xer_check_tag_e tcv;		/* Tag check value */
		asn_TYPE_member_t *elm;
		int n;

		/*
		 * Go inside the inner member of a sequence.
		 */
		if(ctx->phase == 2) {
			asn_dec_rval_t tmprval;
			void *memb_ptr;		/* Pointer to the member */
			void **memb_ptr2;	/* Pointer to that pointer */

			elm = &td->elements[edx];

			if(elm->flags & ATF_POINTER) {
				/* Member is a pointer to another structure */
				memb_ptr2 = (void **)((char *)st + elm->memb_offset);
			} else {
				memb_ptr = (char *)st + elm->memb_offset;
				memb_ptr2 = &memb_ptr;
			}

			/* Invoke the inner type decoder, m.b. multiple times */
			tmprval = elm->type->xer_decoder(opt_codec_ctx,
					elm->type, memb_ptr2, elm->name,
					buf_ptr, size);
			XER_ADVANCE(tmprval.consumed);
			if(tmprval.code != RC_OK)
				RETURN(tmprval.code);
			ctx->phase = 1;	/* Back to body processing */
			ctx->step = ++edx;
			ASN_DEBUG("XER/SEQUENCE phase => %d, step => %d",
				ctx->phase, ctx->step);
			/* Fall through */
		}

		/*
		 * Get the next part of the XML stream.
		 */
		ch_size = xer_next_token(&ctx->context, buf_ptr, size,
			&ch_type);
		if(ch_size == -1) {
		    RETURN(RC_FAIL);
        } else {
			switch(ch_type) {
            case PXER_WMORE:
                RETURN(RC_WMORE);
			case PXER_COMMENT:	/* Got XML comment */
			case PXER_TEXT:		/* Ignore free-standing text */
				XER_ADVANCE(ch_size);	/* Skip silently */
				continue;
			case PXER_TAG:
				break;	/* Check the rest down there */
			}
		}

		tcv = xer_check_tag(buf_ptr, ch_size, xml_tag);
		ASN_DEBUG("XER/SEQUENCE: tcv = %d, ph=%d [%s]",
			tcv, ctx->phase, xml_tag);

		/* Skip the extensions section */
		if(ctx->phase == 3) {
			switch(xer_skip_unknown(tcv, &ctx->left)) {
			case -1:
				ctx->phase = 4;
				RETURN(RC_FAIL);
			case 0:
				XER_ADVANCE(ch_size);
				continue;
			case 1:
				XER_ADVANCE(ch_size);
				ctx->phase = 1;
				continue;
			case 2:
				ctx->phase = 1;
				break;
			}
		}

		switch(tcv) {
		case XCT_CLOSING:
			if(ctx->phase == 0) break;
			ctx->phase = 0;
			/* Fall through */
		case XCT_BOTH:
			if(ctx->phase == 0) {
				if(edx >= td->elements_count
				   ||
				   /* Explicit OPTIONAL specs reaches the end */
				    (edx + elements[edx].optional
					== td->elements_count)
				   ||
				   /* All extensions are optional */
				   (IN_EXTENSION_GROUP(specs, edx)
					&& specs->ext_before
						> td->elements_count)
				) {
					XER_ADVANCE(ch_size);
					ctx->phase = 4;	/* Phase out */
					RETURN(RC_OK);
				} else {
					ASN_DEBUG("Premature end of XER SEQUENCE");
					RETURN(RC_FAIL);
				}
			}
			/* Fall through */
		case XCT_OPENING:
			if(ctx->phase == 0) {
				XER_ADVANCE(ch_size);
				ctx->phase = 1;	/* Processing body phase */
				continue;
			}
			/* Fall through */
		case XCT_UNKNOWN_OP:
		case XCT_UNKNOWN_BO:

			ASN_DEBUG("XER/SEQUENCE: tcv=%d, ph=%d, edx=%d",
				tcv, ctx->phase, edx);
			if(ctx->phase != 1) {
				break;	/* Really unexpected */
			}

			if(edx < td->elements_count) {
				/*
				 * Search which member corresponds to this tag.
				 */
				edx_end = edx + elements[edx].optional + 1;
				if(edx_end > td->elements_count)
					edx_end = td->elements_count;
				for(n = edx; n < edx_end; n++) {
					elm = &td->elements[n];
					tcv = xer_check_tag(buf_ptr,
						ch_size, elm->name);
					switch(tcv) {
					case XCT_BOTH:
					case XCT_OPENING:
						/*
						 * Process this member.
						 */
						ctx->step = edx = n;
						ctx->phase = 2;
						break;
					case XCT_UNKNOWN_OP:
					case XCT_UNKNOWN_BO:
						continue;
					default:
						n = edx_end;
						break;	/* Phase out */
					}
					break;
				}
				if(n != edx_end)
					continue;
			} else {
				ASN_DEBUG("Out of defined members: %d/%d",
					edx, td->elements_count);
			}

			/* It is expected extension */
			if(IN_EXTENSION_GROUP(specs,
				edx + (edx < td->elements_count
					? elements[edx].optional : 0))) {
				ASN_DEBUG("Got anticipated extension at %d",
					edx);
				/*
				 * Check for (XCT_BOTH or XCT_UNKNOWN_BO)
				 * By using a mask. Only record a pure
				 * <opening> tags.
				 */
				if(tcv & XCT_CLOSING) {
					/* Found </extension> without body */
				} else {
					ctx->left = 1;
					ctx->phase = 3;	/* Skip ...'s */
				}
				XER_ADVANCE(ch_size);
				continue;
			}

			/* Fall through */
		default:
			break;
		}

		ASN_DEBUG("Unexpected XML tag in SEQUENCE [%c%c%c%c%c%c]",
			size>0?((const char *)buf_ptr)[0]:'.',
			size>1?((const char *)buf_ptr)[1]:'.',
			size>2?((const char *)buf_ptr)[2]:'.',
			size>3?((const char *)buf_ptr)[3]:'.',
			size>4?((const char *)buf_ptr)[4]:'.',
			size>5?((const char *)buf_ptr)[5]:'.');
		break;
	}

	ctx->phase = 4;	/* "Phase out" on hard failure */
	RETURN(RC_FAIL);
}

asn_enc_rval_t
SEQUENCE_encode_xer(asn_TYPE_descriptor_t *td, void *sptr,
	int ilevel, enum xer_encoder_flags_e flags,
		asn_app_consume_bytes_f *cb, void *app_key) {
	asn_enc_rval_t er;
	int xcan = (flags & XER_F_CANONICAL);
	int edx;

	if(!sptr)
		ASN__ENCODE_FAILED;

	er.encoded = 0;

	for(edx = 0; edx < td->elements_count; edx++) {
		asn_enc_rval_t tmper;
		asn_TYPE_member_t *elm = &td->elements[edx];
		void *memb_ptr;
		const char *mname = elm->name;
		unsigned int mlen = strlen(mname);

		if(elm->flags & ATF_POINTER) {
			memb_ptr = *(void **)((char *)sptr + elm->memb_offset);
			if(!memb_ptr) {
				if(elm->optional)
					continue;
				/* Mandatory element is missing */
				ASN__ENCODE_FAILED;
			}
		} else {
			memb_ptr = (void *)((char *)sptr + elm->memb_offset);
		}

		if(!xcan) ASN__TEXT_INDENT(1, ilevel);
		ASN__CALLBACK3("<", 1, mname, mlen, ">", 1);

		/* Print the member itself */
		tmper = elm->type->xer_encoder(elm->type, memb_ptr,
			ilevel + 1, flags, cb, app_key);
		if(tmper.encoded == -1) return tmper;

		ASN__CALLBACK3("</", 2, mname, mlen, ">", 1);
		er.encoded += 5 + (2 * mlen) + tmper.encoded;
	}

	if(!xcan) ASN__TEXT_INDENT(1, ilevel - 1);

	ASN__ENCODED_OK(er);
cb_failed:
	ASN__ENCODE_FAILED;
}

int
SEQUENCE_print(asn_TYPE_descriptor_t *td, const void *sptr, int ilevel,
		asn_app_consume_bytes_f *cb, void *app_key) {
	int edx;
	int ret;

	if(!sptr) return (cb("<absent>", 8, app_key) < 0) ? -1 : 0;

	/* Dump preamble */
	if(cb(td->name, strlen(td->name), app_key) < 0
	|| cb(" ::= {", 6, app_key) < 0)
		return -1;

	for(edx = 0; edx < td->elements_count; edx++) {
		asn_TYPE_member_t *elm = &td->elements[edx];
		const void *memb_ptr;

		if(elm->flags & ATF_POINTER) {
			memb_ptr = *(const void * const *)((const char *)sptr + elm->memb_offset);
			if(!memb_ptr) {
				if(elm->optional) continue;
				/* Print <absent> line */
				/* Fall through */
			}
		} else {
			memb_ptr = (const void *)((const char *)sptr + elm->memb_offset);
		}

		/* Indentation */
		_i_INDENT(1);

		/* Print the member's name and stuff */
		if(cb(elm->name, strlen(elm->name), app_key) < 0
		|| cb(": ", 2, app_key) < 0)
			return -1;

		/* Print the member itself */
		ret = elm->type->print_struct(elm->type, memb_ptr, ilevel + 1,
			cb, app_key);
		if(ret) return ret;
	}

	ilevel--;
	_i_INDENT(1);

	return (cb("}", 1, app_key) < 0) ? -1 : 0;
}

void
SEQUENCE_free(asn_TYPE_descriptor_t *td, void *sptr, int contents_only) {
	int edx;

	if(!td || !sptr)
		return;

	ASN_DEBUG("Freeing %s as SEQUENCE", td->name);

	for(edx = 0; edx < td->elements_count; edx++) {
		asn_TYPE_member_t *elm = &td->elements[edx];
		void *memb_ptr;
		if(elm->flags & ATF_POINTER) {
			memb_ptr = *(void **)((char *)sptr + elm->memb_offset);
			if(memb_ptr)
				ASN_STRUCT_FREE(*elm->type, memb_ptr);
		} else {
			memb_ptr = (void *)((char *)sptr + elm->memb_offset);
			ASN_STRUCT_FREE_CONTENTS_ONLY(*elm->type, memb_ptr);
		}
	}

	if(!contents_only) {
		FREEMEM(sptr);
	}
}

int
SEQUENCE_constraint(asn_TYPE_descriptor_t *td, const void *sptr,
		asn_app_constraint_failed_f *ctfailcb, void *app_key) {
	int edx;

	if(!sptr) {
		ASN__CTFAIL(app_key, td, sptr,
			"%s: value not given (%s:%d)",
			td->name, __FILE__, __LINE__);
		return -1;
	}

	/*
	 * Iterate over structure members and check their validity.
	 */
	for(edx = 0; edx < td->elements_count; edx++) {
		asn_TYPE_member_t *elm = &td->elements[edx];
		const void *memb_ptr;

		if(elm->flags & ATF_POINTER) {
			memb_ptr = *(const void * const *)((const char *)sptr + elm->memb_offset);
			if(!memb_ptr) {
				if(elm->optional)
					continue;
				ASN__CTFAIL(app_key, td, sptr,
				"%s: mandatory element %s absent (%s:%d)",
				td->name, elm->name, __FILE__, __LINE__);
				return -1;
			}
		} else {
			memb_ptr = (const void *)((const char *)sptr + elm->memb_offset);
		}

		if(elm->memb_constraints) {
			int ret = elm->memb_constraints(elm->type, memb_ptr,
				ctfailcb, app_key);
			if(ret) return ret;
		} else {
			int ret = elm->type->check_constraints(elm->type,
				memb_ptr, ctfailcb, app_key);
			if(ret) return ret;
			/*
			 * Cannot inherit it earlier:
			 * need to make sure we get the updated version.
			 */
			elm->memb_constraints = elm->type->check_constraints;
		}
	}

	return 0;
}

asn_dec_rval_t
SEQUENCE_decode_uper(asn_codec_ctx_t *opt_codec_ctx, asn_TYPE_descriptor_t *td,
	asn_per_constraints_t *constraints, void **sptr, asn_per_data_t *pd) {
	asn_SEQUENCE_specifics_t *specs = (asn_SEQUENCE_specifics_t *)td->specifics;
	void *st = *sptr;	/* Target structure. */
	int extpresent;		/* Extension additions are present */
	uint8_t *opres;		/* Presence of optional root members */
	asn_per_data_t opmd;
	asn_dec_rval_t rv;
	int edx;

	(void)constraints;

	if(ASN__STACK_OVERFLOW_CHECK(opt_codec_ctx))
		ASN__DECODE_FAILED;

	if(!st) {
		st = *sptr = CALLOC(1, specs->struct_size);
		if(!st) ASN__DECODE_FAILED;
	}

	ASN_DEBUG("Decoding %s as SEQUENCE (UPER)", td->name);

	/* Handle extensions */
	if(specs->ext_before >= 0) {
		extpresent = per_get_few_bits(pd, 1);
		if(extpresent < 0) ASN__DECODE_STARVED;
	} else {
		extpresent = 0;
	}

	/* Prepare a place and read-in the presence bitmap */
	memset(&opmd, 0, sizeof(opmd));
	if(specs->roms_count) {
		opres = (uint8_t *)MALLOC(((specs->roms_count + 7) >> 3) + 1);
		if(!opres) ASN__DECODE_FAILED;
		/* Get the presence map */
		if(per_get_many_bits(pd, opres, 0, specs->roms_count)) {
			FREEMEM(opres);
			ASN__DECODE_STARVED;
		}
		opmd.buffer = opres;
		opmd.nbits = specs->roms_count;
		ASN_DEBUG("Read in presence bitmap for %s of %d bits (%x..)",
			td->name, specs->roms_count, *opres);
	} else {
		opres = 0;
	}

	/*
	 * Get the sequence ROOT elements.
	 */
	for(edx = 0; edx < td->elements_count; edx++) {
		asn_TYPE_member_t *elm = &td->elements[edx];
		void *memb_ptr;		/* Pointer to the member */
		void **memb_ptr2;	/* Pointer to that pointer */

		if(IN_EXTENSION_GROUP(specs, edx))
			continue;

		/* Fetch the pointer to this member */
		if(elm->flags & ATF_POINTER) {
			memb_ptr2 = (void **)((char *)st + elm->memb_offset);
		} else {
			memb_ptr = (char *)st + elm->memb_offset;
			memb_ptr2 = &memb_ptr;
		}

		/* Deal with optionality */
		if(elm->optional) {
			int present = per_get_few_bits(&opmd, 1);
			ASN_DEBUG("Member %s->%s is optional, p=%d (%d->%d)",
				td->name, elm->name, present,
				(int)opmd.nboff, (int)opmd.nbits);
			if(present == 0) {
				/* This element is not present */
				if(elm->default_value) {
					/* Fill-in DEFAULT */
					if(elm->default_value(1, memb_ptr2)) {
						FREEMEM(opres);
						ASN__DECODE_FAILED;
					}
					ASN_DEBUG("Filled-in default");
				}
				/* The member is just not present */
				continue;
			}
			/* Fall through */
		}

		/* Fetch the member from the stream */
		ASN_DEBUG("Decoding member %s in %s", elm->name, td->name);
		rv = elm->type->uper_decoder(opt_codec_ctx, elm->type,
			elm->per_constraints, memb_ptr2, pd);
		if(rv.code != RC_OK) {
			ASN_DEBUG("Failed decode %s in %s",
				elm->name, td->name);
			FREEMEM(opres);
			return rv;
		}
	}

	/* Optionality map is not needed anymore */
	FREEMEM(opres);

	/*
	 * Deal with extensions.
	 */
	if(extpresent) {
		ssize_t bmlength;
		uint8_t *epres;		/* Presence of extension members */
		asn_per_data_t epmd;

		bmlength = uper_get_nslength(pd);
		if(bmlength < 0) ASN__DECODE_STARVED;

		ASN_DEBUG("Extensions %ld present in %s", (long)bmlength, td->name);

		epres = (uint8_t *)MALLOC((bmlength + 15) >> 3);
		if(!epres) ASN__DECODE_STARVED;

		/* Get the extensions map */
		if(per_get_many_bits(pd, epres, 0, bmlength)) {
			FREEMEM(epres);
			ASN__DECODE_STARVED;
		}

		memset(&epmd, 0, sizeof(epmd));
		epmd.buffer = epres;
		epmd.nbits = bmlength;
		ASN_DEBUG("Read in extensions bitmap for %s of %ld bits (%x..)",
			td->name, (long)bmlength, *epres);

	    /* Go over extensions and read them in */
	    for(edx = specs->ext_after + 1; edx < td->elements_count; edx++) {
		asn_TYPE_member_t *elm = &td->elements[edx];
		void *memb_ptr;		/* Pointer to the member */
		void **memb_ptr2;	/* Pointer to that pointer */
		int present;

		if(!IN_EXTENSION_GROUP(specs, edx)) {
			ASN_DEBUG("%d is not extension", edx);
			continue;
		}

		/* Fetch the pointer to this member */
		if(elm->flags & ATF_POINTER) {
			memb_ptr2 = (void **)((char *)st + elm->memb_offset);
		} else {
			memb_ptr = (void *)((char *)st + elm->memb_offset);
			memb_ptr2 = &memb_ptr;
		}

		present = per_get_few_bits(&epmd, 1);
		if(present <= 0) {
			if(present < 0) break;	/* No more extensions */
			continue;
		}

		ASN_DEBUG("Decoding member %s in %s %p", elm->name, td->name, *memb_ptr2);
		rv = uper_open_type_get(opt_codec_ctx, elm->type,
			elm->per_constraints, memb_ptr2, pd);
		if(rv.code != RC_OK) {
			FREEMEM(epres);
			return rv;
		}
	    }

		/* Skip over overflow extensions which aren't present
		 * in this system's version of the protocol */
		for(;;) {
			ASN_DEBUG("Getting overflow extensions");
			switch(per_get_few_bits(&epmd, 1)) {
			case -1: break;
			case 0: continue;
			default:
				if(uper_open_type_skip(opt_codec_ctx, pd)) {
					FREEMEM(epres);
					ASN__DECODE_STARVED;
				}
			}
			break;
		}

		FREEMEM(epres);
	}

	/* Fill DEFAULT members in extensions */
	for(edx = specs->roms_count; edx < specs->roms_count
			+ specs->aoms_count; edx++) {
		asn_TYPE_member_t *elm = &td->elements[edx];
		void **memb_ptr2;	/* Pointer to member pointer */

		if(!elm->default_value) continue;

		/* Fetch the pointer to this member */
		if(elm->flags & ATF_POINTER) {
			memb_ptr2 = (void **)((char *)st
					+ elm->memb_offset);
			if(*memb_ptr2) continue;
		} else {
			continue;	/* Extensions are all optionals */
		}

		/* Set default value */
		if(elm->default_value(1, memb_ptr2)) {
			ASN__DECODE_FAILED;
		}
	}

	rv.consumed = 0;
	rv.code = RC_OK;
	return rv;
}

static int
SEQUENCE_handle_extensions(asn_TYPE_descriptor_t *td, void *sptr,
		asn_per_outp_t *po1, asn_per_outp_t *po2) {
	asn_SEQUENCE_specifics_t *specs
		= (asn_SEQUENCE_specifics_t *)td->specifics;
	int exts_present = 0;
	int exts_count = 0;
	int edx;

	if(specs->ext_before < 0)
		return 0;

	/* Find out which extensions are present */
	for(edx = specs->ext_after + 1; edx < td->elements_count; edx++) {
		asn_TYPE_member_t *elm = &td->elements[edx];
		void *memb_ptr;		/* Pointer to the member */
		void **memb_ptr2;	/* Pointer to that pointer */
		int present;

		if(!IN_EXTENSION_GROUP(specs, edx)) {
			ASN_DEBUG("%s (@%d) is not extension", elm->type->name, edx);
			continue;
		}

		/* Fetch the pointer to this member */
		if(elm->flags & ATF_POINTER) {
			memb_ptr2 = (void **)((char *)sptr + elm->memb_offset);
			present = (*memb_ptr2 != 0);
		} else {
			memb_ptr = (void *)((char *)sptr + elm->memb_offset);
			memb_ptr2 = &memb_ptr;
			present = 1;
		}

		ASN_DEBUG("checking %s (@%d) present => %d",
			elm->type->name, edx, present);
		exts_count++;
		exts_present += present;

		/* Encode as presence marker */
		if(po1 && per_put_few_bits(po1, present, 1))
			return -1;
		/* Encode as open type field */
		if(po2 && present && uper_open_type_put(elm->type,
				elm->per_constraints, *memb_ptr2, po2))
			return -1;

	}

	return exts_present ? exts_count : 0;
}

asn_enc_rval_t
SEQUENCE_encode_uper(asn_TYPE_descriptor_t *td,
	asn_per_constraints_t *constraints, void *sptr, asn_per_outp_t *po) {
	asn_SEQUENCE_specifics_t *specs
		= (asn_SEQUENCE_specifics_t *)td->specifics;
	asn_enc_rval_t er;
	int n_extensions;
	int edx;
	int i;

	(void)constraints;

	if(!sptr)
		ASN__ENCODE_FAILED;

	er.encoded = 0;

	ASN_DEBUG("Encoding %s as SEQUENCE (UPER)", td->name);


	/*
	 * X.691#18.1 Whether structure is extensible
	 * and whether to encode extensions
	 */
	if(specs->ext_before >= 0) {
		n_extensions = SEQUENCE_handle_extensions(td, sptr, 0, 0);
		per_put_few_bits(po, n_extensions ? 1 : 0, 1);
	} else {
		n_extensions = 0;	/* There are no extensions to encode */
	}

	/* Encode a presence bitmap */
	for(i = 0; i < specs->roms_count; i++) {
		asn_TYPE_member_t *elm;
		void *memb_ptr;		/* Pointer to the member */
		void **memb_ptr2;	/* Pointer to that pointer */
		int present;

		edx = specs->oms[i];
		elm = &td->elements[edx];

		/* Fetch the pointer to this member */
		if(elm->flags & ATF_POINTER) {
			memb_ptr2 = (void **)((char *)sptr + elm->memb_offset);
			present = (*memb_ptr2 != 0);
		} else {
			memb_ptr = (void *)((char *)sptr + elm->memb_offset);
			memb_ptr2 = &memb_ptr;
			present = 1;
		}

		/* Eliminate default values */
		if(present && elm->default_value
		&& elm->default_value(0, memb_ptr2) == 1)
			present = 0;

		ASN_DEBUG("Element %s %s %s->%s is %s",
			elm->flags & ATF_POINTER ? "ptr" : "inline",
			elm->default_value ? "def" : "wtv",
			td->name, elm->name, present ? "present" : "absent");
		if(per_put_few_bits(po, present, 1))
			ASN__ENCODE_FAILED;
	}

	/*
	 * Encode the sequence ROOT elements.
	 */
	ASN_DEBUG("ext_after = %d, ec = %d, eb = %d", specs->ext_after, td->elements_count, specs->ext_before);
	for(edx = 0; edx < ((specs->ext_after < 0)
		? td->elements_count : specs->ext_before - 1); edx++) {

		asn_TYPE_member_t *elm = &td->elements[edx];
		void *memb_ptr;		/* Pointer to the member */
		void **memb_ptr2;	/* Pointer to that pointer */

		if(IN_EXTENSION_GROUP(specs, edx))
			continue;

		ASN_DEBUG("About to encode %s", elm->type->name);

		/* Fetch the pointer to this member */
		if(elm->flags & ATF_POINTER) {
			memb_ptr2 = (void **)((char *)sptr + elm->memb_offset);
			if(!*memb_ptr2) {
				ASN_DEBUG("Element %s %d not present",
					elm->name, edx);
				if(elm->optional)
					continue;
				/* Mandatory element is missing */
				ASN__ENCODE_FAILED;
			}
		} else {
			memb_ptr = (void *)((char *)sptr + elm->memb_offset);
			memb_ptr2 = &memb_ptr;
		}

		/* Eliminate default values */
		if(elm->default_value && elm->default_value(0, memb_ptr2) == 1)
			continue;

		ASN_DEBUG("Encoding %s->%s", td->name, elm->name);
		er = elm->type->uper_encoder(elm->type, elm->per_constraints,
			*memb_ptr2, po);
		if(er.encoded == -1)
			return er;
	}

	/* No extensions to encode */
	if(!n_extensions) ASN__ENCODED_OK(er);

	ASN_DEBUG("Length of %d bit-map", n_extensions);
	/* #18.8. Write down the presence bit-map length. */
	if(uper_put_nslength(po, n_extensions))
		ASN__ENCODE_FAILED;

	ASN_DEBUG("Bit-map of %d elements", n_extensions);
	/* #18.7. Encoding the extensions presence bit-map. */
	/* TODO: act upon NOTE in #18.7 for canonical PER */
	if(SEQUENCE_handle_extensions(td, sptr, po, 0) != n_extensions)
		ASN__ENCODE_FAILED;

	ASN_DEBUG("Writing %d extensions", n_extensions);
	/* #18.9. Encode extensions as open type fields. */
	if(SEQUENCE_handle_extensions(td, sptr, 0, po) != n_extensions)
		ASN__ENCODE_FAILED;

	ASN__ENCODED_OK(er);
}

