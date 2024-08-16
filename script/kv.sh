#!/bin/bash

#Usage: ./benchmark nodeNR threadNR readNR locality sharing

mpi="/usr/local/bin/mpiexec --allow-run-as-root -hostfile host_mpi -np "
th_nr="4"
time=$(date "+%d-%H-%M-%S")

vary_nodes () {
  nodes_arr=(2 3 4 5 6 7 8)
  for nodes in ${nodes_arr[@]}; do
    ./restartMemc.sh
    ssh 192.168.189.34 "bash /home/zxy/nfs/DSM_prj/concordia_tmp/concordia/p4src/auto_run.sh >> /dev/null"
    ${mpi} ${nodes}  ./dht ${nodes} ${th_nr} ${time}
  done
}

vary_nodes