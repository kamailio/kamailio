/**
 *
 * Copyright (C) 2017 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <phonenumbers/geocoding/phonenumber_offline_geocoder.h>
#include <phonenumbers/phonenumberutil.h>
#include <unicode/locid.h>
#include <string.h>
#include <cstring>
#include <string>
#include "cphonenumber.h"

using namespace i18n::phonenumbers;
using icu::Locale;
using std::string;

const PhoneNumberUtil& _phoneUtil(*PhoneNumberUtil::GetInstance());
static PhoneNumberOfflineGeocoder *_phoneGeoCoder = new PhoneNumberOfflineGeocoder();

const char* telnum_linetype(PhoneNumberUtil::PhoneNumberType ltype)
{
	switch(ltype) {
      case PhoneNumberUtil::FIXED_LINE:
	  	return "fixed-line";
      case PhoneNumberUtil::MOBILE:
	  	return "mobile";
      // In some regions (e.g. the USA), it is impossible to distinguish between
      // fixed-line and mobile numbers by looking at the phone number itself.
      case PhoneNumberUtil::FIXED_LINE_OR_MOBILE:
	  	return "fixed-line-or-mobile";
      // Freephone lines
      case PhoneNumberUtil::TOLL_FREE:
	  	return "toll-free";
      case PhoneNumberUtil::PREMIUM_RATE:
	  	return "premium-rate";
      // The cost of this call is shared between the caller and the recipient, and
      // is hence typically less than PREMIUM_RATE calls. See
      // http://en.wikipedia.org/wiki/Shared_Cost_Service for more information.
      case PhoneNumberUtil::SHARED_COST:
	  	return "shared-cost";
      // Voice over IP numbers. This includes TSoIP (Telephony Service over IP).
      case PhoneNumberUtil::VOIP:
	  	return "voip";
      // A personal number is associated with a particular person, and may be
      // routed to either a MOBILE or FIXED_LINE number. Some more information can
      // be found here: http://en.wikipedia.org/wiki/Personal_Numbers
      case PhoneNumberUtil::PERSONAL_NUMBER:
	  	return "personal-number";
      case PhoneNumberUtil::PAGER:
	  	return "pager";
      // Used for "Universal Access Numbers" or "Company Numbers". They may be
      // further routed to specific offices, but allow one number to be used for a
      // company.
      case PhoneNumberUtil::UAN:
	  	return "uan";
      // Used for "Voice Mail Access Numbers".
      case PhoneNumberUtil::VOICEMAIL:
	  	return "voicemail";
      // A phone number is of type UNKNOWN when it does not fit any of the known
      // patterns for a specific region.
      case PhoneNumberUtil::UNKNOWN:
	  	return "unknown";
    }
	return "unknown";
}
int telnum_possible(char* number, char* region)
{
	string numStr(number);
	string regionStr(region);

	bool isPossible = _phoneUtil.IsPossibleNumberForString(numStr, regionStr);
	return (isPossible ? 1 : 0);
}

char* telnum_cc(char* number)
{
	string numStr(number);
	string defaultRegion("ZZ");
	PhoneNumber parsedNumber;

	PhoneNumberUtil::ErrorType error = _phoneUtil.Parse(numStr, defaultRegion, &parsedNumber);
	if (error != PhoneNumberUtil::NO_PARSING_ERROR) {
		return NULL;
	}
	string regionCode;
	_phoneUtil.GetRegionCodeForNumber(parsedNumber, &regionCode);
	return strdup(regionCode.c_str());
}

telnum_t* telnum_parse(char* number, char* region)
{
	string numStr(number);
	string regionStr(region);

	PhoneNumber parsedNumber;
	PhoneNumberUtil::ErrorType error = _phoneUtil.Parse(numStr, regionStr, &parsedNumber);
	telnum_t* res = telnum_new(number);
	if(res==NULL) {
		return NULL;
	}
	if (error != PhoneNumberUtil::NO_PARSING_ERROR) {
		string error = "Parsing number failed";
		res->error = strdup(error.c_str());
		return res;
	}
	if (!_phoneUtil.IsValidNumber(parsedNumber)) {
		string error = "Invalid number";
		res->error = strdup(error.c_str());
		return res;
	}
	res->valid = 1;
	string formattedNumber;
	_phoneUtil.Format(parsedNumber, PhoneNumberUtil::E164, &formattedNumber);
	res->normalized = strdup(formattedNumber.c_str());
	string descNumber = _phoneGeoCoder->GetDescriptionForNumber(parsedNumber, Locale("en"));
	res->ndesc = strdup(descNumber.c_str());
	res->ltype = strdup(telnum_linetype(_phoneUtil.GetNumberType(parsedNumber)));
	// res->cctel = _phoneUtil.GetCountryCodeForRegion(regionStr);
	string regionCode;
	_phoneUtil.GetRegionCodeForNumber(parsedNumber, &regionCode);
	res->cctel = _phoneUtil.GetCountryCodeForRegion(regionCode);

	return res;
}

telnum_t* telnum_new(char* number)
{
	telnum_t* tn = (telnum_t*)malloc(sizeof(telnum_t));
	if(tn==NULL) {
		return NULL;
	}
	tn->valid = 0;
	tn->cctel = 0;
	tn->number = strdup(number);
	tn->normalized = NULL;
	tn->ltype = NULL;
	tn->ndesc = NULL;
	tn->error = NULL;
	return tn;
}

void telnum_free(telnum_t* tn)
{
	if(tn==NULL) {
		return;
	}
	if (tn->number) {
		free(tn->number);
	}
	if (tn->normalized) {
		free(tn->normalized);
	}
	if (tn->error) {
		free(tn->error);
	}
	if (tn->ltype) {
		free(tn->ltype);
	}
	if (tn->ndesc) {
		free(tn->ndesc);
	}
	free(tn);
}
