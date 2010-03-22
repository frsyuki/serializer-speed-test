#!/bin/sh
msgpack=/usr/local
protobuf=/usr/local
yajl=/usr/local

protoc speedtest_protobuf.proto --cpp_out=.

g++ -Wall -O4 speedtest.cc -o speedtest -lpthread \
	-I$msgpack/include  -L$msgpack/lib  -lmsgpack -lmsgpackc \
	-I$protobuf/include -L$protobuf/lib -lprotobuf \
	-I$yajl/include     -L$yajl/lib     -lyajl

./speedtest 200000

