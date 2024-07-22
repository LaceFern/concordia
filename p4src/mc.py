#!/usr/bin/python

import struct, socket
import sys
import memcache

PRE = "/home/wq/nfs/ccDSM/"

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

def portToPipe(port):
    return port >> 7

def portToPipeLocalId(port):
    return port & 0x7F

def portToBitIdx(port):
    pipe = portToPipe(port)
    index = portToPipeLocalId(port)
    return 72 * pipe + index

def BitIdxToPort(index):
    pipe = index / 72
    local_port = index % 72
    return (pipe << 7) | local_port

def set_port_map(indicies):
    bit_map = [0] * ((288+7)/8)
    for i in indicies:
        index = portToBitIdx(i)
        bit_map[index/8] = (bit_map[index/8] | (1 << (index%8))) & 0xFF
    return bytes_to_string(bit_map)

def set_lag_map(indicies):
    bit_map = [0] * ((256+7)/8)
    for i in indicies:
        bit_map[i/8] = (bit_map[i/8] | (1 << (i%8))) & 0xFF
    return bytes_to_string(bit_map)

dev_id = 0
pipeID = hex_to_i16(0xFFFF)
dev_tgt = DevTarget_t(dev_id, pipeID);

Prefix = "SPre"
f = open(PRE + "memcached.conf")
memcIP = f.readline()
memPort = f.readline()
memc = memcache.Client([memcIP + ":" + memPort], debug=0)


def Hosts():

    host = {}
    f = open(PRE + "host")
    line = f.readline()
    while line:
        ip, mac, port = line.split(" ")
        host[ip] = int(port)
        line = f.readline()
    
    return host

def Memcached():

    memcDict = {}

    k = 0
    ip = memc.get(Prefix + str(k))
    while ip is not None:
        memcDict[k] = ip
        k = k + 1
        ip = memc.get(Prefix + str(k))

    return memcDict

def GetMap():
    host = Hosts()
    memcDict = Memcached()
    return {k : host[v] for k, v in memcDict.items()}

def toBig(v):
    return struct.unpack('i', struct.pack('>I', v))[0]

def addMC():

    Dict = GetMap()
    print Dict

    nr = 2 ** len(Dict)
    
    mgrp_hdl_list = []
    l1_hdl_list = []

    try:
        transport = TSocket.TSocket('127.0.0.1', 9090)
        transport = TTransport.TBufferedTransport(transport)

        protocol = TBinaryProtocol.TBinaryProtocol(transport)
        protocol = TMultiplexedProtocol(protocol, "mc");
        client = mc.Client(protocol)

        transport.open()

        mc_sess_hdl = client.mc_create_session()
        for i in range(0, nr):
            l = []

            for k in range(0, len(Dict)):
                if ((i >> k) & 1) == 1:
                    l.append(Dict[k])

            #  mgrp_hdl = client.mc_mgrp_create(mc_sess_hdl, dev_id, 
                    #  (i >> 8) | ((i & 0xff) << 8));
            mgrp_hdl = client.mc_mgrp_create(mc_sess_hdl, dev_id, 
                   struct.unpack('h', struct.pack('>h', i))[0])

            mgrp_hdl_list.append(mgrp_hdl)

            l1_hdl = client.mc_node_create(mc_sess_hdl, dev_id, 233,
                    set_port_map(l),
                    set_lag_map([]))

            l1_hdl_list.append(l1_hdl)

            client.mc_associate_node(mc_sess_hdl, dev_id, mgrp_hdl, l1_hdl, 0, 0);


        client.mc_complete_operations(mc_sess_hdl)


    except Thrift.TException, ex:
          print "%s" % (ex.message)

    # with open('/home/workspace/DSM/ccDSM/p4src/mc_dict', 'w') as f :
    #     f.writelines(str(Dict) + "\n")
    #     f.writelines(str(mc_sess_hdl)  + "\n")
    #     f.writelines(str(mgrp_hdl_list) + "\n")
    #     f.writelines(str(l1_hdl_list) + "\n")
        


if __name__ == '__main__':
    addMC()
