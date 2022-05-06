#!/bin/bash

IP_ADDR=127.0.0.1
PORT=54401
COMPILER=g++
FLAGS=-std=c++17

[ $1 ] && COMPILER=$1

website_pid=0
sigint_handler () {
    (( $website_pid )) && kill -s SIGINT $website_pid
    exit
}
sigterm_handler () {
    (( $website_pid )) && kill -s SIGTERM $website_pid
    exit
}
sigkill_handler () {
    (( $website_pid )) && kill -s SIGKILL $website_pid
    exit
}

trap sigint_handler SIGINT
trap sigterm_handler SIGTERM
trap sigkill_handler SIGKILL

restart_website () {
    pid=`pidof jb-website`
    (( $? == 0 )) && kill -s SIGINT $pid
    website_pid=0

    $COMPILER $FLAGS -pthread -DADDRESS='"${IP_ADDR}"' -DPORT=$PORT server/*.cpp -o jb-website
    (( $? != 0 )) && exit

    pidof jb-website > /dev/null
    (( $? == 0 )) && exit

    echo Ready
    ./jb-website &
    website_pid=$!
}

restart_website

while true; do
    sleep 5
    if [ -f "reload" ]; then
        rm "reload"
        echo Reloading...
        restart_website
    fi
done