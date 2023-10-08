#!/bin/sh

case "$1" in
    start)
        echo "Starting aesdsocket..."
        start-stop-daemon -S -n aesdsocket -a ./aesdsocket -- -d
        #start-stop-daemon -S -b -m -p /var/run/aesdsocket.pid -d ~/ecen5713/assg3/assignments-3-and-later-mubeena12/server -x ./aesdsocket
        ;;
    stop)
        echo "Stopping aesdsocket..."
        start-stop-daemon -K -n aesdsocket
        #start-stop-daemon -K -p /var/run/aesdsocket.pid
        ;;
    *)
        echo "Usage: $0 {start|stop}"
        exit 1
        ;;
esac