#ifndef __CODEC_H__
#define __CODEC_H__

#define INLINE static inline

// returns 1 if the given buffer is compressed buffer.
// Level | ZLIB  | GZIP
//   1   | 78 01 | 1F 8B
//   2   | 78 5E | 1F 8B
//   3   | 78 5E | 1F 8B
//   4   | 78 5E | 1F 8B
//   5   | 78 5E | 1F 8B
//   6   | 78 9C | 1F 8B
//   7   | 78 DA | 1F 8B
//   8   | 78 DA | 1F 8B
//   9   | 78 DA | 1F 8B
INLINE int is_compressed(char* buf) {
	if (buf && ((buf[0] == 0x78) || (buf[0] == 0x1F))) {
		return 1;
	}
	return 0;
}

int uncompress_data(char *buf, int len, char *buf_tmp, int buf_size);
int compress_data(char *buf, int len);

#endif
