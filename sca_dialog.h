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

#endif /* SCA_DIALOG_H */
