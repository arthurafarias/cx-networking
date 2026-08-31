---
title: "6. Milestones"
weight: 60
---

## 6. Milestones

| Milestone | Covers | Status |
|---|---|---|
| **M1** | Vendor `task` / `thread_pool` / `signal` from cxflow; `core::event` wrapper; `core::buffer`, `core::socket_address` + `resolve()`. | Done |
| **M2** | `event_loop`: `poll()` cycle, wake `eventfd`, fd watches (`watch`/`modify`/`unwatch`), `defer()`, `start`/`stop`/`run`. Test group `core::event_loop`. | Done |
| **M3** | Timers: `set_timeout` / `clear_timeout`, deadline-aware `poll()` timeout. | Done |
| **M4** | An `io_uring` loop backend behind the same API, selected at build time (a `plugins/io_uring/` entry per the Plugin Folder Convention). | Not started |
| **M5** | `kqueue` backend for the BSDs / macOS. | Not started |
