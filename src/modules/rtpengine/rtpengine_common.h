#ifndef _RTPENGINE_COMMON_H_
#define _RTPENGINE_COMMON_H_

#define RTPENGINE_CALLER 0
#define RTPENGINE_CALLEE 1

#define RTP_SUBSCRIBE_MODE_SIPREC (1 << 0)
#define RTP_SUBSCRIBE_MODE_DISABLE (1 << 1)

#define RTP_COPY_LEG_CALLER (1 << 2)
#define RTP_COPY_LEG_CALLEE (1 << 3)
#define RTP_COPY_LEG_BOTH (RTP_COPY_LEG_CALLER | RTP_COPY_LEG_CALLEE)
#define RTP_COPY_MAX_STREAMS 32

struct rtpengine_stream
{
	int leg;
	int medianum;
	int label;
};

struct rtpengine_streams
{
	int count;
	struct rtpengine_stream streams[RTP_COPY_MAX_STREAMS];
};

#endif /* _RTPENGINE_COMMON_H_ */
