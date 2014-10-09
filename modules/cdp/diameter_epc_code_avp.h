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

#ifndef DIAMETER_EPC_CODE_AVP_H_
#define DIAMETER_EPC_CODE_AVP_H_



/** 3GPP AVP Codes */
enum {
/**   1 to 255 reserved for backward compatibility with Radius TS29.061	*/

	AVP_EPC_3GPP_IMSI									= 1,
	AVP_EPC_3GPP_Charging_Id							= 2,
	AVP_EPC_3GPP_PDP_Type								= 3,
	AVP_EPC_3GPP_CG_Address								= 4,
	AVP_EPC_3GPP_GPRS_Negotiated_QoS_Profile			= 5,
	AVP_EPC_3GPP_SGSN_Address							= 6,
	AVP_EPC_3GPP_GGSN_Address							= 7,
	AVP_EPC_3GPP_IMSI_MCC_MNC							= 8,
	AVP_EPC_3GPP_GGSN_MCC_MNC							= 9,
	AVP_EPC_3GPP_NSAPI									= 10,
	AVP_EPC_3GPP_Session_Stop_Indicator					= 11,// TS32.299
	AVP_EPC_3GPP_Selection_Mode							= 12,
	AVP_EPC_3GPP_Charging_Characteristics				= 13,
	AVP_EPC_3GPP_CG_IPv6_Address						= 14,
	AVP_EPC_3GPP_SGSN_IPv6_Address						= 15,
	AVP_EPC_3GPP_GGSN_IPv6_Address						= 16,
	AVP_EPC_3GPP_IPv6_DNS_Servers						= 17,
	AVP_EPC_3GPP_SGSN_MCC_MNC							= 18,
	AVP_EPC_3GPP_IMEISV									= 20,
	AVP_EPC_TGPP_RAT_Type								= 21,
	AVP_EPC_3GPP_User_Location_Info						= 22,
	AVP_EPC_3GPP_MS_TimeZone							= 23,
	AVP_EPC_3GPP_Allocate_IP_Type						= 27,
	AVP_EPC_RAI											= 909, //TS29.061
/** 256 to 299 reserved for future use										*/


/** 300 to 399 reserved for TS29.234											*/
	AVP_EPC_3GPP_AAA_Server_Name	  					= 318, //TS29.234
	
/** 400 to 499 reserved for TS29.109 */
		
/** 500 to 599 reserved for TS29.209											*/
	AVP_EPC_Abort_Cause									= 500,
	AVP_EPC_Access_Network_Charging_Address				= 501,
	AVP_EPC_Access_Network_Charging_Identifier			= 502,
	AVP_EPC_Access_Network_Charging_Identifier_Value	= 503,
	AVP_EPC_AF_Application_Identifier					= 504,
	AVP_EPC_AF_Charging_Identifier						= 505,
	AVP_EPC_Authorization_Token							= 506,
	AVP_EPC_Flow_Description							= 507,
	AVP_EPC_Flow_Grouping								= 508,
	AVP_EPC_Flow_Number									= 509,
	AVP_EPC_Flows										= 510,
	AVP_EPC_Flow_Status									= 511,
	AVP_EPC_Flow_Usage									= 512,
	AVP_EPC_Specific_Action								= 513,
	AVP_EPC_Max_Requested_Bandwidth_DL					= 515,
	AVP_EPC_Max_Requested_Bandwidth_UL					= 516,
	AVP_EPC_Media_Component_Description					= 517,
	AVP_EPC_Media_Component_Number						= 518,
	AVP_EPC_Media_Sub_Component							= 519,
	AVP_EPC_Media_Type									= 520,
	AVP_EPC_RR_Bandwidth								= 521,
	AVP_EPC_RS_Bandwidth								= 522,
	AVP_EPC_SIP_Forking_Indication						= 523,
	AVP_EPC_Codec_Data									= 524,
	AVP_EPC_Service_URN									= 525,
	AVP_EPC_Acceptable_Service_Info						= 526,
	AVP_EPC_Service_Info_Status							= 527,
/** 600 to 699 reserved for TS29.229											*/
/** 700 to 799 reserved for TS29.329											*/
	
/** 800 to 899 reserved for TS29.299											*/

/** 32.299 Partial																*/
	AVP_EPC_Event_Type									= 823,
	AVP_EPC_SIP_Method									= 824,
	AVP_EPC_Event										= 825,
	AVP_EPC_Content_Type								= 826,
	AVP_EPC_Content_Length								= 827,
	AVP_EPC_Content_Disposition							= 828,
	AVP_EPC_Role_Of_Node								= 829,
	AVP_EPC_User_Session_Id								= 830,
	AVP_EPC_Calling_Party_Address						= 831,
	AVP_EPC_Called_Party_Address						= 832,
	AVP_EPC_Time_Stamps									= 833,
	AVP_EPC_SIP_Request_Timestamp						= 834,
	AVP_EPC_SIP_Response_Timestamp						= 835,
	AVP_EPC_Application_Server							= 836,
	AVP_EPC_Application_Provided_Called_Party_Address	= 837,
	AVP_EPC_Inter_Operator_Identifier					= 838,
	AVP_EPC_Originating_IOI								= 839,
	AVP_EPC_Terminating_IOI								= 840,
	AVP_EPC_IMS_Charging_Identifier						= 841,
	AVP_EPC_SDP_Session_Description						= 842,
	AVP_EPC_SDP_Media_Component							= 843,
	AVP_EPC_SDP_Media_Name								= 844,
	AVP_EPC_SDP_Media_Description						= 845,
	AVP_EPC_CG_Address									= 846,
	AVP_EPC_GGSN_Address								= 847,
	AVP_EPC_Served_Party_IP_Address						= 848,
	AVP_EPC_Authorized_QoS								= 849,
	AVP_EPC_Application_Server_Information				= 850,
	AVP_EPC_Trunk_Group_Id								= 851,
	AVP_EPC_Incoming_Trunk_Group_Id						= 852,
	AVP_EPC_Outgoing_Trunk_Group_Id						= 853,
	AVP_EPC_Bearer_Service								= 854,
	AVP_EPC_Service_ID									= 855,
	AVP_EPC_Associated_URI								= 856,
	AVP_EPC_Charged_Party								= 857,
	AVP_EPC_Cause_Code									= 861,
	AVP_EPC_Node_Functionality							= 862,
	AVP_EPC_Service_Specific_Data						= 863,
	AVP_EPC_Originator									= 864,
	AVP_EPC_Service_Information							= 873,
	AVP_EPC_PS_Information								= 874,
	AVP_EPC_IMS_Information								= 876,
	AVP_EPC_Media_Initiator_Flag						= 882,
	AVP_EPC_Expires										= 888,
	AVP_EPC_Message_Body								= 889,
	AVP_EPC_Address_Data								= 897,
	AVP_EPC_Address_Domain								= 898,
	AVP_EPC_Address_Type								= 899,
	AVP_EPC_CN_IP_Multicast_Distribution				= 921,
	
/** 1000   from TS29.212 */
	AVP_EPC_Bearer_Usage								= 1000,
 	AVP_EPC_Charging_Rule_Install						= 1001,
 	AVP_EPC_Charging_Rule_Remove						= 1002,
 	AVP_EPC_Charging_Rule_Definition					= 1003,
 	AVP_EPC_Charging_Rule_Base_Name						= 1004,
 	AVP_EPC_Charging_Rule_Name							= 1005,
 	AVP_EPC_Event_Trigger								= 1006,
 	AVP_EPC_Metering_Method								= 1007,
 	AVP_EPC_Offline										= 1008,
 	AVP_EPC_Online										= 1009,
 	AVP_EPC_Precedence									= 1010,
 	AVP_EPC_Reporting_Level								= 1011,
 	AVP_EPC_TFT_Filter									= 1012,
 	AVP_EPC_TFT_Packet_Filter_Information				= 1013,
 	AVP_EPC_ToS_Traffic_Class							= 1014,
 	AVP_EPC_QoS_Bandwidth								= 1015,  //Not used in the EPC
 	AVP_EPC_QoS_Information								= 1016,
 	AVP_EPC_QoS_Jitter									= 1017,  //Not used in the EPC
 	AVP_EPC_Charging_Rule_Report						= 1018,
 	AVP_EPC_PCC_Rule_Status								= 1019,
 	AVP_EPC_Bearer_Identifier							= 1020,
 	AVP_EPC_Bearer_Operation							= 1021,
 	AVP_EPC_Access_Network_Charging_Identifier_Gx		= 1022,
 	AVP_EPC_Bearer_Control_Mode							= 1023,
 	AVP_EPC_Network_Request_Support						= 1024,
 	AVP_EPC_Guaranteed_Bitrate_DL						= 1025,
 	AVP_EPC_Guaranteed_Bitrate_UL						= 1026,
 	AVP_EPC_IP_CAN_Type									= 1027,
 	AVP_EPC_QoS_Class_Identifier						= 1028,
 	AVP_EPC_QoS_Negotiation								= 1029,
 	AVP_EPC_QoS_Upgrade									= 1030,
 	AVP_EPC_Rule_Failure_Code							= 1031,
 	AVP_EPC_RAT_Type									= 1032,
 	AVP_EPC_Event_Report_Indication						= 1033,
 	AVP_EPC_Allocation_Retention_Priority				= 1034,
 	AVP_EPC_CoA_IP_Address								= 1035,
 	AVP_EPC_Tunnel_Header_Filter						= 1036,
 	AVP_EPC_Tunnel_Header_Length						= 1037,
 	AVP_EPC_Tunnel_Information							= 1038,
 	AVP_EPC_CoA_Information								= 1039,
 	AVP_EPC_APN_Aggregate_Max_Bitrate_DL				= 1040,
 	AVP_EPC_APN_Aggregate_Max_Bitrate_UL				= 1041,
 	AVP_EPC_Revalidation_Time							= 1042,
 	AVP_EPC_Rule_Activation_Time						= 1043,
 	AVP_EPC_Rule_DeActivation_Time						= 1044,
 	AVP_EPC_Session_Release_Cause						= 1045,
 	AVP_EPC_ARP_Value									= 1046, //Depends on the version
 	AVP_EPC_Priority_Level								= 1046,
 	AVP_EPC_Pre_emption_Capability						= 1047,
 	AVP_EPC_Pre_emption_Vulnerability					= 1048,
 	AVP_EPC_Default_EPS_Bearer_QoS						= 1049,
 	AVP_EPC_AN_GW_Address								= 1050,
 	AVP_EPC_QoS_Rule_Install							= 1051, //Gxx
 	AVP_EPC_QoS_Rule_Remove								= 1052,
 	AVP_EPC_QoS_Rule_Definition							= 1053,
 	AVP_EPC_QoS_Rule_Name								= 1054,
 	AVP_EPC_QoS_Rule_Report								= 1055,
 	AVP_EPC_Security_Parameter_Index					= 1056,
 	AVP_EPC_Flow_Label									= 1057,
 	AVP_EPC_Flow_Information							= 1058,
	AVP_EPC_Packet_Filter_Content						= 1059,
	AVP_EPC_Packet_Filter_Identifier					= 1060,
	AVP_EPC_Packet_Filter_Information					= 1061,
	AVP_EPC_Packet_Filter_Operation						= 1062,
	AVP_EPC_Resource_Allocation_Notification			= 1063, //Gx
	AVP_EPC_Session_Linking_Indicator					= 1064,
	AVP_EPC_PDN_Connection_ID							= 1065,
 	
/** TS 32.299  */
	
	AVP_EPC_Additional_Type_Information					= 1205,
	AVP_EPC_Content_Size								= 1206,
	AVP_EPC_Additional_Content_Information				= 1207,
	AVP_EPC_Addressee_Type								= 1208,
	AVP_EPC_Class_Identifier							= 1214,
	AVP_EPC_Adaptations									= 1217,
	AVP_EPC_Applic_ID									= 1218,
	AVP_EPC_Aux_Applic_Info								= 1219,
	AVP_EPC_Content_Class								= 1220,
	AVP_EPC_PDP_Address									= 1227,
	AVP_EPC_SGSN_Address								= 1228,
	AVP_EPC_Service_Specific_Info						= 1249,
	AVP_EPC_Called_Asserted_Identity					= 1250,
	AVP_EPC_Requested_Party_Address						= 1251,
	AVP_EPC_Service_Specific_Type						= 1257,
	AVP_EPC_Access_Network_Information					= 1263,
	AVP_EPC_Base_Time_Interval							= 1265,
	AVP_EPC_Early_Media_Description						= 1272,
	AVP_EPC_SDP_TimeStamps								= 1273,
	AVP_EPC_SDP_Offer_TimeStamp							= 1274,
	AVP_EPC_SDP_Answer_TimeStamp						= 1275,
	AVP_EPC_AF_Correlation_Information					= 1276,
	AVP_EPC_Alternate_Charged_Party_Address				= 1280,
	AVP_EPC_Media_Initiator_Party						= 1288,
	
	
/** TS 29.272  */
	AVP_EPC_Subscription_Data							= 1400,
	AVP_EPC_Terminal_Information						= 1401,
	AVP_EPC_IMEI										= 1402,
	AVP_EPC_Software_Version							= 1403,
	AVP_EPC_QoS_Subscribed								= 1404,
	AVP_EPC_ULR_Flags									= 1405,
	AVP_EPC_ULA_Flags									= 1406,
	AVP_EPC_Visited_PLMN_Id								= 1407,
	AVP_EPC_Requested_EUTRAN_Authentication_Info		= 1408,
	AVP_EPC_Requested_UTRAN_GERAN_Authentication_Info	= 1409,
	AVP_EPC_Number_Of_Requested_Vectors					= 1410,
	AVP_EPC_Re_Synchronization_Info						= 1411,
	AVP_EPC_Immediate_Response_Preferred				= 1412,
	AVP_EPC_Authentication_Info							= 1413,
	AVP_EPC_E_UTRAN_Vector								= 1414,
	AVP_EPC_UTRAN_Vector								= 1415,
	AVP_EPC_GERAN_Vector								= 1416,
	AVP_EPC_Network_Access_Mode							= 1417,
	AVP_EPC_HPLMN_ODB									= 1418,
	AVP_EPC_Item_Number									= 1419,
	AVP_EPC_Cancellation_Type							= 1420,
	AVP_EPC_DSR_Flags									= 1421,
	AVP_EPC_DSA_Flags									= 1422,
	AVP_EPC_Context_Identifier							= 1423,
	AVP_EPC_Subscriber_Status							= 1424,
	AVP_EPC_Operator_Determined_Barring					= 1425,
	AVP_EPC_Access_Restriction_Data						= 1426,
	AVP_EPC_APN_OI_Replacement							= 1427,
	AVP_EPC_All_APN_Configurations_Included_Indicator	= 1428,
	AVP_EPC_APN_Configuration_Profile					= 1429,
	AVP_EPC_APN_Configuration							= 1430,
	AVP_EPC_EPS_Subscribed_QoS_Profile					= 1431,
	AVP_EPC_VPLMN_Dynamic_Address_Allowed				= 1432,
	AVP_EPC_STN_SR										= 1433,
	AVP_EPC_Alert_Reason								= 1434,
	AVP_EPC_AMBR										= 1435,
	AVP_EPC_CSG_Subscription_Data						= 1436,
	AVP_EPC_CSG_Id										= 1437,
	AVP_EPC_PDN_Gw_Allocation_Type						= 1438,
	AVP_EPC_Expiration_Date								= 1439,
	AVP_EPC_RAT_Frequency_Selection_Priority_ID			= 1440,
	AVP_EPC_IDA_Flags									= 1441,
	AVP_EPC_PUA_Flags									= 1442,
	AVP_EPC_NOR_Flags									= 1443,
	AVP_EPC_User_Id										= 1444,
	AVP_EPC_Equipment_Status							= 1445,
	AVP_EPC_Regional_Subscription_Zone_Code				= 1446,
	AVP_EPC_RAND										= 1447,
	AVP_EPC_XRES										= 1448,
	AVP_EPC_AUTN										= 1449,
	AVP_EPC_KASME										= 1450,
	
	AVP_EPC_Trace_Collection_Entity						= 1452,
	AVP_EPC_Kc											= 1453,
	AVP_EPC_SRES										= 1454,

	AVP_EPC_PDN_Type									= 1456,
	AVP_EPC_Roaming_Restricted_Due_To_Unsupported_Feature = 1457,
	AVP_EPC_Trace_Data									= 1458,
	AVP_EPC_Trace_Reference								= 1459,
	
	AVP_EPC_Trace_Depth									= 1462,
	AVP_EPC_Trace_NE_Type_List							= 1463,
	AVP_EPC_Trace_Interface_List						= 1464,
	AVP_EPC_Trace_Event_List							= 1465,
	AVP_EPC_OMC_Id										= 1466,
	AVP_EPC_GPRS_Subscription_Data						= 1467,
	AVP_EPC_Complete_Data_List_Included_Indicator		= 1468,
	AVP_EPC_PDP_Context									= 1469,
	AVP_EPC_PDP_Type									= 1470,
	AVP_EPC_3GPP2_MEID									= 1471,
	AVP_EPC_Specific_APN_Info							= 1472,
	AVP_EPC_LCS_Info									= 1473,
	AVP_EPC_GMLC_Number									= 1474,
	AVP_EPC_LCS_Privacy_Exception						= 1475,
	AVP_EPC_SS_Code										= 1476,
	AVP_EPC_SS_Status									= 1477,
	AVP_EPC_Notification_To_UE_User						= 1478,
	AVP_EPC_External_Client								= 1479,
	AVP_EPC_Client_Identity								= 1480,
	AVP_EPC_GMLC_Restriction							= 1481,
	AVP_EPC_PLMN_Client									= 1482,
	AVP_EPC_Service_Type								= 1483,
	AVP_EPC_Sevice_Type_Identity						= 1484,
	AVP_EPC_MO_LR										= 1485,
	AVP_EPC_Teleservice_List							= 1486,
	AVP_EPC_TS_Code										= 1487,
	AVP_EPC_Call_Barring_Infor_List						= 1488,
	AVP_EPC_SGSN_Number									= 1489,
	AVP_EPC_IDR_Flags									= 1490,
	AVP_EPC_ICS_Indicator								= 1491,
	AVP_EPC_IMS_Voice_Over_PS_Sessions_Supported		= 1492,
	AVP_EPC_Homogenous_Support_of_IMS_Over_PS_Sessions	= 1493,
	AVP_EPC_Last_UE_Activity_Time						= 1494,

/** TS 29.273  */	
	
	AVP_EPC_Non_3GPP_User_Data							= 1500,
	AVP_EPC_Non_3GPP_IP_Access							= 1501,
	AVP_EPC_Non_3GPP_IP_Access_APN						= 1502,
	AVP_EPC_ANID										= 1504,
	AVP_EPC_Trace_Info									= 1505,
	
/** TS 32.299  */
	
	AVP_EPC_Client_Address								= 2018,
	AVP_EPC_Carrier_Select_Routing_Information			= 2023,
	AVP_EPC_Associated_Party_Address					= 2035,
	AVP_EPC_SDP_Type									= 2036,
	AVP_EPC_Change_Condition							= 2037,
	AVP_EPC_Change_Time									= 2038,
	AVP_EPC_Service_Data_Container						= 2040,
	AVP_EPC_Time_First_Usage							= 2043,
	AVP_EPC_Time_Last_Usage								= 2044,
	AVP_EPC_Time_Usage									= 2045,
	AVP_EPC_Accumulated_Cost							= 2052,
	AVP_EPC_AoC_Cost_Information						= 2053,
	AVP_EPC_AoC_Information								= 2054,
	AVP_EPC_AoC_Request_Type							= 2055,
	AVP_EPC_Local_Sequence_Number						= 2063,
	AVP_EPC_Charging_Characteristics_Selection_Mode		= 2066,
	AVP_EPC_Application_Server_ID						= 2101,
	AVP_EPC_Application_Service_Type					= 2102,
	AVP_EPC_Application_Session_ID						= 2103,
	AVP_EPC_Content_ID									= 2116,
	AVP_EPC_Content_Provide_ID							= 2117,
	AVP_EPC_SIP_Request_Timestamp_Fraction				= 2301,
	AVP_EPC_SIP_Response_Timestamp_Fraction				= 2302,	
	AVP_EPC_Account_Expiration							= 2309,
	AVP_EPC_AoC_Cost_Format								= 2310,
	AVP_EPC_AoC_Service									= 2311,
	AVP_EPC_AoC_Service_Obligatory_Type					= 2312,
	AVP_EPC_AoC_Service_Type							= 2313,
	AVP_EPC_AoC_Subscription_Information				= 2314,
	AVP_EPC_Outgoing_Session_Id							= 2320,
	
	
	
/** 2400 to 2407 reserved for TS29.173											*/

	AVP_EPC_GMLC_Address								= 2405,
	
	
	
/** Not yet allocated */	

	AVP_EPC_PDN_Gw_Address								= 42002, 
	AVP_EPC_PDN_Gw_Name									= 42003, 
	AVP_EPC_PDN_Gw_Identity								= 42004, 
	AVP_EPC_QoS_Profile_Name							= 42005, 
	AVP_EPC_GG_Enforce									= 42006, 
	AVP_EPC_GG_IP										= 42007, 
	AVP_EPC_UE_Locator									= 42008, 
	AVP_EPC_UE_Locator_Id_Group							= 42009, 
};

/** Flow-Usage AVP */
enum {
	AVP_EPC_Flow_Usage_No_Information					= 0,
	AVP_EPC_Flow_Usage_Rtcp								= 1,
	AVP_EPC_Flow_Usage_AF_Signaling						= 2,

};

/* 3GPP TS 29.212*/
enum {
	AVP_EPC_Metering_Method_Duration			=0,
	AVP_EPC_Metering_Method_Volume				=1,
	AVP_EPC_Metering_Method_Duration_Volume		=2
};

enum {
	AVP_EPC_Offline_Disable 		=0,
	AVP_EPC_Offline_Enable 			=1
};


enum {
	AVP_EPC_Online_Disable 			=0,
	AVP_EPC_Online_Enable 			=1
};

enum {
	AVP_EPC_Reporting_Level_Serving_Identifier 	=0,
	AVP_EPC_Reporting_Level_Rating_Group		=1
};

enum {
	AVP_EPC_PCC_Rule_Status_Active					=0,
	AVP_EPC_PCC_Rule_Status_Inactive				=1,
	AVP_EPC_PCC_Rule_Status_Temporarily_Inactive 	=2
};
enum {
	AVP_EPC_Bearer_Usage_General 		=0,
	AVP_EPC_Bearer_Usage_IMS_Signaling 	=1
};
enum {
	AVP_EPC_Bearer_Operation_Termination 		=0,
	AVP_EPC_Bearer_Operation_Establishment 		=1,
	AVP_EPC_Bearer_Operation_Modification		=2
};
enum {
	AVP_EPC_Bearer_Control_Mode_UE_Only			=0,
	AVP_EPC_Bearer_Control_Mode_Reserved		=1,
	AVP_EPC_Bearer_Control_Mode_UE_NW			=2
};
enum {
	AVP_EPC_Network_Request_Support_Not_Supported			=0,
	AVP_EPC_Network_Request_Support_Supported				=1
};

/** IP-CAN Type TS 29.212 */ 
enum {
	AVP_EPC_IPCAN_Type_3GPP_GPRS 	= 0,
	AVP_EPC_IPCAN_Type_DOCSIS 		= 1,
	AVP_EPC_IPCAN_Type_xDSL 		= 2,
	AVP_EPC_IPCAN_Type_WiMAX		= 3,
	AVP_EPC_IPCAN_Type_3GPP2		= 4,
	AVP_EPC_IPCAN_Type_3GPP_EPS		= 5
};

enum {
	AVP_EPC_QoS_Negotiation_No			=0,
	AVP_EPC_QoS_Negotiation_Supported	=1
};

enum {
	AVP_EPC_QoS_Upgrade_No				=0,
	AVP_EPC_QoS_Upgrade_Supported		=1
};

enum {
	AVP_EPC_RAT_Type_WLAN				=0,
	AVP_EPC_RAT_Type_UTRAN				=1000,
	AVP_EPC_RAT_Type_GERAN				=1001,
	AVP_EPC_RAT_Type_GAN				=1002,
	AVP_EPC_RAT_Type_HSPA_Evolution		=1003,
	AVP_EPC_RAT_Type_EUTRAN				=1004,
	AVP_EPC_RAT_Type_CDMA2000_1X		=2000,
	AVP_EPC_RAT_Type_HRPD				=2001,
	AVP_EPC_RAT_Type_UMB				=2002 //deprecated
};


enum {
	AVP_EPC_Rule_Failure_Code_Unknown_Rule_Name				=1,
	AVP_EPC_Rule_Failure_Code_Rating_Group_Error			=2,
	AVP_EPC_Rule_Failure_Code_Service_Identifier_Error		=3,
	AVP_EPC_Rule_Failure_Code_GW_PCEF_Malfunction			=4,
	AVP_EPC_Rule_Failure_Code_Resources_Limitation			=5,
	AVP_EPC_Rule_Failure_Code_Max_Nr_Bearers_Reached		=6,
	AVP_EPC_Rule_Failure_Code_Unkown_Bearer_Id				=7,
	AVP_EPC_Rule_Failure_Code_Missing_Bearer_Id				=8,
	AVP_EPC_Rule_Failure_Code_Missing_Flow_Description		=9,
	AVP_EPC_Rule_Failure_Code_Resource_Allocation_Failure	=10,
	AVP_EPC_Rule_Failure_Code_Unsuccessful_QoS_Validation	=11
};

enum {
	AVP_EPC_Session_Release_Cause_Unespecified				=0,
	AVP_EPC_Session_Release_Cause_UE_Subscription			=1,
	AVP_EPC_Session_Release_Cause_Server_Resources			=2
};

enum {
	AVP_EPC_PreEmption_Capability_Enabled			=0,
	AVP_EPC_PreEmption_Capability_Disabled			=1
};

enum {
	AVP_EPC_PreEmption_Vulnerability_Enabled			=0,
	AVP_EPC_PreEmption_Vulnerability_Disabled			=1
};

/** Event-Trigger TS 29.212*/
enum {	
	AVP_EPC_Event_Trigger_SGSN_CHANGE 							=0, 
	AVP_EPC_Event_Trigger_QOS_CHANGE 							=1,
	AVP_EPC_Event_Trigger_RAT_CHANGE 							=2,	
	AVP_EPC_Event_Trigger_TFT_CHANGE 							=3,
	AVP_EPC_Event_Trigger_PLMN_CHANGE 							=4,
	AVP_EPC_Event_Trigger_LOSS_OF_BEARER 						=5,
	AVP_EPC_Event_Trigger_RECOVERY_OF_BEARER					=6,
	AVP_EPC_Event_Trigger_IP_CAN_CHANGE							=7,
	AVP_EPC_Event_Trigger_GW_PCEF_MALFUNCTION 					=8, //Release 7
	AVP_EPC_Event_Trigger_RESOURCES_LIMITATION 					=9, //Release 7
	AVP_EPC_Event_Trigger_MAX_NR_BEARERS_REACHED 				=10, //Release 7
	AVP_EPC_Event_Trigger_QOS_CHANGE_EXCEEDING_AUTHORIZATION	=11,
	AVP_EPC_Event_Trigger_RAI_CHANGE							=12, 
	AVP_EPC_Event_Trigger_USER_LOCATION_CHANGE 					=13,
	AVP_EPC_Event_Trigger_NO_EVENT_TRIGGER						=14,	 
	AVP_EPC_Event_Trigger_OUT_OF_CREDIT							=15, 
	AVP_EPC_Event_Trigger_RELLOCATION_OF_CREDIT					=16,
	AVP_EPC_Event_Trigger_REVALIDATION_TIMEOUT					=17,
	AVP_EPC_Event_Trigger_IP_ADDRESS_ALLOCATE					=18,
	AVP_EPC_Event_Trigger_IP_ADDRESS_RELEASE					=19,
	AVP_EPC_Event_Trigger_DEFAULT_EPS_BEARER_QOS_CHANGE			=20,
	AVP_EPC_Event_Trigger_AN_GW_CHANGE							=21,
	AVP_EPC_Event_Trigger_Successful_Resource_Allocation		=22,
	AVP_EPC_Event_Trigger_Resource_Modification_Request			=23,
	//AVP_EPC_Event_Trigger_PGW_Trace_Control					=24,
	//AVP_EPC_Event_Trigger_UE_Time_Zone_Change					= 25,
};

enum {
	AVP_EPC_Packet_Filter_Operation_Deletion					=0,
	AVP_EPC_Packet_Filter_Operation_Addition					=1,
	AVP_EPC_Packet_Filter_Operation_Modification				=2
};

enum {
	AVP_EPC_Resource_Allocation_Notification_Enable_Notification = 0
};

enum {
	AVP_EPC_Session_Linking_Indicator_Immediate					= 0,
	AVP_EPC_Session_Linking_Indicator_Deferred					= 1,
};

/* This extends the AVP_IMS_Data_Reference_* for Sh to Sp which is not yet standardized */
enum {
	AVP_EPC_Data_Reference_Subscription_Id_MSISDN				= 101,
	AVP_EPC_Data_Reference_Subscription_Id_IMSI					= 102,
	AVP_EPC_Data_Reference_Subscription_Id_IMPU					= 103,
	AVP_EPC_Data_Reference_Subscription_Data					= 104,
	AVP_EPC_Data_Reference_APN_Configuration					= 105,
};

/* from RFC4006 */
enum {
	AVP_EPC_Subscription_Id_Type_End_User_E164					= 0,
	AVP_EPC_Subscription_Id_Type_End_User_IMSI					= 1,
	AVP_EPC_Subscription_Id_Type_End_User_SIP_URI				= 2,
	AVP_EPC_Subscription_Id_Type_End_User_NAI					= 3,	
	AVP_EPC_Subscription_Id_Type_End_User_Private				= 4,	
};

enum {
	RC_EPC_DIAMETER_QOS_RULE_EVENT								= 5145,
	RC_EPC_DIAMETER_BEARER_EVENT								= 5146,
	RC_EPC_DIAMETER_PCC_RULE_EVENT								= 5142,
	RC_EPC_DIAMETER_ERROR_INITIAL_PARAMETERS					= 5140,
	RC_EPC_DIAMETER_ERROR_TRAFFIC_MAPPING_INFO_REJECTED			= 5144,
	RC_EPC_DIAMETER_ERROR_CONFLICTING_REQUEST					= 5147
};

enum {
	RC_EPC_INVALID_SERVICE_INFORMATION							= 5061,
	RC_EPC_FILTER_RESTRICTIONS									= 5062,
	RC_EPC_REQUESTED_SERVICE_NOT_AUTHORIZED						= 5063
};
/**29.214 */
enum {
	AVP_EPC_Specific_Action_Charging_Correlation_Exchange			= 1,
	AVP_EPC_Specific_Action_Indication_of_Loss_of_Bearer			= 2,
	AVP_EPC_Specific_Action_Indication_of_Recovery_of_Bearer		= 3,
	AVP_EPC_Specific_Action_Indication_of_Release_of_Bearer			= 4,
	AVP_EPC_Specific_Action_Indication_of_Establishment_of_Bearer	= 5,
	AVP_EPC_Specific_Action_IPCAN_Change							= 6,
	AVP_EPC_Specific_Action_Indication_of_Out_of_Credit				= 7,
	AVP_EPC_Specific_Action_Indication_of_Succ_Resource_Allocation			= 8,
	AVP_EPC_Specific_Action_Indication_of_Failed_Resource_Allocation		= 9,
	AVP_EPC_Specific_Action_Indication_of_Generic_Gateway_Change			= 10,
};

enum {
	AVP_EPC_Service_Info_Status_Final_Service_Information			= 0,
	AVP_EPC_Service_Info_Status_Preliminary_Service_Information		= 1,
};

/* TS 29.214 */
enum {
	AVP_EPC_Abort_Cause_Bearer_Released								= 0,
	AVP_EPC_Abort_Cause_Insufficient_Server_Resources				= 1,
	AVP_ECP_Abort_Cause_Insufficient_Bearer_Resources				= 2
};


/* TS 29.272 */

enum {
	AVP_EPC_ULR_Flags_Single_Registration_Indication				= 1<<0,
	AVP_EPC_ULR_Flags_S6a_Indicator									= 1<<1,	
	AVP_EPC_ULR_Flags_Skip_Subscriber_Data							= 1<<2,			
	AVP_EPC_ULR_Flags_GPRS_Subscription_Data_Indicator				= 1<<3,			
	AVP_EPC_ULR_Flags_Node_Type_Indicator							= 1<<4,			
	AVP_EPC_ULR_Flags_Initial_Attach_Indicator						= 1<<5,			
	AVP_EPC_ULR_Flags_PS_LCS_Not_Supported_By_UE					= 1<<6,			
};

enum {
	AVP_EPC_ULA_Flags_Separation_Indication							= 1<<0,			
};

enum {
	AVP_EPC_Feature_List_ODB_all_APN								= 1<<0,
	AVP_EPC_Feature_List_ODB_HPLMN_APN								= 1<<1,	
	AVP_EPC_Feature_List_ODB_VPLMN_APN								= 1<<2,			
	AVP_EPC_Feature_List_ODB_all_OG									= 1<<3,			
	AVP_EPC_Feature_List_ODB_all_InternationalOG					= 1<<4,			
	AVP_EPC_Feature_List_ODB_all_InternationalOGNoToHPLMN_Country	= 1<<5,			
	AVP_EPC_Feature_List_ODB_all_InterzonalIOG						= 1<<6,			
	AVP_EPC_Feature_List_ODB_all_InterzonalIOGNotToHPLMN_Country	= 1<<7,			
	AVP_EPC_Feature_List_ODB_all_InterzonalIOGAndInternationalOGNotToHPLMN_Country = 1<<8,			
	AVP_EPC_Feature_List_RegSub										= 1<<9,			
	AVP_EPC_Feature_List_Trace										= 1<<10,			
	AVP_EPC_Feature_List_LCS_all_PrivExcep							= 1<<11,			
	AVP_EPC_Feature_List_LCS_Universal								= 1<<12,			
	AVP_EPC_Feature_List_LCS_CallSessionRelated						= 1<<13,			
	AVP_EPC_Feature_List_LCS_CallSessionUnrelated					= 1<<14,			
	AVP_EPC_Feature_List_LCS_PLMNOperator							= 1<<15,			
	AVP_EPC_Feature_List_LCS_Service_Type							= 1<<16,			
	AVP_EPC_Feature_List_LCS_all_MOLR_SS							= 1<<17,			
	AVP_EPC_Feature_List_LCS_BasicShelfLocation						= 1<<18,			
	AVP_EPC_Feature_List_LCS_AutonomousSelfLocation					= 1<<19,			
	AVP_EPC_Feature_List_LCS_TransferToThirdParty					= 1<<20,			
	AVP_EPC_Feature_List_SM_Mo_PP									= 1<<21,
	AVP_EPC_Feature_List_Barring_OutgoingCalls						= 1<<22,			
	AVP_EPC_Feature_List_BAOC										= 1<<23,			
	AVP_EPC_Feature_List_BOIC										= 1<<24,			
	AVP_EPC_Feature_List_BOICExHC									= 1<<25,			
	AVP_EPC_Feature_List_T_ADSDataRetrieval							= 1<<26,			
};

enum {
	AVP_EPC_Network_Access_Mode_Packed_And_Circuit					= 0,
	AVP_EPC_Network_Access_Mode_Reserved							= 1,
	AVP_EPC_Network_Access_Mode_Only_Packet							= 2,
};
enum {
	AVP_EPC_HPLMN_OBD_HPLM_Specific_Barring_Type_1					= 1<<0,
	AVP_EPC_HPLMN_OBD_HPLM_Specific_Barring_Type_2					= 1<<1,
	AVP_EPC_HPLMN_OBD_HPLM_Specific_Barring_Type_3					= 1<<2,
	AVP_EPC_HPLMN_OBD_HPLM_Specific_Barring_Type_4					= 1<<3,
};

enum {
	AVP_EPC_Cancellation_Type_MME_Update_Procedure					= 0,
	AVP_EPC_Cancellation_Type_SGSN_Update_Procedure					= 1,
	AVP_EPC_Cancellation_Type_Subscription_Withdrawal				= 2,
	AVP_EPC_Cancellation_Type_Update_Procedure_IWF					= 3,
	AVP_EPC_Cancellation_Type_Initial_Attach_Procedure				= 4,
};

enum {
	AVP_EPC_DSR_Flags_Regional_Subscription_Withdrawal				= 1<<0,
	AVP_EPC_DSR_Flags_Complete_APN_Configuration_Profile_Withdrawal	= 1<<1,	
	AVP_EPC_DSR_Flags_Subscribed_Charging_Characteristics_Withdrawal= 1<<2,			
	AVP_EPC_DSR_Flags_PDN_Subscription_Contexts_Withdrawal			= 1<<3,			
	AVP_EPC_DSR_Flags_STN_SR										= 1<<4,			
	AVP_EPC_DSR_Flags_Complete_PDP_Context_List_Withdrawal			= 1<<5,			
	AVP_EPC_DSR_Flags_PDP_Contexts_Withdrawal						= 1<<6,			
	AVP_EPC_DSR_Flags_Roaming_Restricted_Due_To_Unsupported_Feature	= 1<<7,			
	AVP_EPC_DSR_Flags_Trace_Data_Withdrawal							= 1<<8,			
	AVP_EPC_DSR_Flags_CSG_Deleted									= 1<<9,			
	AVP_EPC_DSR_Flags_APN_OI_Replacement							= 1<<10,			
	AVP_EPC_DSR_Flags_GMLC_List_Withdrawal							= 1<<11,			
	AVP_EPC_DSR_Flags_LCS_Withdrawal								= 1<<12,			
	AVP_EPC_DSR_Flags_SMS_Withdrawal								= 1<<13,			
};

enum {
	AVP_EPC_DSA_Flags_Network_Node_Area_Restricted					= 1<<0,
};

enum {
	AVP_EPC_Subscriber_Status_Service_Granted						= 0,
	AVP_EPC_Subscriber_Status_Operator_Determined_Barring			= 1,
};

enum {
	AVP_EPC_Operator_Determined_Barring_All_Packet_Oriented_Services_Barred						= 1<<0,
	AVP_EPC_Operator_Determined_Barring_Roamer_Access_HPLMN_AP_Barred							= 1<<1,	
	AVP_EPC_Operator_Determined_Barring_Roamer_Access_To_VPLMN_AP_Barred						= 1<<2,			
	AVP_EPC_Operator_Determined_Barring_Outgoing_Calls											= 1<<3,			
	AVP_EPC_Operator_Determined_Barring_Outgoing_International_Calls							= 1<<4,			
	AVP_EPC_Operator_Determined_Barring_Outgoing_International_Calls_Except_To_HPLMN_Country	= 1<<5,			
	AVP_EPC_Operator_Determined_Barring_Outgoing_Inter_zonal_Calls								= 1<<6,			
	AVP_EPC_Operator_Determined_Barring_Outgoing_Inter_zonal_Calls_Except_To_HPLMN_Country		= 1<<7,			
	AVP_EPC_Operator_Determined_Barring_Outgoing_International_Calls_Except_To_HPLMN_Country_and_All_Inter_zonal_Calls = 1<<8,			
};

enum {
	AVP_EPC_Access_Restriction_Data_UTRAN_Not_Allowed					= 1<<0,
	AVP_EPC_Access_Restriction_Data_GERAN_Not_Allowed					= 1<<1,	
	AVP_EPC_Access_Restriction_Data_GAN_Not_Allowed 					= 1<<2,			
	AVP_EPC_Access_Restriction_Data_I_HSPA_Evolution_Not_Allowed		= 1<<3,			
	AVP_EPC_Access_Restriction_Data_E_UTRA_Not_Allowed					= 1<<4,			
	AVP_EPC_Access_Restriction_Data_HO_To_Non_3GPP_Access_Not_Allowed 	= 1<<5,		
};

enum {
	AVP_EPC_All_APN_Configurations_Included_Indicator_All_Included				= 0,
	AVP_EPC_All_APN_Configurations_Included_Indicator_Modified_Added_Included	= 1,
};

enum {
	AVP_EPC_VPLMN_Dynamic_Address_Allowed_Not_Allowed				= 0,
	AVP_EPC_VPLMN_Dynamic_Address_Allowed_Allowed					= 1,
};

enum {
	AVP_EPC_PDN_Gw_Allocation_Type_Static							= 0,
	AVP_EPC_PDN_Gw_Allocation_Type_Dynamic							= 1,
};

enum {
	AVP_EPC_IDA_Flags_Network_Node_Area_Restricted					= 1<<0,
};

enum {
	AVP_EPC_PUA_Flags_Freeze_M_TMSI									= 1<<0,
	AVP_EPC_PUA_Flags_Freeze_P_TMSI									= 1<<1,
};

enum {
	AVP_EPC_NOR_Flags_Single_Registration_Indication				= 1<<0,
	AVP_EPC_NOR_Flags_SGSN_Area_Restricted							= 1<<1,
	AVP_EPC_NOR_Flags_Ready_For_SM									= 1<<2,
	AVP_EPC_NOR_Flags_UE_Reachable									= 1<<3,
	AVP_EPC_NOR_Flags_Delete_All_APN_and_PDN_Gw_Identity_Pairs		= 1<<4,
};

enum {
	AVP_EPC_Equipment_Status_Whitelisted							= 0,
	AVP_EPC_Equipment_Status_Blacklisted							= 1,
	AVP_EPC_Equipment_Status_Greylisted								= 2,
};

enum {
	AVP_EPC_PDN_Type_IPv4											= 0,
	AVP_EPC_PDN_Type_IPv6											= 1,
	AVP_EPC_PDN_Type_IPv4v6											= 2,
	AVP_EPC_PDN_Type_IPv4_or_IPv6									= 3,
			
};

enum {
	AVP_EPC_Complete_Data_List_Included_Indicator_All_Included				= 0,
	AVP_EPC_Complete_Data_List_Included_Indicator_Modified_Added_Included	= 1,
};

enum {
	AVP_EPC_Alert_Reason_UE_Present									= 0,
	AVP_EPC_Alert_Reason_UE_Memory_Available						= 1,
};

enum {
	AVP_EPC_Notification_To_UE_User_Notify_Location_Allowed					 		= 0,
	AVP_EPC_Notification_To_UE_User_Notify_And_Verify_Allowed_If_No_Response 		= 1,
	AVP_EPC_Notification_To_UE_User_Notify_And_Verify_Not_Allowed_If_No_Response 	= 2,
	AVP_EPC_Notification_To_UE_User_Location_Not_Allowed					 		= 3,
};

enum {
	AVP_EPC_GMLC_Restriction_GMLC_List								= 0,
	AVP_EPC_GMLC_Restriction_Home_Country							= 1,
};

enum {
	AVP_EPC_PLMN_Client_Broadcast_Service							= 0,
	AVP_EPC_PLMN_Client_O_And_M_HPLMN								= 1,
	AVP_EPC_PLMN_Client_O_And_M_VPLMN								= 2,
	AVP_EPC_PLMN_Client_Anonymous_Location							= 3,
	AVP_EPC_PLMN_Client_Target_UE_Subscribed_Services				= 4,		
};

enum {
	AVP_EPC_IDR_Flags_UE_Reachability_Request						= 1<<0,
	AVP_EPC_IDR_Flags_T_ADS_Data_Request							= 1<<1,
	AVP_EPC_IDR_Flags_EPS_User_State_Request						= 1<<2,
	AVP_EPC_IDR_Flags_EPS_Location_Information_Request				= 1<<3,
	AVP_EPC_IDR_Flags_Current_Location_Request						= 1<<4,
};

enum {
	AVP_EPC_ICS_Indicator_False										= 0,
	AVP_EPC_ICS_Indicator_True										= 1,
};

enum {
	AVP_EPC_IMS_Voice_Over_PS_Sessions_Supported_Not_Supported		= 0,
	AVP_EPC_IMS_Voice_Over_PS_Sessions_Supported_Supported			= 1,
};

enum {
	AVP_EPC_Homogenous_Support_of_IMS_Over_PS_Sessions_Not_Supported	= 0,
	AVP_EPC_Homogenous_Support_of_IMS_Over_PS_Sessions_Supported		= 1,
};

enum {
	AVP_EPC_User_State_Detached										= 0,
	AVP_EPC_User_State_Attached_Not_Reachable_For_Paging			= 1,
	AVP_EPC_User_State_Attached_Reachable_For_Paging				= 2,
	AVP_EPC_User_State_Connected_Not_Reachable_For_Paging			= 3,
	AVP_EPC_User_State_Connected_Reachable_For_Paging				= 4,
	AVP_EPC_User_State_Network_Determined_Not_Reachable				= 5,
};

enum {
	AVP_EPC_Current_Location_Retrieved_Active_Location_Retrieval	= 0			
};

enum {
	AVP_EPC_Role_Of_Node_Originating_Role	= 0,
	AVP_EPC_Role_Of_Node_Terminating_Role	= 1,
};

enum {
	AVP_EPC_Node_Functionality_S_CSCF	= 0,
	AVP_EPC_Node_Functionality_P_CSCF	= 1,
	AVP_EPC_Node_Functionality_I_CSCF	= 2,
	AVP_EPC_Node_Functionality_MRFC		= 3,
	AVP_EPC_Node_Functionality_MGCF		= 4,
	AVP_EPC_Node_Functionality_BGCF		= 5,
	AVP_EPC_Node_Functionality_AS		= 6,
	AVP_EPC_Node_Functionality_IBCF		= 7,
	AVP_EPC_Node_Functionality_S_GW		= 8,
	AVP_EPC_Node_Functionality_P_GW		= 9,
	AVP_EPC_Node_Functionality_HSGW		= 10,	
};

enum {
	AVP_EPC_Cause_Code_Normal_End_Of_Session		= 0,
	AVP_EPC_Cause_Code_Successful_Transaction		= -1,
	AVP_EPC_Cause_Code_End_Of_Subscribe_Dialog		= -2,
	AVP_EPC_Cause_Code_2xx_Final_Response			= -200, /**< use the actual response code */
	AVP_EPC_Cause_Code_3xx_Redirection				= -300, /**< use the actual response code */
	AVP_EPC_Cause_Code_End_Of_REGISTER_Dialog		= -3,
	AVP_EPC_Cause_Code_Unspecified_Error			= 1,
	AVP_EPC_Cause_Code_4xx_Request_Failure			= 400, /**< use the actual response code */
	AVP_EPC_Cause_Code_5xx_Request_Failure			= 500, /**< use the actual response code */
	AVP_EPC_Cause_Code_6xx_Global_Failure			= 600, /**< use the actual response code */
	AVP_EPC_Cause_Code_Unsuccessful_Session_Setup	= 2,
	AVP_EPC_Cause_Code_Internal_Error				= 3,
};

enum {
	AVP_EPC_Media_Initiator_Flag_Called_Party		= 0,
	AVP_EPC_Media_Initiator_Flag_Calling_Party		= 1,
	AVP_EPC_Media_Initiator_Flag_Unknown			= 2,	
};

enum {
	AVP_EPC_Originator_Calling_Party		= 0,
	AVP_EPC_Originator_Called_Party			= 1,
};

enum {
	AVP_EPC_SDP_Type_SDP_Offer				= 0,
	AVP_EPC_SDP_Type_SDP_Answer				= 1,
};

enum {
	AVP_EPC_Non_3GPP_IP_Access_Non_3GPP_Subscription_Allowed	= 0,
	AVP_EPC_Non_3GPP_IP_Access_Non_3GPP_Subscription_Barred		= 1,
};

enum {
	AVP_EPC_Non_3GPP_IP_Access_APN_Non_3GPS_APNS_Enable			= 0,
	AVP_EPC_Non_3GPP_IP_Access_APN_Non_3GPS_APNS_Disable		= 1,
};

/* RFC4006 */
enum {
	AVP_EPC_User_Equipment_Info_Type_IMEISV							= 0,
	AVP_EPC_User_Equipment_Info_Type_MAC							= 1,
	AVP_EPC_User_Equipment_Info_Type_EUI64							= 2,
	AVP_EPC_User_Equipment_Info_Type_MODIFIED_EUI64					= 3,
};

#endif /*DIAMETER_EPC_H_*/
