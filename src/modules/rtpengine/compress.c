
#include <zlib.h>
#include <string.h>
#include <errno.h>

#include "../../core/dprint.h"

#include "compress.h"

#define windowBits 15

#define GZIP_ENCODING 16
#define ENABLE_ZLIB_GZIP 32

static void init_stream(z_stream *stream);

static void init_stream(z_stream *stream)
{
	stream->zalloc = Z_NULL;
	stream->zfree = Z_NULL;
	stream->opaque = Z_NULL;
}

// compress_data compress the data of the given length.
// returns compressed buffer length and replace the original buffer.
int compress_data(char *buf, int len) {
	uLong comp_len = compressBound(len);
	char tmp[comp_len];
	int ret;

	ret = compress2((Bytef *)tmp, &comp_len, (Bytef *)buf, len, Z_BEST_COMPRESSION);
	if (ret != Z_OK) {
		LM_ERR("Could not compress the message. ret: %d\n", ret);
		return -1;
	}

	memcpy(buf, tmp, comp_len);
	return comp_len;
}

// uncompress_data uncompress the compressed data with given length.
// it compare the first byte to check the compressed data or not.
int uncompress_data(char *buf, int len, char *buf_tmp, int buf_size) {
	z_stream stream;
	int ret;

	init_stream(&stream);
	ret = inflateInit2(&stream, windowBits | ENABLE_ZLIB_GZIP);
	if (ret != Z_OK) {
		LM_ERR("Could not initiate zstream.\n");
		return -1;
	}

	stream.next_in = (Bytef *)buf;
	stream.avail_in = len;

	memset(buf_tmp, 0x00, buf_size);
	stream.next_out = (Bytef *)buf_tmp;
	stream.avail_out = buf_size - 1;

	ret = inflate(&stream, Z_NO_FLUSH);
	if (ret != Z_OK && ret != Z_STREAM_END) {
		inflateEnd(&stream);
		LM_ERR("Could not uncompress the data correctly.\n");
		return -1;
	}

	memcpy(buf, buf_tmp, buf_size);
	ret = stream.total_out;
	inflateEnd(&stream);

	return ret;
}
