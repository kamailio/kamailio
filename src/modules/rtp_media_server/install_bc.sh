# https://github.com/jchavanton/rtp_media_server.git
apt-get install automake autogen autoconf libtool pkg-config
# bcunit
git clone https://github.com/BelledonneCommunications/bcunit.git
cd bcunit
git checkout 29c556fa8ac1ab21fba1291231ffa8dea43cf32a
./autogen.sh
./configure
make
make install
cd ..

# bctoolbox
apt-get install libmbedtls-dev
git clone https://github.com/BelledonneCommunications/bctoolbox.git
cd bctoolbox
git checkout 971953a9fa4058e9c8a40ca4a3fa12d832445255
./autogen.sh
./configure
make
make install
cd ..

# oRTP
git clone https://github.com/BelledonneCommunications/ortp.git
git checkout 6e13ef49a55cdd19dae395c38cfff7ffa518a089
cd ortp
./autogen.sh
./configure
make
make install
cd ..

# mediastreamer2
apt-get install intltool libspeex-dev libspeexdsp-dev
git clone https://github.com/BelledonneCommunications/mediastreamer2.git
cd mediastreamer2
git checkout d935123fc497d19a24019c6e7ae4fe0c5f19d55a
./autogen.sh
./configure --disable-sound --disable-video --enable-tools=no --disable-tests
make
make install
cd ..

ldconfig

# download sample voice file
mkdir -p voice_files
cd voice_files
wget http://www.voiptroubleshooter.com/open_speech/american/OSR_us_000_0010_8k.wav
cd ..

