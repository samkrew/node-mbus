#!/bin/sh

git clone https://github.com/samkrew/libmbus.git libmbus
cd ./libmbus/
./build.sh && make
