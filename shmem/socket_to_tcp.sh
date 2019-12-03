#!/bin/sh -e

if [ $# != 2 ]; then
    echo "usage: $0 <public-port> <path-private-socket>"
    echo "example: $0 15432 /srv/my-service.sock"
    exit 0
fi

PUBLIC_PORT=$1
PRIVATE_SOCKET=$2
SOCAT_LOG=/tmp/socat-$PUBLIC_PORT.log

echo "--> socat forwarding:\n\t- from port: $PUBLIC_PORT\n\t- to socket: $PRIVATE_SOCKET"
echo "--> allowed ips:\n\t$(cat socat-allow)"
echo "--> logs: $SOCAT_LOG"

socat -d -d -lf $SOCAT_LOG \
        TCP4-LISTEN:$PUBLIC_PORT,reuseaddr,fork,tcpwrap=socat,allow-table=socat-allow,deny-table=socat-deny \
        UNIX-CONNECT:$PRIVATE_SOCKET
