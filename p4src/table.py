#!/usr/bin/python

import struct, socket
import sys

PRE = "/home/workspace/ccDSM/"

sys.path.append(PRE + 'p4src/build/gen-py')
sys.path.append("/home/wq/bf-sde-8.9.1/install/lib/python2.7/site-packages/p4ccDSMutils")
sys.path.append("/home/wq/bf-sde-8.9.1/install/lib/python2.7/site-packages/tofino")
sys.path.append("/home/wq/bf-sde-8.9.1/install/lib/python2.7/site-packages/")

from ptf import config
from ptf.testutils import *
from ptf.thriftutils import *

from p4_pd_rpc import ccDSM
from mc_pd_rpc import mc
from res_pd_rpc.ttypes import *
from p4_pd_rpc.ttypes import *
from mc_pd_rpc.ttypes import *


from thrift import Thrift
from thrift.transport import TSocket
from thrift.transport import TTransport
from thrift.protocol import TBinaryProtocol, TCompactProtocol
from thrift.protocol.TMultiplexedProtocol import TMultiplexedProtocol

# Request
R_READ_MISS    = 1
R_WRITE_MISS   = 2
R_WRITE_SHARED = 3
R_EVICT_SHARED = 4
R_EVICT_DIRTY  = 5

R_UNLOCK_EVICT = 7
R_UNLOCK = 8
R_READ_MISS_UNLOCK = 9

DEL_DIR = 13

# AGENT_ACK_WRITE_MISS = 67
# AGENT_ACK_WRITE_SHARED = 68

# R_UNLOCK = 8
# M_LOCK_FAIL = 12
# M_CHECK_FAIL = 13
# ACK_UNLOCK = 14

# State
UNSHARED = 0
SHARED = 1
DIRTY = 2


def ip2int(ip):
    return hex_to_i32(reduce(lambda a, b: a << 8 | b, map(int, ip.split("."))))

def mac2v(mac):
    return ''.join(map(lambda x: chr(int(x, 16)), mac.split(":")))

dev_id = 0
pipeID = hex_to_i16(0xFFFF)
dev_tgt = DevTarget_t(dev_id, pipeID);

def addHost():
    try:
        transport = TSocket.TSocket('127.0.0.1', 9090)
        transport = TTransport.TBufferedTransport(transport)

        protocol = TBinaryProtocol.TBinaryProtocol(transport)
        protocol = TMultiplexedProtocol(protocol, "ccDSM");
        client = ccDSM.Client(protocol)

        transport.open()

        # ipv4_route & set_mac_match
        f = open(PRE + "host")
        line = f.readline()
        while line:
            ip, mac, port = line.split(" ")
            client.ipv4_route_table_add_with_set_egr(0, dev_tgt,
                    ccDSM_ipv4_route_match_spec_t(ip2int(ip)),
                    ccDSM_set_egr_action_spec_t(int(port)));

            client.ethernet_set_mac_table_add_with_ethernet_set_mac_act(0, dev_tgt,
                    ccDSM_ethernet_set_mac_match_spec_t(int(port)),
                    ccDSM_ethernet_set_mac_act_action_spec_t(mac2v(mac)));

            line = f.readline()

        # check_vaild_tbl
        client.check_valid_tbl_table_add_with_check_cc_miss(0, dev_tgt, 
                ccDSM_check_valid_tbl_match_spec_t(R_READ_MISS));
        client.check_valid_tbl_table_add_with_check_cc_miss(0, dev_tgt, 
                ccDSM_check_valid_tbl_match_spec_t(R_WRITE_MISS));

        client.check_valid_tbl_table_add_with_check_other(0, dev_tgt, 
                ccDSM_check_valid_tbl_match_spec_t(R_WRITE_SHARED),
                ccDSM_check_other_action_spec_t(SHARED));
        client.check_valid_tbl_table_add_with_check_other(0, dev_tgt, 
                ccDSM_check_valid_tbl_match_spec_t(R_EVICT_SHARED),
                ccDSM_check_other_action_spec_t(SHARED));
        client.check_valid_tbl_table_add_with_check_other(0, dev_tgt, 
                ccDSM_check_valid_tbl_match_spec_t(R_EVICT_DIRTY),
                ccDSM_check_other_action_spec_t(DIRTY));

        # message_trans_tbl
        client.message_trans_tbl_table_add_with_trans_nop(0, dev_tgt, 
                ccDSM_message_trans_tbl_match_spec_t(R_READ_MISS, UNSHARED));
        client.message_trans_tbl_table_add_with_trans_nop(0, dev_tgt, 
                ccDSM_message_trans_tbl_match_spec_t(R_READ_MISS, SHARED));
        client.message_trans_tbl_table_add_with_trans_mcast(0, dev_tgt, 
                ccDSM_message_trans_tbl_match_spec_t(R_READ_MISS, DIRTY));

        client.message_trans_tbl_table_add_with_trans_nop(0, dev_tgt, 
                ccDSM_message_trans_tbl_match_spec_t(R_WRITE_MISS, UNSHARED));
        client.message_trans_tbl_table_add_with_trans_mcast_ucast(0, dev_tgt, 
                ccDSM_message_trans_tbl_match_spec_t(R_WRITE_MISS, SHARED));
        client.message_trans_tbl_table_add_with_trans_mcast(0, dev_tgt, 
                ccDSM_message_trans_tbl_match_spec_t(R_WRITE_MISS, DIRTY));

        client.message_trans_tbl_table_add_with_trans_mcast_without_myself(0, dev_tgt, 
                ccDSM_message_trans_tbl_match_spec_t(R_WRITE_SHARED , SHARED));

        client.message_trans_tbl_table_add_with_trans_loop_back(0, dev_tgt,
                ccDSM_message_trans_tbl_match_spec_t(R_EVICT_DIRTY , DIRTY));

        client.message_trans_tbl_table_add_with_trans_loop_back(0, dev_tgt,
                ccDSM_message_trans_tbl_match_spec_t(R_EVICT_SHARED , SHARED));

        # select_dest
        client.select_dest_table_add_with_to_dir_act(0, dev_tgt, 
                ccDSM_select_dest_match_spec_t(0, 50), 0);
        client.select_dest_table_add_with_to_app_act(0, dev_tgt, 
                ccDSM_select_dest_match_spec_t(60, 85), 0);
        client.select_dest_table_add_with_to_agent_act(0, dev_tgt, 
                ccDSM_select_dest_match_spec_t(90, 100), 0);

        # check_state_tbl
        client.check_state_tbl_table_add_with_check_succ_act(0, dev_tgt,
                ccDSM_check_state_tbl_match_spec_t(0, 0));
        
        # after_unlock_tbl
        client.after_unlock_tbl_table_add_with_after_unlock_act(0, dev_tgt, 
               ccDSM_after_unlock_tbl_match_spec_t(R_UNLOCK))
        client.after_unlock_tbl_table_add_with_after_unlock_act(0, dev_tgt, 
               ccDSM_after_unlock_tbl_match_spec_t(R_READ_MISS_UNLOCK))
        # client.after_unlock_tbl_table_add_with_drop_act(0, dev_tgt, 
        #         ccDSM_after_unlock_tbl_match_spec_t(R_UNLOCK))
        # client.after_unlock_tbl_table_add_with_drop_act(0, dev_tgt, 
        #         ccDSM_after_unlock_tbl_match_spec_t(R_READ_MISS_UNLOCK))
        client.after_unlock_tbl_table_add_with_drop_act(0, dev_tgt,
                ccDSM_after_unlock_tbl_match_spec_t(R_UNLOCK_EVICT))


        return

        # lock_tlb
        client.lock_tbl_1_table_add_with_r_lock_tbl_act_1(0, dev_tgt,
                ccDSM_lock_tbl_1_match_spec_t(R_READ_MISS));
        client.lock_tbl_1_table_add_with_w_lock_tbl_act_1(0, dev_tgt,
                ccDSM_lock_tbl_1_match_spec_t(R_WRITE_MISS));
        client.lock_tbl_1_table_add_with_w_lock_tbl_act_1(0, dev_tgt,
                ccDSM_lock_tbl_1_match_spec_t(R_WRITE_SHARED));
        client.lock_tbl_1_table_add_with_w_lock_tbl_act_1(0, dev_tgt,
                ccDSM_lock_tbl_1_match_spec_t(R_EVICT_SHARED));
        client.lock_tbl_1_table_add_with_w_lock_tbl_act_1(0, dev_tgt,
                ccDSM_lock_tbl_1_match_spec_t(R_EVICT_DIRTY));
        client.lock_tbl_1_table_add_with_w_lock_tbl_act_1(0, dev_tgt,
                ccDSM_lock_tbl_1_match_spec_t(DEL_DIR));        

        client.lock_tbl_3_table_add_with_r_lock_tbl_act_3(0, dev_tgt,
                ccDSM_lock_tbl_3_match_spec_t(R_READ_MISS));
        client.lock_tbl_3_table_add_with_w_lock_tbl_act_3(0, dev_tgt,
                ccDSM_lock_tbl_3_match_spec_t(R_WRITE_MISS));
        client.lock_tbl_3_table_add_with_w_lock_tbl_act_3(0, dev_tgt,
                ccDSM_lock_tbl_3_match_spec_t(R_WRITE_SHARED));
        client.lock_tbl_3_table_add_with_w_lock_tbl_act_3(0, dev_tgt,
                ccDSM_lock_tbl_3_match_spec_t(R_EVICT_SHARED));
        client.lock_tbl_3_table_add_with_w_lock_tbl_act_3(0, dev_tgt,
                ccDSM_lock_tbl_3_match_spec_t(R_EVICT_DIRTY));
        client.lock_tbl_3_table_add_with_w_lock_tbl_act_3(0, dev_tgt,
                ccDSM_lock_tbl_3_match_spec_t(DEL_DIR));

        client.lock_tbl_5_table_add_with_r_lock_tbl_act_5(0, dev_tgt,
                ccDSM_lock_tbl_5_match_spec_t(R_READ_MISS));
        client.lock_tbl_5_table_add_with_w_lock_tbl_act_5(0, dev_tgt,
                ccDSM_lock_tbl_5_match_spec_t(R_WRITE_MISS));
        client.lock_tbl_5_table_add_with_w_lock_tbl_act_5(0, dev_tgt,
                ccDSM_lock_tbl_5_match_spec_t(R_WRITE_SHARED));
        client.lock_tbl_5_table_add_with_w_lock_tbl_act_5(0, dev_tgt,
                ccDSM_lock_tbl_5_match_spec_t(R_EVICT_SHARED));
        client.lock_tbl_5_table_add_with_w_lock_tbl_act_5(0, dev_tgt,
                ccDSM_lock_tbl_5_match_spec_t(R_EVICT_DIRTY));
        client.lock_tbl_5_table_add_with_w_lock_tbl_act_5(0, dev_tgt,
                ccDSM_lock_tbl_5_match_spec_t(DEL_DIR));

        client.lock_tbl_7_table_add_with_r_lock_tbl_act_7(0, dev_tgt,
                ccDSM_lock_tbl_7_match_spec_t(R_READ_MISS));
        client.lock_tbl_7_table_add_with_w_lock_tbl_act_7(0, dev_tgt,
                ccDSM_lock_tbl_7_match_spec_t(R_WRITE_MISS));
        client.lock_tbl_7_table_add_with_w_lock_tbl_act_7(0, dev_tgt,
                ccDSM_lock_tbl_7_match_spec_t(R_WRITE_SHARED));
        client.lock_tbl_7_table_add_with_w_lock_tbl_act_7(0, dev_tgt,
                ccDSM_lock_tbl_7_match_spec_t(R_EVICT_SHARED));
        client.lock_tbl_7_table_add_with_w_lock_tbl_act_7(0, dev_tgt,
                ccDSM_lock_tbl_7_match_spec_t(R_EVICT_DIRTY));
        client.lock_tbl_7_table_add_with_w_lock_tbl_act_7(0, dev_tgt,
                ccDSM_lock_tbl_7_match_spec_t(DEL_DIR));

        client.lock_tbl_9_table_add_with_r_lock_tbl_act_9(0, dev_tgt,
                ccDSM_lock_tbl_9_match_spec_t(R_READ_MISS));
        client.lock_tbl_9_table_add_with_w_lock_tbl_act_9(0, dev_tgt,
                ccDSM_lock_tbl_9_match_spec_t(R_WRITE_MISS));
        client.lock_tbl_9_table_add_with_w_lock_tbl_act_9(0, dev_tgt,
                ccDSM_lock_tbl_9_match_spec_t(R_WRITE_SHARED));
        client.lock_tbl_9_table_add_with_w_lock_tbl_act_9(0, dev_tgt,
                ccDSM_lock_tbl_9_match_spec_t(R_EVICT_SHARED));
        client.lock_tbl_9_table_add_with_w_lock_tbl_act_9(0, dev_tgt,
                ccDSM_lock_tbl_9_match_spec_t(R_EVICT_DIRTY));
        client.lock_tbl_9_table_add_with_w_lock_tbl_act_9(0, dev_tgt,
                ccDSM_lock_tbl_9_match_spec_t(DEL_DIR));


    except Thrift.TException, ex:
          print "%s" % (ex.message)

if __name__ == '__main__':
    addHost()
