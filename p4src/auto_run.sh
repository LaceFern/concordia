#!/bin/bash

bin="/home/workspace/ccDSM/p4src/"

function loop_exe()
{
    CMDLINE=$1
    while true ; do
        sleep 1
        ${CMDLINE}
        if [ $? == 0 ] ; then
            break;
        fi
    done
}

pkill bf_switchd 

env "PATH=$PATH" "LD_LIBRARY_PATH=$LD_LIBRARY_PATH" bf_switchd --conf-file $bin/ccDSM.conf --install-dir $SDE_INSTALL/ --background &

sleep 8

loop_exe "bfshell -f $bin/port_enable.txt"

while true ; do
    sleep 3
    bfshell -f $bin/port_show.txt > port_data
    up_ports=`grep -c UP $bin/port_data`
    echo $up_ports
    if [ $up_ports == "8" ] ; then
        break
    fi
done