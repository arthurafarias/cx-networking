# Cross-stack networking benchmark

A closed-loop **echo** benchmark that runs the *same* workload against four
async networking stacks so their numbers can be put side by side:

| stack | server + client | reactor model here |
|---|---|---|
| **lnw** | LambdaTech Networking (`core::event_loop`, `tcp::*`, `udp::peer`) | single loop thread |
| **asio** | Boost.Asio | one `io_context`, one thread |
| **node** | Node.js `net` / `dgram` | one event loop |
| **python** | `asyncio` Protocol / DatagramProtocol | one event loop |

Every stack ships **its own echo server and its own load generator**, so each
row is that runtime end to end — client and server in the same stack. All
eight programs speak one CLI and emit one identical JSON result line, and
`run.py` drives the matrix.

## Workloads

- **TCP echo** — `--connections` persistent connections, each keeping
  `--pipeline` fixed-size frames in flight; every echoed frame books a
  round-trip latency sample.
- **UDP echo** — the same, with `--connections` independent flows (each its
  own socket). A 100 ms sweep writes off any datagram outstanding > 200 ms as
  a drop and refills the flow, so loopback packet loss can't wedge a run.

Each request is `--payload` bytes (min 8); the first 8 carry a little-endian
`uint64` monotonic-clock timestamp the echo returns, so latency needs no
per-request bookkeeping and pipelining stays honest.

## Running

```bash
# build the C++ side (lnw + asio); Node and Python need only their runtimes
cmake -S . -B build -DLAMBDATECH_NETWORKING_BUILD_BENCHMARKS=ON
cmake --build build --target \
    lnw-bench-echo-server lnw-bench-echo-client \
    asio-bench-echo-server asio-bench-echo-client

# full default matrix: tcp+udp x {1,16,64} conns x 64-byte payload, 10 s each
python benchmarks/run.py --build-dir build

# focused sweep
python benchmarks/run.py --build-dir build --stacks lnw,asio --protos tcp \
    --connections 1,16,64,256 --payloads 64,1024 --duration 15 --repeat 3
```

`run.py` prints Markdown tables and writes `benchmarks/results/bench-<ts>.json`
(raw rows + environment) and `.md`. Stacks whose binary/runtime is absent are
skipped with a note. It has no third-party dependencies.

### Running one pair by hand

```bash
./build/benchmarks/cpp/lnw-bench-echo-server --proto tcp --host 127.0.0.1 --port 7001
# -> READY 7001
./build/benchmarks/cpp/lnw-bench-echo-client --proto tcp --host 127.0.0.1 --port 7001 \
    --connections 64 --duration 10 --warmup 3 --payload 64 --pipeline 1
```

Common flags (all programs): `--proto tcp|udp`, `--host`, `--port`
(`0` = ephemeral, printed on the server's `READY` line), `--connections`,
`--duration`, `--warmup`, `--payload`, `--pipeline`.

## Reading the numbers — and the caveats

- **`req/s`** is completed round-trips per measured second; **`MB/s`** counts
  the payload crossing the wire in *both* directions.
- Latency is wall-clock round-trip: `p50/p99/p99.9` in microseconds, from a
  1 µs-resolution histogram capped at 2 s.
- This is a **loopback, single-machine, single-reactor** comparison. It
  deliberately does not use multiple worker threads / `SO_REUSEPORT` / multiple
  processes, because the point is to compare each stack's core event loop at
  equal parallelism — not to find each stack's peak. Real deployments scale
  out; expect different ratios there.
- lnw and asio run `-O2`. Warm up before trusting a number; pin CPUs
  (`taskset`) and disable turbo for low-variance runs. Numbers are not
  comparable across machines.
- The Python client uses `asyncio` `Protocol` classes (the fast path), not the
  `StreamReader`/`StreamWriter` convenience API.

## Layout

```
benchmarks/
  run.py                     orchestrator (stdlib only)
  cpp/
    bench_common.hpp         CLI + histogram + JSON result, shared
    lnw_echo_server.cpp   lnw_echo_client.cpp
    asio_echo_server.cpp  asio_echo_client.cpp
    CMakeLists.txt           built when -DLAMBDATECH_NETWORKING_BUILD_BENCHMARKS=ON
  node/    common.js  echo-server.js  echo-client.js
  python/  common.py  echo_server.py  echo_client.py
  results/                    run.py output (git-ignored)
```
