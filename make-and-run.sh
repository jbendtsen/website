#!/bin/bash

IP_ADDR=127.0.0.1
PORT=54401

COMPILER=g++-10
FLAGS=-std=c++17

$COMPILER $FLAGS -pthread -DADDRESS='"${IP_ADDR}"' -DPORT=$PORT server/*.cpp -o website
if (( $? == 0 )); then
	echo "Ready"
	./website
fi

