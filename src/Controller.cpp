#include "Controller.h"
#include "Common.h"

#include <fstream>

#define ADD_ENTRY_TO_SWITCH_NO_LOCK(i)                                         \
  ccDSM_check_dir_exist_##i##_match_spec_t k;                                  \
  k.message_dirKey = toBigEndian32(dirKey);                                    \
  k.message_dirNodeID = myNodeID;                                              \
  ccDSM_check_dir_exist_act_##i##_action_spec_t v;                             \
  v.action_index = index;                                                      \
                                                                               \
  try {                                                                        \
    client->check_dir_exist_##i##_table_add_with_check_dir_exist_act_##i(      \
        session, dev_tgt, k, v);                                               \
  } catch (TException & tx) {                                                  \
    Debug::notifyError("Thrift error addEntry: %s", tx.what());                \
    return false;                                                              \
  }                                                                            \
  return true;

#define ADD_ENTRY_TO_SWITCH(i, j)                                              \
  assert(i == j - 1);                                                          \
  ccDSM_check_dir_exist_##i##_match_spec_t k;                                  \
  k.message_dirKey = toBigEndian32(dirKey);                                    \
  k.message_dirNodeID = myNodeID;                                              \
                                                                               \
  ccDSM_check_dir_exist_act_##i##_action_spec_t v;                             \
  v.action_index = index;                                                      \
                                                                               \
  try {                                                                        \
    client->register_write_lock_##j(session, dev_tgt, index, 1);               \
    client->check_dir_exist_##i##_table_add_with_check_dir_exist_act_##i(      \
        session, dev_tgt, k, v);                                               \
  } catch (TException & tx) {                                                  \
    Debug::notifyError("Thrift error addEntry: %s", tx.what());                \
    return false;                                                              \
  }                                                                            \
  return true;

Controller::Controller(uint16_t myNodeID, uint16_t myPort)
    : myNodeID(myNodeID), myPort(myPort) {
  dev_tgt.dev_id = dev_id;
  dev_tgt.dev_pipe_id = pipe_id;

  socket = boost::shared_ptr<TSocket>(new TSocket("192.168.189.34", 9090));
  transport = boost::shared_ptr<TTransport>(new TBufferedTransport(socket));
  tBinProtocol = boost::shared_ptr<TProtocol>(new TBinaryProtocol(transport));
  protocol = boost::shared_ptr<TProtocol>(
      new TMultiplexedProtocol(tBinProtocol, "ccDSM"));

  client = boost::shared_ptr<ccDSMClient>(new ccDSMClient(protocol));
  transport->open();
}

Controller::~Controller() { transport->close(); }

void Controller::dirQP(uint16_t qpn, uint8_t dirID) {

  // printf("dirID %d, qpn %d\n", qpn, dirID);

  ccDSM_dir_set_port_tbl_match_spec_t k;
  k.message_dirNodeID = myNodeID;
  k.route_md_dir_id = dirID;

  ccDSM_set_port_and_qpn_action_spec_t v;
  v.action_port = myPort;
  v.action_qpn = toBigEndian16(qpn);

  // Debug::notifyError("dir-------nodeID:%d  port: %d, qpn: %d", myNodeID,
  // myPort, qpn);

  try {
    client->dir_set_port_tbl_table_add_with_set_port_and_qpn(session, dev_tgt,
                                                             k, v);
  } catch (TException &tx) {
    Debug::notifyError("Thrift error dirQP: %s", tx.what());
  }
}

void Controller::appQP(uint16_t qpn, uint8_t appID) {
  ccDSM_app_set_port_tbl_match_spec_t k1;
  k1.message_nodeID = myNodeID;
  k1.message_appID = appID;

  ccDSM_set_port_and_qpn_action_spec_t v;
  v.action_port = myPort;
  v.action_qpn = toBigEndian16(qpn);

  // Debug::notifyError("node ID %d, appID %d, port: %d, qpn: %d", myNodeID,
  // appID, myPort, qpn);

  try {
    client->app_set_port_tbl_table_add_with_set_port_and_qpn(session, dev_tgt,
                                                             k1, v);
  } catch (TException &tx) {
    Debug::notifyError("Thrift error appQP: %s", tx.what());
  }
}

void Controller::agentQP(uint16_t qpn, uint8_t agentID) {
  ccDSM_agent_set_tbl_match_spec_t k1;
  k1.eg_intr_md_egress_port = myPort;
  k1.route_md_agent_id = (agentID);

  ccDSM_set_qpn_action_spec_t v1;
  v1.action_qpn = toBigEndian16(qpn);

  ccDSM_agent_set_port_tbl_match_spec_t k2;
  k2.message_mybitmap = toBigEndian16(myNodeID);
  k2.route_md_agent_id = (agentID);

  ccDSM_set_port_and_qpn_action_spec_t v2;
  v2.action_port = myPort;
  v2.action_qpn = toBigEndian16(qpn);

  // Debug::notifyError("agent --- port %d, qpn %d", myPort, qpn);

  try {
    client->agent_set_tbl_table_add_with_set_qpn(session, dev_tgt, k1, v1);
    client->agent_set_port_tbl_table_add_with_set_port_and_qpn(session, dev_tgt,
                                                               k2, v2);
  } catch (TException &tx) {
    Debug::notifyError("Thrift error agentQP: %s", tx.what());
  }
}

// NEED BIG FIXME
bool Controller::addEntry(uint32_t dirKey, uint32_t index, uint8_t tableID) {
  return false;

  // switch (tableID) {
  // case 0: {
  //   ADD_ENTRY_TO_SWITCH(0, 1);
  // }
  // case 1: {
  //   ADD_ENTRY_TO_SWITCH(2, 3);
  // }
  // case 2: {
  //   ADD_ENTRY_TO_SWITCH(4, 5);
  // }
  // case 3: {
  //   ADD_ENTRY_TO_SWITCH(6, 7);
  // }
  // case 4: {
  //   ADD_ENTRY_TO_SWITCH(8, 9);
  // }
  // }
  // assert(false);
}

bool Controller::addEntryWithOutLock(uint32_t dirKey, uint32_t index,
                                     uint8_t tableID) {

  // switch (tableID) {
  // case 0: {
  //   ADD_ENTRY_TO_SWITCH_NO_LOCK(0);
  // }
  // case 1: {
  //   ADD_ENTRY_TO_SWITCH_NO_LOCK(2);
  // }
  // case 2: {
  //   ADD_ENTRY_TO_SWITCH_NO_LOCK(4);
  // }
  // case 3: {
  //   ADD_ENTRY_TO_SWITCH_NO_LOCK(6);
  // }
  // case 4: {
  //   ADD_ENTRY_TO_SWITCH_NO_LOCK(8);
  // }
  // }
  assert(false);
}

void Controller::reset() {

  try {
    for (uint16_t nodeID = 0; nodeID < 8; ++nodeID) {
      for (uint8_t dirID = 0; dirID < NR_DIRECTORY; ++dirID) {
        ccDSM_dir_set_port_tbl_match_spec_t k;
        k.message_dirNodeID = myNodeID;
        k.route_md_dir_id = dirID;

        // client->dir_set_po
      }
    }
  } catch (TException &tx) {
    Debug::notifyError("Thrift error delete : %s", tx.what());
  }

  try {
    client->dir_set_port_tbl_table_reset_default_entry(session, dev_tgt);
    client->app_set_port_tbl_table_reset_default_entry(session, dev_tgt);
    client->agent_set_tbl_table_reset_default_entry(session, dev_tgt);
    client->agent_set_port_tbl_table_reset_default_entry(session, dev_tgt);

    client->register_reset_all_dir_0(session, dev_tgt);
    client->register_reset_all_dir_2(session, dev_tgt);
    client->register_reset_all_dir_4(session, dev_tgt);
    client->register_reset_all_dir_6(session, dev_tgt);
    client->register_reset_all_dir_8(session, dev_tgt);

    client->register_reset_all_lock_1(session, dev_tgt);
    client->register_reset_all_lock_3(session, dev_tgt);
    client->register_reset_all_lock_5(session, dev_tgt);
    client->register_reset_all_lock_7(session, dev_tgt);
    client->register_reset_all_lock_9(session, dev_tgt);

    client->register_reset_all_state_1(session, dev_tgt);
    client->register_reset_all_state_3(session, dev_tgt);
    client->register_reset_all_state_5(session, dev_tgt);
    client->register_reset_all_state_7(session, dev_tgt);
    client->register_reset_all_state_9(session, dev_tgt);

    client->register_reset_all_bitmap_1(session, dev_tgt);
    client->register_reset_all_bitmap_3(session, dev_tgt);
    client->register_reset_all_bitmap_5(session, dev_tgt);
    client->register_reset_all_bitmap_7(session, dev_tgt);
    client->register_reset_all_bitmap_9(session, dev_tgt);

  } catch (TException &tx) {
    Debug::notifyError("Thrift error agentQP: %s", tx.what());
  }
}

void Controller::benchmark() {
  static const uint64_t S2N = 1000000000;
  static const uint64_t t = 100000;

  timespec s, e;

  clock_gettime(CLOCK_REALTIME, &s);
  for (size_t i = 0; i < t; ++i) {
    client->register_write_lock_1(session, dev_tgt, i % 19999, i % 1000);
  }
  clock_gettime(CLOCK_REALTIME, &e);

  uint64_t cap = e.tv_nsec + e.tv_sec * S2N - s.tv_nsec - s.tv_sec * S2N;

  Debug::notifyInfo("one rpc time: %lluns", cap / t);
}
