---
title: "7. Milestones"
weight: 70
---

## 7. Milestones

| Milestone | Covers | Status |
|---|---|---|
| **M1** | `core/config.hpp`; the `descriptor` and `poller` façades + `concept` checks; `core/impl/posix/{descriptor,poller,endpoint}.hpp`; `event_loop` ported onto `poller` (wake fd folded in, no `<poll.h>`). Test group `core::event_loop` green. | Done |
| **M2** | `socket_ops` and `resolver` façades + `core/impl/posix/{socket_ops,resolver}.hpp`; `core/address.hpp` reduced to the neutral value; `tcp/socket.hpp`, `tcp/server.hpp`, `udp/peer.hpp` ported off raw syscalls; `core/native.hpp` removed. `tcp::*` / `udp::peer` / `dns::*` test groups green. | Done |
| **M3** | `core/impl/standalone/*`: in-process pipe table (`descriptor`), deterministic readiness queue (`poller`), loopback socket fabric (`socket_ops`), static host table (`resolver`). A `-DLNW_NET_BACKEND_STANDALONE=1` test build that links and runs the full suite. | Not started |
| **M4** | `epoll` `poller` backend (`core/impl/posix/poller.hpp` gains an `epoll` path or a sibling `core/impl/linux/`), selected by `config.hpp`. Absorbs SRS-002 M4's `io_uring` intent as a follow-on. | Not started |
| **M5** | `kqueue` `poller` backend for the BSDs / macOS (SRS-002 M5). | Not started |

### Verification

- **M1/M2:** the existing `core::event_loop`, `tcp::client+server`,
  `udp::peer`, `dns::client+server` test groups pass unchanged in behaviour;
  NFR-1's `grep` is clean.
- **M3:** the same groups pass a second time in the `standalone` build, with
  no loopback sockets opened (checked with `strace -f -e trace=socket`).
- **M4/M5:** each new backend passes the M1 `core::event_loop` group and a
  high-fd-count stress case added alongside it.
