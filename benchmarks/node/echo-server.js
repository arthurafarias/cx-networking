// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

// Node.js echo server for the cross-stack benchmark. Speaks the common CLI
// (--proto/--host/--port); echoes TCP bytes and UDP datagrams back verbatim;
// prints "READY <port>" once bound. The orchestrator ends it with SIGTERM.
//
//   node echo-server.js --proto tcp --host 127.0.0.1 --port 7001

'use strict';
const net = require('node:net');
const dgram = require('node:dgram');
const { parseArgs } = require('./common');

const o = parseArgs(process.argv.slice(2));

if (o.proto === 'udp') {
  const sock = dgram.createSocket('udp4');
  sock.on('message', (msg, rinfo) => {
    sock.send(msg, rinfo.port, rinfo.address);
  });
  sock.on('error', (e) => console.error('[node-echo]', e.message));
  sock.bind(o.port, o.host, () => {
    console.log(`READY ${sock.address().port}`);
  });
} else {
  const server = net.createServer({ noDelay: true }, (socket) => {
    socket.on('error', () => {});
    socket.on('data', (chunk) => socket.write(chunk));
  });
  server.on('error', (e) => console.error('[node-echo]', e.message));
  server.listen(o.port, o.host, () => {
    console.log(`READY ${server.address().port}`);
  });
}
