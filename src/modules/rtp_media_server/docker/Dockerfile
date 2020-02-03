FROM debian

ENV DEB_PKG_CORES="libevent-dev libjansson-dev libpcre3 libpcre3-dev"

ENV DEB_PKG="procps netcat curl dnsutils sqlite net-tools vim ngrep wget libhiredis-dev ${DEB_PKG_CORES}"
ENV DEB_PKG_TMP="bison flex build-essential git libssl-dev libpq-dev liblua5.1-0-dev libcurl4-openssl-dev libncurses5-dev libxml2-dev libsqlite3-dev"
ENV DEB_PKG_DEP="libcurl3 libssl1.1 libpq5 liblua5.1-0 libncurses5 libxml2 libsqlite3-0"

ENV KAM_INC_MOD="uac debugger rtp_media_server usrloc registrar siputils"
ENV KAM_SKIP_MOD="uid_domain diversion smsops mediaproxy rtpproxy mqueue topoh app_jsdt tsilo drouting kex acc stun usrloc p_usrloc dmq dmq_usrloc sipjson userblacklist path libtrie kex sipdump  uid_uri_db msrp sst imc mohqueue nattraversal pike xhttp rtpengine sca pdt matrix auth_diameter keepalive seas carrierroute ss7ops pipelimit db_cluster secfilter mangler acc_diameter enum app_sqlang pdb mtree sms"

ENV COMMIT_KAMAILIO=master
ENV COMMIT_BCUNIT=29c556fa8ac1ab21fba1291231ffa8dea43cf32a
ENV COMMIT_BCTOOLBOX=971953a9fa4058e9c8a40ca4a3fa12d832445255
ENV COMMIT_ORTP=6e13ef49a55cdd19dae395c38cfff7ffa518a089
ENV COMMIT_MEDIASTREAMER2=d935123fc497d19a24019c6e7ae4fe0c5f19d55a

RUN echo "building Kamailio with RTP Media Server" \
	&& apt-get update && apt-get -y install ${DEB_PKG} ${DEB_PKG_TMP} \
	&& apt-get install -y libopus-dev opus-tools \
	&& mkdir -p /git && cd /git

RUN apt-get install -y automake autogen autoconf libtool pkg-config

# RUN echo "building Opus" \
# 	&& mkdir -p /git && cd /git \
# 	&& git clone https://github.com/xiph/opus.git \
# 	&& ./configure \
# 	&& make && make install

RUN echo "building bcunit" \
	&& mkdir -p /git && cd /git \
	&& git clone https://github.com/BelledonneCommunications/bcunit.git \
	&& cd bcunit \
	&& git checkout ${COMMIT_BCUNIT} \
	&& ./autogen.sh && ./configure \
	&& make && make install

RUN echo "building bctoolbox" \
	&& apt-get install -y libmbedtls-dev \
	&& mkdir -p /git && cd /git \
	&& git clone https://github.com/BelledonneCommunications/bctoolbox.git \
	&& cd bctoolbox \
	&& git checkout ${COMMIT_BCTOOLBOX} \
	&& ./autogen.sh && ./configure \
	&& make && make install

RUN echo "building oRTP" \
	&& mkdir -p /git && cd /git \
	&& git clone https://github.com/BelledonneCommunications/ortp.git \
	&& cd ortp \
	&& git checkout ${COMMIT_ORTP} \
	&& ./autogen.sh && ./configure \
	&& make && make install

RUN echo "building mediastreamer2" \
	&& mkdir -p /git && cd /git \
	&& apt-get install -y intltool libspeex-dev libspeexdsp-dev \
	&& git clone https://github.com/BelledonneCommunications/mediastreamer2.git \
	&& cd mediastreamer2 \
	&& git checkout ${COMMIT_MEDIASTREAMER2} \
	&& ./autogen.sh && ./configure --disable-sound --disable-video --enable-tools=no --disable-tests \
	&& make && make install \
	&& ldconfig

RUN echo "download sample voice files" \
	&& mkdir -p /opt/voice_files && cd /opt/voice_files \
	&& wget http://www.voiptroubleshooter.com/open_speech/american/OSR_us_000_0010_8k.wav

RUN echo "building Kamailio" \
 	&& mkdir -p /git && cd /git \
 	&& git clone https://github.com/kamailio/kamailio.git \
 	&& cd kamailio && git checkout ${COMMIT_KAMAILIO} \
 	&& make include_modules="${KAM_INC_MOD}" skip_modules="\$(mod_list_extra) \$(mod_list_db) ${KAM_SKIP_MOD}" cfg \
 	&& make install

