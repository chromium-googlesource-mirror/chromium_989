// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/async_host_resolver.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "net/base/host_cache.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/sys_addrinfo.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const int kPortNum = 80;
const size_t kMaxTransactions = 2;
const size_t kMaxPendingRequests = 1;

void VerifyAddressList(const std::vector<const char*>& ip_addresses,
                       int port,
                       const AddressList& addrlist) {
  ASSERT_LT(0u, ip_addresses.size());
  ASSERT_NE(static_cast<addrinfo*>(NULL), addrlist.head());

  IPAddressNumber ip_number;
  const struct addrinfo* ainfo = addrlist.head();
  for (std::vector<const char*>::const_iterator i = ip_addresses.begin();
       i != ip_addresses.end(); ++i, ainfo = ainfo->ai_next) {
    ASSERT_NE(static_cast<addrinfo*>(NULL), ainfo);
    EXPECT_EQ(sizeof(struct sockaddr_in), ainfo->ai_addrlen);

    const struct sockaddr* sa = ainfo->ai_addr;
    const struct sockaddr_in* sa_in = (const struct sockaddr_in*) sa;
    EXPECT_TRUE(htons(port) == sa_in->sin_port);
    EXPECT_STREQ(*i, NetAddressToString(sa, ainfo->ai_addrlen).c_str());
  }
  ASSERT_EQ(static_cast<addrinfo*>(NULL), ainfo);
}

class MockDnsClient : public DnsClient,
                      public base::SupportsWeakPtr<MockDnsClient> {
 public:
  // Using WeakPtr to support cancellation.
  // All MockRequests succeed unless canceled or MockDnsClient is destroyed.
  class MockRequest : public DnsClient::Request,
                      public base::SupportsWeakPtr<MockRequest> {
   public:
    MockRequest(const base::StringPiece& qname,
                uint16 qtype,
                const RequestCallback& callback,
                const base::WeakPtr<MockDnsClient>& client)
        : Request(qname, qtype, callback), started_(false), client_(client) {
    }

    virtual int Start() OVERRIDE {
      EXPECT_FALSE(started_);
      started_ = true;
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&MockRequest::Finish, AsWeakPtr()));
      return ERR_IO_PENDING;
    }

   private:
    void Finish() {
      if (!client_) {
        DoCallback(ERR_DNS_SERVER_FAILED, NULL);
        return;
      }
      DoCallback(OK, client_->responses[Key(qname(), qtype())]);
    }

    bool started_;
    base::WeakPtr<MockDnsClient> client_;
  };

  typedef std::pair<std::string, uint16> Key;

  MockDnsClient() : num_requests(0) {}
  ~MockDnsClient() {
    STLDeleteValues(&responses);
  }

  Request* CreateRequest(const base::StringPiece& qname,
                         uint16 qtype,
                         const RequestCallback& callback,
                         const BoundNetLog&) {
    ++num_requests;
    return new MockRequest(qname, qtype, callback, AsWeakPtr());
  }

  int num_requests;
  std::map<Key, DnsResponse*> responses;
};

}  // namespace


// The following fixture sets up an environment for four different lookups
// with their data defined in dns_test_util.h.  All tests make use of these
// predefined variables instead of each defining their own, to avoid
// boilerplate code in every test.  Assuming every coming query is for a
// distinct hostname, as |kMaxTransactions| is set to 2 and
// |kMaxPendingRequests| is set to 1, first two queries start immediately
// and the next one is sent to pending queue; as a result, the next query
// should either fail itself or cause the pending query to fail depending
// on its priority.
class AsyncHostResolverTest : public testing::Test {
 public:
  AsyncHostResolverTest()
      : info0_(HostPortPair(kT0HostName, kPortNum)),
        info1_(HostPortPair(kT1HostName, kPortNum)),
        info2_(HostPortPair(kT2HostName, kPortNum)),
        info3_(HostPortPair(kT3HostName, kPortNum)),
        ip_addresses0_(kT0IpAddresses,
            kT0IpAddresses + arraysize(kT0IpAddresses)),
        ip_addresses1_(kT1IpAddresses,
            kT1IpAddresses + arraysize(kT1IpAddresses)),
        ip_addresses2_(kT2IpAddresses,
            kT2IpAddresses + arraysize(kT2IpAddresses)),
        ip_addresses3_(kT3IpAddresses,
            kT3IpAddresses + arraysize(kT3IpAddresses)) {
    // AF_INET only for now.
    info0_.set_address_family(ADDRESS_FAMILY_IPV4);
    info1_.set_address_family(ADDRESS_FAMILY_IPV4);
    info2_.set_address_family(ADDRESS_FAMILY_IPV4);
    info3_.set_address_family(ADDRESS_FAMILY_IPV4);

    client_.reset(new MockDnsClient());

    AddResponse(std::string(kT0DnsName, arraysize(kT0DnsName)), kT0Qtype,
        new DnsResponse(reinterpret_cast<const char*>(kT0ResponseDatagram),
                        arraysize(kT0ResponseDatagram),
                        arraysize(kT0QueryDatagram)));

    AddResponse(std::string(kT1DnsName, arraysize(kT1DnsName)), kT1Qtype,
        new DnsResponse(reinterpret_cast<const char*>(kT1ResponseDatagram),
                        arraysize(kT1ResponseDatagram),
                        arraysize(kT1QueryDatagram)));

    AddResponse(std::string(kT2DnsName, arraysize(kT2DnsName)), kT2Qtype,
        new DnsResponse(reinterpret_cast<const char*>(kT2ResponseDatagram),
                        arraysize(kT2ResponseDatagram),
                        arraysize(kT2QueryDatagram)));

    AddResponse(std::string(kT3DnsName, arraysize(kT3DnsName)), kT3Qtype,
        new DnsResponse(reinterpret_cast<const char*>(kT3ResponseDatagram),
                        arraysize(kT3ResponseDatagram),
                        arraysize(kT3QueryDatagram)));

    resolver_.reset(
        new AsyncHostResolver(kMaxTransactions, kMaxPendingRequests,
              HostCache::CreateDefaultCache(),
              client_.get(), NULL));
  }

  void AddResponse(const std::string& name, uint8 type, DnsResponse* response) {
    client_->responses[MockDnsClient::Key(name, type)] = response;
  }

 protected:
  AddressList addrlist0_, addrlist1_, addrlist2_, addrlist3_;
  HostResolver::RequestInfo info0_, info1_, info2_, info3_;
  std::vector<const char*> ip_addresses0_, ip_addresses1_,
    ip_addresses2_, ip_addresses3_;
  scoped_ptr<HostResolver> resolver_;
  scoped_ptr<MockDnsClient> client_;
  TestCompletionCallback callback0_, callback1_, callback2_, callback3_;
};

TEST_F(AsyncHostResolverTest, EmptyHostLookup) {
  info0_.set_host_port_pair(HostPortPair("", kPortNum));
  int rv = resolver_->Resolve(info0_, &addrlist0_, callback0_.callback(), NULL,
                              BoundNetLog());
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, rv);
}

TEST_F(AsyncHostResolverTest, IPv4LiteralLookup) {
  const char* kIPLiteral = "192.168.1.2";
  info0_.set_host_port_pair(HostPortPair(kIPLiteral, kPortNum));
  info0_.set_host_resolver_flags(HOST_RESOLVER_CANONNAME);
  int rv = resolver_->Resolve(info0_, &addrlist0_, callback0_.callback(), NULL,
                              BoundNetLog());
  EXPECT_EQ(OK, rv);
  std::vector<const char*> ip_addresses(1, kIPLiteral);
  VerifyAddressList(ip_addresses, kPortNum, addrlist0_);
  EXPECT_STREQ(kIPLiteral, addrlist0_.head()->ai_canonname);
}

TEST_F(AsyncHostResolverTest, IPv6LiteralLookup) {
  info0_.set_host_port_pair(HostPortPair("2001:db8:0::42", kPortNum));
  int rv = resolver_->Resolve(info0_, &addrlist0_, callback0_.callback(), NULL,
                              BoundNetLog());
  // When support for IPv6 is added, this should succeed.
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, rv);
}

TEST_F(AsyncHostResolverTest, CachedLookup) {
  int rv = resolver_->ResolveFromCache(info0_, &addrlist0_, BoundNetLog());
  EXPECT_EQ(ERR_DNS_CACHE_MISS, rv);

  // Cache the result of |info0_| lookup.
  rv = resolver_->Resolve(info0_, &addrlist0_, callback0_.callback(), NULL,
                          BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback0_.WaitForResult();
  EXPECT_EQ(OK, rv);
  VerifyAddressList(ip_addresses0_, kPortNum, addrlist0_);

  // Now lookup |info0_| from cache only, store results in |addrlist1_|,
  // should succeed synchronously.
  rv = resolver_->ResolveFromCache(info0_, &addrlist1_, BoundNetLog());
  EXPECT_EQ(OK, rv);
  VerifyAddressList(ip_addresses0_, kPortNum, addrlist1_);
}

TEST_F(AsyncHostResolverTest, InvalidHostNameLookup) {
  const std::string kHostName1(64, 'a');
  info0_.set_host_port_pair(HostPortPair(kHostName1, kPortNum));
  int rv = resolver_->Resolve(info0_, &addrlist0_, callback0_.callback(), NULL,
                              BoundNetLog());
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, rv);

  const std::string kHostName2(4097, 'b');
  info0_.set_host_port_pair(HostPortPair(kHostName2, kPortNum));
  rv = resolver_->Resolve(info0_, &addrlist0_, callback0_.callback(), NULL,
                          BoundNetLog());
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, rv);
}

TEST_F(AsyncHostResolverTest, Lookup) {
  int rv = resolver_->Resolve(info0_, &addrlist0_, callback0_.callback(), NULL,
                              BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback0_.WaitForResult();
  EXPECT_EQ(OK, rv);
  VerifyAddressList(ip_addresses0_, kPortNum, addrlist0_);
}

TEST_F(AsyncHostResolverTest, ConcurrentLookup) {
  int rv0 = resolver_->Resolve(info0_, &addrlist0_, callback0_.callback(), NULL,
                               BoundNetLog());
  int rv1 = resolver_->Resolve(info1_, &addrlist1_, callback1_.callback(), NULL,
                               BoundNetLog());
  int rv2 = resolver_->Resolve(info2_, &addrlist2_, callback2_.callback(), NULL,
                               BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv0);
  EXPECT_EQ(ERR_IO_PENDING, rv1);
  EXPECT_EQ(ERR_IO_PENDING, rv2);

  rv0 = callback0_.WaitForResult();
  EXPECT_EQ(OK, rv0);
  VerifyAddressList(ip_addresses0_, kPortNum, addrlist0_);

  rv1 = callback1_.WaitForResult();
  EXPECT_EQ(OK, rv1);
  VerifyAddressList(ip_addresses1_, kPortNum, addrlist1_);

  rv2 = callback2_.WaitForResult();
  EXPECT_EQ(OK, rv2);
  VerifyAddressList(ip_addresses2_, kPortNum, addrlist2_);

  EXPECT_EQ(3, client_->num_requests);
}

TEST_F(AsyncHostResolverTest, SameHostLookupsConsumeSingleTransaction) {
  // We pass the info0_ to all requests.
  int rv0 = resolver_->Resolve(info0_, &addrlist0_, callback0_.callback(), NULL,
                               BoundNetLog());
  int rv1 = resolver_->Resolve(info0_, &addrlist1_, callback1_.callback(), NULL,
                               BoundNetLog());
  int rv2 = resolver_->Resolve(info0_, &addrlist2_, callback2_.callback(), NULL,
                               BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv0);
  EXPECT_EQ(ERR_IO_PENDING, rv1);
  EXPECT_EQ(ERR_IO_PENDING, rv2);

  rv0 = callback0_.WaitForResult();
  EXPECT_EQ(OK, rv0);
  VerifyAddressList(ip_addresses0_, kPortNum, addrlist0_);

  rv1 = callback1_.WaitForResult();
  EXPECT_EQ(OK, rv1);
  VerifyAddressList(ip_addresses0_, kPortNum, addrlist1_);

  rv2 = callback2_.WaitForResult();
  EXPECT_EQ(OK, rv2);
  VerifyAddressList(ip_addresses0_, kPortNum, addrlist2_);

  // Although we have three lookups, a single UDP socket was used.
  EXPECT_EQ(1, client_->num_requests);
}

TEST_F(AsyncHostResolverTest, CancelLookup) {
  HostResolver::RequestHandle req0 = NULL, req2 = NULL;
  int rv0 = resolver_->Resolve(info0_, &addrlist0_, callback0_.callback(),
                               &req0, BoundNetLog());
  int rv1 = resolver_->Resolve(info1_, &addrlist1_, callback1_.callback(), NULL,
                               BoundNetLog());
  int rv2 = resolver_->Resolve(info2_, &addrlist2_, callback2_.callback(),
                               &req2, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv0);
  EXPECT_EQ(ERR_IO_PENDING, rv1);
  EXPECT_EQ(ERR_IO_PENDING, rv2);

  resolver_->CancelRequest(req0);
  resolver_->CancelRequest(req2);

  MessageLoop::current()->RunAllPending();

  EXPECT_FALSE(callback0_.have_result());
  EXPECT_FALSE(callback2_.have_result());

  rv1 = callback1_.WaitForResult();
  EXPECT_EQ(OK, rv1);
  VerifyAddressList(ip_addresses1_, kPortNum, addrlist1_);
}

// Tests the following scenario: start two resolutions for the same host,
// cancel one of them, make sure that the other one completes.
TEST_F(AsyncHostResolverTest, CancelSameHostLookup) {
  HostResolver::RequestHandle req0 = NULL;

  // Pass the info0_ to both requests.
  int rv0 = resolver_->Resolve(info0_, &addrlist0_, callback0_.callback(),
                               &req0, BoundNetLog());
  int rv1 = resolver_->Resolve(info0_, &addrlist1_, callback1_.callback(), NULL,
                               BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv0);
  EXPECT_EQ(ERR_IO_PENDING, rv1);

  resolver_->CancelRequest(req0);
  MessageLoop::current()->RunAllPending();
  EXPECT_FALSE(callback0_.have_result());

  rv1 = callback1_.WaitForResult();
  EXPECT_EQ(OK, rv1);
  VerifyAddressList(ip_addresses0_, kPortNum, addrlist1_);

  EXPECT_EQ(1, client_->num_requests);
}

// Test that a queued lookup completes.
TEST_F(AsyncHostResolverTest, QueuedLookup) {
  // kMaxTransactions is 2, thus the following requests consume all
  // available transactions.
  int rv0 = resolver_->Resolve(info0_, &addrlist0_, callback0_.callback(), NULL,
                               BoundNetLog());
  int rv1 = resolver_->Resolve(info1_, &addrlist1_, callback1_.callback(), NULL,
                               BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv0);
  EXPECT_EQ(ERR_IO_PENDING, rv1);

  // The following request will end up in queue.
  int rv2 = resolver_->Resolve(info2_, &addrlist2_, callback2_.callback(), NULL,
                               BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv2);
  EXPECT_EQ(1u,
      static_cast<AsyncHostResolver*>(resolver_.get())->GetNumPending());

  // Make sure all requests complete.
  rv0 = callback0_.WaitForResult();
  EXPECT_EQ(OK, rv0);
  VerifyAddressList(ip_addresses0_, kPortNum, addrlist0_);

  rv1 = callback1_.WaitForResult();
  EXPECT_EQ(OK, rv1);
  VerifyAddressList(ip_addresses1_, kPortNum, addrlist1_);

  rv2 = callback2_.WaitForResult();
  EXPECT_EQ(OK, rv2);
  VerifyAddressList(ip_addresses2_, kPortNum, addrlist2_);
}

// Test that cancelling a queued lookup works.
TEST_F(AsyncHostResolverTest, CancelPendingLookup) {
  // kMaxTransactions is 2, thus the following requests consume all
  // available transactions.
  int rv0 = resolver_->Resolve(info0_, &addrlist0_, callback0_.callback(), NULL,
                               BoundNetLog());
  int rv1 = resolver_->Resolve(info1_, &addrlist1_, callback1_.callback(), NULL,
                               BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv0);
  EXPECT_EQ(ERR_IO_PENDING, rv1);

  // The following request will end up in queue.
  HostResolver::RequestHandle req2 = NULL;
  int rv2 = resolver_->Resolve(info2_, &addrlist2_, callback2_.callback(),
                               &req2, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv2);
  EXPECT_EQ(1u,
      static_cast<AsyncHostResolver*>(resolver_.get())->GetNumPending());

  resolver_->CancelRequest(req2);

  // Make sure first two requests complete while the cancelled one doesn't.
  MessageLoop::current()->RunAllPending();
  EXPECT_FALSE(callback2_.have_result());

  rv0 = callback0_.WaitForResult();
  EXPECT_EQ(OK, rv0);
  VerifyAddressList(ip_addresses0_, kPortNum, addrlist0_);

  rv1 = callback1_.WaitForResult();
  EXPECT_EQ(OK, rv1);
  VerifyAddressList(ip_addresses1_, kPortNum, addrlist1_);
}

TEST_F(AsyncHostResolverTest, ResolverDestructionCancelsLookups) {
  int rv0 = resolver_->Resolve(info0_, &addrlist0_, callback0_.callback(), NULL,
                               BoundNetLog());
  int rv1 = resolver_->Resolve(info1_, &addrlist1_, callback1_.callback(), NULL,
                               BoundNetLog());
  // This one is queued.
  int rv2 = resolver_->Resolve(info2_, &addrlist2_, callback2_.callback(), NULL,
                               BoundNetLog());
  EXPECT_EQ(1u,
      static_cast<AsyncHostResolver*>(resolver_.get())->GetNumPending());

  EXPECT_EQ(ERR_IO_PENDING, rv0);
  EXPECT_EQ(ERR_IO_PENDING, rv1);
  EXPECT_EQ(ERR_IO_PENDING, rv2);

  resolver_.reset();

  MessageLoop::current()->RunAllPending();

  EXPECT_FALSE(callback0_.have_result());
  EXPECT_FALSE(callback1_.have_result());
  EXPECT_FALSE(callback2_.have_result());
}

// Test that when the number of pending lookups is at max, a new lookup
// with a priority lower than all of those in the queue fails.
TEST_F(AsyncHostResolverTest, OverflowQueueWithLowPriorityLookup) {
  int rv0 = resolver_->Resolve(info0_, &addrlist0_, callback0_.callback(), NULL,
                               BoundNetLog());
  int rv1 = resolver_->Resolve(info1_, &addrlist1_, callback1_.callback(), NULL,
                               BoundNetLog());
  // This one is queued and fills up the queue since its size is 1.
  int rv2 = resolver_->Resolve(info2_, &addrlist2_, callback2_.callback(), NULL,
                               BoundNetLog());
  EXPECT_EQ(1u,
      static_cast<AsyncHostResolver*>(resolver_.get())->GetNumPending());

  EXPECT_EQ(ERR_IO_PENDING, rv0);
  EXPECT_EQ(ERR_IO_PENDING, rv1);
  EXPECT_EQ(ERR_IO_PENDING, rv2);

  // This one fails.
  info3_.set_priority(LOWEST);
  int rv3 = resolver_->Resolve(info3_, &addrlist3_, callback3_.callback(), NULL,
                               BoundNetLog());
  EXPECT_EQ(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE, rv3);

  MessageLoop::current()->RunAllPending();
  EXPECT_FALSE(callback3_.have_result());
}

// Test that when the number of pending lookups is at max, a new lookup
// with a priority higher than any of those in the queue succeeds and
// causes the lowest priority lookup in the queue to fail.
TEST_F(AsyncHostResolverTest, OverflowQueueWithHighPriorityLookup) {
  int rv0 = resolver_->Resolve(info0_, &addrlist0_, callback0_.callback(), NULL,
                               BoundNetLog());
  int rv1 = resolver_->Resolve(info1_, &addrlist1_, callback1_.callback(), NULL,
                               BoundNetLog());

  // Next lookup is queued.  Since this will be ejected from the queue and
  // will not consume a socket from our factory, we are not passing it
  // predefined members.
  HostResolver::RequestInfo info(HostPortPair("cnn.com", 80));
  info.set_address_family(ADDRESS_FAMILY_IPV4);
  AddressList addrlist_fail;
  TestCompletionCallback callback_fail;
  int rv_fail = resolver_->Resolve(info, &addrlist_fail,
                                   callback_fail.callback(), NULL,
                                   BoundNetLog());
  EXPECT_EQ(1u,
      static_cast<AsyncHostResolver*>(resolver_.get())->GetNumPending());

  EXPECT_EQ(ERR_IO_PENDING, rv0);
  EXPECT_EQ(ERR_IO_PENDING, rv1);
  EXPECT_EQ(ERR_IO_PENDING, rv_fail);

  // Lookup 2 causes the above to fail, but itself should succeed.
  info2_.set_priority(HIGHEST);
  int rv2 = resolver_->Resolve(info2_, &addrlist2_, callback2_.callback(), NULL,
                               BoundNetLog());

  rv0 = callback0_.WaitForResult();
  EXPECT_EQ(OK, rv0);
  VerifyAddressList(ip_addresses0_, kPortNum, addrlist0_);

  rv1 = callback1_.WaitForResult();
  EXPECT_EQ(OK, rv1);
  VerifyAddressList(ip_addresses1_, kPortNum, addrlist1_);

  rv_fail = callback_fail.WaitForResult();
  EXPECT_EQ(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE, rv_fail);
  EXPECT_EQ(static_cast<addrinfo*>(NULL), addrlist_fail.head());

  rv2 = callback2_.WaitForResult();
  EXPECT_EQ(OK, rv2);
  VerifyAddressList(ip_addresses2_, kPortNum, addrlist2_);
}

}  // namespace net