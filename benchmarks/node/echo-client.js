// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

// Node.js closed-loop load generator for the cross-stack benchmark. One
// event loop; `--connections` sockets each keeping `--pipeline` frames in
// flight. Emits one JSON result line (see ./common.js).
//
//   node echo-client.js --proto tcp --host 127.0.0.1 --port 7001 \
//       --connections 64 --duration 10 --warmup 3 --payload 64 --pipeline 1

'use strict';
const net = require('node:net');
const dgram = require('node:dgram');
const { parseArgs, Histogram, emitResult } = require('./common');

const o = parseArgs(process.argv.slice(2));
const hist = new Histogram();
const state = { measuring: false, running: true, requests: 0, errors: 0 };
const nowNs = () => process.hrtime.bigint();

function makeFrame() {
  const b = Buffer.alloc(o.payload);
  b.writeBigUInt64LE(process.hrtime.bigint(), 0);
  return b;
}

const LATE_NS = 250_000_000n; // a straggler (dropped datagram), not a latency sample

function book(tsNs) {
  const rtt = process.hrtime.bigint() - tsNs;
  if (rtt > LATE_NS) { state.errors++; return; }
  if (state.measuring) {
    hist.record(Number(rtt));
    state.requests++;
  }
}

function startTcp(done) {
  const conns = [];
  for (let i = 0; i < o.connections; i++) {
    const sock = net.connect({ host: o.host, port: o.port, noDelay: true });
    let acc = Buffer.alloc(0);
    let inflight = 0;
    const pump = () => {
      while (state.running && inflight < o.pipeline) { sock.write(makeFrame()); inflight++; }
    };
    sock.on('connect', pump);
    sock.on('error', () => { state.errors++; });
    sock.on('data', (chunk) => {
      acc = acc.length ? Buffer.concat([acc, chunk]) : chunk;
      while (acc.length >= o.payload) {
        const ts = acc.readBigUInt64LE(0);
        acc = acc.subarray(o.payload);
        inflight--;
        book(ts);
        pump();
      }
    });
    conns.push(sock);
  }
  done(() => { for (const s of conns) s.destroy(); });
}

function startUdp(done) {
  const flows = [];
  for (let i = 0; i < o.connections; i++) {
    const sock = dgram.createSocket('udp4');
    const f = { sock, inflight: 0, lastRx: 0n };
    const pump = () => {
      while (state.running && f.inflight < o.pipeline) { sock.send(makeFrame(), o.port, o.host); f.inflight++; }
      f.lastRx = process.hrtime.bigint();
    };
    sock.on('message', (msg) => {
      if (msg.length >= 8) book(msg.readBigUInt64LE(0));
      if (f.inflight > 0) f.inflight--;
      f.lastRx = process.hrtime.bigint();
      pump();
    });
    sock.on('error', () => { state.errors++; });
    f.pump = pump;
    flows.push(f);
  }
  const sweep = setInterval(() => {
    const now = process.hrtime.bigint();
    for (const f of flows) {
      if (f.inflight > 0 && f.lastRx && now - f.lastRx > 200_000_000n) { state.errors += f.inflight; f.inflight = 0; }
      f.pump();
    }
  }, 100);
  for (const f of flows) f.pump();
  done(() => { clearInterval(sweep); for (const f of flows) f.sock.close(); });
}

const start = o.proto === 'udp' ? startUdp : startTcp;
start((teardown) => {
  setTimeout(() => {
    state.measuring = true;
    const t0 = nowNs();
    setTimeout(() => {
      state.measuring = false;
      state.running = false;
      const elapsed = Number(nowNs() - t0) / 1e9;
      setTimeout(() => {
        teardown();
        emitResult(o, hist, state.requests, state.errors, elapsed);
        process.exit(0);
      }, 100);
    }, o.duration * 1000);
  }, o.warmup * 1000);
});
