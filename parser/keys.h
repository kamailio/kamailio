#ifndef KEYS_H
#define KEYS_H


#define _allo_ 0x6f6c6c61   /* "allo" */
#define _allo_hash ((unsigned char)((_allo_ >> 13) ^ _allo_))

#define _auth_ 0x68747561   /* "auth" */
#define _auth_hash ((unsigned char)((_auth_ >> 13) ^ _allo_))

#define _oriz_ 0x7a69726f   /* "oriz" */
#define _oriz_hash ((unsigned char)((_oriz_ >> 13) ^ _oriz_))

#define _atio_ 0x6f697461   /* "atio" */
#define _atio_hash ((unsigned char)((_atio_ >> 13) ^ _atio_))

#define _call_ 0x6c6c6163   /* "call" */
#define _call_hash ((unsigned char)((_call_ >> 13) ^ _call_))

#define __id2_ 0x2064692d   /* "-id " */
#define __id2_hash ((unsigned char)((__id2_ >> 13) ^ __id2_))

#define __id1_ 0x3a64692d   /* "-id:" */
#define __id1_hash ((unsigned char)((__id1_ >> 13) ^ __id1_))

#define _cont_ 0x746e6f63   /* "cont" */
#define _cont_hash ((unsigned char)((_cont_ >> 13) ^ _cont_))

#define _act2_ 0x20746361   /* "act " */
#define _act2_hash ((unsigned char)((_act2_ >> 13) ^ _act2_))

#define _act1_ 0x3a746361   /* "act:" */
#define _act1_hash ((unsigned char)((_act1_ >> 13) ^ _act1_))

#define _ent__ 0x2d746e65   /* "ent-" */
#define _ent__hash ((unsigned char)((_ent__ >> 13) ^ _ent__))

#define _leng_ 0x676e656c   /* "leng" */
#define _leng_hash ((unsigned char)((_leng_ >> 13) ^ _leng_))

#define _th12_ 0x203a6874   /* "th: " */
#define _th12_hash ((unsigned char)((_th12_ >> 13) ^ _th12_))

#define _type_ 0x65707974   /* "type" */
#define _type_hash ((unsigned char)((_type_ >> 13) ^ _type_))

#define _cseq_ 0x71657363   /* "cseq" */
#define _cseq_hash ((unsigned char)((_cseq_ >> 13) ^ _cseq_))

#define _expi_ 0x69707865   /* "expi" */
#define _expi_hash ((unsigned char)((_expi_ >> 13) ^ _expi_))

#define _res2_ 0x20736572   /* "res " */
#define _res2_hash ((unsigned char)((_res2_ >> 13) ^ _res2_))

#define _res1_ 0x3a736572   /* "res:" */
#define _res1_hash ((unsigned char)((_res1_ >> 13) ^ _res1_))

#define _from_ 0x6d6f7266   /* "from" */
#define _from_hash ((unsigned char)((_from_ >> 13) ^ _from_))

#define _max__ 0x2d78616d   /* "max-" */
#define _max__hash ((unsigned char)((_max__ >> 13) ^ _max__))

#define _forw_ 0x77726f66   /* "forw" */
#define _forw_hash ((unsigned char)((_forw_ >> 13) ^ _forw_))

#define _ards_ 0x73647261   /* "ards" */
#define _ards_hash ((unsigned char)((_ards_ >> 13) ^ _ards_))

#define _prox_ 0x786f7270   /* "prox" */
#define _prox_hash ((unsigned char)((_prox_ >> 13) ^ _prox_))

#define _y_au_ 0x75612d79   /* "y-au" */
#define _y_au_hash ((unsigned char)((_y_au_ >> 13) ^ _y_au_))

#define _thor_ 0x726f6874   /* "thor" */
#define _thor_hash ((unsigned char)((_thor_ >> 13) ^ _thor_))

#define _izat_ 0x74617a69   /* "izat" */
#define _izat_hash ((unsigned char)((_izat_ >> 13) ^ _izat_))

#define _ion2_ 0x206e6f69   /* "ion " */
#define _ion2_hash ((unsigned char)((_ion2_ >> 13) ^ _ion2_))

#define _ion1_ 0x3a6e6f69   /* "ion:" */
#define _ion1_hash ((unsigned char)((_ion1_ >> 13) ^ _ion1_))

#define _y_re_ 0x65722d79   /* "y-re" */
#define _y_re_hash ((unsigned char)((_y_re_ >> 13) ^ _y_re_))

#define _quir_ 0x72697571   /* "quir" */
#define _quir_hash ((unsigned char)((_quir_ >> 13) ^ _quir_))

#define _reco_ 0x6f636572   /* "reco" */
#define _reco_hash ((unsigned char)((_reco_ >> 13) ^ _reco_))

#define _rd_r_ 0x722d6472   /* "rd-r" */
#define _rd_r_hash ((unsigned char)((_rd_r_ >> 13) ^ _rd_r_))

#define _oute_ 0x6574756f   /* "oute" */
#define _oute_hash ((unsigned char)((_oute_ >> 13) ^ _oute_))

#define _requ_ 0x75716572   /* "requ" */
#define _requ_hash ((unsigned char)((_requ_ >> 13) ^ _requ_))

#define _ire2_ 0x20657269   /* "ire " */
#define _ire2_hash ((unsigned char)((_ire2_ >> 13) ^ _ire2_))

#define _ire1_ 0x3a657269   /* "ire:" */
#define _ire1_hash ((unsigned char)((_ire1_ >> 13) ^ _ire1_))

#define _rout_ 0x74756f72   /* "rout" */
#define _rout_hash ((unsigned char)((_rout_ >> 13) ^ _rout_))

#define _supp_ 0x70707573   /* "supp" */
#define _supp_hash ((unsigned char)((_supp_ >> 13) ^ _supp_))

#define _orte_ 0x6574726f   /* "orte" */
#define _orte_hash ((unsigned char)((_orte_ >> 13) ^ _orte_))

#define _to12_ 0x203a6f74   /* "to: " */
#define _to12_hash ((unsigned char)((_to12_ >> 13) ^ _to12_))

#define _unsu_ 0x75736e75   /* "unsu" */
#define _unsu_hash ((unsigned char)((_unsu_ >> 13) ^ _unsu_))

#define _ppor_ 0x726f7070   /* "ppor" */
#define _ppor_hash ((unsigned char)((_ppor_ >> 13) ^ _ppor_))

#define _ted2_ 0x20646574   /* "ted " */
#define _ted2_hash ((unsigned char)((_ted2_ >> 13) ^ _ted2_))

#define _ted1_ 0x3a646574   /* "ted:" */
#define _ted1_hash ((unsigned char)((_ted1_ >> 13) ^ _ted1_))

#define _via2_ 0x20616976   /* "via " */
#define _via2_hash ((unsigned char)((_via2_ >> 13) ^ _via2_))

#define _via1_ 0x3a616976   /* "via:" */
#define _via1_hash ((unsigned char)((_via1_ >> 13) ^ _via1_))

#define _www__ 0x2d777777   /* "www-" */
#define _www__hash ((unsigned char)((_www__ >> 13) ^ _www__))

#define _enti_ 0x69746e65   /* "enti" */
#define _enti_hash ((unsigned char)((_enti_ >> 13) ^ _enti_))

#define _cate_ 0x65746163   /* "cate" */
#define _cate_hash ((unsigned char)((_cate_ >> 13) ^ _cate_))

#define _even_ 0x6e657665   /* "even" */
#define _even_hash ((unsigned char)((_even_ >> 13) ^ _even_))

#endif /* KEYS_H */

