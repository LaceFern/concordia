#!/bin/bash

addr=$(head -1 ../memcached.conf)
port=$(awk 'NR==2{print}' ../memcached.conf)

# kill old me
sudo service memcached stop
ps aux |grep memcac |grep -v grep | awk '{print $2}'|xargs sudo kill -9
sleep 1

# launch memcached
memcached -u tzr -l ${addr} -p  ${port} -c 10000 -d -P /tmp/memcached.pid.tzr
sleep 3

# init 
echo -e "set serverNum 0 0 1\r\n0\r\nquit\r" | nc ${addr} ${port}
echo -e "set clientNum 0 0 1\r\n0\r\nquit\r" | nc ${addr} ${port}
sleep 1