/* Generated from ISUP-C-ParserGenerator. Do not modify */
#include "isup_generated.h"

static const struct isup_ie_fixed acm_fixed[] = {
	{
		.name = "ISUPBackwardCallIndicators",
		.type = ISUPBackwardCallIndicators,
		.len = 2,
	},
	{ 0, },
};
static const struct isup_ie_fixed cgb_fixed[] = {
	{
		.name = "ISUPCircuitGroupSupervisionMessageType",
		.type = ISUPCircuitGroupSupervisionMessageType,
		.len = 1,
	},
	{ 0, },
};
static const struct isup_ie_fixed cgba_fixed[] = {
	{
		.name = "ISUPCircuitGroupSupervisionMessageType",
		.type = ISUPCircuitGroupSupervisionMessageType,
		.len = 1,
	},
	{ 0, },
};
static const struct isup_ie_fixed cgu_fixed[] = {
	{
		.name = "ISUPCircuitGroupSupervisionMessageType",
		.type = ISUPCircuitGroupSupervisionMessageType,
		.len = 1,
	},
	{ 0, },
};
static const struct isup_ie_fixed cgua_fixed[] = {
	{
		.name = "ISUPCircuitGroupSupervisionMessageType",
		.type = ISUPCircuitGroupSupervisionMessageType,
		.len = 1,
	},
	{ 0, },
};
static const struct isup_ie_fixed con_fixed[] = {
	{
		.name = "ISUPBackwardCallIndicators",
		.type = ISUPBackwardCallIndicators,
		.len = 2,
	},
	{ 0, },
};
static const struct isup_ie_fixed cot_fixed[] = {
	{
		.name = "ISUPContinuityIndicators",
		.type = ISUPContinuityIndicators,
		.len = 1,
	},
	{ 0, },
};
static const struct isup_ie_fixed cpg_fixed[] = {
	{
		.name = "ISUPEventInformation",
		.type = ISUPEventInformation,
		.len = 1,
	},
	{ 0, },
};
static const struct isup_ie_fixed faa_fixed[] = {
	{
		.name = "ISUPFacilityIndicator",
		.type = ISUPFacilityIndicator,
		.len = 1,
	},
	{ 0, },
};
static const struct isup_ie_fixed far_fixed[] = {
	{
		.name = "ISUPFacilityIndicator",
		.type = ISUPFacilityIndicator,
		.len = 1,
	},
	{ 0, },
};
static const struct isup_ie_fixed frj_fixed[] = {
	{
		.name = "ISUPFacilityIndicator",
		.type = ISUPFacilityIndicator,
		.len = 1,
	},
	{ 0, },
};
static const struct isup_ie_fixed iam_fixed[] = {
	{
		.name = "ISUPNatureOfConnectionIndicators",
		.type = ISUPNatureOfConnectionIndicators,
		.len = 1,
	},
	{
		.name = "ISUPForwardCallIndicators",
		.type = ISUPForwardCallIndicators,
		.len = 2,
	},
	{
		.name = "ISUPCallingPartysCategory",
		.type = ISUPCallingPartysCategory,
		.len = 1,
	},
	{
		.name = "ISUPTransmissionMediumRequirement",
		.type = ISUPTransmissionMediumRequirement,
		.len = 1,
	},
	{ 0, },
};
static const struct isup_ie_fixed inf_fixed[] = {
	{
		.name = "ISUPInformationIndicators",
		.type = ISUPInformationIndicators,
		.len = 2,
	},
	{ 0, },
};
static const struct isup_ie_fixed inr_fixed[] = {
	{
		.name = "ISUPInformationRequestIndicators",
		.type = ISUPInformationRequestIndicators,
		.len = 2,
	},
	{ 0, },
};
static const struct isup_ie_fixed res_fixed[] = {
	{
		.name = "ISUPSuspendResumeIndicators",
		.type = ISUPSuspendResumeIndicators,
		.len = 1,
	},
	{ 0, },
};
static const struct isup_ie_fixed sus_fixed[] = {
	{
		.name = "ISUPSuspendResumeIndicators",
		.type = ISUPSuspendResumeIndicators,
		.len = 1,
	},
	{ 0, },
};
static const struct isup_ie_variable cfn_variable[] = {
	{
		.name = "ISUPCauseIndicators",
		.type = ISUPCauseIndicators,
		.min_len = 2,
		.max_len = UINT8_MAX,
	},
	{ 0, },
};
static const struct isup_ie_variable cgb_variable[] = {
	{
		.name = "ISUPRangeAndStatus",
		.type = ISUPRangeAndStatus,
		.min_len = 2,
		.max_len = 33,
	},
	{ 0, },
};
static const struct isup_ie_variable cgba_variable[] = {
	{
		.name = "ISUPRangeAndStatus",
		.type = ISUPRangeAndStatus,
		.min_len = 2,
		.max_len = 33,
	},
	{ 0, },
};
static const struct isup_ie_variable cgu_variable[] = {
	{
		.name = "ISUPRangeAndStatus",
		.type = ISUPRangeAndStatus,
		.min_len = 2,
		.max_len = 33,
	},
	{ 0, },
};
static const struct isup_ie_variable cgua_variable[] = {
	{
		.name = "ISUPRangeAndStatus",
		.type = ISUPRangeAndStatus,
		.min_len = 2,
		.max_len = 33,
	},
	{ 0, },
};
static const struct isup_ie_variable cqr_variable[] = {
	{
		.name = "ISUPRange",
		.type = ISUPRange,
		.min_len = 1,
		.max_len = UINT8_MAX,
	},
	{
		.name = "ISUPCircuitStateIndicator",
		.type = ISUPCircuitStateIndicator,
		.min_len = 1,
		.max_len = 32,
	},
	{ 0, },
};
static const struct isup_ie_variable frj_variable[] = {
	{
		.name = "ISUPCauseIndicators",
		.type = ISUPCauseIndicators,
		.min_len = 2,
		.max_len = UINT8_MAX,
	},
	{ 0, },
};
static const struct isup_ie_variable grs_variable[] = {
	{
		.name = "ISUPRange",
		.type = ISUPRange,
		.min_len = 1,
		.max_len = UINT8_MAX,
	},
	{ 0, },
};
static const struct isup_ie_variable gra_variable[] = {
	{
		.name = "ISUPRange",
		.type = ISUPRange,
		.min_len = 1,
		.max_len = UINT8_MAX,
	},
	{ 0, },
};
static const struct isup_ie_variable iam_variable[] = {
	{
		.name = "ISUPCalledPartyNumber",
		.type = ISUPCalledPartyNumber,
		.min_len = 3,
		.max_len = UINT8_MAX,
	},
	{ 0, },
};
static const struct isup_ie_variable rel_variable[] = {
	{
		.name = "ISUPCauseIndicators",
		.type = ISUPCauseIndicators,
		.min_len = 2,
		.max_len = UINT8_MAX,
	},
	{ 0, },
};
static const struct isup_ie_variable sam_variable[] = {
	{
		.name = "ISUPSubsequentNumber",
		.type = ISUPSubsequentNumber,
		.min_len = 2,
		.max_len = UINT8_MAX,
	},
	{ 0, },
};
static const struct isup_ie_variable usr_variable[] = {
	{
		.name = "ISUPUserToUserInformation",
		.type = ISUPUserToUserInformation,
		.min_len = 1,
		.max_len = 129,
	},
	{ 0, },
};
const struct isup_msg isup_msgs[256] = {
	[1] = {
		.name = "IAM",
		.fixed_ies = iam_fixed,
		.variable_ies = iam_variable,
		.has_optional = 1,
	},
	[2] = {
		.name = "SAM",
		.variable_ies = sam_variable,
		.has_optional = 1,
	},
	[3] = {
		.name = "INR",
		.fixed_ies = inr_fixed,
		.has_optional = 1,
	},
	[4] = {
		.name = "INF",
		.fixed_ies = inf_fixed,
		.has_optional = 1,
	},
	[5] = {
		.name = "COT",
		.fixed_ies = cot_fixed,
	},
	[6] = {
		.name = "ACM",
		.fixed_ies = acm_fixed,
		.has_optional = 1,
	},
	[7] = {
		.name = "CON",
		.fixed_ies = con_fixed,
		.has_optional = 1,
	},
	[8] = {
		.name = "FOT",
		.has_optional = 1,
	},
	[9] = {
		.name = "AMN",
		.has_optional = 1,
	},
	[12] = {
		.name = "REL",
		.variable_ies = rel_variable,
		.has_optional = 1,
	},
	[13] = {
		.name = "SUS",
		.fixed_ies = sus_fixed,
		.has_optional = 1,
	},
	[14] = {
		.name = "RES",
		.fixed_ies = res_fixed,
		.has_optional = 1,
	},
	[16] = {
		.name = "RLC",
		.has_optional = 1,
	},
	[17] = {
		.name = "CCR",
	},
	[18] = {
		.name = "RSC",
	},
	[19] = {
		.name = "BLO",
	},
	[20] = {
		.name = "UBL",
	},
	[21] = {
		.name = "BLA",
	},
	[22] = {
		.name = "UBA",
	},
	[23] = {
		.name = "GRS",
		.variable_ies = grs_variable,
	},
	[24] = {
		.name = "CGB",
		.fixed_ies = cgb_fixed,
		.variable_ies = cgb_variable,
	},
	[25] = {
		.name = "CGU",
		.fixed_ies = cgu_fixed,
		.variable_ies = cgu_variable,
	},
	[26] = {
		.name = "CGBA",
		.fixed_ies = cgba_fixed,
		.variable_ies = cgba_variable,
	},
	[27] = {
		.name = "CGUA",
		.fixed_ies = cgua_fixed,
		.variable_ies = cgua_variable,
	},
	[31] = {
		.name = "FAR",
		.fixed_ies = far_fixed,
		.has_optional = 1,
	},
	[32] = {
		.name = "FAA",
		.fixed_ies = faa_fixed,
		.has_optional = 1,
	},
	[33] = {
		.name = "FRJ",
		.fixed_ies = frj_fixed,
		.variable_ies = frj_variable,
		.has_optional = 1,
	},
	[36] = {
		.name = "LPA",
	},
	[41] = {
		.name = "GRA",
		.variable_ies = gra_variable,
	},
	[43] = {
		.name = "CQR",
		.variable_ies = cqr_variable,
	},
	[44] = {
		.name = "CPG",
		.fixed_ies = cpg_fixed,
		.has_optional = 1,
	},
	[45] = {
		.name = "USR",
		.variable_ies = usr_variable,
		.has_optional = 1,
	},
	[46] = {
		.name = "UCIC",
	},
	[47] = {
		.name = "CFN",
		.variable_ies = cfn_variable,
		.has_optional = 1,
	},
	[48] = {
		.name = "OLM",
	},
	[50] = {
		.name = "NRM",
		.has_optional = 1,
	},
	[51] = {
		.name = "FAC",
		.has_optional = 1,
	},
	[52] = {
		.name = "UPT",
		.has_optional = 1,
	},
	[53] = {
		.name = "UPA",
		.has_optional = 1,
	},
	[54] = {
		.name = "IDR",
		.has_optional = 1,
	},
	[55] = {
		.name = "IDS",
		.has_optional = 1,
	},
	[56] = {
		.name = "SEG",
		.has_optional = 1,
	},
	[64] = {
		.name = "LPR",
		.has_optional = 1,
	},
	[65] = {
		.name = "APT",
		.has_optional = 1,
	},
	[66] = {
		.name = "PRI",
		.has_optional = 1,
	},
	[67] = {
		.name = "SAN",
		.has_optional = 1,
	},
};
