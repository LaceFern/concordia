#!/bin/bash

p4c-tofino  --jobs 6 ccDSM.p4 
generate_tofino_pd -o ./build/ --context_json ccDSM.tofino/context/context.json

cp -f Makefile build/
cd build/

thrift --gen cpp thrift/p4_pd_rpc.thrift
thrift --gen cpp thrift/res.thrift
thrift --gen py thrift/p4_pd_rpc.thrift
thrift --gen py thrift/res.thrift 

rm gen-cpp/ccDSM_server.skeleton.cpp

sed -i 's/p4_prefix/ccDSM/g' thrift-src/p4_pd_rpc_server.ipp

make clean
make all -j

cd ..
echo "----------------------------"
echo "----Compile successfully----"
echo "----------------------------"
