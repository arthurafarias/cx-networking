# ---------------------------------------------------------------------------
# PROPRIETARY CODE – Arthur de Araújo Farias 2025
# All rights reserved.  No part of this file may be reproduced, stored in a
# retrieval system, or transmitted in any form or by any means—electronic,
# mechanical, photocopying, recording, or otherwise—without the prior written
# permission of the copyright holder.
# ---------------------------------------------------------------------------

"""asyncio closed-loop load generator for the cross-stack benchmark. One
event loop; --connections sockets each keeping --pipeline frames in flight.
Emits one JSON result line (see common.py).

    python echo_client.py --proto tcp --host 127.0.0.1 --port 7001 \
        --connections 64 --duration 10 --warmup 3 --payload 64 --pipeline 1
"""

from __future__ import annotations

import asyncio
import sys
import time

from common import Histogram, emit_result, parse_args

o = parse_args(sys.argv[1:])
hist = Histogram()
st = {"measuring": False, "running": True, "requests": 0, "errors": 0}
now_ns = time.monotonic_ns


def make_frame() -> bytes:
    return now_ns().to_bytes(8, "little") + b"\0" * (o.payload - 8)


LATE_NS = 250_000_000  # a straggler (dropped datagram), not a latency sample


def book(ts_ns: int) -> None:
    rtt = now_ns() - ts_ns
    if rtt > LATE_NS:
        st["errors"] += 1
        return
    if st["measuring"]:
        hist.record(rtt)
        st["requests"] += 1


class TcpClient(asyncio.Protocol):
    def connection_made(self, transport: asyncio.Transport) -> None:
        try:
            transport.get_extra_info("socket").setsockopt(6, 1, 1)  # TCP_NODELAY
        except OSError:
            pass
        self.transport = transport
        self.buf = bytearray()
        self.inflight = 0
        self.pump()

    def pump(self) -> None:
        while st["running"] and self.inflight < o.pipeline:
            self.transport.write(make_frame())
            self.inflight += 1

    def data_received(self, data: bytes) -> None:
        self.buf += data
        p = o.payload
        while len(self.buf) >= p:
            ts = int.from_bytes(self.buf[:8], "little")
            del self.buf[:p]
            self.inflight -= 1
            book(ts)
            self.pump()

    def connection_lost(self, exc) -> None:
        if st["running"]:
            st["errors"] += 1


class UdpClient(asyncio.DatagramProtocol):
    def connection_made(self, transport: asyncio.DatagramTransport) -> None:
        self.transport = transport
        self.inflight = 0
        self.last_rx = now_ns()
        self.pump()

    def pump(self) -> None:
        while st["running"] and self.inflight < o.pipeline:
            self.transport.sendto(make_frame())
            self.inflight += 1
        self.last_rx = now_ns()

    def datagram_received(self, data: bytes, addr) -> None:
        if len(data) >= 8:
            book(int.from_bytes(data[:8], "little"))
        if self.inflight > 0:
            self.inflight -= 1
        self.last_rx = now_ns()
        self.pump()

    def error_received(self, exc) -> None:
        st["errors"] += 1


async def main() -> None:
    loop = asyncio.get_running_loop()
    protos: list = []

    if o.proto == "udp":
        for _ in range(o.connections):
            _, proto = await loop.create_datagram_endpoint(UdpClient, remote_addr=(o.host, o.port))
            protos.append(proto)

        async def sweeper() -> None:
            while st["running"]:
                await asyncio.sleep(0.1)
                now = now_ns()
                for pr in protos:
                    if pr.inflight > 0 and now - pr.last_rx > 200_000_000:
                        st["errors"] += pr.inflight
                        pr.inflight = 0
                    pr.pump()

        sweep_task = asyncio.create_task(sweeper())
    else:
        for _ in range(o.connections):
            _, proto = await loop.create_connection(TcpClient, o.host, o.port)
            protos.append(proto)
        sweep_task = None

    await asyncio.sleep(o.warmup)
    st["measuring"] = True
    t0 = now_ns()
    await asyncio.sleep(o.duration)
    st["measuring"] = False
    st["running"] = False
    elapsed = (now_ns() - t0) / 1e9
    await asyncio.sleep(0.1)

    if sweep_task:
        sweep_task.cancel()
    for pr in protos:
        pr.transport.close()

    emit_result(o, hist, st["requests"], st["errors"], elapsed)


if __name__ == "__main__":
    asyncio.run(main())
