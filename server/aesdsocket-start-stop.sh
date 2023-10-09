#!/bin/sh

#--------------------------------------
# Author: Mubeena Udyavar Kazi
# Course: ECEN 5713 - AESD
#--------------------------------------

case "$1" in
    start)
        echo "Starting aesdsocket..."
        start-stop-daemon -S -n aesdsocket -a aesdsocket -- -d
        ;;
    stop)
        echo "Stopping aesdsocket..."
        start-stop-daemon -K -n aesdsocket
        ;;
    *)
        echo "Usage: $0 {start|stop}"
        exit 1
        ;;
esac