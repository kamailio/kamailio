#include "../config.h"
#include "../../src/core/utils/srjson.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void walk_json(srjson_doc_t *doc, srjson_t *item)
{
	int i;
	int count;

	if(item == NULL) {
		return;
	}

	if((item->type & 255) == srjson_Object) {
		(void)srjson_GetObjectItem(doc, item, "id");
		(void)srjson_GetObjectItem(doc, item, "result");
	}

	if((item->type & 255) != srjson_Array
			&& (item->type & 255) != srjson_Object) {
		return;
	}

	count = srjson_GetArraySize(doc, item);
	for(i = 0; i < count; i++) {
		walk_json(doc, srjson_GetArrayItem(doc, item, i));
	}
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	srjson_doc_t doc;
	char *input;
	char *printed;

	if(size == 0 || size > 1U << 20) {
		return 0;
	}

	input = (char *)malloc(size + 1);
	if(input == NULL) {
		return 0;
	}
	memcpy(input, data, size);
	input[size] = '\0';

	if(srjson_InitDoc(&doc, NULL) == 0) {
		doc.root = srjson_Parse(&doc, input);
		if(doc.root != NULL) {
			walk_json(&doc, doc.root);
			printed = srjson_Print(&doc, doc.root);
			free(printed);
			printed = srjson_PrintUnformatted(&doc, doc.root);
			free(printed);
		}
		srjson_DestroyDoc(&doc);
	}

	free(input);
	return 0;
}