#!/bin/bash

SDE_INSTALL="/root/Software/bf-sde/install"

bin="/home/zxy/nfs/DSM_prj/concordia_tmp/concordia/p4src/"

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

export LD_LIBRARY_PATH=$SDE_INSTALL/lib; nohup $SDE_INSTALL/bin/bf_switchd --conf-file ./ccDSM.conf --install-dir $SDE_INSTALL/ --background > /dev/null 2>&1 &
# export LD_LIBRARY_PATH=$SDE_INSTALL/lib; $SDE_INSTALL/bin/bf_switchd --conf-file ./ccDSM.conf --install-dir $SDE_INSTALL/ --background &

sleep 8

loop_exe "$SDE_INSTALL/bin/bfshell -f $bin/port_enable.txt"

while true ; do
    sleep 3
    $SDE_INSTALL/bin/bfshell -f $bin/port_show.txt > port_data
    up_ports=`grep -c UP $bin/port_data`
    echo $up_ports
    if [ $up_ports == "8" ] ; then
        break
    fi
done