#include <gtest/gtest.h>

#define private public
#include "rpc.h"

// These tests never run event loop, so SM pkts sent by Rpc have no consequence
namespace erpc {

// An Rpc with no established sessions
class RpcTest : public ::testing::Test {
 public:
  static constexpr size_t kTestUdpPort = 3185;
  static constexpr size_t kTestPhyPort = 0;
  static constexpr size_t kTestRpcId = 0;
  static constexpr size_t kTestNumBgThreads = 0;
  static constexpr size_t kTestNumaNode = 0;
  static constexpr size_t kTestUniqToken = 42;
  static constexpr auto kTestTransportType =
      Transport::TransportType::kInfiniBand;

  static void sm_handler(int, SmEventType, SmErrType, void *) {}

  RpcTest() {
    nexus = new Nexus("localhost", kTestUdpPort, kTestNumBgThreads);
    rt_assert(nexus != nullptr, "RpcTest: Failed to create nexus");
    nexus->drop_all_rx();

    rpc = new Rpc<IBTransport>(nexus, nullptr, kTestRpcId, sm_handler,
                               kTestPhyPort, kTestNumaNode);
    rt_assert(rpc != nullptr, "RpcTest: Failed to create Rpc");

    rpc->udp_client.enable_recording();

    // int sn = rpc->create_session("localhost", kServerRpcId, kPhyPort);
    // rt_assert(sn == 0, "RpcTest: Failed to create session");
  }

  ~RpcTest() {
    delete rpc;
    delete nexus;
  }

  SessionEndpoint gen_session_endpoint(uint8_t rpc_id, uint16_t session_num) {
    rt_assert(rpc != nullptr,
              "RpcTest: gen_session_endpoint() requires valid Rpc");

    SessionEndpoint se;
    se.transport_type = kTestTransportType;
    strcpy(se.hostname, "localhost");
    se.phy_port = kTestPhyPort;
    se.rpc_id = rpc_id;
    se.session_num = session_num;

    // Any routing info that's resolvable is fine
    rpc->transport->fill_local_routing_info(&se.routing_info);
    return se;
  }

  Nexus *nexus = nullptr;
  Rpc<IBTransport> *rpc = nullptr;
};

TEST_F(RpcTest, handle_connect_req_st) {
  auto server = gen_session_endpoint(kTestRpcId, kInvalidSessionNum);
  auto client = gen_session_endpoint(kTestRpcId + 1, /* session number */ 0);
  SmPkt conn_req(SmPktType::kConnectReq, SmErrType::kNoError, kTestUniqToken,
                 client, server);

  // Process first connect request - session is created
  rpc->handle_connect_req_st(conn_req);
  ASSERT_EQ(rpc->session_vec.size(), 1);
  SmPkt resp = rpc->udp_client.sent_queue_pop();
  ASSERT_EQ(resp.pkt_type, SmPktType::kConnectResp);
  ASSERT_EQ(resp.err_type, SmErrType::kNoError);

  // Process connect request again.
  // New session is not created and response is re-sent.
  rpc->handle_connect_req_st(conn_req);
  ASSERT_EQ(rpc->session_vec.size(), 1);
  resp = rpc->udp_client.sent_queue_pop();
  ASSERT_EQ(resp.pkt_type, SmPktType::kConnectResp);
  ASSERT_EQ(resp.err_type, SmErrType::kNoError);

  // Artifically destroy the session and re-handle connect request.
  // New session is not created and response is not sent.
  Session *session = rpc->session_vec[0];
  rpc->session_vec[0] = nullptr;
  rpc->handle_connect_req_st(conn_req);
  ASSERT_EQ(rpc->udp_client.sent_queue.empty(), true);
  rpc->session_vec[0] = session;  // Restore the session
}

}  // End erpc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
