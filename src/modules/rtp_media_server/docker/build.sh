docker build . -t rtp_media_server

VERSION="0.2"
echo
echo "execute these commands to update the public docker image:"
echo
echo "  docker tag rtp_media_server:latest jchavanton/rtp_media_server:latest"
echo "  docker tag rtp_media_server:latest jchavanton/rtp_media_server:${VERSION}"
echo "  docker push jchavanton/rtp_media_server:latest"
echo "  docker push jchavanton/rtp_media_server:${VERSION}"
