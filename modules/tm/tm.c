#include <stdio.h>
#include <stdlib.h>

int request_received(  table hash_table, char* incoming_req_uri, char *from, char* to, char* tag, char* call_id, char* cseq_nr ,char* cseq_method, char* outgoing_req_uri)
{
   struct cell*  matched_trans = 0;
   struct cell*  new_trans         = 0;
   int status;

   /* looking for transaction
    * IMPORTANT NOTE : the lookup_for_Transaction_by_XXXX returns the found trans referenceted -> when finidh you
    * have to unreference manually, otherwise the cell wann't be ever deleted!!!!
    */

   if ( !strcmp( cseq_method , "ACK") )
   {
      /* we got an ACK -> do we have a transaction pair for it?*/
      matched_trans =  lookup_for_Transaction_by_ACK( hash_table, from , to , tag , call_id , cseq_nr );
      if ( matched_trans )
      {
         /* the trans pair was found -> let's check the trans's status  */
         status = matched_trans->status;
         /* done with reading the transaction -> unref it*/
         unref_Cell( matched_trans );
         if ( status>=200 && status<300 )
         {
            /* response for PROXY -> route the ACK */
          return 1;
         }
         else
         {
            /* response for PROXY->DROP the request!! */
            return 0;
         }
      }
      else
      {
         /* response for PROXY -> route the ACK */
         return 1;
      }
   }


   /* looks up for a perfect mache transaction */
   matched_trans =  lookup_for_Transaction_by_req( hash_table, from , to , tag , call_id , cseq_nr , cseq_method );

   if ( matched_trans )
   {
      /* the trans already exists -> the last response is retransmited  */
      status = matched_trans->status;
      /* done with reading the transaction -> unref it*/
      unref_Cell( matched_trans );
      // TO DO -> replay with <status> response
      /* response for PROXY->DROP the request!! */
      return 0 ;
   }


    /* if is not a request -> route it further */
   if ( strcmp( cseq_method , "CANCEL") )
   {
      /* we got a CANCEL -> do we have a transaction pair for it?*/
      matched_trans = lookup_for_Transaction_by_CANCEL( hash_table, incoming_req_uri , from , to , tag , call_id , cseq_nr );
      if ( matched_trans )
      {
         /* the trans pair was found -> let's check the trans's status  */
         status = matched_trans->status;
         /* done with reading the transaction -> unref it*/
         unref_Cell( matched_trans );
        if ( status>=200 )
         {
            /* the trasaction has a final response -> send a 200 reply for the CANCEL */
            // TO DO send 200 reply
            /*response for PROXY -> DROP the request!!! */
            return 0;
         }
      }
   }

   /* a new trans is created based on the received request */
   new_trans = add_Transaction( hash_table , incoming_req_uri , from , to , tag , call_id , cseq_nr , cseq_method );
   if ( !new_trans )
   {
      /* response for PROXY -> error:cannot alloc memory */
      return -1;
   }
   /* a 100 replay is sent */
   //TO DO ->send a 100 reply

   /* the FINAL RESPONSE timer is engaged */
   start_FR_timer( hash_table, new_trans );
   /* sets the outgoing req URI */
   new_trans->outgoing_req_uri = sh_malloc( strlen(outgoing_req_uri) + 1 );
   strcpy( new_trans->outgoing_req_uri , outgoing_req_uri );

   /* response for PROXY -> route the request */
   return 1;
}






int response_received(  table hash_table, char* reply_code, char* incoming_url , char* via, char* label , char* from , char* to , char* tag , char* call_id , char* cseq_nr ,char* cseq_method )
{
   struct cell*  matched_trans = 0;
   char* p;
   int  res_status, trans_status;

   /* looking for a transaction match
    * IMPORTANT NOTE : the lookup_fir_Transaction returns the found trans referenceted -> when finidh you
    * have to unreference manually, otherwise the cell wann't be ever deleted!!!!
    */
   matched_trans =  lookup_for_Transaction_by_res( hash_table, label , from , to , tag , call_id , cseq_nr , cseq_method );

   if ( !matched_trans )
   {
      /* the trans wasn't found ->  the response is forwarded to next Via hop  */
      return 1;
   }

   /* getting the matched trasaction  status */
   trans_status = matched_trans->status;

   /* getting the response status from reply as int form string */
   p = reply_code;
   res_status = strtol( reply_code , &p , 10 );
   if ( *p!=0 )
   {
      /* done with the transaction -> unref it*/
      unref_Cell( matched_trans );
      /* response for PROXY -> error:cannot convert string to number */
      return -1;
   }

   if ( res_status==100)
   {
      /* done with the transaction -> unref it*/
      unref_Cell( matched_trans );
      /* response for PROXY -> DROP the response!!! */
      return 0;
   }

   if ( res_status >100 && res_status<200 )
      if ( res_status > trans_status )
      {
         /* updating the transaction status */
         matched_trans->status = res_status;
         /* store the reply's tag (if no response tag has been set)*/
         if ( !matched_trans->res_tag )
         {
            matched_trans->res_tag_length = strlen( tag );
            matched_trans->res_tag = sh_malloc( matched_trans->res_tag_length+1 );
            strcpy( matched_trans->res_tag , tag );
         }
         /* done with the transaction -> unref it*/
         unref_Cell( matched_trans );
         /* response to PROXY ->forward upstream the response */
         return 1;
      }
      else
      {
         /* done with the transaction -> unref it*/
         unref_Cell( matched_trans );
         /* response to PROXY -> DROP the response !!! */
         return 0;
      }

   /* if is the response for a local CANCEL*/
   if ( via==0 && !strcmp( cseq_method,"CANCEL" ) )
   {
      /* done with the transaction -> unref it*/
      unref_Cell( matched_trans );
      /* response to PROXY -> DROP the response !!! */
      return 0;
   }

   if ( res_status<200 || res_status>=300 )
      if ( !strcmp( cseq_method,"INVITE" ) )
      {
         char   ack_req[500];
         char* tag = 0;
         /* send ACK for an error INVITE response */
         strcpy( ack_req , "ACK " );
         strcat( ack_req , matched_trans->incoming_req_uri ); strcat( ack_req , "\n" );
         // via TO DO ?!!?!?!
         strcat( ack_req , "From: " ); strcat( ack_req , matched_trans->from ); strcat( ack_req , "\n" );
         strcat( ack_req , "To: " ); strcat( ack_req , matched_trans->to );
         if ( matched_trans->req_tag )
            tag = matched_trans->req_tag;
         if ( !tag && matched_trans->res_tag )
            tag = matched_trans->res_tag;
         if (tag)
            {strcat(ack_req," ;tag=");strcat(ack_req,tag);}
         strcat( ack_req , "\n" );
         strcat( ack_req , "Call-ID: " ); strcat( ack_req , matched_trans->call_id ); strcat( ack_req , "\n" );
         strcat( ack_req , "CSeq: " ); strcat( ack_req , matched_trans->cseq_nr ); strcat( ack_req , " ACK\n\n" );
         //TO DO - send the request
      }
   else
      if ( strcmp( cseq_method,"INVITE" ) && trans_status>=200 )
      {
         /* done with the transaction -> unref it*/
         unref_Cell( matched_trans );
         /* response to PROXY -> DROP the response !!! */
         return 0;
      }


   /* Final clean up */

   /* updating the transaction status */
   matched_trans->status = res_status;
   /* store the reply's tag (if it's unset)*/
   if ( !matched_trans->res_tag )
   {
      matched_trans->res_tag_length = strlen( tag );
      matched_trans->res_tag = sh_malloc( matched_trans->res_tag_length+1 );
      strcpy( matched_trans->res_tag , tag );
   }
   /* starts the WAIT timer */
   start_WT_timer( hash_table, matched_trans );
   /* done with the transaction -> unref it*/
   unref_Cell( matched_trans );

   /* response to PROXY ->forward upstream the response */
   return 1;
}





int main()
{
   table  hash_table;
    struct cell* cell1,*cell2 , *cell3 ,*cell4;

   hash_table = init_hash_table();

   cell1 = add_Transaction( hash_table , "iptel.org" , "sfo@iptel.org", "sfo@iptel.org", 0 , "1234ef2312@fesarius.gmd.de" , "100" , "INVITE"  );
   cell2 = add_Transaction( hash_table , "iptel.org" , "sfo@iptel.org", "sfo@iptel.org", 0 , "3437625476@cucu.de" , "200" ,"REGISTER" );

   start_FR_timer( hash_table, cell1 ) ;
   start_FR_timer( hash_table, cell2 ) ;

   cell3 = lookup_for_Transaction_by_req( hash_table, "sfo@iptel.org", "sfo@iptel.org", 0 , "1234ef2312@fesarius.gmd.de" , "100" ,"INVITE");

    if (cell3)
    {
       printf(" From %s to %s a %s request\n",cell3->from,cell3->to,cell3->cseq_method);
        unref_Cell ( cell3 );
    }

   while(1)
   {
      cell3 = lookup_for_Transaction_by_req( hash_table, "sfo@iptel.org", "sfo@iptel.org", 0 , "1234ef2312@fesarius.gmd.de" , "100" ,"INVITE");
      if (cell3)  unref_Cell(cell3);
      cell4 = lookup_for_Transaction_by_req( hash_table, "sfo@iptel.org" , "sfo@iptel.org", 0 , "3437625476@cucu.de" , "200" ,"REGISTER" );
      if (cell4)  unref_Cell(cell4);
   }

}
