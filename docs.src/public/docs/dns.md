---
title: "DNS"
weight: 50
---

`protocol::dns` is a DNS message codec plus a client/server pair built on
`udp::peer`.

## Messages

`dns::message` ([protocol/dns/message.hpp](../../../../include/lambdatech/networking/protocol/dns/message.hpp))
is `dns::header` + `std::vector<dns::question>`.

```c++
namespace dns = lambdatech::networking::protocol::dns;

dns::message query;
query.header.id = 0x1234;
query.header.recursion_desired = true;
query.questions.push_back({"www.lambdatech.io", dns::record_type::a, dns::record_class::in});

auto wire   = query.serialize();          // std::optional<std::vector<std::byte>>
auto parsed = dns::message::parse(*wire);  // std::optional<dns::message>
// *parsed == query
```

`parse()` and `serialize()` are **total** — they return `std::optional` and
never throw or read out of bounds on a malformed datagram. The answer /
authority / additional resource-record sections are not modeled yet (SRS-001
M3); `parse()` still reads `QDCOUNT` questions so the header counts stay
honest.

## `dns::client`  — a stub resolver

```c++
auto resolver = dns::client::create("1.1.1.1", 53, loop);
resolver->set_timeout(std::chrono::seconds(3));
resolver->query("example.com", dns::record_type::a, [](std::optional<dns::message> reply) {
  if (!reply)                    { /* timed out */ }
  else if (reply->header.rcode == dns::rcode::no_error) { /* ... */ }
});
```

It sends the query over a `udp::peer`, matches replies to outstanding queries
by the 16-bit id, and calls back with `std::nullopt` if the loop timer fires
first. `on_error()` reports socket / parse failures.

## `dns::server`

```c++
auto server = dns::server::create(loop);
server->on_query() += [](const dns::message &request, const dns::server::responder &reply) {
  dns::message answer;
  answer.header.rcode = dns::rcode::not_implemented;
  answer.questions = request.questions;
  reply.send(answer);            // id and the QR bit are filled in for you
};
server->listen(5353, "127.0.0.1");
```

`listen()` binds a `udp::peer`; each datagram is parsed and emitted as
`query` together with a `responder` bound to the sender's address.
