# ---------------------------------------------------------------------------
# PROPRIETARY CODE – Arthur de Araújo Farias 2025
# All rights reserved.  No part of this file may be reproduced, stored in a
# retrieval system, or transmitted in any form or by any means—electronic,
# mechanical, photocopying, recording, or otherwise—without the prior written
# permission of the copyright holder.
# ---------------------------------------------------------------------------

"""asyncio echo server for the cross-stack benchmark. Speaks the common CLI
(--proto/--host/--port); echoes TCP bytes and UDP datagrams back verbatim;
prints "READY <port>" once bound. The orchestrator ends it with SIGTERM.

    python echo_server.py --proto tcp --host 127.0.0.1 --port 7001
"""

from __future__ import annotations

import asyncio
import sys

from common import parse_args


class TcpEcho(asyncio.Protocol):
    def connection_made(self, transport: asyncio.Transport) -> None:
        transport.set_write_buffer_limits(1 << 20)
        try:
            transport.get_extra_info("socket").setsockopt(6, 1, 1)  # IPPROTO_TCP, TCP_NODELAY
        except OSError:
            pass
        self.transport = transport

    def data_received(self, data: bytes) -> None:
        self.transport.write(data)


class UdpEcho(asyncio.DatagramProtocol):
    def connection_made(self, transport: asyncio.DatagramTransport) -> None:
        self.transport = transport

    def datagram_received(self, data: bytes, addr) -> None:
        self.transport.sendto(data, addr)


async def main() -> None:
    o = parse_args(sys.argv[1:])
    loop = asyncio.get_running_loop()

    if o.proto == "udp":
        transport, _ = await loop.create_datagram_endpoint(UdpEcho, local_addr=(o.host, o.port))
        port = transport.get_extra_info("sockname")[1]
        print(f"READY {port}", flush=True)
        await asyncio.Event().wait()
    else:
        server = await loop.create_server(TcpEcho, o.host, o.port)
        port = server.sockets[0].getsockname()[1]
        print(f"READY {port}", flush=True)
        async with server:
            await server.serve_forever()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
