#ifndef __WINFO_DOC_H
#define __WINFO_DOC_H

#include "presentity.h"
#include "watcher.h"

int create_winfo_document(struct presentity* p, struct watcher* w, str *dst, str *dst_content_type);

#endif
