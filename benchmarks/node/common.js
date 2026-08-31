// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

// Shared bits for the Node.js benchmark scripts: CLI parsing, the latency
// histogram, and the single-line JSON result (identical shape to the C++
// harness in ../cpp/bench_common.hpp).

'use strict';

function parseArgs(argv) {
  const o = {
    proto: 'tcp', host: '127.0.0.1', port: 0,
    connections: 1, duration: 10, warmup: 3, payload: 64, pipeline: 1,
  };
  for (let i = 0; i < argv.length; i++) {
    let key = argv[i], val;
    const eq = key.indexOf('=');
    if (eq !== -1) { val = key.slice(eq + 1); key = key.slice(0, eq); }
    else { val = argv[i + 1]; i++; }
    switch (key) {
      case '--proto': o.proto = val; break;
      case '--host': o.host = val; break;
      case '--port': o.port = parseInt(val, 10); break;
      case '--connections': case '-c': o.connections = parseInt(val, 10); break;
      case '--duration': case '-d': o.duration = parseFloat(val); break;
      case '--warmup': o.warmup = parseFloat(val); break;
      case '--payload': o.payload = parseInt(val, 10); break;
      case '--pipeline': o.pipeline = parseInt(val, 10); break;
      default: throw new Error(`unknown arg: ${key}`);
    }
  }
  o.payload = Math.max(o.payload, 8);
  o.pipeline = Math.max(o.pipeline, 1);
  o.connections = Math.max(o.connections, 1);
  return o;
}

const MAX_US = 2_000_000;

class Histogram {
  constructor() {
    this.counts = new Uint32Array(MAX_US);
    this.n = 0; this.sumNs = 0; this.minNs = Infinity; this.maxNs = 0; this.overflow = 0;
  }
  record(ns) {
    let us = Math.floor(ns / 1000);
    if (us >= MAX_US) { this.overflow++; us = MAX_US - 1; }
    this.counts[us]++;
    this.n++;
    this.sumNs += ns;
    if (ns < this.minNs) this.minNs = ns;
    if (ns > this.maxNs) this.maxNs = ns;
  }
  reset() { this.counts.fill(0); this.n = 0; this.sumNs = 0; this.minNs = Infinity; this.maxNs = 0; this.overflow = 0; }
  percentileUs(p) {
    if (!this.n) return 0;
    const target = Math.max(1, Math.ceil(p * this.n));
    let seen = 0;
    for (let i = 0; i < MAX_US; i++) { seen += this.counts[i]; if (seen >= target) return i; }
    return MAX_US - 1;
  }
  get minUs() { return this.n ? this.minNs / 1000 : 0; }
  get maxUs() { return this.n ? this.maxNs / 1000 : 0; }
  get meanUs() { return this.n ? this.sumNs / this.n / 1000 : 0; }
}

function emitResult(o, h, requests, errors, elapsedS) {
  const r1 = (x) => Math.round(x * 10) / 10;
  const r2 = (x) => Math.round(x * 100) / 100;
  process.stdout.write(JSON.stringify({
    stack: 'node', proto: o.proto, connections: o.connections, pipeline: o.pipeline,
    payload: o.payload, duration_s: r2(elapsedS), requests, errors,
    rps: r1(elapsedS > 0 ? requests / elapsedS : 0),
    throughput_mbps: r2(elapsedS > 0 ? (requests * o.payload * 2) / elapsedS / 1e6 : 0),
    lat_us: {
      min: r1(h.minUs), mean: r1(h.meanUs), p50: r1(h.percentileUs(0.5)),
      p90: r1(h.percentileUs(0.9)), p99: r1(h.percentileUs(0.99)),
      p999: r1(h.percentileUs(0.999)), max: r1(h.maxUs),
    },
  }) + '\n');
}

module.exports = { parseArgs, Histogram, emitResult };
