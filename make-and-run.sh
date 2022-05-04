#!/bin/bash

IP_ADDR=127.0.0.1
PORT=54401
COMPILER=g++-10
FLAGS=-std=c++17

website_proc=`pidof jb-website`
(( $? == 0 )) && kill -s SIGINT $website_proc

$COMPILER $FLAGS -pthread -DADDRESS='"${IP_ADDR}"' -DPORT=$PORT server/*.cpp -o jb-website
(( $? != 0 )) && exit

echo "Ready"
./jb-website

