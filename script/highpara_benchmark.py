# please warning don't use sudo to run this scripts

import paramiko
import threading
import time
import sys
import subprocess

# attention!
# distributed dir: always average
# sys thread num should modify in src code file and p4src code file (see README)

base = "~/nfs/DSM_prj/concordia_tmp/concordia"
user = "zxy"
init_page = True


# 4 machines
switch_machine = "192.168.189.34"

master_machine = "192.168.189.7"
master_nic_name = "enp28s0np0"
master_arp_script = "arp-r1.sh"

requester_machine = "192.168.189.8"
requester_nic_name = "enp63s0np0"
requester_arp_script = "arp-r2.sh"

home_machine = "192.168.189.9"
home_nic_name = "enp28s0np0"
home_arp_script = "arp-r3.sh"
home_node_id = 2

cache_machine = "192.168.189.10"
cache_nic_name = "enp62s0np0"
cache_arp_script = "arp-r4.sh"

other_machine = ["192.168.189.11", "192.168.189.12", "192.168.189.13", "192.168.189.14"]
other_nic_name = ["enp26s0np0", "enp30s0np0", "enp30s0np0", "enp30s0np0"]
other_arp_script = ["arp-r5.sh", "arp-r6.sh", "arp-r7.sh", "arp-r8.sh"]

# other_machine = []
# other_nic_name = []
# other_arp_script = []

output_directory = "/home/zxy/motivation_3n4_v2"
program_name = "highpara_benchmark"

app_thread_num = [16]
blksize_MB = [1024]
# app_thread_num = [16,24]
# blksize_MB = [64, 128, 256, 512, 1024, 1536]

# RLock/R is 0, WLock/R is 1
request_type = 1
# if need cache -> 1
cache_init = 1 
# RLock is 0, WLock is 1
cache_type = 1

def node_init_4_page(ssh):
    stdin, stdout, stderr = ssh.exec_command(
        "cd {0} && sudo bash ./hugepage.sh".format(base)
    )
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)

def node_init_4_arp_and_memcache(ssh, arp_script, is_master, nic_name):
    stdin, stdout, stderr = ssh.exec_command(
        "export NIC_NAME={2} && cd {0} && sudo bash ./{1}".format(base, arp_script, nic_name)
    )
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)

    if is_master: 
        stdin, stdout, stderr = ssh.exec_command(
            "cd {0}/script && bash ./restartMemc.sh".format(base)
        )
        str1 = stdout.read().decode('utf-8')
        str2 = stderr.read().decode('utf-8')
        print(str1)
        print(str2)

def switch_init_4_switch(ssh):
    print("cd {0}/p4src && sudo -E ./auto_run_ssh.sh".format(base))
    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}/p4src && sudo -E ./auto_run_ssh.sh".format(base)
    )
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)    

def master_run(ssh, program, app_thread_num, blksize_MB, output_dir, node_num, nic_name):
    print("export NIC_NAME={6} && cd {0}/build && sudo -E ./{1} "
        "--no_node {2} --no_thread {3} "
        "--locality 0 --shared_ratio 100 --read_ratio 50 "
        "--is_cache 0 --cache_rw 0 --is_request 0 --request_rw 0 --is_home 0 --home_node_id {4} "
        "--result_dir {5} --blksize_MB {7}".format(base, program, node_num, app_thread_num, home_node_id, output_dir, nic_name, blksize_MB))
    stdin, stdout, stderr = ssh.exec_command("export NIC_NAME={6} && cd {0}/build && sudo -E ./{1} "
        "--no_node {2} --no_thread {3} "
        "--locality 0 --shared_ratio 100 --read_ratio 50 "
        "--is_cache 0 --cache_rw 0 --is_request 0 --request_rw 0 --is_home 0 --home_node_id {4} "
        "--result_dir {5} --blksize_MB {7} 2>&1".format(base, program, node_num, app_thread_num, home_node_id, output_dir, nic_name, blksize_MB))

    while not stdout.channel.exit_status_ready():
        result = stdout.readline()
        print(result)
        if stdout.channel.exit_status_ready():
            a = stdout.readlines()
            print(a)
            break


def requester_run(ssh, program, app_thread_num, blksize_MB, output_dir, node_num, nic_name):
    print("export NIC_NAME={7} && cd {0}/build && sudo -E ./{1} "
        "--no_node {2} --no_thread {3} "
        "--locality 0 --shared_ratio 100 --read_ratio 50 "
        "--is_cache 0 --cache_rw 0 --is_request 1 --request_rw {6} --is_home 0 --home_node_id {4} "
        "--result_dir {5} --blksize_MB {8}".format(base, program, node_num, app_thread_num, home_node_id, output_dir, request_type, nic_name, blksize_MB))
    stdin, stdout, stderr = ssh.exec_command("export NIC_NAME={7} && cd {0}/build && sudo -E ./{1} "
        "--no_node {2} --no_thread {3} "
        "--locality 0 --shared_ratio 100 --read_ratio 50 "
        "--is_cache 0 --cache_rw 0 --is_request 1 --request_rw {6} --is_home 0 --home_node_id {4} "
        "--result_dir {5} --blksize_MB {8} 2>&1".format(base, program, node_num, app_thread_num, home_node_id, output_dir, request_type, nic_name, blksize_MB))

    while not stdout.channel.exit_status_ready():
        result = stdout.readline()
        print(result)
        if stdout.channel.exit_status_ready():
            a = stdout.readlines()
            print(a)
            break

def home_run(ssh, program, app_thread_num, blksize_MB, output_dir, node_num, nic_name):
    print("export NIC_NAME={6} && cd {0}/build && sudo -E ./{1} "
        "--no_node {2} --no_thread {3} "
        "--locality 0 --shared_ratio 100 --read_ratio 50 "
        "--is_cache 0 --cache_rw 0 --is_request 0 --request_rw 0 --is_home 1 --home_node_id {4} "
        "--result_dir {5} --blksize_MB {7}".format(base, program, node_num, app_thread_num, home_node_id, output_dir, nic_name, blksize_MB))
    stdin, stdout, stderr = ssh.exec_command("export NIC_NAME={6} && cd {0}/build && sudo -E ./{1} "
        "--no_node {2} --no_thread {3} "
        "--locality 0 --shared_ratio 100 --read_ratio 50 "
        "--is_cache 0 --cache_rw 0 --is_request 0 --request_rw 0 --is_home 1 --home_node_id {4} "
        "--result_dir {5} --blksize_MB {7} 2>&1".format(base, program, node_num, app_thread_num, home_node_id, output_dir, nic_name, blksize_MB))

    while not stdout.channel.exit_status_ready():
        result = stdout.readline()
        print(result)
        if stdout.channel.exit_status_ready():
            a = stdout.readlines()
            print(a)
            break

def cache_run(ssh, program, app_thread_num, blksize_MB, output_dir, node_num, nic_name):
    print("export NIC_NAME={7} && cd {0}/build && sudo -E ./{1} "
        "--no_node {2} --no_thread {3} "
        "--locality 0 --shared_ratio 100 --read_ratio 50 "
        "--is_cache {8} --cache_rw {6} --is_request 0 --request_rw 0 --is_home 0 --home_node_id {4} "
        "--result_dir {5} --blksize_MB {9}".format(base, program, node_num, app_thread_num, home_node_id, output_dir, cache_type, nic_name, cache_init, blksize_MB))
    stdin, stdout, stderr = ssh.exec_command("export NIC_NAME={7} && cd {0}/build && sudo -E ./{1} "
        "--no_node {2} --no_thread {3} "
        "--locality 0 --shared_ratio 100 --read_ratio 50 "
        "--is_cache {8} --cache_rw {6} --is_request 0 --request_rw 0 --is_home 0 --home_node_id {4} "
        "--result_dir {5} --blksize_MB {9} 2>&1".format(base, program, node_num, app_thread_num, home_node_id, output_dir, cache_type, nic_name, cache_init, blksize_MB))

    while not stdout.channel.exit_status_ready():
        result = stdout.readline()
        print(result)
        if stdout.channel.exit_status_ready():
            a = stdout.readlines()
            print(a)
            break

def other_run(ssh, program, app_thread_num, blksize_MB, output_dir, node_num, nic_name):
    print("export NIC_NAME={6} && cd {0}/build && sudo -E ./{1} "
        "--no_node {2} --no_thread {3} "
        "--locality 0 --shared_ratio 100 --read_ratio 50 "
        "--is_cache 0 --cache_rw 0 --is_request 0 --request_rw 0 --is_home 0 --home_node_id {4} "
        "--result_dir {5} --blksize_MB {7}".format(base, program, node_num, app_thread_num, home_node_id, output_dir, nic_name, blksize_MB))
    stdin, stdout, stderr = ssh.exec_command("export NIC_NAME={6} && cd {0}/build && sudo -E ./{1} "
        "--no_node {2} --no_thread {3} "
        "--locality 0 --shared_ratio 100 --read_ratio 50 "
        "--is_cache 0 --cache_rw 0 --is_request 0 --request_rw 0 --is_home 0 --home_node_id {4} "
        "--result_dir {5} --blksize_MB {7} 2>&1".format(base, program, node_num, app_thread_num, home_node_id, output_dir, nic_name, blksize_MB))

    while not stdout.channel.exit_status_ready():
        result = stdout.readline()
        print(result)
        if stdout.channel.exit_status_ready():
            a = stdout.readlines()
            print(a)
            break

def ssh_connect(ip, user):
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(ip, 22, user, None, key_filename="/home/zxy/.ssh/id_rsa")
    return ssh


def wait_for_space():
    print("Press the space bar to continue...")
    while True:
        user_input = input()
        if user_input == ' ':
            break
        else:
            print("Please press the space bar to continue...")

if __name__ == '__main__':

    # try:
    #     result = subprocess.run(
    #         ["python3", "kill_all.py"],
    #         check=True
    #     )
    #     print("kill_all脚本执行成功")
    #     print("输出:", result.stdout)
    # except subprocess.CalledProcessError as e:
    #     print("脚本执行失败，退出码：", e.returncode)
    #     print("错误信息：", e.stderr)

    ssh_switch = ssh_connect(switch_machine, user)
    ssh_master = ssh_connect(master_machine, user)
    ssh_requester = ssh_connect(requester_machine, user)
    ssh_home = ssh_connect(home_machine, user)
    ssh_cache = ssh_connect(cache_machine, user)
    ssh_others = [ssh_connect(other_machine[i], user) for i in range(len(other_machine))]

    for b_i in app_thread_num:
        for blk_i in blksize_MB:
            # init switch
            print("init switch starts")
            tt = threading.Thread(target=switch_init_4_switch,
                                    args=(ssh_switch,))
            tt.start()    
            tt.join()    
            print("init switch ends")        

            # init node (page)
            if(init_page):
                print("init node (page) starts")
                t1 = threading.Thread(target=node_init_4_page, args=(ssh_master,))
                t2 = threading.Thread(target=node_init_4_page, args=(ssh_requester,))
                t3 = threading.Thread(target=node_init_4_page, args=(ssh_home,))
                t4 = threading.Thread(target=node_init_4_page, args=(ssh_cache,))
                tother_list = [threading.Thread(target=node_init_4_page,
                                        args=(ssh_others[i],))  for i in range(len(ssh_others))]            
                t1.start()
                t2.start()
                t3.start()
                t4.start()
                for i in range(len(ssh_others)):
                    tother_list[i].start()
                t1.join()
                t2.join()
                t3.join()
                t4.join()
                for i in range(len(ssh_others)):
                    tother_list[i].join()
                print("init node (page) ends")

            # init node (arp_and_memcache)
            print("init node (arp_and_memcache) starts")
            t1 = threading.Thread(target=node_init_4_arp_and_memcache, args=(ssh_master, master_arp_script, True, master_nic_name))
            t2 = threading.Thread(target=node_init_4_arp_and_memcache, args=(ssh_requester, requester_arp_script, False, requester_nic_name))
            t3 = threading.Thread(target=node_init_4_arp_and_memcache, args=(ssh_home, home_arp_script, False, home_nic_name))
            t4 = threading.Thread(target=node_init_4_arp_and_memcache, args=(ssh_cache, cache_arp_script, False, cache_nic_name))
            tother_list = [threading.Thread(target=node_init_4_arp_and_memcache, args=(ssh_others[i], other_arp_script[i], False, other_nic_name[i]))  for i in range(len(ssh_others))]            
            t1.start()
            t2.start()
            t3.start()
            t4.start()
            for i in range(len(ssh_others)):
                tother_list[i].start()
            t1.join()
            t2.join()
            t3.join()
            t4.join()
            for i in range(len(ssh_others)):
                tother_list[i].join()
            print("init node (arp_and_memcache) ends")      

            # wait_for_space()
            # time.sleep(15)

            print("run")
            # run
            t1 = threading.Thread(target=master_run, args=(ssh_master, program_name, b_i, blk_i, output_directory, 4+len(ssh_others), master_nic_name))
            t2 = threading.Thread(target=requester_run, args=(ssh_requester, program_name, b_i, blk_i, output_directory, 4+len(ssh_others), requester_nic_name))
            t3 = threading.Thread(target=home_run, args=(ssh_home, program_name, b_i, blk_i, output_directory, 4+len(ssh_others), home_nic_name))
            t4 = threading.Thread(target=cache_run, args=(ssh_cache, program_name, b_i, blk_i, output_directory, 4+len(ssh_others), cache_nic_name))
            tother_list = [threading.Thread(target=other_run, args=(ssh_others[i], program_name, b_i, blk_i, output_directory, 4+len(ssh_others), other_nic_name[i]))  for i in range(len(ssh_others))]            
            t1.start()
            time.sleep(5)
            # wait_for_space()

            t2.start()
            time.sleep(5)
            # wait_for_space()

            t3.start()
            time.sleep(5)
            # wait_for_space()

            t4.start()
            time.sleep(5)
            # wait_for_space()

            for i in range(len(ssh_others)):
                tother_list[i].start()
                time.sleep(5)
                # wait_for_space()
            t1.join()
            t2.join()
            t3.join()
            t4.join()
            for i in range(len(ssh_others)):
                tother_list[i].join()

    print("finish: app_thread_num {}, blksize_MB {}\n".format(b_i, blk_i))
    time.sleep(10)
