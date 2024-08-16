#!/bin/bash
set -o nounset

# With the specified arguments for benchmark setting, 
# this script runs tpcc for varied distributed ratios

# specify your hosts_file here 
# hosts_file specify a list of host names and port numbers, with the host names in the first column
hosts_file="../tpcc/config.txt"
# specify your directory for log files
output_dir="/home/zxy_workspace/ccDSM/build/"

# working environment
proj_dir="/home/zxy_workspace/ccDSM/"
bin_dir="${proj_dir}/build"
script_dir="{proj_dir}/database/scripts"
ssh_opts="-o StrictHostKeyChecking=no"

hosts_list=`./get_servers.sh ${hosts_file} | tr "\\n" " "`
hosts=(`echo ${hosts_list}`)
master_host=${hosts[0]}

USER_ARGS="$@"
echo "input Arguments: ${USER_ARGS}"
echo "launch..."



launch () {
  dist_ratio=$1
  
  output_file="${output_dir}/${master_host}_${dist_ratio}_tpcc.log"
  script="cd ${bin_dir} && ./tpcc ${USER_ARGS} -d${dist_ratio} > ${output_file} 2>&1"
  echo "start master: ssh ${ssh_opts} ${master_host} "$script" &"
  ssh ${ssh_opts} ${master_host} "$script" &

  sleep 3

  for ((i=1;i<${#hosts[@]};i++)); do
    host=${hosts[$i]}
    output_file="${output_dir}/${host}_${dist_ratio}_tpcc.log"
    script="cd ${bin_dir} && ./tpcc ${USER_ARGS} -d${dist_ratio} > ${output_file} 2>&1"
    echo "start worker: ssh ${ssh_opts} ${host} "$script" &"
    ssh ${ssh_opts} ${host} "$script" &
    sleep 1
  done
  wait
  echo "done for ${dist_ratio}" 
}

run_tpcc () {
  dist_ratios=(100 90 80 70 60 50 40 30 20 10)
  for dist_ratio in ${dist_ratios[@]}; do
    cd ${bin_dir}
    ./restartMemc.sh
    ssh 192.168.189.34 "bash /home/zxy/nfs/DSM_prj/concordia_tmp/concordia/p4src/auto_run.sh >> /dev/null"
    cd -
    launch ${dist_ratio}
  done
}

vary_read_ratios () {
  #read_ratios=(0 30 50 70 90 100)
  read_ratios=(0)
  for read_ratio in ${read_ratios[@]}; do
    old_user_args=${USER_ARGS}
    USER_ARGS="${USER_ARGS} -r${read_ratio}"
    run_tpcc
    USER_ARGS=${old_user_args}
  done
}

vary_temp_locality () {
  #localities=(0 30 50 70 90 100)
  localities=(0 50 100)
  for locality in ${localities[@]}; do
    old_user_args=${USER_ARGS}
    USER_ARGS="${USER_ARGS -l${locality}}"
    run_tpcc
    USER_ARGS=${old_user_args}
  done
}

auto_fill_params () {
  # so that users don't need to specify parameters for themselves
  USER_ARGS="-p11111 -sf32 -sf10 -c4 -t200000"
}

auto_fill_params
# run standard tpcc
run_tpcc

# vary_read_ratios
#vary_temp_locality
