/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 */

#ifndef __DIAMETER_IMS_CODE_AVP_H
#define __DIAMETER_IMS_CODE_AVP_H


/** 3GPP AVP Codes */ 
enum {
/**   1 to 255 reserved for backward compatibility with IMS Radius TS29.061	*/
	AVP_IMS_Digest_Realm								= 104,
	AVP_IMS_Digest_QoP									= 110,
	AVP_IMS_Digest_Algorithm							= 111,
	AVP_IMS_Digest_Auth_Param							= 117,
	AVP_IMS_Digest_Domain								= 119,
	AVP_IMS_Digest_HA1									= 121,
/** 256 to 299 reserved for future use										*/
	AVP_IMS_Vendor_Id									= 266,
	AVP_IMS_Experimental_Result_Code					= 298,
	AVP_IMS_Experimental_Result							= 297,
/** 300 to 399 reserved for TS29.234											*/

/** 400 to 499 reserved for TS29.109											*/
	AVP_IMS_CCR_Number									= 415,
	AVP_IMS_CCR_Type									= 416,
 	AVP_IMS_Service_Identifier							= 439,
 /**  or   RFC 4006 															*/
 	AVP_IMS_Subscription_Id								= 443,
 	AVP_IMS_Subscription_Id_Data						= 444,
 	AVP_IMS_Subscription_Id_Type						= 450,
 /* This require ETSI vendor id and are from ETSI TS 183 017*/
 	AVP_ETSI_Binding_Information						= 450,
 	AVP_ETSI_Binding_Input_List							= 451,
 	AVP_ETSI_Binding_Output_List						= 452,
 	AVP_ETSI_V6_transport_address						= 453,
 	AVP_ETSI_V4_transport_address						= 454,
 	AVP_ETSI_Port_Number								= 455,
 	AVP_ETSI_Reservation_Class							= 456,
 	AVP_ETSI_Latching_Indication						= 457,
 	AVP_ETSI_Reservation_Priority						= 458,
	AVP_ETSI_Service_Class								= 459,
	AVP_ETSI_Overbooking_Indication						= 460,
	AVP_ETSI_Authorization_Package_Id  					= 461,
	AVP_ETSI_Media_Authorization_Context_Id 			= 462,

/** 500 to 599 reserved for TS29.209											*/
	AVP_IMS_Abort_Cause									= 500,
	AVP_IMS_Access_Network_Charging_Address				= 501,
	AVP_IMS_Access_Network_Charging_Identifier			= 502,
	AVP_IMS_Access_Network_Charging_Identifier_Value	= 503,
	AVP_IMS_AF_Application_Identifier					= 504,
	AVP_IMS_AF_Charging_Identifier						= 505,
	AVP_IMS_Authorization_Token							= 506,
	AVP_IMS_Flow_Description							= 507,
	AVP_IMS_Flow_Grouping								= 508,
	AVP_IMS_Flow_Number									= 509,
	AVP_IMS_Flows										= 510,
	AVP_IMS_Flow_Status									= 511,
	AVP_IMS_Flow_Usage									= 512,
	AVP_IMS_Specific_Action								= 513,
	AVP_IMS_Max_Requested_Bandwidth_DL					= 515,
	AVP_IMS_Max_Requested_Bandwidth_UL					= 516,
	AVP_IMS_Media_Component_Description					= 517,
	AVP_IMS_Media_Component_Number						= 518,
	AVP_IMS_Media_Sub_Component							= 519,
	AVP_IMS_Media_Type									= 520,
	AVP_IMS_RR_Bandwidth								= 521,
	AVP_IMS_RS_Bandwidth								= 522,
	AVP_IMS_SIP_Forking_Indication						= 523,
/** Codec-Data is from TS 29.214*/
	AVP_IMS_Codec_Data									= 524,
	AVP_IMS_Service_URN									= 525,
	AVP_IMS_Acceptable_Service_Info						= 526,
	AVP_IMS_Service_Info_Status							= 527,
/** 600 to 699 reserved for TS29.229											*/
	AVP_IMS_Visited_Network_Identifier					= 600,
	AVP_IMS_Public_Identity								= 601,
	AVP_IMS_Server_Name									= 602,
	AVP_IMS_Server_Capabilities							= 603,
	AVP_IMS_Mandatory_Capability						= 604,
	AVP_IMS_Optional_Capability							= 605,
	AVP_IMS_User_Data_Cx								= 606,
	AVP_IMS_SIP_Number_Auth_Items						= 607,
	AVP_IMS_SIP_Authentication_Scheme					= 608,
	AVP_IMS_SIP_Authenticate							= 609,
	AVP_IMS_SIP_Authorization							= 610,
	AVP_IMS_SIP_Authentication_Context					= 611,
	AVP_IMS_SIP_Auth_Data_Item							= 612,
	AVP_IMS_SIP_Item_Number								= 613,
	AVP_IMS_Server_Assignment_Type						= 614,
	AVP_IMS_Deregistration_Reason						= 615,
	AVP_IMS_Reason_Code									= 616,
	AVP_IMS_Reason_Info									= 617,
	AVP_IMS_Charging_Information						= 618,
	AVP_IMS_Primary_Event_Charging_Function_Name		= 619,
	AVP_IMS_Secondary_Event_Charging_Function_Name		= 620,
	AVP_IMS_Primary_Charging_Collection_Function_Name	= 621,
	AVP_IMS_Secondary_Charging_Collection_Function_Name	= 622,
	AVP_IMS_User_Authorization_Type						= 623,
	AVP_IMS_User_Data_Already_Available					= 624,
	AVP_IMS_Confidentiality_Key							= 625,
	AVP_IMS_Integrity_Key								= 626,
	AVP_IMS_User_Data_Request_Type						= 627,
	AVP_IMS_Supported_Features							= 628,
	AVP_IMS_Feature_List_ID								= 629,
	AVP_IMS_Feature_List								= 630,
	AVP_IMS_Supported_Applications						= 631,
	AVP_IMS_Associated_Identities						= 632,
	AVP_IMS_Originating_Request							= 633,
	AVP_IMS_Wildcarded_PSI								= 634,
	AVP_IMS_SIP_Digest_Authenticate 					= 635,
	AVP_IMS_Wildcarded_IMPU								= 636,
	AVP_IMS_UAR_Flags									= 637,
	AVP_IMS_Loose_Route_Indication						= 638,
	AVP_IMS_SCSCF_Restoration_Info						= 639,
	AVP_IMS_Path										= 640,
	AVP_IMS_Contact										= 641,
	AVP_IMS_Subscription_Info							= 642,
	AVP_IMS_Call_ID_SIP_Header							= 643,
	AVP_IMS_From_SIP_Header								= 644,
	AVP_IMS_To_SIP_Header								= 645,
	AVP_IMS_Record_Route								= 646,
	AVP_IMS_Associated_Registered_Identities			= 647,
	AVP_IMS_Multiple_Registration_Indication			= 648,
	AVP_IMS_Restoration_Info							= 649,
	
/** 700 to 799 reserved for TS29.329											*/
	AVP_IMS_User_Identity								= 700,
	AVP_IMS_MSISDN										= 701,
	AVP_IMS_User_Data_Sh								= 702,
	AVP_IMS_Data_Reference								= 703,
	AVP_IMS_Service_Indication							= 704,
	AVP_IMS_Subs_Req_Type								= 705,
	AVP_IMS_Requested_Domain							= 706,
	AVP_IMS_Current_Location							= 707,
	AVP_IMS_Identity_Set								= 708,
	AVP_IMS_Expiry_Time									= 709,
	AVP_IMS_Send_Data_Indication						= 710,
	AVP_IMS_DSAI_Tag									= 711,
	
/** 800 to 899 reserved for TS29.299											*/
	AVP_IMS_Event_Type 									= 823,
	AVP_IMS_SIP_Method									= 824,
	AVP_IMS_Event										= 825,
	AVP_IMS_Content_Type								= 826,
	AVP_IMS_Content_Length								= 827,
	AVP_IMS_Content_Disposition							= 828,
	AVP_IMS_Role_Of_Node 								= 829,
	AVP_IMS_User_Session_Id								= 830,
	AVP_IMS_Calling_Party_Address						= 831,
	AVP_IMS_Called_Party_Address						= 832,
	AVP_IMS_Time_Stamps									= 833,
	AVP_IMS_SIP_Request_Timestamp						= 834,
	AVP_IMS_SIP_Response_Timestamp						= 835,
	AVP_IMS_Application_Server							= 836,
	AVP_IMS_Application_Provided_Called_Party_Address	= 837,
	AVP_IMS_Inter_Operator_Identifier					= 838,
	AVP_IMS_Originating_IOI								= 839,
	AVP_IMS_Terminating_IOI								= 840,
	AVP_IMS_IMS_Charging_identifier						= 841,
	AVP_IMS_SDP_Session_Description						= 842,
	AVP_IMS_SDP_Media_Component							= 843,
	AVP_IMS_SDP_Media_Name								= 844,
	AVP_IMS_SDP_Media_Description						= 845,
	AVP_IMS_CG_Address									= 846,
	AVP_IMS_GGSN_Address								= 847,
	AVP_IMS_Served_Party_IP_Address						= 848,
	AVP_IMS_Authorized_QoS								= 849,
	AVP_IMS_Application_Service_Information				= 850,
	AVP_IMS_Trunk_Group_Id								= 851,
	AVP_IMS_Incoming_Trunk_Group_Id						= 852,
	AVP_IMS_Outgoing_Trunk_Group_Id						= 853,
	AVP_IMS_Bear_Service								= 854,
	AVP_IMS_Service_Id									= 855,
	AVP_IMS_Associated_URI								= 856,
	AVP_IMS_Charged_Party								= 857,
	AVP_IMS_PoC_Controlling_Address						= 858,
	AVP_IMS_PoC_Group_Name								= 859,
	AVP_IMS_Cause										= 860,
	AVP_IMS_Cause_Code									= 861,
	
	/* TODO finish the list... */
	AVP_IMS_Node_Functionality							= 862,
	AVP_IMS_Service_Information							= 873,
	AVP_IMS_IMS_Information								= 876,
	AVP_IMS_Expires										= 888,
	AVP_IMS_Message_Body								= 889,
/** 1000   from TS29.212 */
 	AVP_IMS_Charging_Rule_Install						= 1001,
 	AVP_IMS_Charging_Rule_Remove						= 1002,
 	AVP_IMS_Charging_Rule_Definition					= 1003,
 	AVP_IMS_Charging_Rule_Base_Name						= 1004,
 	AVP_IMS_Charging_Rule_Name							= 1005,
 	AVP_IMS_Event_Trigger								= 1006,
 	AVP_IMS_QoS_Information								= 1016,
 	AVP_IMS_Charging_Rule_Report						= 1018,
 	AVP_IMS_Pcc_Rule_Status								= 1019,
 	AVP_IMS_Bearer_Identifier							= 1020,
 	AVP_IMS_QoS_Class_Identifier						= 1028,

	AVP_IMS_Service_Specific_Info						= 1249,
	AVP_IMS_Requested_Party_Address						= 1251,
	AVP_IMS_Access_Network_Information					= 1263,

};

/** ETSI AVP Codes */ 
enum {
	
	/*added from ETSI 283 034 */
	AVP_ETSI_Globally_Unique_Address					=300,
	AVP_ETSI_Address_Realm								=301,
	AVP_ETSI_Logical_Access_Id							=302,
	AVP_ETSI_Initial_Gate_Setting						=303, 
	AVP_ETSI_QoS_Profile								=304,
	AVP_ETSI_IP_Connectivity_Status						=305,
	AVP_ETSI_Access_Network_Type						=306,
	AVP_ETSI_Aggregation_Network_Type					=307,
	AVP_ETSI_Maximum_Allowed_Bandwidth_UL				=308,
	AVP_ETSI_Maximum_Allowed_Bandwidth_DL				=309, 
	AVP_ETSI_Transport_Class							=311,
	AVP_ETSI_Application_Class_ID						=312,
	AVP_ETSI_Physical_Access_ID							=313,
	AVP_ETSI_Location_Information						=350,
	AVP_ETSI_RACS_Contact_Point							=351, 
	AVP_ETSI_Terminal_Type								=352, 
	AVP_ETSI_Requested_Information						=353,
	AVP_ETSI_Event_Type									=354,
	
	AVP_ETSI_Line_Identifier							= 500,
	AVP_ETSI_SIP_Authenticate 							= 501, 
	AVP_ETSI_SIP_Authorization 							= 502, 
	AVP_ETSI_SIP_Authentication_Info 					= 503, 
	AVP_ETSI_Digest_Realm 								= 504,  
	AVP_ETSI_Digest_Nonce 								= 505,  
	AVP_ETSI_Digest_Domain								= 506,  
	AVP_ETSI_Digest_Opaque 								= 507,  
	AVP_ETSI_Digest_Stale 								= 508,  
	AVP_ETSI_Digest_Algorithm 							= 509,  
	AVP_ETSI_Digest_QoP 								= 510,  
	AVP_ETSI_Digest_HA1 								= 511,  
	AVP_ETSI_Digest_Auth_Param 							= 512,  
	AVP_ETSI_Digest_Username 							= 513,  
	AVP_ETSI_Digest_URI 								= 514,  
	AVP_ETSI_Digest_Response 							= 515,  
	AVP_ETSI_Digest_CNonce 								= 516,  
	AVP_ETSI_Digest_Nonce_Count 						= 517,  
	AVP_ETSI_Digest_Method 								= 518,  
	AVP_ETSI_Digest_Entity_Body_Hash 					= 519,  
	AVP_ETSI_Digest_Nextnonce 							= 520,  
	AVP_ETSI_Digest_Response_Auth						= 521	
};

/** CableLabs AVP Codes */ 
enum {
	AVP_CableLabs_SIP_Digest_Authenticate 				= 228,
	AVP_CableLabs_Digest_Realm 							= 209,
	AVP_CableLabs_Digest_Domain 						= 206,
	AVP_CableLabs_Digest_Algorithm 						= 204,
	AVP_CableLabs_Digest_QoP 							= 208,
	AVP_CableLabs_Digest_HA1 							= 207,
	AVP_CableLabs_Digest_Auth_Param 					= 205
};

/** Server-Assignment-Type Enumerated AVP */
enum {
	AVP_IMS_SAR_ERROR									= -1,
	AVP_IMS_SAR_NO_ASSIGNMENT							= 0,
	AVP_IMS_SAR_REGISTRATION							= 1,
	AVP_IMS_SAR_RE_REGISTRATION							= 2,
	AVP_IMS_SAR_UNREGISTERED_USER						= 3,
	AVP_IMS_SAR_TIMEOUT_DEREGISTRATION					= 4,
	AVP_IMS_SAR_USER_DEREGISTRATION						= 5,
	AVP_IMS_SAR_TIMEOUT_DEREGISTRATION_STORE_SERVER_NAME= 6,
	AVP_IMS_SAR_USER_DEREGISTRATION_STORE_SERVER_NAME	= 7,
	AVP_IMS_SAR_ADMINISTRATIVE_DEREGISTRATION			= 8,
	AVP_IMS_SAR_AUTHENTICATION_FAILURE					= 9,
	AVP_IMS_SAR_AUTHENTICATION_TIMEOUT					= 10,
	AVP_IMS_SAR_DEREGISTRATION_TOO_MUCH_DATA			= 11,
	AVP_IMS_SAR_AAA_USER_DATA_REQUEST					= 12,
	AVP_IMS_SAR_PGW_UPDATE								= 13,
};

/** User-Data-Already-Available Enumerated AVP */
enum {
	AVP_IMS_SAR_USER_DATA_NOT_AVAILABLE					= 0,
	AVP_IMS_SAR_USER_DATA_ALREADY_AVAILABLE				= 1
};

/** User-Authorization-Type Enumerated AVP */
enum {
	AVP_IMS_UAR_REGISTRATION							= 0,
	AVP_IMS_UAR_DE_REGISTRATION							= 1,
	AVP_IMS_UAR_REGISTRATION_AND_CAPABILITIES			= 2
};

/** Originating-Request Enumerated AVP */
enum {
	AVP_IMS_LIR_ORIGINATING_REQUEST						= 0	
};

/** Data-Reference AVP */
enum {
	AVP_IMS_Data_Reference_Repository_Data				= 0,
	AVP_IMS_Data_Reference_IMS_Public_Identity			= 10,
	AVP_IMS_Data_Reference_IMS_User_State				= 11,
	AVP_IMS_Data_Reference_SCSCF_Name					= 12,
	AVP_IMS_Data_Reference_Initial_Filter_Criteria		= 13,
	AVP_IMS_Data_Reference_Location_Information			= 14,
	AVP_IMS_Data_Reference_User_State					= 15,
	AVP_IMS_Data_Reference_Charging_Information			= 16,
	AVP_IMS_Data_Reference_MSISDN						= 17,	
	AVP_IMS_Data_Reference_PSI_Activation				= 18,	
	AVP_IMS_Data_Reference_DSAI							= 19,	
	AVP_IMS_Data_Reference_Aliases_Repository_Data		= 20,
	AVP_IMS_Data_Reference_Service_Level_Trace_Info		= 21,
	AVP_IMS_Data_Reference_IP_Address_Secure_Binding_Information = 22,	
};

/** Subs-Req-Type AVP */
enum {
	AVP_IMS_Subs_Req_Type_Subscribe						= 0,
	AVP_IMS_Subs_Req_Type_Unsubscribe					= 1
};

/** Requested-Domain AVP */
enum {
	AVP_IMS_Requested_Domain_CS							= 0,
	AVP_IMS_Requested_Domain_PS							= 1
};

/** UAR-Flags AVP	*/
enum{
	AVP_IMS_UAR_Flags_None								= 0,
	AVP_IMS_UAR_Flags_Emergency_Registration			= 1 /*(1<<0)*/
};

/** Loose-Route-Indication AVP */
enum{
	AVP_IMS_Loose_Route_Not_Required					= 0,
	AVP_IMS_Loose_Route_Required						= 1
};

/** Feature-List-ID AVP for Cx */
enum{
	AVP_IMS_Feature_List_ID_Shared_iFC_Sets				= 1<<0,
	AVP_IMS_Feature_List_ID_Alias_Indication			= 1<<1,
	AVP_IMS_Feature_List_ID_IMS_Restoration_Indication  = 1<<2, 
};

/** Feature-List-ID AVP for Sh */
enum{
	AVP_IMS_Feature_List_ID_Notif_Eff					= 1<<0,
};

/** Multiple-Registration-Indication */
enum{
	AVP_IMS_Not_Multiple_Registration					= 0,
	AVP_IMS_Multiple_Registration						= 1,
};

/** Current-Location AVP */
enum {
	AVP_IMS_Current_Location_Do_Not_Need_Initiate_Active_Location_Retrieval	=0,
	AVP_IMS_Current_Location_Initiate_Active_Location_Retrieval				=1
};

/** Identity-Set AVP */
enum {
	AVP_IMS_Identity_Set_All_Identities					= 0,
	AVP_IMS_Identity_Set_Registered_Identities			= 1,
	AVP_IMS_Identity_Set_Implicit_Identities			= 2,	
	AVP_IMS_Identity_Set_Alias_Identities				= 3	
};

/** Deregistration-Reason AVP */
enum {
	AVP_IMS_Deregistration_Reason_Permanent_Termination	= 0,
	AVP_IMS_Deregistration_Reason_New_Server_Assigned	= 1,
	AVP_IMS_Deregistration_Reason_Server_Change			= 2,	
	AVP_IMS_Deregistration_Reason_Remove_S_CSCF			= 3
};


/** Abort-Cause AVP */
enum {
	AVP_IMS_Abort_Cause_Bearer_Released					= 0,
	AVP_IMS_Abort_Cause_Insufficient_Server_Resources	= 1,
	AVP_IMS_Abort_Cause_Insufficient_Bearer_Resources	= 2
};
/** Flow-Status AVP */
enum {
	AVP_IMS_Flow_Status_Enabled_Uplink					= 0,
	AVP_IMS_Flow_Status_Enabled_Downlink				= 1,
	AVP_IMS_Flow_Status_Enabled							= 2,
	AVP_IMS_Flow_Status_Disabled						= 3,
	AVP_IMS_Flow_Status_Removed							= 4
};

/** Specific-Action AVP */
enum {
	AVP_IMS_Specific_Action_Service_Information_Request						= 0,
	AVP_IMS_Specific_Action_Charging_Correlation_Exchange					= 1,
	AVP_IMS_Specific_Action_Indication_Of_Loss_Of_Bearer					= 2,
	AVP_IMS_Specific_Action_Indication_Of_Recovery_Of_Bearer				= 3,
	AVP_IMS_Specific_Action_Indication_Of_Release_Of_Bearer					= 4,
	AVP_IMS_Specific_Action_Indication_Of_Establishment_Of_Bearer			= 5
};

/** Media-Type AVP */
enum {
	AVP_IMS_Media_Type_Audio					= 0,
	AVP_IMS_Media_Type_Video					= 1,
	AVP_IMS_Media_Type_Data						= 2,
	AVP_IMS_Media_Type_Application				= 3,
	AVP_IMS_Media_Type_Control					= 4,
	AVP_IMS_Media_Type_Text						= 5,
	AVP_IMS_Media_Type_Message					= 6,
	AVP_IMS_Media_Type_Other					= 0xFFFFFFFF
};

/** Latching Indication AVP **/
enum {
	AVP_ETSI_Latching_Indication_Latch 			= 0,
	AVP_ETSI_Latching_Indication_Relatch		= 1
};

/** Send-Data-Indication AVP **/
enum {
	AVP_IMS_Send_Data_Indication_User_Data_Not_Requested 	= 0,
	AVP_IMS_Send_Data_Indication_User_Data_Requested		= 1
};

enum {
	AVP_Re_Auth_Request_Type_Authorize_Only			=0,
	AVP_Re_Auth_Request_Type_Authorize_Authenticate	=1,		
};



/*
access-info for each access type  
"ADSL" / "ADSL2" / "ADSL2+" / "RADSL" / "SDSL" / "HDSL" / "HDSL2" / "G.SHDSL" / "VDSL" / "IDSL"  -> dsl-location
"3GPP-GERAN" -> cgi-3gpp
"3GPP-UTRAN-FDD" / "3GPP-UTRAN-TDD" -> utran-cell-id-3gpp
"3GPP2-1X" / "3GPP2-1X-HRPD" -> ci-3gpp2
"IEEE-802.11" / "IEEE-802.11a" / "IEEE-802.11b" / "IEEE-802.11g" -> i-wlan-node-id = MAC
"DOCSIS" -> NULL
*/

#endif /* __DIAMETER_IMS_CODE_AVP_H */
