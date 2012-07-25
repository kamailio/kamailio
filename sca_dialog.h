#ifndef SCA_DIALOG_H
#define SCA_DIALOG_H

struct _sca_dialog {
    str		id;	/* call-id + from-tag + to-tag */
    str		call_id;
    str		from_tag;
    str		to_tag;

    int		notify_cseq;
    int		subscribe_cseq;
};
typedef struct _sca_dialog		sca_dialog;

#define SCA_DIALOG_EMPTY(d) \
	((d) == NULL || (SCA_STR_EMPTY( &(d)->call_id ) && \
			    SCA_STR_EMPTY( &(d)->from_tag ) && \
			    SCA_STR_EMPTY( &(d)->to_tag )))

int	sca_dialog_build_from_tags( sca_dialog *, int, str *, str *, str * );

#endif /* SCA_DIALOG_H */
