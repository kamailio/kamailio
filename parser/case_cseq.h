/*
 * $Id$
 *
 * CSeq header field parser macros
 */

#ifndef CASE_CSEQ_H
#define CASE_CSEQ_H


#define CSeq_CASE          \
     hdr->type = HDR_CSEQ; \
     p += 4;               \
     goto dc_end


#endif /* CASE_CSEQ_H */
