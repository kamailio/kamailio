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

#ifndef DIAMETER_CODE_AVP_H_
#define DIAMETER_CODE_AVP_H_

/** Standard AVP Codes */
typedef enum {
	
	/* RFC 3588 */
	
	AVP_User_Name                     =    1,
	AVP_User_Password				  =    2, //RFC4005
	AVP_NAS_IP_Addresss				  =    4, //RFC4005
	AVP_NAS_Port					  =    5, //RFC4005
	AVP_Service_Type				  =    6, //RFC4005
	AVP_Framed_Protocol				  =    7, //RFC4005
	AVP_Framed_IP_Address             =	   8, //RFC4005
	AVP_Framed_IP_Netmask			  =    9, //RFC4005
	AVP_Framed_Routing				  =   10, //RFC4005
	AVP_Filter_Id					  =   11, //RFC4005
	AVP_Framed_MTU					  =   12, //RFC4005
	AVP_Framed_Compression			  =   13, //RFC4005
	AVP_Login_IP_Host				  =   14, //RFC4005
	AVP_Login_Service				  =   15, //RFC4005
	AVP_Login_TCP_Port				  =   16, //RFC4005
	AVP_Reply_Message				  =   18, //RFC4005
	AVP_Callback_Number				  =   19, //RFC4005
	AVP_Callback_Id					  =   20, //RFC4005
	AVP_Framed_Route				  =   22, //RFC4005
	AVP_Framed_IPX_Network			  =   23, //RFC4005
	AVP_State						  =   24, //RFC4005
	AVP_Class                         =   25,
	AVP_Session_Timeout               =   27,
	AVP_Idle_Timeout				  =   28, //RFC4005
	AVP_Called_Station_Id			  =   30, //RFC4005
	AVP_Calling_Station_Id			  =   31, //RFC4005
	AVP_NAS_Identifier				  =   32, //RFC4005
	AVP_Proxy_State                   =   33,
	AVP_Login_LAT_Service			  =   34, //RFC4005
	AVP_Login_LAT_Node				  =   35, //RFC4005
	AVP_Login_LAT_Group				  =   36, //RFC4005
	AVP_Framed_Appletalk_Link		  =   37, //RFC4005
	AVP_Framed_Appletalk_Network	  =   38, //RFC4005
	AVP_Framed_Appletalk_Zone		  =   39, //RFC4005
	AVP_Acct_Delay_Time				  =   41, //RFC4005
	AVP_Acct_Session_Id				  =   44,
	AVP_Acct_Authentic				  =   45, //RFC4005
	AVP_Acct_Session_Time			  =   46, //RFC4005
	AVP_Acct_Multi_Session_Id		  =   50,
	AVP_Link_Count					  =   51, //RFC4005
	AVP_Event_Timestamp               =   55,
	AVP_CHAP_Challenge				  =   60, //RFC4005
	AVP_NAS_Port_Type                 =   61, //RFC4005
	AVP_Port_Limit					  =   62, //RFC4005
	AVP_Login_LAT_Port				  =   63, //RFC4005
	AVP_Tunnel_Type					  =   64, //RFC4005
	AVP_Tunnel_Medium_Type			  =   65, //RFC4005
	AVP_Tunnel_Client_Endpoint		  =   66, //RFC4005
	AVP_Tunnel_Server_Endpoint		  =   67, //RFC4005
	AVP_Acct_Tunnel_Connection		  =   68, //RFC4005
	AVP_Tunnel_Password				  =   69, //RFC4005
	AVP_ARAP_Password				  =   70, //RFC4005
	AVP_ARAP_Features				  =   71, //RFC4005
	AVP_ARAP_Zone_Access			  =   72, //RFC4005
	AVP_ARAP_Security				  =   73, //RFC4005
	AVP_ARAP_Security_Data			  =   74, //RFC4005
	AVP_Password_Retry				  =   75, //RFC4005
	AVP_Prompt						  =   76, //RFC4005
	AVP_Connect_Info				  =   77, //RFC4005
	AVP_Configuration_Token			  =   78, //RFC4005
	AVP_Tunnel_Private_Group_Id		  =   81, //RFC4005
	AVP_Tunnel_Assignment_Id		  =   82, //RFC4005
	AVP_Tunnel_Preference			  =   83, //RFC4005
	AVP_ARAP_Challenge_Response		  =   84, //RFC4005
	AVP_Acct_Interim_Interval         =   85,
	AVP_Acct_Tunnel_Packets_Lost	  =   86, //RFC4005
	AVP_NAS_Port_Id					  =   87, //RFC4005
	AVP_Framed_Pool					  =   88, //RFC4005
	AVP_Tunnel_Client_Auth_Id		  =   90, //RFC4005
	AVP_Tunnel_Server_Auth_Id		  =   91,
	AVP_Originating_Line_Info		  =   94, //RFC4005
	AVP_NAS_IPv6_Address			  =   95, //RFC4005
	AVP_Framed_Interface_Id           =   96, //RFC4005
	AVP_Framed_IPv6_Prefix            =   97, //RFC4005
	AVP_Framed_IPv6_Route			  =   98, //RFC4005
#define AVP_Login_IPv6_Host				  98	
	AVP_Framed_IPv6_Pool			  =  100, //RFC4005
	
	AVP_MIP6_Feature_Vector			  =  124, //RFC5447
	AVP_MIP6_Home_Link_Prefix		  =  125, //RFC5447
	
	AVP_Host_IP_Address               =  257,
	AVP_Auth_Application_Id           =  258,
	AVP_Acct_Application_Id           =  259,	
	AVP_Vendor_Specific_Application_Id=  260,
	AVP_Redirect_Host_Usage			  =  261,
	AVP_Redirect_Max_Cache_Time       =  262,
	AVP_Session_Id                    =  263,
	AVP_Origin_Host                   =  264,
	AVP_Supported_Vendor_Id           =  265,
	AVP_Vendor_Id                     =  266,
	AVP_Firmware_Revision             =  267,
	AVP_Result_Code                   =  268,
	AVP_Product_Name                  =  269,
	AVP_Session_Binding               =  270,
	AVP_Session_Server_Failover		  =  271,
	AVP_Multi_Round_Time_Out          =  272,
	AVP_Disconnect_Cause              =  273,
	AVP_Auth_Request_Type             =  274,
	AVP_Auth_Grace_Period             =  276,
	AVP_Auth_Session_State            =  277,
	AVP_Origin_State_Id               =  278,
	AVP_Failed_AVP					  =  279,
	AVP_Proxy_Host                    =  280,
	AVP_Error_Message                 =  281,
	AVP_Route_Record                  =  282,
	AVP_Destination_Realm             =  283,
	AVP_Proxy_Info                    =  284,
	AVP_Re_Auth_Request_Type          =  285,
	AVP_Accounting_Sub_Session_Id	  =  287,
	AVP_Authorization_Lifetime        =  291,
	AVP_Redirect_Host                 =  292,
	AVP_Destination_Host              =  293,
	AVP_Error_Reporting_Host		  =  294,
	AVP_Termination_Cause             =  295,
	AVP_Origin_Realm                  =  296,
	AVP_Experimental_Result			  =  297,
	AVP_Experimental_Result_Code      =  298,
	AVP_Inband_Security_Id			  =  299,
	
	AVP_E2E_Sequence				  =  300,
	AVP_Accounting_Input_Octets		  =  363, //RFC4005
	AVP_Accounting_Output_Octets	  =  364, //RFC4005
	AVP_Accounting_Input_Packets	  =  365, //RFC4005
	AVP_Accounting_Output_Packets	  =  366, //RFC4005
	
	/* RFC 4004 */
	AVP_MIP_Reg_Request               =  320, 
	AVP_MIP_Reg_Reply                 =  321, 
	AVP_MIP_MN_AAA_Auth               =  322, 
	AVP_MIP_Mobile_Node_Address       =  333, 
	AVP_MIP_Home_Agent_Address        =  334, 
	AVP_MIP_Candidate_Home_Agent_Host =  336, 
	AVP_MIP_Feature_Vector            =  337, 
	AVP_MIP_Auth_Input_Data_Length    =  338, 
	AVP_MIP_Authenticator_Length      =  339, 
	AVP_MIP_Authenticator_Offset      =  340, 
	AVP_MIP_MN_AAA_SPI                =  341, 
	AVP_MIP_Filter_Rule               =  342, 
	AVP_MIP_FA_Challenge              =  344, 
	AVP_MIP_Originating_Foreign_AAA   =  347, 
	AVP_MIP_Home_Agent_Host           =  348, 
	
	/* RFC 4005 */
	AVP_NAS_Filter_Rule				  =  400,
	AVP_Tunneling					  =  401,
	AVP_CHAP_Auth					  =  402,
	AVP_CHAP_Algorithm				  =  403,
	AVP_CHAP_Ident					  =  404,
	AVP_CHAP_Response				  =  405,
	AVP_Accounting_Auth_Method		  =  406,
	AVP_QoS_Filter_Rule				  =  407,
	AVP_Origin_AAA_Protocol			  =  408,
	
	/* RFC 4006 */
	AVP_CC_Correlation_Id			  =  411,
	AVP_CC_Input_Octets				  =  412,
	AVP_CC_Money                      =  413,
	AVP_CC_Output_Octets              =  414,
	AVP_CC_Request_Number             =  415, 
	AVP_CC_Request_Type               =  416,
	AVP_CC_Service_Specific_Units     =  417,
	AVP_CC_Session_Failover           =  418,
	AVP_CC_Sub_Session_Id             =  419,
	AVP_CC_Time                       =  420,
	AVP_CC_Total_Octets               =  421,
	AVP_Check_Balance_Result          =  422,
	AVP_Cost_Information              =  423,
	AVP_Cost_Unit                     =  424,
	AVP_Currency_Code                 =  425,
	AVP_Credit_Control                =  426,
	AVP_Credit_Control_Failure_Handling= 427,
	AVP_Direct_Debiting_Failure_Handling=428,
	AVP_Exponent                      =  429,
	AVP_Final_Unit_Indication		  =  430,
	AVP_Granted_Service_Unit          =  431,
	AVP_Rating_Group				  =  432,
	AVP_Redirect_Address_Type         =  433,
	AVP_Redirect_Server               =  434,
	AVP_Redirect_Server_Address       =  435,
	AVP_Requested_Action              =  436,
	AVP_Requested_Service_Unit        =  437,
	AVP_Restriction_Filter_Rule       =  438,
	AVP_Service_Identifier			  =  439,
	AVP_Service_Parameter_Info        =  440,
	AVP_Service_Parameter_Type        =  441,
	AVP_Service_Parameter_Value       =  442,
	AVP_Subscription_Id				  =  443, 
	AVP_Subscription_Id_Data		  =  444,
	AVP_Unit_Value                    =  445,
	AVP_Used_Service_Unit             =  446,
	AVP_Value_Digits                  =  447,
	AVP_Validity_Time                 =  448,
	AVP_Final_Unit_Action			  =  449, 
	AVP_Subscription_Id_Type		  =  450,
	AVP_Tariff_Time_Change            =  451, 
	AVP_Tariff_Change_Usage           =  452,
	AVP_G_S_U_Pool_Identifier         =  453,
	AVP_CC_Unit_Type                  =  454,
	AVP_Multiple_Services_Indicator   =  455,
	AVP_Multiple_Services_Credit_Control=456,
	AVP_G_S_U_Pool_Reference          =  457,
	AVP_User_Equipment_Info	          =  458, 
	AVP_User_Equipment_Info_Type      =  459, 
	AVP_User_Equipment_Info_Value	  =  460,
	AVP_Service_Context_Id            =  461,
	
	
	AVP_Accounting_Record_Type        =  480,
	AVP_Accounting_Realtime_Required  =  483,
	AVP_Accounting_Record_Number      =  485,
	AVP_MIP6_Agent_Info				  =  486, //RFC5447
	
	AVP_Service_Selection			  =  493, //RFC5778  
	AVP_Call_Id                       =  494,
	
}AAA_AVPCodeNr;

enum {
	AVP_CC_Request_Type_Initial_Request		= 1,
	AVP_CC_Request_Type_Update_Request		= 2,
	AVP_CC_Request_Type_Termination_Request	= 3,
	AVP_CC_Request_Type_Event_Request		= 4,
};

enum  {
	AVP_CC_Session_Failover_Failover_Not_Supported	= 0,
	AVP_CC_Session_Failover_Failover_Supported		= 1,
};

enum  {
	AVP_Check_Balance_Result_Enough_Credit	= 0,
	AVP_Check_Balance_Result_No_Credit		= 1,
};

enum  {
	AVP_Credit_Control_Credit_Authorization	= 0,
	AVP_Credit_Control_Re_Authorization		= 1,
};

enum  {
	AVP_Credit_Control_Failure_Handling_Terminate			= 0,
	AVP_Credit_Control_Failure_Handling_Continue			= 1,
	AVP_Credit_Control_Failure_Handling_Retry_And_Terminate	= 2,	
};

enum  {
	AVP_Direct_Debiting_Failure_Handling_Terminate_Or_Buffer	= 0,
	AVP_Direct_Debiting_Failure_Handling_Continue				= 1,
};

enum  {
	AVP_Tariff_Change_Usage_Unit_Before_Tariff_Change	= 0,
	AVP_Tariff_Change_Usage_Unit_After_Tariff_Change	= 1,
	AVP_Tariff_Change_Usage_Unit_Indeterminate			= 2,
};

enum  {
	AVP_CC_Unit_Type_Time					= 0,
	AVP_CC_Unit_Type_Money					= 1,
	AVP_CC_Unit_Type_Total_Octets			= 2,
	AVP_CC_Unit_Type_Input_Octets			= 3,
	AVP_CC_Unit_Type_Output_Octets			= 4,
	AVP_CC_Unit_Type_Service_Specific_Units	= 5,
};

enum  {
	AVP_Final_Unit_Action_Terminate			= 0,
	AVP_Final_Unit_Action_Redirect			= 1,
	AVP_Final_Unit_Action_Restrict_Access	= 2,
};

enum  {
	AVP_Redirect_Address_Type_IPv4_Address	= 0,
	AVP_Redirect_Address_Type_IPv6_Address	= 1,
	AVP_Redirect_Address_Type_URL			= 2,
	AVP_Redirect_Address_Type_SIP_URI		= 3
};

enum  {
	AVP_Multiple_Services_Indicator_Multiple_Services_Not_Supported	= 0,
	AVP_Multiple_Services_Indicator_Multiple_Services_Supported		= 1,
};

enum  {
	AVP_Redirect_Action_Direct_Debiting	= 0,
	AVP_Redirect_Action_Refund_Account	= 1,
	AVP_Redirect_Action_Check_Ballance	= 2,
	AVP_Redirect_Action_Price_Enquiry	= 3,
};

enum {
	AVP_Subscription_Id_Type_E164			= 0,
	AVP_Subscription_Id_Type_IMSI			= 1,
	AVP_Subscription_Id_Type_SIP_URI		= 2,
	AVP_Subscription_Id_Type_NAI			= 3,
	AVP_Subscription_Id_Type_USER_PRIVATE	= 4
};

enum {
	AVP_User_Equipment_Info_Type_IMEISV			= 0,
	AVP_User_Equipment_Info_Type_MAC			= 1,
	AVP_User_Equipment_Info_Type_EUI64			= 2,
	AVP_User_Equipment_Info_Type_MODIFIED_EUI64	= 3,
};



typedef enum
{
	AVP_NAS_Port_Type_Async					= 0,
	AVP_NAS_Port_Type_Sync 					= 1, 
	AVP_NAS_Port_Type_ISDN_Sync 			= 2, 
	AVP_NAS_Port_Type_ISDN_Async_V120 		= 3,
	AVP_NAS_Port_Type_ISDN_Async_V110		= 4,
	AVP_NAS_Port_Type_Virtual				= 5, 
	AVP_NAS_Port_Type_PIAFS					= 6, 	
	AVP_NAS_Port_Type_HDLC_Clear_Channel	= 7, 
	AVP_NAS_Port_Type_X_25					= 8, 
	AVP_NAS_Port_Type_X_75					= 9, 
	AVP_NAS_Port_Type_G_3_Fax				=10,
	AVP_NAS_Port_Type_Symmetric_DSL			=11, 	
	AVP_NAS_Port_Type_ADSL_CAP				=12,
	AVP_NAS_Port_Type_ADSL_DMT				=13, 
	AVP_NAS_Port_Type_IDSL					=14, 
	AVP_NAS_Port_Type_Ethernet				=15, 
	AVP_NAS_Port_Type_xDSL					=16, 
	AVP_NAS_Port_Type_Cable					=17, 
	AVP_NAS_Port_Type_Wireless_Other		=18,
	AVP_NAS_Port_Type_Wireless_IEEE_802_11	=19,
	AVP_NAS_Port_Type_Token_Ring			=20, 
	AVP_NAS_Port_Type_FDDI					=21,
	AVP_NAS_Port_Type_Wireless_CDMA2000		=22,
	AVP_NAS_Port_Type_Wireless_UMTS			=23,
	AVP_NAS_Port_Type_Wireless_1X_EV		=24,
	AVP_NAS_Port_Type_IAPP  				=25
}	nas_port_type;						

enum {
	AVP_Prompt_No_Echo	= 0,
	AVP_Prompt_Echo		= 1,
};

enum {
	AVP_CHAP_Algorithm_CHAP_with_MD5	= 5,
};


enum {
	AVP_Service_Type_Login						= 1,
	AVP_Service_Type_Framed						= 2,
	AVP_Service_Type_Callback_Login				= 3,
	AVP_Service_Type_Callback_Framed			= 4,
	AVP_Service_Type_Outbound					= 5,
	AVP_Service_Type_Administrative				= 6,
	AVP_Service_Type_NAS_Prompt					= 7,
	AVP_Service_Type_Authenticate_Only			= 8,
	AVP_Service_Type_Callback_NAS_Prompt		= 9,
	AVP_Service_Type_Call_Check					= 10,
	AVP_Service_Type_Callback_Administrative	= 11,
	AVP_Service_Type_Voice						= 12,
	AVP_Service_Type_Fax						= 13,
	AVP_Service_Type_Modem_Relay				= 14,
	AVP_Service_Type_IAPP_Register				= 15,
	AVP_Service_Type_IAPP_AP_Check				= 16,
	AVP_Service_Type_Authorize_Only				= 17,
};

enum {
	AVP_Framed_Protocol_PPP						= 0,
	AVP_Framed_Protocol_SLIP					= 1,
	AVP_Framed_Protocol_ARAP					= 2,
	AVP_Framed_Protocol_Gandalf					= 3,
	AVP_Framed_Protocol_Xylogics_IPX_SLIP		= 4,
	AVP_Framed_Protocol_X_75_Synchronous		= 5,
};

enum {
	AVP_Framed_Routing_None							= 0,
	AVP_Framed_Routing_Send_Routing_Packets			= 1,
	AVP_Framed_Routing_Listen_for_Routing_Packets	= 2,
	AVP_Framed_Routing_Send_and_Listen				= 3,
};

enum {
	AVP_Framed_Compression_None							= 0,
	AVP_Framed_Compression_VJ_TCP_IP_Header_Compression	= 1,
	AVP_Framed_Compression_IPX_Header_Compression		= 2,
	AVP_Framed_Compression_Stac_LZS_Compression			= 3,
	AVP_Framed_Compression_
};

enum {
	AVP_Login_Service_Telnet			= 0,
	AVP_Login_Service_Rlogin			= 1,
	AVP_Login_Service_TCP_Clear			= 2,
	AVP_Login_Service_PortMaster		= 3,
	AVP_Login_Service_LAT				= 4,		
	AVP_Login_Service_X25_PAD			= 5,
	AVP_Login_Service_X25_T3POS			= 6,
	AVP_Login_Service_TCP_Clear_Quiet	= 7,
};

enum {
	AVP_Tunnel_Type_PPTP					= 1,
	AVP_Tunnel_Type_L2F						= 2,
	AVP_Tunnel_Type_L2TP					= 3,
	AVP_Tunnel_Type_ATMP					= 4,
	AVP_Tunnel_Type_VTP						= 5,
	AVP_Tunnel_Type_AH						= 6,
	AVP_Tunnel_Type_IPIP_Encapsulation		= 7,
	AVP_Tunnel_Type_MIN_IPIP_Encapsulation	= 8,
	AVP_Tunnel_Type_ESP						= 9,
	AVP_Tunnel_Type_GRE						= 10,
	AVP_Tunnel_Type_DVS						= 11,
	AVP_Tunnel_Type_IPIP_Tunneling 			= 12,
	AVP_Tunnel_Type_VLAN					= 13,
};

enum {
	AVP_Tunnel_Medium_Type_IPv4			= 1,
	AVP_Tunnel_Medium_Type_IPv6			= 2,
	AVP_Tunnel_Medium_Type_NSAP			= 3,
	AVP_Tunnel_Medium_Type_HDLC			= 4,
	AVP_Tunnel_Medium_Type_BBN_1822		= 5,
	AVP_Tunnel_Medium_Type_802			= 6,
	AVP_Tunnel_Medium_Type_E_163		= 7,
	AVP_Tunnel_Medium_Type_E_164		= 8,
	AVP_Tunnel_Medium_Type_F_69			= 9,
	AVP_Tunnel_Medium_Type_X_121		= 10,
	AVP_Tunnel_Medium_Type_IPX			= 11,
	AVP_Tunnel_Medium_Type_Appletalk	= 12,
	AVP_Tunnel_Medium_Type_Decnet_IV	= 13,
	AVP_Tunnel_Medium_Type_Banyan_Vines	= 14,
	AVP_Tunnel_Medium_Type_E_164_NSAP	= 15,
};

enum {
	AVP_Acct_Authentic_RADIUS	= 1,
	AVP_Acct_Authentic_Local	= 2,
	AVP_Acct_Authentic_Remote	= 3,
	AVP_Acct_Authentic_Diameter	= 4,
};

enum {
	AVP_Accounting_Auth_Method_PAP			= 1,
	AVP_Accounting_Auth_Method_CHAP			= 2,
	AVP_Accounting_Auth_Method_MS_CHAP_1	= 3,
	AVP_Accounting_Auth_Method_MS_CHAP_2	= 4,
	AVP_Accounting_Auth_Method_EAP			= 5,
	AVP_Accounting_Auth_Method_None			= 7,
};

enum {
	AVP_Termination_Cause_User_Request				= 11,
	AVP_Termination_Cause_Lost_Carrier				= 12,
	AVP_Termination_Cause_Lost_Service				= 13,
	AVP_Termination_Cause_Idle_Timeout				= 14,
	AVP_Termination_Cause_Session_Timeout			= 15,
	AVP_Termination_Cause_Admin_Reset				= 16,
	AVP_Termination_Cause_Admin_Reboot				= 17,
	AVP_Termination_Cause_Port_Error				= 18,
	AVP_Termination_Cause_NAS_Error					= 19,
	AVP_Termination_Cause_NAS_Request				= 20,
	AVP_Termination_Cause_NAS_Reboot				= 21,
	AVP_Termination_Cause_Port_Unneeded				= 22,
	AVP_Termination_Cause_Port_Preempted			= 23,
	AVP_Termination_Cause_Port_Suspended			= 24,
	AVP_Termination_Cause_Service_Unavailable		= 25,
	AVP_Termination_Cause_Callback					= 26,
	AVP_Termination_Cause_User_Error				= 27,
	AVP_Termination_Cause_Host_Request				= 28,
	AVP_Termination_Cause_Supplicant_Restart		= 29,
	AVP_Termination_Cause_Reauthentication_Failure	= 30,
};

enum {
	AVP_Origin_AAA_Protocol_RADIUS	= 1,
};

enum {
	AVP_Accounting_Record_Type_Event_Record		= 1,
	AVP_Accounting_Record_Type_Start_Record		= 2,
	AVP_Accounting_Record_Type_Interim_Record	= 3,
	AVP_Accounting_Record_Type_Stop_Record		= 4,			
};





typedef enum {
        Permanent_Termination   = 0,
        New_Server_Assigned     = 1,
        Server_Change           = 2,
        Remove_S_CSCF           = 3,
}AAA_AVPReasonCode;

typedef enum {
	STATE_MAINTAINED			= 0,
	NO_STATE_MAINTAINED			= 1
} AAA_AVP_Auth_Session_State;


/** Accounting message types */
typedef enum {
	AAA_ACCT_EVENT = 1,
	AAA_ACCT_START = 2,
	AAA_ACCT_INTERIM = 3,
	AAA_ACCT_STOP = 4
} AAAAcctMessageType;


#endif /*DIAMETER_H_*/
