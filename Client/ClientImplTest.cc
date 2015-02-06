/* Copyright (c) 2012-2014 Stanford University
 * Copyright (c) 2015 Diego Ongaro
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <gtest/gtest.h>

#include "Client/ClientImpl.h"
#include "Client/LeaderRPCMock.h"
#include "Core/ProtoBuf.h"
#include "Core/StringUtil.h"
#include "Protocol/Common.h"
#include "RPC/Server.h"
#include "RPC/ServiceMock.h"
#include "build/Protocol/Client.pb.h"

// Most of the tests for ClientImpl are in ClientTest.cc.

namespace LogCabin {
namespace {

using Core::ProtoBuf::fromString;
typedef Client::ClientImpl::TimePoint TimePoint;

class ClientClientImplExactlyOnceTest : public ::testing::Test {
  public:
    typedef Client::LeaderRPCMock::OpCode OpCode;
    typedef Protocol::Client::ExactlyOnceRPCInfo RPCInfo;
    ClientClientImplExactlyOnceTest()
        : client()
        , mockRPC()
        , rpcInfo1()
        , rpcInfo2()
    {
        mockRPC = new Client::LeaderRPCMock();
        client.leaderRPC = std::unique_ptr<Client::LeaderRPCBase>(mockRPC);

        mockRPC->expect(OpCode::OPEN_SESSION,
            fromString<Protocol::Client::OpenSession::Response>(
                        "client_id: 3"));
        rpcInfo1 = client.exactlyOnceRPCHelper.getRPCInfo(TimePoint::max());
        rpcInfo2 = client.exactlyOnceRPCHelper.getRPCInfo(TimePoint::max());
    }
    Client::ClientImpl client;
    Client::LeaderRPCMock* mockRPC;
    RPCInfo rpcInfo1;
    RPCInfo rpcInfo2;

    ClientClientImplExactlyOnceTest(
        const ClientClientImplExactlyOnceTest&) = delete;
    ClientClientImplExactlyOnceTest&
    operator=(const ClientClientImplExactlyOnceTest&) = delete;
};


TEST_F(ClientClientImplExactlyOnceTest, getRPCInfo) {
    EXPECT_EQ((std::set<uint64_t>{1, 2}),
              client.exactlyOnceRPCHelper.outstandingRPCNumbers);
    EXPECT_EQ(3U, client.exactlyOnceRPCHelper.clientId);
    EXPECT_EQ(3U, client.exactlyOnceRPCHelper.nextRPCNumber);
    EXPECT_EQ(3U, rpcInfo1.client_id());
    EXPECT_EQ(1U, rpcInfo1.first_outstanding_rpc());
    EXPECT_EQ(1U, rpcInfo1.rpc_number());
    EXPECT_EQ(3U, rpcInfo2.client_id());
    EXPECT_EQ(1U, rpcInfo2.first_outstanding_rpc());
    EXPECT_EQ(2U, rpcInfo2.rpc_number());
}

TEST_F(ClientClientImplExactlyOnceTest, getRPCInfo_timeout) {
    Client::ClientImpl client2;
    Client::LeaderRPCMock* mockRPC2 = new Client::LeaderRPCMock();
    client2.leaderRPC = std::unique_ptr<Client::LeaderRPCBase>(mockRPC2);

    rpcInfo1 = client2.exactlyOnceRPCHelper.getRPCInfo(TimePoint::min());
    EXPECT_EQ(0U, client2.exactlyOnceRPCHelper.clientId);
    EXPECT_EQ(0U, rpcInfo1.client_id());

    mockRPC2->expect(OpCode::OPEN_SESSION,
        fromString<Protocol::Client::OpenSession::Response>(
                    "client_id: 4"));
    rpcInfo2 = client2.exactlyOnceRPCHelper.getRPCInfo(TimePoint::max());
    EXPECT_EQ(4U, client2.exactlyOnceRPCHelper.clientId);
    EXPECT_EQ(4U, rpcInfo2.client_id());
}

TEST_F(ClientClientImplExactlyOnceTest, doneWithRPC) {
    client.exactlyOnceRPCHelper.doneWithRPC(rpcInfo1);
    EXPECT_EQ((std::set<uint64_t>{2}),
              client.exactlyOnceRPCHelper.outstandingRPCNumbers);
    RPCInfo rpcInfo3 = client.exactlyOnceRPCHelper.getRPCInfo(TimePoint::max());
    EXPECT_EQ(2U, rpcInfo3.first_outstanding_rpc());
    client.exactlyOnceRPCHelper.doneWithRPC(rpcInfo3);
    EXPECT_EQ((std::set<uint64_t>{2}),
              client.exactlyOnceRPCHelper.outstandingRPCNumbers);
    RPCInfo rpcInfo4 = client.exactlyOnceRPCHelper.getRPCInfo(TimePoint::max());
    EXPECT_EQ(2U, rpcInfo4.first_outstanding_rpc());
    client.exactlyOnceRPCHelper.doneWithRPC(rpcInfo2);
    EXPECT_EQ((std::set<uint64_t>{4}),
              client.exactlyOnceRPCHelper.outstandingRPCNumbers);
    RPCInfo rpcInfo5 = client.exactlyOnceRPCHelper.getRPCInfo(TimePoint::max());
    EXPECT_EQ(4U, rpcInfo5.first_outstanding_rpc());
}

// This test is timing-sensitive. Not sure how else to do it.
TEST_F(ClientClientImplExactlyOnceTest, keepAliveThreadMain_TimingSensitive) {
    std::string disclaimer("This test depends on timing, so failures are "
                           "likely under heavy load, valgrind, etc.");
    EXPECT_EQ(1U, mockRPC->requestLog.size());
    for (uint64_t i = 0; i < 6; ++i) {
        mockRPC->expect(OpCode::READ_WRITE_TREE,
            fromString<Protocol::Client::ReadWriteTree::Response>(
                        "status: CONDITION_NOT_MET, error: 'err'"));
    }
    client.exactlyOnceRPCHelper.keepAliveIntervalMs = 2;
    client.exactlyOnceRPCHelper.keepAliveCV.notify_all();
    // in 2ms, 4ms, 6ms, 8ms, 10ms
    usleep(11000);
    EXPECT_EQ(6U, mockRPC->requestLog.size()) << disclaimer;

    // Disable heartbeat.
    client.exactlyOnceRPCHelper.keepAliveIntervalMs = 0;
    client.exactlyOnceRPCHelper.keepAliveCV.notify_all();
    usleep(3000);
    EXPECT_EQ(6U, mockRPC->requestLog.size()) << disclaimer;

    // Now enable but "make a request" ourselves to prevent heartbeat.
    client.exactlyOnceRPCHelper.getRPCInfo(TimePoint::max());
    client.exactlyOnceRPCHelper.keepAliveIntervalMs = 10;
    client.exactlyOnceRPCHelper.keepAliveCV.notify_all();
    usleep(7500);
    client.exactlyOnceRPCHelper.getRPCInfo(TimePoint::max());
    usleep(6000);
    EXPECT_EQ(6U, mockRPC->requestLog.size()) << disclaimer;
    usleep(6000);
    EXPECT_EQ(7U, mockRPC->requestLog.size()) << disclaimer;
}

class ClientClientImplTest : public ::testing::Test {
    ClientClientImplTest()
        : client()
    {
        client.rpcProtocolVersion = 1;
        client.init("127.0.0.1");
    }

    Client::ClientImpl client;
};

class ClientClientImplServiceMockTest : public ClientClientImplTest {
  public:
    ClientClientImplServiceMockTest()
        : service()
        , server()
    {
        service = std::make_shared<RPC::ServiceMock>();
        server.reset(new RPC::Server(client.eventLoop,
                                     Protocol::Common::MAX_MESSAGE_LENGTH));
        RPC::Address address("127.0.0.1", Protocol::Common::DEFAULT_PORT);
        address.refresh(RPC::Address::TimePoint::max());
        EXPECT_EQ("", server->bind(address));
        server->registerService(Protocol::Common::ServiceId::CLIENT_SERVICE,
                                service, 1);
    }
    ~ClientClientImplServiceMockTest()
    {
    }

    std::shared_ptr<RPC::ServiceMock> service;
    std::unique_ptr<RPC::Server> server;
};

TEST_F(ClientClientImplServiceMockTest, getServerStats) {
    Protocol::Client::GetServerStats::Request request;
    Protocol::Client::GetServerStats::Response response;
    Protocol::ServerStats& ret = *response.mutable_server_stats();
    ret.set_server_id(3);

    service->closeSession(Protocol::Client::OpCode::GET_SERVER_STATS,
                          request);
    service->reply(Protocol::Client::OpCode::GET_SERVER_STATS,
                   request, response);
    Protocol::ServerStats stats;
    Client::Result result = client.getServerStats("127.0.0.1",
                                                  TimePoint::max(),
                                                  stats);
    EXPECT_EQ(Client::Status::OK, result.status);
    EXPECT_EQ("server_id: 3",
              stats);
}

TEST_F(ClientClientImplTest, getServerStats_timeout) {
    Protocol::ServerStats stats;
    Client::Result result = client.getServerStats("127.0.0.1",
                                                  TimePoint::min(),
                                                  stats);
    EXPECT_EQ(Client::Status::TIMEOUT, result.status);
    EXPECT_EQ("Client-specified timeout elapsed", result.error);
    EXPECT_EQ("", stats);
}

TEST_F(ClientClientImplTest, makeDirectory_getRPCInfo_timeout) {
    EXPECT_EQ(0U, client.exactlyOnceRPCHelper.clientId);
    Client::Result result =
        client.makeDirectory("/foo",
                             "/",
                             Client::Condition {"", ""},
                             TimePoint::min());
    EXPECT_EQ(Client::Status::TIMEOUT, result.status);
    EXPECT_EQ("Client-specified timeout elapsed", result.error);
    EXPECT_EQ(0U, client.exactlyOnceRPCHelper.clientId);
}

TEST_F(ClientClientImplTest, makeDirectory_timeout) {
    client.exactlyOnceRPCHelper.clientId = 4;
    Client::Result result =
        client.makeDirectory("/foo",
                             "/",
                             Client::Condition {"", ""},
                             TimePoint::min());
    EXPECT_EQ(Client::Status::TIMEOUT, result.status);
    EXPECT_EQ("Client-specified timeout elapsed", result.error);
}

TEST_F(ClientClientImplTest, listDirectory_timeout) {
    std::vector<std::string> children { "hi" };
    Client::Result result =
        client.listDirectory("/",
                             "/",
                             Client::Condition {"", ""},
                             TimePoint::min(),
                             children);
    EXPECT_EQ(Client::Status::TIMEOUT, result.status);
    EXPECT_EQ("Client-specified timeout elapsed", result.error);
    EXPECT_EQ(std::vector<std::string> { }, children);
}

class KeepAliveThreadMain_cancel_Helper {
    explicit KeepAliveThreadMain_cancel_Helper(
            Client::ClientImpl::ExactlyOnceRPCHelper& exactlyOnceRPCHelper)
        : exactlyOnceRPCHelper(exactlyOnceRPCHelper)
        , iter(0)
    {
    }
    void operator()() {
        ++iter;
        if (iter == 2) {
            EXPECT_TRUE(exactlyOnceRPCHelper.keepAliveCall.get() != NULL);
            exactlyOnceRPCHelper.keepAliveCall->cancel();
            exactlyOnceRPCHelper.exiting = true;
        }
    }
    Client::ClientImpl::ExactlyOnceRPCHelper& exactlyOnceRPCHelper;
    uint64_t iter;
};


TEST_F(ClientClientImplExactlyOnceTest, keepAliveThreadMain_cancel) {
    client.exactlyOnceRPCHelper.exit();
    client.exactlyOnceRPCHelper.exiting = false;
    mockRPC->expect(OpCode::READ_WRITE_TREE,
        fromString<Protocol::Client::ReadWriteTree::Response>(
                    ""));
    client.exactlyOnceRPCHelper.lastKeepAliveStart = TimePoint::min();
    client.exactlyOnceRPCHelper.keepAliveIntervalMs = 200;
    KeepAliveThreadMain_cancel_Helper helper(client.exactlyOnceRPCHelper);
    client.exactlyOnceRPCHelper.mutex.callback = std::ref(helper);
    client.exactlyOnceRPCHelper.keepAliveThreadMain();
    client.exactlyOnceRPCHelper.mutex.callback = std::function<void()>();
    EXPECT_EQ(4U, helper.iter);
}

using Client::Result;
using Client::Status;

#define EXPECT_OK(c) do { \
    Result result = (c); \
    EXPECT_EQ(Status::OK, result.status) << result.error; \
} while (0)

class ClientClientImplCanonicalizeTest : public ::testing::Test {
    ClientClientImplCanonicalizeTest()
        : client()
    {
    }
    Client::ClientImpl client;
};

TEST_F(ClientClientImplCanonicalizeTest, canonicalize)
{
    std::string real;
    Result result;

    // path is absolute, working directory is don't care
    EXPECT_OK(client.canonicalize("/foo/bar/baz", "invalid", real));
    EXPECT_EQ("/foo/bar/baz", real);

    // path is relative, working directory is broken
    result = client.canonicalize("bar/baz", "invalid", real);
    EXPECT_EQ(Status::INVALID_ARGUMENT, result.status);
    EXPECT_EQ("Can't use relative path 'bar/baz' from working directory "
              "'invalid' (working directory should be an absolute path)",
              result.error);

    // path is relative, working directory is absolute
    EXPECT_OK(client.canonicalize("bar/baz", "/foo", real));
    EXPECT_EQ("/foo/bar/baz", real);

    // path is relative with ., ..
    EXPECT_OK(client.canonicalize(".././bar", "/foo", real));
    EXPECT_EQ("/bar", real);

    // path is relative with too many ..
    result = client.canonicalize("bar/../..", "/", real);
    EXPECT_EQ(Status::INVALID_ARGUMENT, result.status);
    EXPECT_EQ("Path 'bar/../..' from working directory '/' attempts to look "
              "up directory above root ('/')",
              result.error);

    // path ends up at /
    EXPECT_OK(client.canonicalize(".", "/", real));
    EXPECT_EQ("/", real);

    // leading or trailing slash, duplicate slashes
    EXPECT_OK(client.canonicalize("bar////baz//", "///", real));
    EXPECT_EQ("/bar/baz", real);
}


} // namespace LogCabin::<anonymous>
} // namespace LogCabin
