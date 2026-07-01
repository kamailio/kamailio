#ifndef PCSCF_DB_LAYOUT_H
#define PCSCF_DB_LAYOUT_H

#define PCSCF_LOAD_NCOLS 22
#define PCSCF_ROW_ID_OFF 0
#define PCSCF_ROW_DOMAIN_OFF 1
#define PCSCF_ROW_AOR_OFF 2
#define PCSCF_ROW_BARRED_OFF 20

static inline unsigned int pcscf_location_id_from_int(unsigned int id)
{
	return id;
}

#endif
