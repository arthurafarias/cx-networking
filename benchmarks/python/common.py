# ---------------------------------------------------------------------------
# PROPRIETARY CODE – Arthur de Araújo Farias 2025
# All rights reserved.  No part of this file may be reproduced, stored in a
# retrieval system, or transmitted in any form or by any means—electronic,
# mechanical, photocopying, recording, or otherwise—without the prior written
# permission of the copyright holder.
# ---------------------------------------------------------------------------

"""Shared bits for the Python benchmark scripts: CLI parsing, the latency
histogram, and the single-line JSON result (identical shape to the C++
harness in ../cpp/bench_common.hpp)."""

from __future__ import annotations

import json
import math
import sys
from array import array
from dataclasses import dataclass

MAX_US = 2_000_000


@dataclass
class Options:
    proto: str = "tcp"
    host: str = "127.0.0.1"
    port: int = 0
    connections: int = 1
    duration: float = 10.0
    warmup: float = 3.0
    payload: int = 64
    pipeline: int = 1


def parse_args(argv: list[str]) -> Options:
    o = Options()
    aliases = {"-c": "connections", "-d": "duration"}
    i = 0
    while i < len(argv):
        tok = argv[i]
        if "=" in tok:
            key, val = tok.split("=", 1)
        else:
            key, val = tok, argv[i + 1] if i + 1 < len(argv) else None
            i += 1
        i += 1
        name = key[2:] if key.startswith("--") else aliases.get(key)
        if name is None or not hasattr(o, name) or val is None:
            raise SystemExit(f"bad or unknown argument: {key}")
        cur = getattr(o, name)
        setattr(o, name, type(cur)(val))
    o.payload = max(o.payload, 8)
    o.pipeline = max(o.pipeline, 1)
    o.connections = max(o.connections, 1)
    return o


class Histogram:
    __slots__ = ("counts", "n", "sum_ns", "min_ns", "max_ns", "overflow")

    def __init__(self) -> None:
        self.counts = array("Q", bytes(8 * MAX_US))
        self.n = 0
        self.sum_ns = 0
        self.min_ns = 1 << 63
        self.max_ns = 0
        self.overflow = 0

    def record(self, ns: int) -> None:
        us = ns // 1000
        if us >= MAX_US:
            self.overflow += 1
            us = MAX_US - 1
        self.counts[us] += 1
        self.n += 1
        self.sum_ns += ns
        if ns < self.min_ns:
            self.min_ns = ns
        if ns > self.max_ns:
            self.max_ns = ns

    def reset(self) -> None:
        self.counts = array("Q", bytes(8 * MAX_US))
        self.n = self.sum_ns = self.max_ns = self.overflow = 0
        self.min_ns = 1 << 63

    def summary_us(self, percentiles=(0.50, 0.90, 0.99, 0.999)) -> dict:
        out = {"min": 0.0, "mean": 0.0, "max": 0.0}
        for p in percentiles:
            out[f"p{int(p * 1000):d}"] = 0.0
        if not self.n:
            return out
        out["min"] = self.min_ns / 1000
        out["max"] = self.max_ns / 1000
        out["mean"] = self.sum_ns / self.n / 1000
        targets = sorted((max(1, math.ceil(p * self.n)), p) for p in percentiles)
        ti = 0
        seen = 0
        for us, c in enumerate(self.counts):
            if not c:
                continue
            seen += c
            while ti < len(targets) and seen >= targets[ti][0]:
                out[f"p{int(targets[ti][1] * 1000):d}"] = float(us)
                ti += 1
            if ti >= len(targets):
                break
        return out


def emit_result(o: Options, h: Histogram, requests: int, errors: int, elapsed_s: float) -> None:
    lat = h.summary_us()
    row = {
        "stack": "python",
        "proto": o.proto,
        "connections": o.connections,
        "pipeline": o.pipeline,
        "payload": o.payload,
        "duration_s": round(elapsed_s, 3),
        "requests": requests,
        "errors": errors,
        "rps": round(requests / elapsed_s, 1) if elapsed_s > 0 else 0.0,
        "throughput_mbps": round(requests * o.payload * 2 / elapsed_s / 1e6, 2) if elapsed_s > 0 else 0.0,
        "lat_us": {
            "min": round(lat["min"], 1),
            "mean": round(lat["mean"], 1),
            "p50": round(lat["p500"], 1),
            "p90": round(lat["p900"], 1),
            "p99": round(lat["p990"], 1),
            "p999": round(lat["p999"], 1),
            "max": round(lat["max"], 1),
        },
    }
    sys.stdout.write(json.dumps(row) + "\n")
    sys.stdout.flush()
