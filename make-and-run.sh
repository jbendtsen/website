#!/bin/bash

IP_ADDR=127.0.0.1
PORT=54401

g++ -pthread -DADDRESS='"${IP_ADDR}"' -DPORT=$PORT server/*.cpp -o website
if (( $? == 0 )); then
	echo "Ready"
	./website
fi

