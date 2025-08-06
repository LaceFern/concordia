#ifndef __CONTROLLER_H__
#define __CONTROLLER_H__

#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TTransportUtils.h>

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/protocol/TMultiplexedProtocol.h>

#include "DSM.h"
#include "res_types.h"
#include "p4_pd_rpc_types.h"
#include "ccDSM.h"

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;

using namespace p4_pd_rpc;
using namespace res_pd_rpc;
using boost::shared_ptr;

class Controller {

   private:
    const static int32_t session = 0;
    const int32_t dev_id = 0;
    const int16_t pipe_id = 0xFFFF;
    DevTarget_t dev_tgt;

    boost::shared_ptr<TSocket> socket;
    boost::shared_ptr<TTransport> transport;
    boost::shared_ptr<TProtocol> tBinProtocol;
    boost::shared_ptr<TProtocol> protocol;
    boost::shared_ptr<ccDSMClient> client;

    uint16_t myNodeID;
    uint16_t myPort;
    std::string myIP;

    void readConfig();

   public:
    Controller(uint16_t myNodeID, uint16_t myPort);
    ~Controller();

    void agentQP(uint16_t qpn, uint8_t agentID);
    void appQP(uint16_t qpn, uint8_t appID);
    void dirQP(uint16_t qpn, uint8_t dirID);

    bool addEntry(uint32_t dirKey, uint32_t index, uint8_t tableID);
    bool addEntryWithOutLock(uint32_t dirKey, uint32_t index, uint8_t tableID);

    void benchmark();

    void reset();

};

#endif /* __CONTROLLER_H__ */
