#include "../config.h"
#include "../cfg_parser.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CFG_INPUT (64 * 1024)

struct fuzz_cfg_values
{
	int integer;
	int boolean;
	int mode;
	str text;
};

static cfg_option_t fuzz_modes[] = {
		{"fast", .val = 1}, {"safe", .val = 2}, {"compat", .val = 3}, {0}};

static int fuzz_parse_section(
		void *param, cfg_parser_t *parser, unsigned int flags)
{
	str section = STR_NULL;
	int ret;

	(void)param;
	ret = cfg_parse_section(&section, parser, flags | CFG_STR_MALLOC);
	free(section.s);
	return ret;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct fuzz_cfg_values values = {0};
	cfg_parser_t parser = {0};
	cfg_token_t token;
	cfg_option_t options[] = {
			{"integer", .param = &values.integer, .f = cfg_parse_int_opt},
			{"boolean", .param = &values.boolean, .f = cfg_parse_bool_opt},
			{"mode", .param = fuzz_modes, .f = cfg_parse_enum_opt},
			{"text", .param = &values.text, .flags = CFG_STR_MALLOC,
					.f = cfg_parse_str_opt},
			{0}};
	char *input;
	FILE *stream;

	if(size == 0 || size > MAX_CFG_INPUT) {
		return 0;
	}

	input = (char *)malloc(size);
	if(input == NULL) {
		return 0;
	}
	memcpy(input, data, size);

	stream = tmpfile();
	if(stream == NULL) {
		free(input);
		return 0;
	}
	if(fwrite(input, 1, size, stream) != size
			|| fseek(stream, 0, SEEK_SET) != 0) {
		fclose(stream);
		free(input);
		return 0;
	}

	parser.f = stream;
	parser.file = (char *)"fuzz.cfg";
	parser.line = 1;
	cfg_set_options(&parser, options);
	cfg_section_parser(&parser, fuzz_parse_section, NULL);
	sr_cfg_parse(&parser);
	while(cfg_get_token(&token, &parser, 0) == 0) {
	}

	if(values.text.s != NULL) {
		free(values.text.s);
	}
	fclose(stream);
	free(input);
	return 0;
}
