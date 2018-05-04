/*
 * Copyright (C) 2013-17 Robert Boisvert
 *
 * This file is part of the mohqueue module for Kamailio, a free SIP server.
 *
 * The mohqueue module is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * The mohqueue module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdarg.h>

#include "mohqueue_mod.h"
#include "mohq_db.h"
#include "mohq_funcs.h"

/**********
* definitions
**********/

#define ALLOWHDR "Allow: INVITE, ACK, BYE, CANCEL, NOTIFY, PRACK"
#define CLENHDR "Content-Length"
#define SIPEOL  "\r\n"
#define USRAGNT "Kamailio MOH Queue v1.4"

/**********
* local constants
**********/

str p100rel [1] = {STR_STATIC_INIT ("100rel")};
str pallq [1] = {STR_STATIC_INIT ("*")};
str paudio [1] = {STR_STATIC_INIT ("audio")};
str pbye [1] = {STR_STATIC_INIT ("BYE")};
str pinvite [1] = {STR_STATIC_INIT ("INVITE")};
str prefer [1] = {STR_STATIC_INIT ("REFER")};
str presp_busy [1] = {STR_STATIC_INIT ("Busy Here")};
str presp_noaccept [1] = {STR_STATIC_INIT ("Not Acceptable Here")};
str presp_noallow [1] = {STR_STATIC_INIT ("Method Not Allowed")};
str presp_nocall [1] = {STR_STATIC_INIT ("Call/Transaction Does Not Exist")};
str presp_ok [1] = {STR_STATIC_INIT ("OK")};
str presp_reqpend [1] = {STR_STATIC_INIT ("Request Pending")};
str presp_reqterm [1] = {STR_STATIC_INIT ("Request Terminated")};
str presp_ring [1] = {STR_STATIC_INIT ("Ringing")};
str psipfrag [1] = {STR_STATIC_INIT ("message/sipfrag")};
str presp_srverr [1] = {STR_STATIC_INIT ("Server Internal Error")};
str presp_trying [1] = {STR_STATIC_INIT ("Trying to enter MOH queue")};
str presp_unsupp [1] = {STR_STATIC_INIT ("Unsupported Media Type")};

rtpmap prtpmap [] =
  {
  {9, "G722/8000"},
  {0, "PCMU/8000"},
  {8, "PCMA/8000"},
  {18, "G729/8000"},
  {3, "GSM/8000"},
  {4, "G723/8000"},
  {15, "G728/8000"},
  {5, "DVI4/8000"},
  {7, "LPC/8000"},
  {12, "QCELP/8000"},
  {13, "CN/8000"},
  {16, "DVI4/11025"},
  {6, "DVI4/16000"},
  {17, "DVI4/22050"},
  {10, "L16/44100"},
  {11, "L16/44100"},
  {14, "MPA/90000"},
  {0, 0}
  };

rtpmap *pmohfiles [30]; // element count should be equal or greater than prtpmap

str pallowhdr [1] = { STR_STATIC_INIT (ALLOWHDR SIPEOL) };

char pbyemsg [] =
  {
  "%s"
  "%s"
  "Max-Forwards: 70" SIPEOL
  "Contact: <%s>" SIPEOL
  };

str pextrahdr [1] =
  {
  STR_STATIC_INIT (
  ALLOWHDR SIPEOL
  "Supported: 100rel" SIPEOL
  "Accept-Language: en" SIPEOL
  "Content-Type: application/sdp" SIPEOL
  )
  };

char pinvitesdp [] =
  {
  "v=0" SIPEOL
  "o=- %d %d IN %s" SIPEOL
  "s=" USRAGNT SIPEOL
  "c=IN %s" SIPEOL
  "t=0 0" SIPEOL
  "a=send%s" SIPEOL
  "m=audio %d RTP/AVP "
  };

char prefermsg [] =
  {
  "%s"
  "%s"
  "Contact: <%s>" SIPEOL
  "Max-Forwards: 70" SIPEOL
  "Refer-To: <%s>" SIPEOL
  "Referred-By: <%s>" SIPEOL
  };

char preinvitemsg [] =
  {
  "%s"
  "Max-Forwards: 70" SIPEOL
  "Contact: <%s>" SIPEOL
  ALLOWHDR SIPEOL
  "Supported: 100rel" SIPEOL
  "Accept-Language: en" SIPEOL
  "Content-Type: application/sdp" SIPEOL
  };

char prtpsdp [] =
  {
  "v=0" SIPEOL
  // IP address and audio port faked since they will be replaced
  "o=- 1 1 IN IP4 1.1.1.1" SIPEOL
  "s=" USRAGNT SIPEOL
  "c=IN IP4 1.1.1.1" SIPEOL
  "t=0 0" SIPEOL
  "a=sendrecv" SIPEOL
  "m=audio 1 RTP/AVP"
  };

/**********
* local function declarations
**********/

void delete_call (call_lst *);
void end_RTP (sip_msg_t *, call_lst *);
dlg_t *form_dialog (call_lst *, struct to_body *);
int form_rtp_SDP (str *, call_lst *, char *);
static void invite_cb (struct cell *, int, struct tmcb_params *);
int refer_call (call_lst *, mohq_lock *);
static void refer_cb (struct cell *, int, struct tmcb_params *);
int send_prov_rsp (sip_msg_t *, call_lst *);
int send_rtp_answer (sip_msg_t *, call_lst *);
int search_hdr_ext (struct hdr_field *, str *);
int start_stream (sip_msg_t *, call_lst *, int);
int stop_stream (sip_msg_t *, call_lst *, int);

/**********
* local functions
**********/

/**********
* Process ACK Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = call pointer
* OUTPUT: none
**********/

void ack_msg (sip_msg_t *pmsg, call_lst *pcall)

{
/**********
* part of INVITE?
**********/

char *pfncname = "ack_msg: ";
struct cell *ptrans;
tm_api_t *ptm = pmod_data->ptm;
if (pcall->call_state != CLSTA_INVITED)
  {
  /**********
  * ignore if from rejected re-INVITE
  **********/

  if (pcall->call_state != CLSTA_INQUEUE)
    { LM_ERR ("%sUnexpected ACK (%s)!\n", pfncname, pcall->call_from); }
  else
    {
    mohq_debug (pcall->pmohq, "%sACK from refused re-INVITE (%s)!\n",
      pfncname, pcall->call_from);
    }
  return;
  }

/**********
* o release INVITE transaction
* o save SDP address info
* o put in queue
**********/

if (ptm->t_lookup_ident (&ptrans, pcall->call_hash, pcall->call_label) < 0)
  {
  LM_ERR ("%sINVITE transaction missing for call (%s)!\n",
    pfncname, pcall->call_from);
  return;
  }
else
  {
  if (ptm->t_release (pcall->call_pmsg) < 0)
    {
    LM_ERR ("%sRelease transaction failed for call (%s)!\n",
      pfncname, pcall->call_from);
    return;
    }
  }
pcall->call_hash = pcall->call_label = 0;
sprintf (pcall->call_addr, "%s %s",
  pmsg->rcv.dst_ip.af == AF_INET ? "IP4" : "IP6",
  ip_addr2a (&pmsg->rcv.dst_ip));
pcall->call_state = CLSTA_INQUEUE;
update_call_rec (pcall);
pcall->call_cseq = 1;
mohq_debug (pcall->pmohq,
  "%sACK received for call (%s); placed in queue (%s)",
  pfncname, pcall->call_from, pcall->pmohq->mohq_name);
return;
}

/**********
* Add String to Buffer
*
* INPUT:
*   Arg (1) = string pointer
*   Arg (2) = string length
*   Arg (3) = pointer to buffer pointer
*   Arg (4) = pointer to buffer size
*   Arg (5) = add NUL flag
* OUTPUT: =0 if insufficent space
*   bufpointer incremented, size decremented
**********/

int addstrbfr (char *pstr, size_t nlen, char **pbuf, size_t *nmax, int bnull)

{
/**********
* o enough space?
* o copy string
* o adjust position/size
**********/

size_t nsize = nlen;
if (bnull)
  { nsize++; }
if (nsize > *nmax)
  { return 0; }
if (nlen)
  {
  strncpy (*pbuf, pstr, nlen);
  *pbuf += nlen;
  }
if (bnull)
  {
  **pbuf = '\0';
  (*pbuf)++;
  }
*nmax -= nsize;
return 1;
}

/**********
* BYE Callback
*
* INPUT:
*   Arg (1) = cell pointer
*   Arg (2) = callback type
*   Arg (3) = callback parms
* OUTPUT: none
**********/

static void bye_cb
  (struct cell *ptrans, int ntype, struct tmcb_params *pcbp)

{
/**********
* o error means must have hung after REFER
* o delete the call
**********/

char *pfncname = "bye_cb: ";
call_lst *pcall = (call_lst *)*pcbp->param;
if (ntype == TMCB_ON_FAILURE)
  {
  LM_ERR ("%sCall (%s) did not respond to BYE!\n", pfncname,
    pcall->call_from);
  }
else
  {
  int nreply = pcbp->code;
  if ((nreply / 100) != 2)
    {
    LM_ERR ("%sCall (%s) BYE error (%d)!\n", pfncname,
      pcall->call_from, nreply);
    }
  else
    {
    mohq_debug (pcall->pmohq, "%sCall (%s) BYE reply=%d", pfncname,
      pcall->call_from, nreply);
    }
  }
delete_call (pcall);
return;
}

/**********
* Process BYE Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = call pointer
* OUTPUT: none
**********/

void bye_msg (sip_msg_t *pmsg, call_lst *pcall)

{
/**********
* o responded?
* o teardown RTP
**********/

char *pfncname = "bye_msg: ";
if (pcall->call_state == CLSTA_BYEOK)
  { return; }
if (pcall->call_state >= CLSTA_INQUEUE)
  {
  pcall->call_state = CLSTA_BYEOK;
  end_RTP (pmsg, pcall);
  }
else
  {
  LM_ERR ("%sEnding call (%s) before placed in queue!\n",
    pfncname, pcall->call_from);
  }

/**********
* send OK and delete from queue
**********/

if (pmod_data->psl->freply (pmsg, 200, presp_ok) < 0)
  {
  LM_ERR ("%sUnable to create reply to call (%s)!\n", pfncname,
    pcall->call_from);
  return;
  }
delete_call (pcall);
return;
}

/**********
* Process CANCEL Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = call pointer
* OUTPUT: none
**********/

void cancel_msg (sip_msg_t *pmsg, call_lst *pcall)

{
/**********
* RFC 3261 section 9.2
* o still in INVITE dialog?
**********/

char *pfncname = "cancel_msg: ";
if (pcall->call_state < CLSTA_INQUEUE)
  {
  pcall->call_state = CLSTA_CANCEL;
  mohq_debug (pcall->pmohq, "%sCANCELed call (%s)",
    pfncname, pcall->call_from);
  if (pmod_data->psl->freply (pmsg, 487, presp_reqterm) < 0)
    { LM_ERR ("%sUnable to create reply!\n", pfncname); }
  }
else
  {
  LM_ERR ("%sUnable to CANCEL because accepted INVITE for call (%s)!\n",
    pfncname, pcall->call_from);
  if (pmod_data->psl->freply (pmsg, 481, presp_nocall) < 0)
    { LM_ERR ("%sUnable to create reply!\n", pfncname); }
  }
return;
}

/**********
* Check if RTP Still Active
*
* INPUT:
*   Arg (1) = SIP message pointer
* OUTPUT: =0 if inactive
**********/

int chk_rtpstat (sip_msg_t *pmsg)

{
pv_value_t pval [1];
memset (pval, 0, sizeof (pv_value_t));
if (pv_get_spec_value (pmsg, prtp_pv, pval))
  { return 0; }
if (pval->flags & PV_VAL_NULL)
  { return 0; }
return 1;
}

/**********
* Close the Call
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = call pointer
* OUTPUT: none
**********/

void close_call (sip_msg_t *pmsg, call_lst *pcall)

{
/**********
* o destroy RTP connection
* o create dialog
**********/

char *pfncname = "close_call: ";
int bsent = 0;
char *phdr = 0;
end_RTP (pmsg, pcall);
struct to_body ptob [2];
dlg_t *pdlg = form_dialog (pcall, ptob);
if (!pdlg)
  { goto bye_err; }
pdlg->state = DLG_CONFIRMED;

/**********
* form BYE header
* o calculate size
* o create buffer
**********/

tm_api_t *ptm = pmod_data->ptm;
char *pquri = pcall->pmohq->mohq_uri;
int npos1 = sizeof (pbyemsg) // BYE template
  + strlen (pcall->call_via) // Via
  + strlen (pcall->call_route) // Route
  + strlen (pquri); // Contact
phdr = pkg_malloc (npos1);
if (!phdr)
  {
  LM_ERR ("%sNo more memory!\n", pfncname);
  goto bye_err;
  }
sprintf (phdr, pbyemsg,
  pcall->call_via, // Via
  pcall->call_route, // Route
  pquri); // Contact
str phdrs [1];
phdrs->s = phdr;
phdrs->len = strlen (phdr);

/**********
* send BYE request
**********/

uac_req_t puac [1];
set_uac_req (puac, pbye, phdrs, 0, pdlg,
  TMCB_LOCAL_COMPLETED | TMCB_ON_FAILURE, bye_cb, pcall);
pcall->call_state = CLSTA_BYE;
if (ptm->t_request_within (puac) < 0)
  {
  LM_ERR ("%sUnable to create BYE request for call (%s)!\n",
    pfncname, pcall->call_from);
  goto bye_err;
  }
mohq_debug (pcall->pmohq, "%sSent BYE request for call (%s)",
  pfncname, pcall->call_from);
bsent = 1;

/**********
* o free memory
* o delete call
**********/

bye_err:
if (pdlg)
  { pkg_free (pdlg); }
if (phdr)
  { pkg_free (phdr); }
if (!bsent)
  { delete_call (pcall); }
return;
}

/**********
* Create New Call Record
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = call pointer
*   Arg (3) = call index
*   Arg (4) = queue index
* OUTPUT: initializes call record; =0 if failed
**********/

int
create_call (sip_msg_t *pmsg, call_lst *pcall, int ncall_idx, int mohq_idx)

{
/**********
* add values to new entry
**********/

char *pfncname = "create_call: ";
pcall->pmohq = &pmod_data->pmohq_lst [mohq_idx];
str *pstr = &pmsg->callid->body;
char *pbuf = pcall->call_buffer;
pcall->call_buflen = sizeof (pcall->call_buffer);
pcall->call_id = pbuf;
if (!addstrbfr (pstr->s, pstr->len, &pbuf, &pcall->call_buflen, 1))
  { return 0; }
pstr = &pmsg->from->body;
pcall->call_from = pbuf;
if (!addstrbfr (pstr->s, pstr->len, &pbuf, &pcall->call_buflen, 1))
  { return 0; }
pcall->call_contact = pbuf;
if (pmsg->contact)
  {
  pstr = &pmsg->contact->body;
  if (!addstrbfr (pstr->s, pstr->len, &pbuf, &pcall->call_buflen, 0))
    { return 0; }
  }
if (!addstrbfr (0, 0, &pbuf, &pcall->call_buflen, 1))
  { return 0; }

/**********
* extract Via headers
**********/

pcall->call_via = pbuf;
hdr_field_t *phdr;
for (phdr = pmsg->h_via1; phdr; phdr = next_sibling_hdr (phdr))
  {
  struct via_body *pvia;
  char *pviabuf;
  int npos;
  for (pvia = (struct via_body *)phdr->parsed; pvia; pvia = pvia->next)
    {
    /**********
    * skip trailing whitespace
    **********/

    npos = pvia->bsize;
    pviabuf = pvia->name.s;
    while (npos)
      {
      --npos;
      if (pviabuf [npos] == ' ' || pviabuf [npos] == '\r'
        || pviabuf [npos] == '\n' || pviabuf [npos] == '\t'
        || pviabuf [npos] == ',')
        { continue; }
      break;
      }

    /**********
    * copy via
    **********/

    if (!addstrbfr ("Via: ", 5, &pbuf, &pcall->call_buflen, 0))
      { return 0; }
    if (!addstrbfr (pviabuf, npos + 1, &pbuf, &pcall->call_buflen, 0))
      { return 0; }
    if (!addstrbfr (SIPEOL, 2, &pbuf, &pcall->call_buflen, 0))
      { return 0; }
    }
  }
if (!addstrbfr (0, 0, &pbuf, &pcall->call_buflen, 1))
  { return 0; }

/**********
* extract Route headers
**********/

pcall->call_route = pbuf;
struct hdr_field *proute;
for (proute = pmsg->record_route; proute; proute = next_sibling_hdr (proute))
  {
  if (parse_rr (proute) < 0)
    { return 0; }
  rr_t *prouterr;
  for (prouterr = proute->parsed; prouterr; prouterr = prouterr->next)
    {
    if (!addstrbfr ("Route: ", 7, &pbuf, &pcall->call_buflen, 0))
      { return 0; }
    if (!addstrbfr (prouterr->nameaddr.name.s, prouterr->len,
      &pbuf, &pcall->call_buflen, 0))
      { return 0; }
    if (!addstrbfr (SIPEOL, 2, &pbuf, &pcall->call_buflen, 0))
      { return 0; }
    }
  }
if (!addstrbfr (0, 0, &pbuf, &pcall->call_buflen, 1))
  { return 0; }

/**********
* o place tag at the end
* o update DB
**********/

pcall->call_tag = pbuf;
if (!addstrbfr (0, 0, &pbuf, &pcall->call_buflen, 1))
  { return 0; }
pcall->call_state = CLSTA_ENTER;
add_call_rec (ncall_idx);
mohq_debug (pcall->pmohq, "%sAdded call (%s) to queue (%s)",
  pfncname, pcall->call_from, pcall->pmohq->mohq_name);
return 1;
}

/**********
* Delete Call
*
* INPUT:
*   Arg (1) = call pointer
* OUTPUT: none
**********/

void delete_call (call_lst *pcall)

{
/**********
* release transaction
**********/

char *pfncname = "delete_call: ";
struct cell *ptrans;
tm_api_t *ptm = pmod_data->ptm;
if (pcall->call_hash || pcall->call_label)
  {
  if (ptm->t_lookup_ident (&ptrans, pcall->call_hash, pcall->call_label) < 0)
    {
    LM_ERR ("%sLookup transaction failed for call (%s) from queue (%s)!\n",
      pfncname, pcall->call_from, pcall->pmohq->mohq_name);
    }
  else
    {
    if (ptm->t_release (pcall->call_pmsg) < 0)
      {
      LM_ERR ("%sRelease transaction failed for call (%s) from queue (%s)!\n",
        pfncname, pcall->call_from, pcall->pmohq->mohq_name);
      }
    }
  pcall->call_hash = pcall->call_label = 0;
  }

/**********
* o update DB
* o inactivate slot
**********/

if (!mohq_lock_set (pmod_data->pcall_lock, 1, 5000))
  {
  LM_ERR ("%sUnable to set call lock for call (%s) from queue (%s)!\n",
    pfncname, pcall->call_from, pcall->pmohq->mohq_name);
  }
else
  {
  mohq_debug (pcall->pmohq, "%sDeleting call (%s) from queue (%s)",
    pfncname, pcall->call_from, pcall->pmohq->mohq_name);
  delete_call_rec (pcall);
  mohq_lock_release (pmod_data->pcall_lock);
  }
pcall->call_state = 0;
return;
}

/**********
* Deny Method
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = call pointer
* OUTPUT: none
**********/

void deny_method (sip_msg_t *pmsg, call_lst *pcall)

{
/**********
* RFC 3261 section 8.2.1
* o get transaction
* o respond with 405 and Allow header
**********/

char *pfncname = "deny_method: ";
tm_api_t *ptm = pmod_data->ptm;
if (ptm->t_newtran (pmsg) < 0)
  {
  LM_ERR ("%sUnable to create new transaction!\n", pfncname);
  if (pmod_data->psl->freply (pmsg, 500, presp_srverr) < 0)
    {
    LM_ERR ("%sUnable to create reply to %.*s!\n", pfncname,
      STR_FMT (&REQ_LINE (pmsg).method));
    }
  return;
  }
if (!add_lump_rpl2 (pmsg, pallowhdr->s, pallowhdr->len, LUMP_RPL_HDR))
  { LM_ERR ("%sUnable to add Allow header!\n", pfncname); }
LM_ERR ("%sRefused %.*s for call (%s)!\n", pfncname,
  STR_FMT (&REQ_LINE (pmsg).method), pcall->call_from);
if (ptm->t_reply (pmsg, 405, presp_noallow->s) < 0)
  {
  LM_ERR ("%sUnable to create reply to %.*s!\n", pfncname,
    STR_FMT (&REQ_LINE (pmsg).method));
  }
return;
}

/**********
* End RTP
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = call pointer
* OUTPUT: none
**********/

void end_RTP (sip_msg_t *pmsg, call_lst *pcall)

{
/**********
* destroy RTP connection
**********/

char *pfncname = "end_RTP: ";
if ((pmsg != FAKED_REPLY) && (pcall->call_state != CLSTA_ENTER))
  {
  mohq_debug (pcall->pmohq, "%sDestroying RTP link for call (%s)",
    pfncname, pcall->call_from);
  if (pmod_data->fn_rtp_destroy (pmsg, 0, 0) != 1)
    {
    LM_ERR ("%srtpproxy_destroy refused for call (%s)!\n",
      pfncname, pcall->call_from);
    }
  }
return;
}

/**********
* Find Call
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = queue index
* OUTPUT: call pointer; =0 if unable to find/create
**********/

call_lst *find_call (sip_msg_t *pmsg, int mohq_idx)

{
/**********
* o get to tag
* o get callID
* o search calls
**********/

char *pfncname = "find_call: ";
str *ptotag = &(get_to (pmsg)->tag_value);
if (!ptotag->len)
  { ptotag = 0; }
if (!pmsg->callid)
  {
  LM_ERR ("%sNo call ID!\n", pfncname);
  return 0;
  }
str *pcallid = &pmsg->callid->body;
if (!pcallid)
  {
  LM_ERR ("%sNo call ID!\n", pfncname);
  return 0;
  }
int nopen = -1;
int nidx;
call_lst *pcall;
for (nidx = 0; nidx < pmod_data->call_cnt; nidx++)
  {
  /**********
  * o call active?
  * o call timed out on ACK?
  * o callID matches?
  * o to tag matches?
  **********/

  pcall = &pmod_data->pcall_lst [nidx];
  if (!pcall->call_state)
    {
    nopen = nidx;
    continue;
    }
#if 0 /* ??? need to handle */
  if (pcall->call_time && (pcall->call_state < CLSTA_INQUEUE))
    {
    if ((pcall->call_time + 32) < time (0))
      {
      LM_ERR ("%sNo ACK response for call (%s)!\n", pfncname, pcall->call_from);
      delete_call (pcall);
      continue;
      }
    }
#endif /* ??? */
  str tmpstr [1];
  tmpstr->s = pcall->call_id;
  tmpstr->len = strlen (tmpstr->s);
  if (!STR_EQ (*tmpstr, *pcallid))
    { continue; }
  if (ptotag)
    {
    tmpstr->s = pcall->call_tag;
    tmpstr->len = strlen (tmpstr->s);
    if (!STR_EQ (*tmpstr, *ptotag))
      { continue; }
    }
  else
    {
    /**********
    * match not allowed for INVITE
    **********/

    if (pmsg->REQ_METHOD == METHOD_INVITE)
      { return 0; }
    }
  return pcall;
  }

/**********
* o first INVITE?
* o create a new call record
**********/

if (pmsg->REQ_METHOD != METHOD_INVITE)
  { return 0; }
if (ptotag)
  { return 0; }
if (nopen < 0)
  {
  LM_ERR ("%sNo call slots available!\n", pfncname);
  return 0;
  }
pcall = &pmod_data->pcall_lst [nopen];
if (!create_call (pmsg, pcall, nopen, mohq_idx))
  { return 0; }
return pcall;
}

/**********
* Find Queue
*
* INPUT:
*   Arg (1) = SIP message pointer
* OUTPUT: queue index; -1 if unable to find
**********/

int find_queue (sip_msg_t *pmsg)

{
/**********
* o find current RURI
* o strip off parms or headers
* o search queues
**********/

str *pruri =
  pmsg->new_uri.s ? &pmsg->new_uri : &pmsg->first_line.u.request.uri;
int nidx;
str pstr [1];
pstr->s = pruri->s;
pstr->len = pruri->len;
for (nidx = 0; nidx < pruri->len; nidx++)
  {
  if (pstr->s [nidx] == ';' || pstr->s [nidx] == '?')
    {
    pstr->len = nidx;
    break;
    }
  }
mohq_lst *pqlst = pmod_data->pmohq_lst;
int nqidx;
for (nqidx = 0; nqidx < pmod_data->mohq_cnt; nqidx++)
  {
  str pmohstr [1];
  pmohstr->s = pqlst [nqidx].mohq_uri;
  pmohstr->len = strlen (pmohstr->s);
  if (STR_EQ (*pmohstr, *pstr))
    { break; }
  }
if (nqidx == pmod_data->mohq_cnt)
  { return -1;}
return nqidx;
}

/**********
* Find Queue Name
*
* INPUT:
*   Arg (1) = queue name str pointer
* OUTPUT: queue index; -1 if unable to find
**********/

int find_qname (str *pqname)

{
char *pfncname = "find_qname: ";
int nidx;
str tmpstr;
if (!mohq_lock_set (pmod_data->pmohq_lock, 0, 500))
  {
  LM_ERR ("%sUnable to lock queues!\n", pfncname);
  return -1;
  }
for (nidx = 0; nidx < pmod_data->mohq_cnt; nidx++)
  {
  tmpstr.s = pmod_data->pmohq_lst [nidx].mohq_name;
  tmpstr.len = strlen (tmpstr.s);
  if (STR_EQ (tmpstr, *pqname))
    { break; }
  }
if (nidx == pmod_data->mohq_cnt)
  {
  LM_ERR ("%sUnable to find queue (%.*s)!\n", pfncname, STR_FMT (pqname));
  nidx = -1;
  }
mohq_lock_release (pmod_data->pmohq_lock);
return nidx;
}

/**********
* Find Referred Call
*
* INPUT:
*   Arg (1) = referred-by value
* OUTPUT: call index; -1 if unable to find
**********/

int find_referred_call (str *pvalue)

{
/**********
* get URI
**********/

char *pfncname = "find_referred_call: ";
struct to_body pref [1];
parse_to (pvalue->s, &pvalue->s [pvalue->len + 1], pref);
if (pref->error != PARSE_OK)
  {
  // should never happen
  LM_ERR ("%sInvalid Referred-By URI (%.*s)!\n", pfncname, STR_FMT (pvalue));
  return -1;
  }
if (pref->param_lst)
  { free_to_params (pref); }

/**********
* search calls for matching
**********/

int nidx;
str tmpstr;
struct to_body pfrom [1];
for (nidx = 0; nidx < pmod_data->call_cnt; nidx++)
  {
  if (!pmod_data->pcall_lst [nidx].call_state)
    { continue; }
  tmpstr.s = pmod_data->pcall_lst [nidx].call_from;
  tmpstr.len = strlen (tmpstr.s);
  parse_to (tmpstr.s, &tmpstr.s [tmpstr.len + 1], pfrom);
  if (pfrom->error != PARSE_OK)
    {
    // should never happen
    LM_ERR ("%sInvalid From URI (%.*s)!\n", pfncname, STR_FMT (&tmpstr));
    continue;
    }
  if (pfrom->param_lst)
    { free_to_params (pfrom); }
  if (STR_EQ (pfrom->uri, pref->uri))
    { return nidx; }
  }
return -1;
}

/**********
* Process First INVITE Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = call pointer
* OUTPUT: none
**********/

void first_invite_msg (sip_msg_t *pmsg, call_lst *pcall)

{
char *pfncname = "first_invite_msg: ";

/**********
* o SDP exists?
* o accepts REFER?
* o send RTP offer
**********/

if (!(pmsg->msg_flags & FL_SDP_BODY))
  {
  if (parse_sdp (pmsg))
    {
    if (pmod_data->psl->freply (pmsg, 488, presp_noaccept) < 0)
      {
      LM_ERR ("%sUnable to create reply!\n", pfncname);
      return;
      }
    LM_ERR ("%sINVITE lacks SDP (%s) from queue (%s)!\n",
      pfncname, pcall->call_from, pcall->pmohq->mohq_name);
    delete_call (pcall);
    return;
    }
  }
if (pmsg->allow)
  {
  if (!search_hdr_ext (pmsg->allow, prefer))
    {
    if (pmod_data->psl->freply (pmsg, 488, presp_noaccept) < 0)
      {
      LM_ERR ("%sUnable to create reply!\n", pfncname);
      return;
      }
    LM_ERR ("%sMissing REFER support (%s) from queue (%s)!\n",
      pfncname, pcall->call_from, pcall->pmohq->mohq_name);
    delete_call (pcall);
    return;
    }
  }
mohq_debug (pcall->pmohq,
  "%sMaking offer for RTP link for call (%s) from queue (%s)",
  pfncname, pcall->call_from, pcall->pmohq->mohq_name);
if (pmod_data->fn_rtp_offer (pmsg, 0, 0) != 1)
  {
  if (pmod_data->psl->freply (pmsg, 486, presp_busy) < 0)
    {
    LM_ERR ("%sUnable to create reply!\n", pfncname);
    return;
    }
  LM_ERR ("%srtpproxy_offer refused for call (%s)!\n",
      pfncname, pcall->call_from);
  delete_call (pcall);
  return;
  }

/**********
* o create new transaction
* o save To tag
* o catch failures
* o save transaction data
**********/

tm_api_t *ptm = pmod_data->ptm;
if (ptm->t_newtran (pmsg) < 0)
  {
  LM_ERR ("%sUnable to create new transaction for call (%s)!\n",
    pfncname, pcall->call_from);
  end_RTP (pmsg, pcall);
  delete_call (pcall);
  return;
  }
struct cell *ptrans = ptm->t_gett ();
pcall->call_hash = ptrans->hash_index;
pcall->call_label = ptrans->label;
str ptotag [1];
if (ptm->t_get_reply_totag (pmsg, ptotag) != 1)
  {
  LM_ERR ("%sUnable to create totag for call (%s)!\n",
    pfncname, pcall->call_from);
  if (ptm->t_reply (pmsg, 500, presp_srverr->s) < 0)
    { LM_ERR ("%sUnable to reply to INVITE!\n", pfncname); }
  end_RTP (pmsg, pcall);
  delete_call (pcall);
  return;
  }
char *pbuf = pcall->call_tag;
if (!addstrbfr (ptotag->s, ptotag->len, &pbuf, &pcall->call_buflen, 1))
  {
  LM_ERR ("%sInsufficient buffer space for call (%s)!\n",
    pfncname, pcall->call_from);
  if (ptm->t_reply (pmsg, 500, presp_srverr->s) < 0)
    { LM_ERR ("%sUnable to reply to INVITE!\n", pfncname); }
  end_RTP (pmsg, pcall);
  delete_call (pcall);
  return;
  }
pcall->call_cseq = 1;
if (ptm->register_tmcb (pmsg, 0, TMCB_DESTROY | TMCB_ON_FAILURE,
  invite_cb, pcall, 0) < 0)
  {
  LM_ERR ("%sUnable to set callback for call (%s)!\n",
    pfncname, pcall->call_from);
  if (ptm->t_reply (pmsg, 500, presp_srverr->s) < 0)
    { LM_ERR ("%sUnable to reply to INVITE!\n", pfncname); }
  end_RTP (pmsg, pcall);
  delete_call (pcall);
  return;
  }

/**********
* reply with trying
**********/

if (ptm->t_reply (pmsg, 100, presp_trying->s) < 0)
  {
  LM_ERR ("%sUnable to create reply!\n", pfncname);
  end_RTP (pmsg, pcall);
  delete_call (pcall);
  return;
  }
pcall->call_state = CLSTA_TRYING;

/**********
* o add contact to reply
* o supports/requires PRACK? (RFC 3262 section 3)
* o exit if not ringing
**********/

str pcontact [1];
char *pcontacthdr = "Contact: <%s>" SIPEOL;
pcontact->s
  = pkg_malloc (strlen (pcall->pmohq->mohq_uri) + strlen (pcontacthdr));
if (!pcontact->s)
  {
  LM_ERR ("%sNo more memory!\n", pfncname);
  end_RTP (pmsg, pcall);
  delete_call (pcall);
  return;
  }
sprintf (pcontact->s, pcontacthdr, pcall->pmohq->mohq_uri);
pcontact->len = strlen (pcontact->s);
if (!add_lump_rpl2 (pmsg, pcontact->s, pcontact->len, LUMP_RPL_HDR))
  {
  LM_ERR ("%sUnable to add contact (%s) to call (%s)!\n",
    pfncname, pcontact->s, pcall->call_from);
  }
pkg_free (pcontact->s);
pcall->call_pmsg = pmsg;
if (search_hdr_ext (pmsg->require, p100rel))
  {
  if (!send_prov_rsp (pmsg, pcall))
    {
    end_RTP (pmsg, pcall);
    delete_call (pcall);
    return;
    }
  if (pcall->call_state == CLSTA_CANCEL)
    {
    end_RTP (pmsg, pcall);
    delete_call (pcall);
    return;
    }
  }

/**********
* accept call with RTP
**********/

if (!send_rtp_answer (pmsg, pcall))
  {
  if (pmod_data->psl->freply (pmsg, 500, presp_srverr) < 0)
    { LM_ERR ("%sUnable to create reply!\n", pfncname); }
  end_RTP (pmsg, pcall);
  delete_call (pcall);
  }
return;
}

/**********
* Form Dialog
*
* INPUT:
*   Arg (1) = call pointer
*   Arg (2) = to_body [2] pointer
* OUTPUT: dlg_t * if successful; 0=if not
**********/

dlg_t *form_dialog (call_lst *pcall, struct to_body *pto_body)

{
/**********
* get from/to values
**********/

char *pfncname = "form_dialog: ";
str pdsturi [1], ptarget [1];
int index;
name_addr_t pname [1];
struct to_body *ptob = &pto_body [0];
struct to_body *pcontact = &pto_body [1];
parse_to (pcall->call_from,
  &pcall->call_from [strlen (pcall->call_from) + 1], ptob);
if (ptob->error != PARSE_OK)
  {
  // should never happen
  LM_ERR ("%sInvalid from URI (%s)!\n", pfncname, pcall->call_from);
  return 0;
  }
if (ptob->param_lst)
  { free_to_params (ptob); }

/**********
* form dest URI from record route
**********/

if (!*pcall->call_route)
  { pdsturi->s = 0; }
else
  {
  /**********
  * o find first route URI
  * o strip off parameter
  **********/

  pdsturi->s = pcall->call_route;
  pdsturi->len = strlen (pcall->call_route);
  if (parse_nameaddr (pdsturi, pname) < 0)
    {
    // should never happen
    LM_ERR ("%sUnable to parse route (%s)!\n", pfncname, pcall->call_from);
    return 0;
    }
  pdsturi->s = pname->uri.s;
  pdsturi->len = pname->uri.len;
  for (index = 1; index < pdsturi->len; index++)
    {
    if (pdsturi->s [index] == ';')
      {
      pdsturi->len = index;
      break;
      }
    }
  }

/**********
* form target URI
**********/

if (!*pcall->call_contact)
  {
  ptarget->s = ptob->uri.s;
  ptarget->len = ptob->uri.len;
  }
else
  {
  parse_to (pcall->call_contact,
    &pcall->call_contact [strlen (pcall->call_contact) + 1], pcontact);
  if (pcontact->error != PARSE_OK)
    {
    // should never happen
    LM_ERR ("%sInvalid contact (%s) for call (%s)!\n", pfncname,
      pcall->call_contact, pcall->call_from);
    return 0;
    }
  if (pcontact->param_lst)
    { free_to_params (pcontact); }
  ptarget->s = pcontact->uri.s;
  ptarget->len = pcontact->uri.len;
  }

/**********
* create dialog
**********/

dlg_t *pdlg = (dlg_t *)pkg_malloc (sizeof (dlg_t));
if (!pdlg)
  {
  LM_ERR ("%sNo more memory!\n", pfncname);
  return 0;
  }
memset (pdlg, 0, sizeof (dlg_t));
pdlg->loc_seq.value = pcall->call_cseq++;
pdlg->loc_seq.is_set = 1;
pdlg->id.call_id.s = pcall->call_id;
pdlg->id.call_id.len = strlen (pcall->call_id);
pdlg->id.loc_tag.s = pcall->call_tag;
pdlg->id.loc_tag.len = strlen (pcall->call_tag);
pdlg->id.rem_tag.s = ptob->tag_value.s;
pdlg->id.rem_tag.len = ptob->tag_value.len;
pdlg->rem_target.s = ptarget->s;
pdlg->rem_target.len = ptarget->len;
pdlg->loc_uri.s = pcall->pmohq->mohq_uri;
pdlg->loc_uri.len = strlen (pdlg->loc_uri.s);
pdlg->rem_uri.s = ptob->uri.s;
pdlg->rem_uri.len = ptob->uri.len;
if (pdsturi->s)
  {
  pdlg->dst_uri.s = pdsturi->s;
  pdlg->dst_uri.len = pdsturi->len;
  }
return pdlg;
}

/**********
* Form RTP SDP String
*
* INPUT:
*   Arg (1) = string pointer
*   Arg (2) = call pointer
*   Arg (3) = SDP body pointer
* OUTPUT: 0 if failed
**********/

int form_rtp_SDP (str *pstr, call_lst *pcall, char *pSDP)

{
/**********
* o find available files
* o calculate size of SDP
**********/

char *pfncname = "form_rtp_SDP: ";
rtpmap **pmohfiles = find_MOH (pcall->pmohq->mohq_mohdir,
  pcall->pmohq->mohq_mohfile);
if (!pmohfiles [0])
  {
  LM_ERR ("%sUnable to find any MOH files for queue (%s)!\n", pfncname,
    pcall->pmohq->mohq_name);
  return 0;
  }
int nsize = strlen (pSDP) + 2;
int nidx;
for (nidx = 0; pmohfiles [nidx]; nidx++)
  {
  nsize += strlen (pmohfiles [nidx]->pencode) // encode length
    + 19; // space, type number, "a=rtpmap:%d ", EOL
  }

/**********
* o allocate memory
* o form SDP
**********/

pstr->s = pkg_malloc (nsize + 1);
if (!pstr->s)
  {
  LM_ERR ("%sNo more memory!\n", pfncname);
  return 0;
  }
strcpy (pstr->s, pSDP);
nsize = strlen (pstr->s);
for (nidx = 0; pmohfiles [nidx]; nidx++)
  {
  /**********
  * add payload types to media description
  **********/

  sprintf (&pstr->s [nsize], " %d", pmohfiles [nidx]->ntype);
  nsize += strlen (&pstr->s [nsize]);
  }
strcpy (&pstr->s [nsize], SIPEOL);
nsize += 2;
for (nidx = 0; pmohfiles [nidx]; nidx++)
  {
  /**********
  * add rtpmap attributes
  **********/

  sprintf (&pstr->s [nsize], "a=rtpmap:%d %s %s",
    pmohfiles [nidx]->ntype, pmohfiles [nidx]->pencode, SIPEOL);
  nsize += strlen (&pstr->s [nsize]);
  }
pstr->len = nsize;
return 1;
}

/**********
* Invite Callback
*
* INPUT:
*   Arg (1) = cell pointer
*   Arg (2) = callback type
*   Arg (3) = callback parms
* OUTPUT: none
**********/

static void
  invite_cb (struct cell *ptrans, int ntype, struct tmcb_params *pcbp)

{
call_lst *pcall = (call_lst *)*pcbp->param;
if (ntype == TMCB_DESTROY)
  { pcall->call_hash = pcall->call_label = 0; }
LM_ERR ("invite_cb: INVITE failed for call (%s)!\n", pcall->call_from);
delete_call (pcall);
return;
}

/**********
* Process NOTIFY Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = call pointer
* OUTPUT: none
**********/

void notify_msg (sip_msg_t *pmsg, call_lst *pcall)

{
/**********
* waiting on REFER?
**********/

char *pfncname = "notify_msg: ";
if (pcall->call_state != CLSTA_RFRWAIT)
  {
  LM_ERR ("%sNot waiting on a REFER for call (%s)!\n", pfncname,
    pcall->call_from);
  if (pmod_data->psl->freply (pmsg, 481, presp_nocall) < 0)
    { LM_ERR ("%sUnable to create reply!\n", pfncname); }
  return;
  }

/**********
* o sipfrag?
* o get status from body
* o add CRLF so parser can go beyond first line
**********/

if (!search_hdr_ext (pmsg->content_type, psipfrag))
  {
  LM_ERR ("%sNot a %s type for call (%s)!\n", pfncname,
    psipfrag->s, pcall->call_from);
  if (pmod_data->psl->freply (pmsg, 415, presp_unsupp) < 0)
    { LM_ERR ("%sUnable to create reply!\n", pfncname); }
  return;
  }
char *pfrag = get_body (pmsg);
if (!pfrag)
  {
  LM_ERR ("%s%s body missing for call (%s)!\n", pfncname,
    psipfrag->s, pcall->call_from);
  if (pmod_data->psl->freply (pmsg, 415, presp_unsupp) < 0)
    { LM_ERR ("%sUnable to create reply!\n", pfncname); }
  return;
  }
str pbody [1];
pbody->len = pmsg->len - (int)(pfrag - pmsg->buf);
pbody->s = pkg_malloc (pbody->len + 2);
if (!pbody->s)
  {
  LM_ERR ("%sNo more memory!\n", pfncname);
  return;
  }
strncpy (pbody->s, pfrag, pbody->len);
if (pbody->s [pbody->len - 1] != '\n')
  {
  strncpy (&pbody->s [pbody->len], SIPEOL, 2);
  pbody->len += 2;
  }
struct msg_start pstart [1];
parse_first_line (pbody->s, pbody->len + 1, pstart);
pkg_free (pbody->s);
if (pstart->type != SIP_REPLY)
  {
  LM_ERR ("%sReply missing for call (%s)!\n", pfncname, pcall->call_from);
  if (pmod_data->psl->freply (pmsg, 415, presp_unsupp) < 0)
    { LM_ERR ("%sUnable to create reply!\n", pfncname); }
  return;
  }

/**********
* o send OK
* o REFER done?
**********/

int nreply = pstart->u.reply.statuscode;
if (pmod_data->psl->freply (pmsg, 200, presp_ok) < 0)
  {
  LM_ERR ("%sUnable to create reply for call (%s)!\n",
    pfncname, pcall->call_from);
  return;
  }
mohq_debug (pcall->pmohq, "%sNOTIFY received reply (%d) for call (%s)",
  pfncname, nreply, pcall->call_from);
switch (nreply / 100)
  {
  case 1:
    pcall->refer_time = time (0);
    break;
  case 2:
    close_call (pmsg, pcall);
    break;
  default:
    LM_WARN ("%sUnable to redirect call (%s)\n", pfncname, pcall->call_from);
    if (nreply == 487)
      {
      /**********
      * call was cancelled
      **********/

      end_RTP (pmsg, pcall);
      delete_call (pcall);
      return;
      }

    /**********
    * return call to queue
    **********/

    pcall->call_state = CLSTA_INQUEUE;
    update_call_rec (pcall);
    break;
  }
return;
}

/**********
* Process PRACK Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = call pointer
* OUTPUT: none
**********/

void prack_msg (sip_msg_t *pmsg, call_lst *pcall)

{
/**********
* waiting on PRACK?
**********/

char *pfncname = "prack_msg: ";
tm_api_t *ptm = pmod_data->ptm;
if (pcall->call_state != CLSTA_PRACKSTRT)
  {
  LM_ERR ("%sUnexpected PRACK (%s)!\n", pfncname, pcall->call_from);
  if (pmod_data->psl->freply (pmsg, 481, presp_nocall) < 0)
    { LM_ERR ("%sUnable to create reply!\n", pfncname); }
  return;
  }

/**********
* o check RAck ??? need to check
* o accept PRACK
**********/

if (ptm->t_newtran (pmsg) < 0)
  {
  LM_ERR ("%sUnable to create new transaction for call (%s)!\n",
    pfncname, pcall->call_from);
  if (pmod_data->psl->freply (pmsg, 500, presp_srverr) < 0)
    { LM_ERR ("%sUnable to create reply!\n", pfncname); }
  return;
  }
if (ptm->t_reply (pmsg, 200, presp_ok->s) < 0)
  {
  LM_ERR ("%sUnable to reply to PRACK for call (%s)!\n",
    pfncname, pcall->call_from);
  return;
  }
pcall->call_state = CLSTA_PRACKRPLY;
return;
}

/**********
* Refer Call
*
* INPUT:
*   Arg (1) = call pointer
*   Arg (2) = lock pointer
* OUTPUT: 0 if failed
**********/

int refer_call (call_lst *pcall, mohq_lock *plock)

{
/**********
* create dialog
**********/

char *pfncname = "refer_call: ";
int nret = 0;
struct to_body ptob [2];
dlg_t *pdlg = form_dialog (pcall, ptob);
if (!pdlg)
  {
  mohq_lock_release (plock);
  return 0;
  }
pdlg->state = DLG_CONFIRMED;

/**********
* form REFER message
* o calculate basic size
* o create buffer
**********/

str puri [1];
puri->s = pcall->call_referto;
puri->len = strlen (puri->s);
int npos1 = sizeof (prefermsg) // REFER template
  + strlen (pcall->call_via) // Via
  + strlen (pcall->call_route) // Route
  + strlen (pcall->pmohq->mohq_uri) // Contact
  + puri->len // Refer-To
  + strlen (pcall->pmohq->mohq_uri); // Referred-By
char *pbuf = pkg_malloc (npos1);
if (!pbuf)
  {
  LM_ERR ("%sNo more memory!\n", pfncname);
  goto refererr;
  }
sprintf (pbuf, prefermsg,
  pcall->call_via, // Via
  pcall->call_route, // Route
  pcall->pmohq->mohq_uri, // Contact
  puri->s, // Refer-To
  pcall->pmohq->mohq_uri); // Referred-By

/**********
* send REFER request
**********/

tm_api_t *ptm = pmod_data->ptm;
uac_req_t puac [1];
str phdrs [1];
phdrs->s = pbuf;
phdrs->len = strlen (pbuf);
set_uac_req (puac, prefer, phdrs, 0, pdlg,
  TMCB_LOCAL_COMPLETED | TMCB_ON_FAILURE, refer_cb, pcall);
pcall->refer_time = time (0);
pcall->call_state = CLSTA_REFER;
update_call_rec (pcall);
mohq_lock_release (plock);
if (ptm->t_request_within (puac) < 0)
  {
  pcall->call_state = CLSTA_INQUEUE;
  LM_ERR ("%sUnable to create REFER request for call (%s)!\n",
    pfncname, pcall->call_from);
  update_call_rec (pcall);
  goto refererr;
  }
mohq_debug (pcall->pmohq, "%sSent REFER request for call (%s) to %s",
  pfncname, pcall->call_from, pcall->call_referto);
nret = -1;

refererr:
if (pdlg)
  { pkg_free (pdlg); }
pkg_free (pbuf);
return nret;
}

/**********
* REFER Callback
*
* INPUT:
*   Arg (1) = cell pointer
*   Arg (2) = callback type
*   Arg (3) = callback parms
* OUTPUT: none
**********/

static void refer_cb
  (struct cell *ptrans, int ntype, struct tmcb_params *pcbp)
{
char *pfncname = "refer_cb: ";
call_lst *pcall = (call_lst *)*pcbp->param;
if (pcall->call_state != CLSTA_REFER)
  {
  if (!pcall->call_state)
    {
    LM_ERR
      ("%sREFER response ignored because call not in queue!\n", pfncname);
    }
  else
    {
    LM_ERR ("%sCall (%s) ignored because not in REFER state!\n", pfncname,
      pcall->call_from);
    }
  return;
  }
if ((ntype == TMCB_ON_FAILURE) || (pcbp->req == FAKED_REPLY))
  {
  LM_ERR ("%sCall (%s) did not respond to REFER!\n", pfncname,
    pcall->call_from);
  end_RTP (pcbp->req, pcall);
  delete_call (pcall);
  return;
  }

/**********
* check reply
**********/

int nreply = pcbp->code;
if ((nreply / 100) == 2)
  {
  pcall->refer_time = time (0);
  pcall->call_state = CLSTA_RFRWAIT;
  mohq_debug (pcall->pmohq, "%sCall (%s) REFER reply=%d",
    pfncname, pcall->call_from, nreply);
  }
else
  {
  LM_ERR ("%sCall (%s) REFER error (%d)!\n", pfncname,
    pcall->call_from, nreply);
  if (nreply == 481)
    { delete_call (pcall); }
  else
    {
    if (!chk_rtpstat (pcbp->req))
      {
      LM_ERR ("%sRTP for call (%s) no longer active!\n",
        pfncname, pcall->call_from);
      delete_call (pcall);
      }
    else
      {
      pcall->call_state = CLSTA_INQUEUE;
      update_call_rec (pcall);
      }
    }
  }
return;
}

/**********
* Process re-INVITE Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = call pointer
* OUTPUT: none
**********/

void reinvite_msg (sip_msg_t *pmsg, call_lst *pcall)

{
/**********
* RFC 3261 section 14.2
* o dialog pending?
* o get SDP
**********/

char *pfncname = "reinvite_msg: ";
if ((pcall->call_state / 100) < 2)
  {
  mohq_debug (pcall->pmohq, "%sINVITE still pending for call (%s)",
    pfncname, pcall->call_from);
  if (pmod_data->psl->freply (pmsg, 491, presp_reqpend) < 0)
    { LM_ERR ("%sUnable to create reply!\n", pfncname); }
  return;
  }
if (!(pmsg->msg_flags & FL_SDP_BODY))
  {
  if (parse_sdp (pmsg))
    {
    LM_ERR ("%sre-INVITE lacks SDP (%s)!\n", pfncname, pcall->call_from);
    if (pmod_data->psl->freply (pmsg, 488, presp_noaccept) < 0)
      { LM_ERR ("%sUnable to create reply!\n", pfncname); }
    return;
    }
  }

/**********
* o find available MOH files
* o look for hold condition and matching payload type
**********/

rtpmap **pmohfiles = find_MOH (pcall->pmohq->mohq_mohdir,
  pcall->pmohq->mohq_mohfile);
int bhold = 0;
int bmatch = 0;
int nsession;
sdp_session_cell_t *psession;
for (nsession = 0; (psession = get_sdp_session (pmsg, nsession)); nsession++)
  {
  int nstream;
  sdp_stream_cell_t *pstream;
  for (nstream = 0; (pstream = get_sdp_stream (pmsg, nsession, nstream));
    nstream++)
    {
    /**********
    * o RTP?
    * o audio?
    * o hold?
    * o at least one payload matches?
    **********/

    if (!pstream->is_rtp)
      { continue; }
    if (!STR_EQ (*paudio, pstream->media))
      { continue; }
    if (pstream->is_on_hold)
      {
      bhold = 1;
      break;
      }
    if (bmatch)
      { continue; }

    /**********
    * check payload types for a match
    **********/

    sdp_payload_attr_t *ppayload;
    for (ppayload = pstream->payload_attr; ppayload; ppayload = ppayload->next)
      {
      int ntype = atoi (ppayload->rtp_payload.s);
      int nidx;
      for (nidx = 0; pmohfiles [nidx]; nidx++)
        {
        if (pmohfiles [nidx]->ntype == ntype)
          {
          bmatch = 1;
          break;
          }
        }
      }
    }
  }

/**********
* if no hold, allow re-INVITE if matching file
**********/

if (!bhold)
  {
  if (!bmatch)
    {
    LM_ERR ("%sre-INVITE refused because no matching payload for call (%s)!\n",
      pfncname, pcall->call_from);
    if (pmod_data->psl->freply (pmsg, 488, presp_noaccept) < 0)
      {
      LM_ERR ("%sUnable to create reply!\n", pfncname);
      return;
      }
    }
  else
    {
    mohq_debug (pcall->pmohq, "%sAccepted re-INVITE for call (%s)",
      pfncname, pcall->call_from);
    if (pmod_data->psl->freply (pmsg, 200, presp_ok) < 0)
      {
      LM_ERR ("%sUnable to create reply!\n", pfncname);
      return;
      }
    }
  return;
  }

/**********
* hold not allowed, say good-bye
**********/

LM_ERR ("%sTerminating call (%s) because hold not allowed!\n",
  pfncname, pcall->call_from);
if (pmod_data->psl->freply (pmsg, 200, presp_ok) < 0)
  {
  LM_ERR ("%sUnable to create reply!\n", pfncname);
  return;
  }
close_call (pmsg, pcall);
return;
}

/**********
* Search Header for Extension
*
* INPUT:
*   Arg (1) = header field pointer
*   Arg (2) = extension str pointer
* OUTPUT: 0=not found
**********/

int search_hdr_ext (struct hdr_field *phdr, str *pext)

{
if (!phdr)
  { return 0; }
str *pstr = &phdr->body;
int npos1, npos2;
for (npos1 = 0; npos1 < pstr->len; npos1++)
  {
  /**********
  * o find non-space
  * o search to end, space or comma
  * o same size?
  * o same name?
  **********/

  if (pstr->s [npos1] == ' ')
    { continue; }
  for (npos2 = npos1++; npos1 < pstr->len; npos1++)
    {
    if (pstr->s [npos1] == ' ' || pstr->s [npos1] == ',')
      { break; }
    }
  if (npos1 - npos2 != pext->len)
    { continue; }
  if (!strncasecmp (&pstr->s [npos2], pext->s, pext->len))
    { return 1; }
  }
return 0;
}

/**********
* Send Provisional Response
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = call pointer
* OUTPUT: 0=unable to process; 1=processed
**********/

int send_prov_rsp (sip_msg_t *pmsg, call_lst *pcall)

{
/**********
* o send ringing response with require
* o update record
**********/

char *pfncname = "send_prov_rsp: ";
tm_api_t *ptm = pmod_data->ptm;
pcall->call_cseq = rand ();
char phdrtmp [200];
char *phdrtmplt =
  "Accept-Language: en" SIPEOL
  "Require: 100rel" SIPEOL
  "RSeq: %d" SIPEOL
  ;
sprintf (phdrtmp, phdrtmplt, pcall->call_cseq);
struct lump_rpl **phdrlump = add_lump_rpl2 (pmsg, phdrtmp,
  strlen (phdrtmp), LUMP_RPL_HDR);
if (!phdrlump)
  {
  LM_ERR ("%sUnable to create new header for call (%s)!\n",
    pfncname, pcall->call_from);
  if (pmod_data->psl->freply (pmsg, 500, presp_srverr) < 0)
    { LM_ERR ("%sUnable to create reply!\n", pfncname); }
  return 0;
  }
if (ptm->t_reply (pmsg, 180, presp_ring->s) < 0)
  {
  LM_ERR ("%sUnable to reply to INVITE for call (%s)!\n",
    pfncname, pcall->call_from);
  return 0;
  }
pcall->call_state = CLSTA_PRACKSTRT;
mohq_debug (pcall->pmohq, "%sSent PRACK RINGING for call (%s)",
  pfncname, pcall->call_from);

/**********
* o wait until PRACK (64*T1 RFC 3261 section 7.1.1)
* o remove header lump
**********/

time_t nstart = time (0) + 32;
while (1)
  {
  usleep (USLEEP_LEN);
  if (pcall->call_state != CLSTA_PRACKSTRT)
    { break; }
  if (nstart < time (0))
    {
    LM_ERR ("%sNo PRACK response for call (%s)!\n",
      pfncname, pcall->call_from);
    break;
    }
  }
unlink_lump_rpl (pmsg, *phdrlump);
if (pcall->call_state != CLSTA_PRACKRPLY)
  { return 0; }
return 1;
}

/**********
* Send RTPProxy Answer
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = call pointer
* OUTPUT: 0=unable to process; 1=processed
**********/

int send_rtp_answer (sip_msg_t *pmsg, call_lst *pcall)

{
/**********
* build response from request
**********/

char *pfncname = "send_rtp_answer: ";
int nret = 0;
tm_api_t *ptm = pmod_data->ptm;
struct cell *ptrans = ptm->t_gett ();
str ptotag [1];
ptotag->s = pcall->call_tag;
ptotag->len = strlen (pcall->call_tag);
str pbuf [1];
struct bookmark pBM [1];
pbuf->s = build_res_buf_from_sip_req (200, presp_ok, ptotag, ptrans->uas.request,
  (unsigned int *)&pbuf->len, pBM);
if (!pbuf->s || !pbuf->len)
  {
  LM_ERR ("%sUnable to create SDP response for call (%s)!\n",
    pfncname, pcall->call_from);
  return 0;
  }

/**********
* parse out first line and headers
**********/

char *pclenhdr = CLENHDR;
str pparse [20];
int npos1, npos2;
int nhdrcnt = 0;
for (npos1 = 0; npos1 < pbuf->len; npos1++)
  {
  /**********
  * find EOL
  **********/

  for (npos2 = npos1++; npos1 < pbuf->len; npos1++)
    {
    /**********
    * o not EOL? (CRLF assumed)
    * o next line a continuation? (RFC 3261 section 7.3.1)
    **********/

    if (pbuf->s [npos1] != '\n')
      { continue; }
    if (npos1 + 1 == pbuf->len)
      { break; }
    if (pbuf->s [npos1 + 1] == ' '
      || pbuf->s [npos1 + 1] == '\t')
      { continue; }
    break;
    }

  /**********
  * o blank is end of header (RFC 3261 section 7)
  * o ignore Content-Length (assume followed by colon)
  * o save header
  **********/

  if (npos1 - npos2 == 1)
    { break; }
  if (npos1 - npos2 > 14)
    {
    if (!strncasecmp (&pbuf->s [npos2], pclenhdr, 14))
      { continue; }
    }
  pparse [nhdrcnt].s = &pbuf->s [npos2];
  pparse [nhdrcnt++].len = npos1 - npos2 + 1;
  }

/**********
* recreate buffer with extra headers and SDP
* o form SDP
* o count hdrs, extra hdrs, content-length hdr, SDP
* o alloc new buffer
* o form new buffer
* o replace orig buffer
**********/

str pSDP [1] = {STR_NULL};
if (!form_rtp_SDP (pSDP, pcall, prtpsdp))
  { goto answer_done; }
for (npos1 = npos2 = 0; npos2 < nhdrcnt; npos2++)
  { npos1 += pparse [npos2].len; }
char pbodylen [30];
sprintf (pbodylen, "%s: %d\r\n\r\n", pclenhdr, pSDP->len);
npos1 += pextrahdr->len + strlen (pbodylen) + pSDP->len + 1;
char *pnewbuf = pkg_malloc (npos1);
if (!pnewbuf)
  {
  LM_ERR ("%sNo more memory!\n", pfncname);
  goto answer_done;
  }
for (npos1 = npos2 = 0; npos2 < nhdrcnt; npos2++)
  {
  memcpy (&pnewbuf [npos1], pparse [npos2].s, pparse [npos2].len);
  npos1 += pparse [npos2].len;
  }
npos2 = pextrahdr->len;
memcpy (&pnewbuf [npos1], pextrahdr->s, npos2);
npos1 += npos2;
npos2 = strlen (pbodylen);
memcpy (&pnewbuf [npos1], pbodylen, npos2);
npos1 += npos2;
npos2 = pSDP->len;
memcpy (&pnewbuf [npos1], pSDP->s, npos2);
npos1 += npos2;
pkg_free (pbuf->s);
pbuf->s = pnewbuf;
pbuf->len = npos1;

/**********
* build SIP msg
**********/

struct sip_msg pnmsg [1];
build_sip_msg_from_buf (pnmsg, pbuf->s, pbuf->len, 0);
memcpy (&pnmsg->rcv, &pmsg->rcv, sizeof (struct receive_info));

/**********
* o send RTP answer
* o form stream file
* o send stream
**********/

mohq_debug (pcall->pmohq, "%sAnswering RTP link for call (%s)",
  pfncname, pcall->call_from);
if (pmod_data->fn_rtp_answer (pnmsg, 0, 0) != 1)
  {
  LM_ERR ("%srtpproxy_answer refused for call (%s)!\n",
    pfncname, pcall->call_from);
  goto answer_done;
  }
if (!start_stream (pnmsg, pcall, 0))
  { goto answer_done; }

/**********
* o create buffer from response
* o find SDP
**********/

pbuf->s = build_res_buf_from_sip_res (pnmsg, (unsigned int *)&pbuf->len);
pkg_free (pnewbuf);
free_sip_msg (pnmsg);
if (!pbuf->s || !pbuf->len)
  {
  LM_ERR ("%sUnable to create SDP response for call (%s)!\n",
    pfncname, pcall->call_from);
  goto answer_done;
  }
str pnewSDP [1];
for (npos1 = 0; npos1 < pbuf->len; npos1++)
  {
  if (pbuf->s [npos1] != '\n')
    { continue; }
  if (pbuf->s [npos1 - 3] == '\r')
    { break; }
  }
pnewSDP->s = &pbuf->s [npos1 + 1];
pnewSDP->len = pbuf->len - npos1 - 1;

/**********
* o save media port number
* o send adjusted reply
**********/

char *pfnd = strstr (pnewSDP->s, "m=audio ");
if (!pfnd)
  {
  // should not happen
  LM_ERR ("%sUnable to find audio port for call (%s)!\n",
    pfncname, pcall->call_from);
  goto answer_done;
  }
pcall->call_aport = strtol (pfnd + 8, NULL, 10);
if (!add_lump_rpl2 (pmsg, pextrahdr->s, pextrahdr->len, LUMP_RPL_HDR))
  {
  LM_ERR ("%sUnable to add header for call (%s)!\n",
    pfncname, pcall->call_from);
  goto answer_done;
  }
if (!add_lump_rpl2 (pmsg, pnewSDP->s, pnewSDP->len, LUMP_RPL_BODY))
  {
  LM_ERR ("%sUnable to add SDP body for call (%s)!\n",
    pfncname, pcall->call_from);
  goto answer_done;
  }
if (ptm->t_reply (pmsg, 200, presp_ok->s) < 0)
  {
  LM_ERR ("%sUnable to reply to INVITE for call (%s)!\n",
    pfncname, pcall->call_from);
  goto answer_done;
  }
pcall->call_state = CLSTA_INVITED;
mohq_debug (pcall->pmohq, "%sResponded to INVITE with RTP for call (%s)",
  pfncname, pcall->call_from);
nret = 1;

/**********
* free buffer and return
**********/

answer_done:
if (pSDP->s)
  { pkg_free (pSDP->s); }
pkg_free (pbuf->s);
return nret;
}

/**********
* Start Streaming
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = call pointer
*   Arg (3) = server flag
* OUTPUT: 0 if failed
**********/

int start_stream (sip_msg_t *pmsg, call_lst *pcall, int bserver)

{
char *pfncname = "start_stream: ";
char pfile [MOHDIRLEN + MOHFILELEN + 2];
strcpy (pfile, pcall->pmohq->mohq_mohdir);
int npos = strlen (pfile);
pfile [npos++] = '/';
strcpy (&pfile [npos], pcall->pmohq->mohq_mohfile);
npos += strlen (&pfile [npos]);
str pMOH [1] = {{pfile, npos}};
pv_elem_t *pmodel;
if(pv_parse_format (pMOH, &pmodel)<0) {
  LM_ERR("failed to parse pv format string\n");
  return 0;
}
cmd_function fn_stream = bserver ? pmod_data->fn_rtp_stream_s
  : pmod_data->fn_rtp_stream_c;
mohq_debug (pcall->pmohq, "%sStarting RTP link for call (%s)",
  pfncname, pcall->call_from);
if (fn_stream (pmsg, (char *)pmodel, (char *)-1) != 1)
  {
  LM_ERR ("%srtpproxy_stream refused for call (%s)!\n",
    pfncname, pcall->call_from);
  return 0;
  }
return 1;
}

/**********
* Stop Streaming
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = call pointer
*   Arg (3) = server flag
* OUTPUT: 0 if failed
**********/

int stop_stream (sip_msg_t *pmsg, call_lst *pcall, int bserver)

{
char *pfncname = "stop_stream: ";
cmd_function fn_stop = bserver ? pmod_data->fn_rtp_stop_s
  : pmod_data->fn_rtp_stop_c;
mohq_debug (pcall->pmohq, "%sStopping RTP link for call (%s)",
  pfncname, pcall->call_from);
if (fn_stop (pmsg, (char *)-1, (char *)-1) != 1)
  {
  LM_ERR ("%srtpproxy_stop refused for call (%s)!\n",
    pfncname, pcall->call_from);
  return 0;
  }
return 1;
}

/**********
* Form Char Array from STR
*
* INPUT:
*   Arg (1) = str pointer
* OUTPUT: char pointer; NULL if unable to allocate
**********/

char *form_tmpstr (str *pstr)

{
char *pcstr = malloc (pstr->len + 1);
if (!pcstr)
  {
  LM_ERR ("No more memory!\n");
  return NULL;
  }
memcpy (pcstr, pstr->s, pstr->len);
pcstr [pstr->len] = 0;
return pcstr;
}

/**********
* Release Char Array
*
* INPUT:
*   Arg (1) = char pointer
* OUTPUT: none
**********/

void free_tmpstr (char *pcstr)

{
if (pcstr)
  { free (pcstr); }
return;
}

/**********
* external functions
**********/

/**********
* Find MOH Files
*
* INPUT:
*   Arg (1) = mohdir pointer
*   Arg (2) = mohfile pointer
* OUTPUT: array of pointers for matching files; last element=0
**********/

rtpmap **find_MOH (char *pmohdir, char *pmohfile)

{
/**********
* form base file name
**********/

char pfile [MOHDIRLEN + MOHFILELEN + 6];
strcpy (pfile, pmohdir);
int nflen = strlen (pfile);
pfile [nflen++] = '/';
strcpy (&pfile [nflen], pmohfile);
nflen += strlen (&pfile [nflen]);
pfile [nflen++] = '.';

/**********
* find available files based on RTP payload type
**********/

int nidx;
int nfound = 0;
for (nidx = 0; prtpmap [nidx].pencode; nidx++)
  {
  /**********
  * o form file name based on payload type
  * o exists?
  **********/

  sprintf (&pfile [nflen], "%d", prtpmap [nidx].ntype);
  struct stat psb [1];
  if (lstat (pfile, psb))
    { continue; }
  pmohfiles [nfound++] = &prtpmap [nidx];
  }
pmohfiles [nfound] = 0;
return pmohfiles;
}

/**********
* RPC Debug
*
* PARAMETERS:
* queue name = queue to use
* state = 0=off, <>0=on
*
* INPUT:
*   Arg (1) = rpc handle
*   Arg (2) = rpc context pointer
* OUTPUT: none
**********/

void mohqueue_rpc_debug (rpc_t *prpc, void *pctx)

{
/**********
* o parm count correct?
* o find queue
* o lock queue
**********/

str pqname [1];
int bdebug;
if (prpc->scan (pctx, "Sd", pqname, &bdebug) != 2)
  {
  prpc->fault (pctx, 400, "Too few parameters!");
  return;
  }
int nq_idx = find_qname (pqname);
if (nq_idx == -1)
  {
  prpc->fault (pctx, 401, "No such queue (%.*s)!", STR_FMT (pqname));
  return;
  }
if (!mohq_lock_set (pmod_data->pcall_lock, 0, 5000))
  {
  prpc->fault
    (pctx, 402, "Unable to lock the queue (%.*s)!", STR_FMT (pqname));
  return;
  }

/**********
* o set flag
* o update queue table
* o release lock
**********/

mohq_lst *pqueue = &pmod_data->pmohq_lst [nq_idx];
if (bdebug)
  { pqueue->mohq_flags |= MOHQF_DBG; }
else
  { pqueue->mohq_flags &= ~MOHQF_DBG; }
update_debug (pqueue, bdebug);
mohq_lock_release (pmod_data->pmohq_lock);
return;
}

/**********
* RPC Drop Call
*
* PARAMETERS:
* queue name = queue to use
* callID = *=all, otherwise callID
*
* INPUT:
*   Arg (1) = rpc handle
*   Arg (2) = rpc context pointer
* OUTPUT: none
**********/

void mohqueue_rpc_drop_call (rpc_t *prpc, void *pctx)

{
/**********
* o parm count correct?
* o find queue
* o lock calls
**********/

str pcallid [1], pqname [1];
if (prpc->scan (pctx, "SS", pqname, pcallid) != 2)
  {
  prpc->fault (pctx, 400, "Too few parameters!");
  return;
  }
int nq_idx = find_qname (pqname);
if (nq_idx == -1)
  {
  prpc->fault (pctx, 401, "No such queue (%.*s)!", STR_FMT (pqname));
  return;
  }
if (!mohq_lock_set (pmod_data->pcall_lock, 0, 5000))
  {
  prpc->fault
    (pctx, 402, "Unable to lock the queue (%.*s)!", STR_FMT (pqname));
  return;
  }

/**********
* o find matching calls
* o release lock
**********/

mohq_lst *pqueue = &pmod_data->pmohq_lst [nq_idx];
int nidx;
for (nidx = 0; nidx < pmod_data->call_cnt; nidx++)
  {
  /**********
  * o call active?
  * o callID matches?
  * o close call
  **********/

  call_lst *pcall = &pmod_data->pcall_lst [nidx];
  if (!pcall->call_state)
    { continue; }
  if (pqueue->mohq_id != pcall->pmohq->mohq_id)
    { continue; }
  str tmpstr [1];
  if (!STR_EQ (*pcallid, *pallq))
    {
    tmpstr->s = pcall->call_id;
    tmpstr->len = strlen (tmpstr->s);
    if (!STR_EQ (*tmpstr, *pcallid))
      { continue; }
    }
  close_call (FAKED_REPLY, pcall);
  }
mohq_lock_release (pmod_data->pcall_lock);
return;
}

/**********
* Count Messages
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = queue name
*   Arg (3) = pv result name
* OUTPUT: -1 if no items in queue; else result = count
**********/

int mohq_count (sip_msg_t *pmsg, char *pqueue, pv_spec_t *presult)

{
/**********
* get queue and pv names
**********/

char *pfncname = "mohq_count: ";
str pqname [1];
if (!pqueue || !presult)
  {
  LM_ERR ("%sParameters missing!\n", pfncname);
  return -1;
  }
if (fixup_get_svalue (pmsg, (gparam_p)pqueue, pqname))
  {
  LM_ERR ("%sInvalid queue name!\n", pfncname);
  return -1;
  }

/**********
* o find queue
* o lock calls
* o count items in queue
**********/

int nq_idx = find_qname (pqname);
int ncount = 0;
call_lst *pcalls = pmod_data->pcall_lst;
int ncall_idx, mohq_id;
if (!mohq_lock_set (pmod_data->pcall_lock, 0, 200))
  { LM_ERR ("%sUnable to lock calls!\n", pfncname); }
else
  {
  if (nq_idx != -1)
    {
    mohq_id = pmod_data->pmohq_lst [nq_idx].mohq_id;
    for (ncall_idx = 0; ncall_idx < pmod_data->call_cnt; ncall_idx++)
      {
      if (!pcalls [ncall_idx].call_state)
        { continue; }
      if (pcalls [ncall_idx].pmohq->mohq_id == mohq_id
        && pcalls [ncall_idx].call_state == CLSTA_INQUEUE)
        { ncount++; }
      }
    }
  mohq_lock_release (pmod_data->pcall_lock);
  }

/**********
* o set pv result
* o exit with result
**********/

pv_value_t pavp_val [1];
memset (pavp_val, 0, sizeof (pv_value_t));
pavp_val->ri = ncount;
pavp_val->flags = PV_TYPE_INT | PV_VAL_INT;
if (presult->setf (pmsg, &presult->pvp, (int)EQ_T, pavp_val) < 0)
  {
  LM_ERR ("%sUnable to set pv value for mohq_count ()!\n", pfncname);
  return -1;
  }
return 1;
}

/**********
* Log Debug Statement
*
* INPUT:
*   Arg (1) = MOH queue pointer
*   Arg (2) = format pointer
*   Arg (...) = optional format values
* OUTPUT: outputs debugging values
**********/

void mohq_debug (mohq_lst *pmohq, char *pfmt, ...)

{
/**********
* o get system and MOHQ log level
* o exit if no debug printing
* o force local debug
* o form message and log
* o reset log level
**********/

int nsys_log = get_debug_level (LOG_MNAME, LOG_MNAME_LEN);
int nmohq_log = (pmohq->mohq_flags & MOHQF_DBG) ? L_DBG : L_INFO;
if (nmohq_log < L_DBG && nsys_log < L_DBG)
  { return; }
if (nsys_log < nmohq_log)
  { set_local_debug_level (nmohq_log); }
char ptext [1024];
va_list ap;
va_start (ap, pfmt);
vsnprintf (ptext, sizeof (ptext), pfmt, ap);
va_end (ap);
LM_DBG ("%s\n", ptext);
if (nsys_log < nmohq_log)
  { reset_local_debug_level (); }
return;
}

/**********
* Process Message
*
* INPUT:
*   Arg (1) = SIP message pointer
* OUTPUT: -1=not directed to queue or other error; 1=processed
**********/

int mohq_process (sip_msg_t *pmsg)

{
/**********
* read lock queue and check for updates
**********/

char *pfncname = "mohq_process: ";
if (!mohq_lock_set (pmod_data->pmohq_lock, 0, 500))
  {
  LM_ERR ("%sUnable to read lock queue!\n", pfncname);
  return -1;
  }
db1_con_t *pconn = mohq_dbconnect ();
if (pconn)
  {
  /**********
  * o last update older than 1 minute?
  * o update write locked queue
  **********/

  if (pmod_data->mohq_update + 60 < time (0))
    {
    if (mohq_lock_change (pmod_data->pmohq_lock, 1))
      {
      update_mohq_lst (pconn);
      mohq_lock_change (pmod_data->pmohq_lock, 0);
      pmod_data->mohq_update = time (0);
      }
    }
  mohq_dbdisconnect (pconn);
  }

/**********
* o parse headers
* o directed to message queue?
* o write lock calls
* o search for call
* o release call lock
**********/

if (parse_headers (pmsg, HDR_EOH_F, 0) < 0)
  {
  mohq_lock_release (pmod_data->pmohq_lock);
  LM_ERR ("%sUnable to parse header!\n", pfncname);
  return -1;
  }
int mohq_idx = find_queue (pmsg);
if (mohq_idx < 0)
  {
  mohq_lock_release (pmod_data->pmohq_lock);
  return -1;
  }
if (!mohq_lock_set (pmod_data->pcall_lock, 1, 500))
  {
  mohq_lock_release (pmod_data->pmohq_lock);
  LM_ERR ("%sUnable to write lock calls!\n", pfncname);
  return 1;
  }
call_lst *pcall = find_call (pmsg, mohq_idx);
mohq_lock_release (pmod_data->pcall_lock);
if (!pcall)
  {
  mohq_lock_release (pmod_data->pmohq_lock);
  return 1;
  }

/**********
* o process message
* o release queue lock
**********/

mohq_debug (&pmod_data->pmohq_lst [mohq_idx],
  "%sProcessing %.*s, queue (%s)", pfncname,
  STR_FMT (&REQ_LINE (pmsg).method),
  pmod_data->pmohq_lst [mohq_idx].mohq_name);
str *ptotag;
switch (pmsg->REQ_METHOD)
  {
  case METHOD_INVITE:
    /**********
    * initial INVITE?
    **********/

    ptotag = &(get_to (pmsg)->tag_value);
    if (!ptotag->len)
      { ptotag = 0; }
    if (!ptotag)
      { first_invite_msg (pmsg, pcall); }
    else
      { reinvite_msg (pmsg, pcall); }
    break;
  case METHOD_NOTIFY:
    notify_msg (pmsg, pcall);
    break;
  case METHOD_PRACK:
    prack_msg (pmsg, pcall);
    break;
  case METHOD_ACK:
    ack_msg (pmsg, pcall);
    break;
  case METHOD_BYE:
    bye_msg (pmsg, pcall);
    break;
  case METHOD_CANCEL:
    cancel_msg (pmsg, pcall);
    break;
  default:
    deny_method (pmsg, pcall);
    break;
  }
mohq_lock_release (pmod_data->pmohq_lock);
return 1;
}

/**********
* Retrieve Oldest Queued Call
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = queue name
*   Arg (3) = redirect URI
* OUTPUT: -1 if no items in queue or error; 1 redirects oldest call
**********/

int mohq_retrieve (sip_msg_t *pmsg, char *pqueue, char *pURI)

{
/**********
* o get queue name and URI
* o check URI
**********/

char *pfncname = "mohq_retrieve: ";
str puri [1], pqname [1];
if (!pqueue || !pURI)
  {
  LM_ERR ("%sParameters missing!\n", pfncname);
  return -1;
  }
if (fixup_get_svalue (pmsg, (gparam_p)pqueue, pqname))
  {
  LM_ERR ("%sInvalid queue name!\n", pfncname);
  return -1;
  }
if (fixup_get_svalue (pmsg, (gparam_p)pURI, puri))
  {
  LM_ERR ("%sInvalid URI!\n", pfncname);
  return -1;
  }
if (puri->len > URI_LEN)
  {
  LM_ERR ("%sURI too long!\n", pfncname);
  return -1;
  }
struct sip_uri puri_parsed [1];
if (parse_uri (puri->s, puri->len, puri_parsed))
  {
  LM_ERR ("%sInvalid URI (%.*s)!\n", pfncname, STR_FMT (puri));
  return -1;
  }

/**********
* o find queue
* o lock calls
* o find oldest call
**********/

int nq_idx = find_qname (pqname);
if (nq_idx == -1)
  { return -1; }
if (!mohq_lock_set (pmod_data->pcall_lock, 0, 200))
  {
  LM_ERR ("%sUnable to lock calls!\n", pfncname);
  return -1;
  }
call_lst *pcall = 0;
int ncall_idx;
time_t ntime = 0;
int nfound = -1;
int mohq_id = pmod_data->pmohq_lst [nq_idx].mohq_id;
for (ncall_idx = 0; ncall_idx < pmod_data->call_cnt; ncall_idx++)
  {
  /**********
  * o active call?
  * o matching queue?
  * o refer stuck?
  * o in queue?
  * o check age
  **********/

  pcall = &pmod_data->pcall_lst [ncall_idx];
  if (!pcall->call_state)
    { continue; }
  if (pcall->pmohq->mohq_id != mohq_id)
    { continue; }
  if ((pcall->call_state == CLSTA_REFER)
    || (pcall->call_state == CLSTA_RFRWAIT))
    {
    if ((pcall->refer_time + 32) < time (0))
      {
      LM_ERR
        ("%sDropping call because no response to REFER for call (%s)!\n",
        pfncname, pcall->call_from);
      close_call (FAKED_REPLY, pcall);
      }
    }
  if (pcall->call_state != CLSTA_INQUEUE)
    { continue; }
  if (!ntime)
    {
    nfound = ncall_idx;
    ntime = pcall->call_time;
    }
  else
    {
    if (pcall->call_time < ntime)
      {
      nfound = ncall_idx;
      ntime = pcall->call_time;
      }
    }
  }
if (nfound == -1)
  {
  LM_WARN ("%sNo calls in queue (%.*s)\n", pfncname, STR_FMT (pqname));
  mohq_lock_release (pmod_data->pcall_lock);
  return -1;
  }
pcall = &pmod_data->pcall_lst [nfound];

/**********
* o save refer-to URI
* o send refer
**********/

strncpy (pcall->call_referto, puri->s, puri->len);
pcall->call_referto [puri->len] = '\0';
if (refer_call (pcall, pmod_data->pcall_lock))
  { return 1; }
LM_ERR ("%sUnable to refer call (%s)!\n", pfncname, pcall->call_from);
return -1;
}

/**********
* Send Message to Queue
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = queue name
* OUTPUT: -1 if no items in queue; 1 if successfull
**********/

int mohq_send (sip_msg_t *pmsg, char *pqueue)

{
/**********
* o first INVITE?
* o get queue name
**********/

char *pfncname = "mohq_send: ";
if (pmsg->REQ_METHOD != METHOD_INVITE)
  {
  LM_ERR ("%sNot an INVITE message!\n", pfncname);
  return -1;
  }
to_body_t *pto_body = get_to (pmsg);
if (pto_body->tag_value.len)
  {
  LM_ERR ("%sNot a first INVITE message!\n", pfncname);
  return -1;
  }
str pqname [1];
if (!pqueue)
  {
  LM_ERR ("%sParameters missing!\n", pfncname);
  return -1;
  }
if (fixup_get_svalue (pmsg, (gparam_p)pqueue, pqname))
  {
  LM_ERR ("%sInvalid queue name!\n", pfncname);
  return -1;
  }

/**********
* o find queue
* o change RURI
* o relay message
**********/

int nq_idx = find_qname (pqname);
if (nq_idx == -1)
  { return -1; }
str pruri [1] = {{0, strlen (pmod_data->pmohq_lst [nq_idx].mohq_uri)}};
pruri->s = pkg_malloc (pruri->len + 1);
if (!pruri->s)
  {
  LM_ERR ("%sNo more memory!\n", pfncname);
  return -1;
  }
strcpy (pruri->s, pmod_data->pmohq_lst [nq_idx].mohq_uri);
if (pmsg->new_uri.s)
  { pkg_free (pmsg->new_uri.s); }
pmsg->new_uri.s = pruri->s;
pmsg->new_uri.len = pruri->len;
pmsg->parsed_uri_ok = 0;
pmsg->parsed_orig_ruri_ok = 0;
if (pmod_data->ptm->t_relay (pmsg, 0, 0) < 0)
  {
  LM_ERR ("%sUnable to relay INVITE!\n", pfncname);
  return -1;
  }
return 1;
}