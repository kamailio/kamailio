/* Generated from ISUP-C-ParserGenerator. Do not modify */
#pragma once

#include <stdint.h>
#define ISUPEndOfOptionalParameters 0 /* End of optional parameters spec: 3.20*/
#define ISUPCallReference 1 /* Call reference spec: 3.8*/
#define ISUPTransmissionMediumRequirement 2 /* Transmission medium requirement spec: 3.54*/
#define ISUPAccessTransport 3 /* Access transport spec: 3.3*/
#define ISUPCalledPartyNumber 4 /* Called party number spec: 3.9*/
#define ISUPSubsequentNumber 5 /* Subsequent number spec: 3.51*/
#define ISUPNatureOfConnectionIndicators 6 /* Nature of connection indicators spec: 3.35*/
#define ISUPForwardCallIndicators 7 /* Forward call indicators spec: 3.23*/
#define ISUPOptionalForwardCallIndicators 8 /* Optional forward call indicators spec: 3.38*/
#define ISUPCallingPartysCategory 9 /* Calling party's category spec: 3.11*/
#define ISUPCallingPartyNumber 10 /* Calling party number spec: 3.10*/
#define ISUPRedirectingNumber 11 /* Redirecting number spec: 3.44*/
#define ISUPRedirectionNumber 12 /* Redirection number spec: 3.46*/
#define ISUPConnectionRequest 13 /* Connection request spec: 3.17*/
#define ISUPInformationRequestIndicators 14 /* Information request indicators spec: 3.29*/
#define ISUPInformationIndicators 15 /* Information indicators spec: 3.28*/
#define ISUPContinuityIndicators 16 /* Continuity indicators spec: 3.18*/
#define ISUPBackwardCallIndicators 17 /* Backward call indicators spec: 3.5*/
#define ISUPCauseIndicators 18 /* Cause indicators spec: 3.12*/
#define ISUPRedirectionInformation 19 /* Redirection information spec: 3.45*/
#define ISUPCircuitGroupSupervisionMessageType 21 /* Circuit group supervision message type spec: 3.13*/
#define ISUPRange 22 /* Range spec: 3.43b*/
#define ISUPRangeAndStatus 22 /* Range and status spec: 3.43*/
#define ISUPFacilityIndicator 24 /* Facility indicator spec: 3.22*/
#define ISUPClosedUserGroupInterlockCode 26 /* Closed user group interlock code spec: 3.15*/
#define ISUPUserServiceInformation 29 /* User service information spec: 3.57*/
#define ISUPSignallingPointCode 30 /* Signalling point code spec: 3.50*/
#define ISUPUserToUserInformation 32 /* User-to-user information spec: 3.61*/
#define ISUPConnectedNumber 33 /* Connected number spec: 3.16*/
#define ISUPSuspendResumeIndicators 34 /* Suspend/resume indicators spec: 3.52*/
#define ISUPTransitNetworkSelection 35 /* Transit network selection spec: 3.53*/
#define ISUPEventInformation 36 /* Event information spec: 3.21*/
#define ISUPCircuitAssignmentMap 37 /* Circuit assignment map spec: 3.69*/
#define ISUPCircuitStateIndicator 38 /* Circuit state indicator spec: 3.14*/
#define ISUPAutomaticCongestionLevel 39 /* Automatic congestion level spec: 3.4*/
#define ISUPOriginalCalledNumber 40 /* Original called number spec: 3.39*/
#define ISUPOptionalBackwardCallIndicators 41 /* Optional backward call indicators spec: 3.37*/
#define ISUPUserToUserIndicators 42 /* User-to-user indicators spec: 3.60*/
#define ISUPOriginationISCPointCode 43 /* Origination ISC point code spec: 3.40*/
#define ISUPGenericNotificationIndicator 44 /* Generic notification indicator spec: 3.25*/
#define ISUPCallHistoryInformation 45 /* Call history information spec: 3.7*/
#define ISUPAccessDeliveryInformation 46 /* Access delivery information spec: 3.2*/
#define ISUPNetworkSpecificFacility 47 /* Network specific facility spec: 3.36*/
#define ISUPUserServiceInformationPrime 48 /* User service information prime spec: 3.58*/
#define ISUPPropagationDelayCounter 49 /* Propagation delay counter spec: 3.42*/
#define ISUPRemoteOperations 50 /* Remote operations spec: 3.48*/
#define ISUPServiceActivation 51 /* Service activation spec: 3.49*/
#define ISUPUserTeleserviceInformation 52 /* User teleservice information spec: 3.59*/
#define ISUPTransmissionMediumUsed 53 /* Transmission medium used spec: 3.56*/
#define ISUPCallDiversionInformation 54 /* Call diversion information spec: 3.6*/
#define ISUPEchoControlInformation 55 /* Echo control information spec: 3.19*/
#define ISUPMessageCompatibilityInformation 56 /* Message compatibility information spec: 3.33*/
#define ISUPParameterCompatibilityInformation 57 /* Parameter compatibility information spec: 3.41*/
#define ISUPMLPPPrecedence 58 /* MLPP precedence spec: 3.34*/
#define ISUPMCIDRequestIndicators 59 /* MCID request indicators spec: 3.31*/
#define ISUPMCIDResponseIndicators 60 /* MCID response indicators spec: 3.32*/
#define ISUPHopCounter 61 /* Hop counter spec: 3.80*/
#define ISUPTransmissionMediumRequirementPrime 62 /* Transmission medium requirement prime spec: 3.55*/
#define ISUPLocationNumber 63 /* Location number spec: 3.30*/
#define ISUPRedirectionNumberRestriction 64 /* Redirection number restriction spec: 3.47*/
#define ISUPCallTransferReference 67 /* Call transfer reference spec: 3.65*/
#define ISUPLoopPreventionIndicators 68 /* Loop prevention indicators spec: 3.67*/
#define ISUPCallTransferNumber 69 /* Call transfer number spec: 3.64*/
#define ISUPCCSS 75 /* CCSS spec: 3.63*/
#define ISUPForwardGVNS 76 /* Forward GVNS spec: 3.66*/
#define ISUPBackwardGVNS 77 /* Backward GVNS spec: 3.62*/
#define ISUPRedirectCapability 78 /* Redirect capability spec: 3.96*/
#define ISUPNetworkManagementControls 91 /* Network management controls spec: 3.68*/
#define ISUPCorrelationId 101 /* Correlation id spec: 3.70*/
#define ISUPSCFId 102 /* SCF id spec: 3.71*/
#define ISUPCallDiversionTreatmentIndicators 110 /* Call diversion treatment indicators spec: 3.72*/
#define ISUPCalledINNumber 111 /* Called IN number spec: 3.73*/
#define ISUPCallOfferingTreatmentIndicators 112 /* Call offering treatment indicators spec: 3.74*/
#define ISUPChargedPartyIdentification 113 /* Charged party identification spec: 3.75*/
#define ISUPConferenceTreatmentIndicators 114 /* Conference treatment indicators spec: 3.76*/
#define ISUPDisplayInformation 115 /* Display information spec: 3.77*/
#define ISUPUIDActionIndicators 116 /* UID action indicators spec: 3.78*/
#define ISUPUIDCapabilityIndicators 117 /* UID capability indicators spec: 3.79*/
#define ISUPRedirectCounter 119 /* Redirect counter spec: 3.97*/
#define ISUPApplicationTransportParameter 120 /* Application transport parameter spec: 3.82*/
#define ISUPCollectCallRequest 121 /* Collect call request spec: 3.81*/
#define ISUPCCNRPossibleIndicator 122 /* CCNR possible indicator spec: 3.83*/
#define ISUPPivotCapability 123 /* Pivot capability spec: 3.84*/
#define ISUPPivotRoutingIndicators 124 /* Pivot routing indicators spec: 3.85*/
#define ISUPCalledDirectoryNumber 125 /* Called directory number spec: 3.86*/
#define ISUPOriginalCalledINNumber 127 /* Original called IN number spec: 3.87*/
#define ISUPCallingGeodeticLocation 129 /* Calling geodetic location spec: 3.88*/
#define ISUPGenericReference 130 /* Generic reference spec: 3.27*/
#define ISUPHTRInformation 130 /* HTR information spec: 3.89*/
#define ISUPNetworkRoutingNumber 132 /* Network routing number spec: 3.90*/
#define ISUPQoRCapability 133 /* QoR capability spec: 3.91*/
#define ISUPPivotStatus 134 /* Pivot status spec: 3.92*/
#define ISUPPivotCounter 135 /* Pivot counter spec: 3.93*/
#define ISUPPivotRoutingForwardInformation 136 /* Pivot routing forward information spec: 3.94*/
#define ISUPPivotRoutingBackwardInformation 137 /* Pivot routing backward information spec: 3.95*/
#define ISUPRedirectStatus 138 /* Redirect status spec: 3.98*/
#define ISUPRedirectForwardInformation 139 /* Redirect forward information spec: 3.99*/
#define ISUPRedirectBackwardInformation 140 /* Redirect backward information spec: 3.100*/
#define ISUPNumberPortabilityForwardInformation 141 /* Number portability forward information spec: 3.101*/
#define ISUPGenericNumber 192 /* Generic number spec: 3.26*/
#define ISUPGenericDigits 193 /* Generic digits spec: 3.24*/
struct isup_ie_fixed {
	const char *name;
	uint8_t type;
	uint8_t len;
};
struct isup_ie_variable {
	const char *name;
	uint8_t type;
	uint8_t min_len;
	uint8_t max_len;
};
struct isup_ie_optional {
	const char *name;
	uint8_t type;
	uint8_t min_len;
	uint8_t max_len;
};
struct isup_msg {
	const char      *name;

	const struct isup_ie_fixed *fixed_ies;
	const struct isup_ie_variable *variable_ies;
	int has_optional;
};
extern const struct isup_msg isup_msgs[256];
