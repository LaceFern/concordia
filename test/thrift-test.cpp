#include <iostream>

#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TTransportUtils.h>

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/protocol/TMultiplexedProtocol.h>

#include "ccDSM.h"
#include "res_types.h"
#include "p4_pd_rpc_types.h"

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;

using namespace p4_pd_rpc;
using namespace res_pd_rpc;
using boost::shared_ptr;

int32_t session = 0;
int32_t dev_id = 0;
int16_t pipe_id = 0xFFFF;
DevTarget_t dev_tgt;

int main(int argc, char **argv) {

    dev_tgt.dev_id = dev_id;
    dev_tgt.dev_pipe_id = pipe_id;

    boost::shared_ptr<TSocket> socket(new TSocket("192.168.189.34", 9090));
    boost::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
    boost::shared_ptr<TProtocol> protocol__(new TBinaryProtocol(transport));
    boost::shared_ptr<TProtocol> protocol(new TMultiplexedProtocol(protocol__, "ccDSM"));

    ccDSMClient client(protocol);
    transport->open();

    try {
        // uint8_t addr[4] = {111, 2, 168, 192};
        ccDSM_ipv4_route_match_spec_t ip;
        // ip.ipv4_dstAddr = *(int *)addr;

        ccDSM_set_egr_action_spec_t port;
        port.action_port = 132;

        client.ipv4_route_table_add_with_set_egr(session, dev_tgt, ip, port);
                
    } catch (TException& tx) {
        std::cout << "Thrift error: " << tx.what() << std::endl;
    }

    transport->close();


    return 0;
}

