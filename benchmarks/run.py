#!/usr/bin/env python3
# ---------------------------------------------------------------------------
# PROPRIETARY CODE – Arthur de Araújo Farias 2025
# All rights reserved.  No part of this file may be reproduced, stored in a
# retrieval system, or transmitted in any form or by any means—electronic,
# mechanical, photocopying, recording, or otherwise—without the prior written
# permission of the copyright holder.
# ---------------------------------------------------------------------------

"""Cross-stack echo benchmark orchestrator.

Runs the same closed-loop TCP/UDP echo workload against LambdaTech Networking,
Boost.Asio, Node.js and Python, one (stack x proto x connections x payload)
combination at a time: it starts that stack's echo server, waits for its
"READY <port>" line, runs that stack's load generator, parses the single JSON
result line, tears the server down, and moves on. Results are printed as
Markdown tables and written to benchmarks/results/.

Examples
--------
    # everything, default matrix
    python benchmarks/run.py --build-dir build

    # just lnw vs asio, TCP, a connection sweep
    python benchmarks/run.py --build-dir build --stacks lnw,asio \\
        --protos tcp --connections 1,16,64,256 --duration 15
"""

from __future__ import annotations

import argparse
import contextlib
import json
import os
import platform
import shutil
import signal
import socket
import statistics
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

HERE = Path(__file__).resolve().parent
ALL_STACKS = ["lnw", "asio", "node", "python"]


def discover(build_dir: Path | None):
    """Return {stack: {"server": [...argv], "client": [...argv]}} for stacks
    whose binaries / runtimes are actually present."""
    reg: dict[str, dict[str, list[str]]] = {}
    node = shutil.which("node")
    py = sys.executable

    cpp_dirs = []
    if build_dir:
        cpp_dirs += [build_dir / "benchmarks" / "cpp", build_dir]
    cpp_dirs += [HERE.parent / "build" / "benchmarks" / "cpp"]

    def find_cpp(name: str) -> str | None:
        for d in cpp_dirs:
            cand = Path(d) / name
            if cand.is_file() and os.access(cand, os.X_OK):
                return str(cand)
        return None

    lnw_s, lnw_c = find_cpp("lnw-bench-echo-server"), find_cpp("lnw-bench-echo-client")
    if lnw_s and lnw_c:
        reg["lnw"] = {"server": [lnw_s], "client": [lnw_c]}

    asio_s, asio_c = find_cpp("asio-bench-echo-server"), find_cpp("asio-bench-echo-client")
    if asio_s and asio_c:
        reg["asio"] = {"server": [asio_s], "client": [asio_c]}

    if node:
        reg["node"] = {
            "server": [node, str(HERE / "node" / "echo-server.js")],
            "client": [node, str(HERE / "node" / "echo-client.js")],
        }

    reg["python"] = {
        "server": [py, str(HERE / "python" / "echo_server.py")],
        "client": [py, str(HERE / "python" / "echo_client.py")],
    }
    return reg


@contextlib.contextmanager
def echo_server(argv: list[str], proto: str, host: str):
    """Start an echo server, yield the port it bound, guarantee teardown."""
    env = dict(os.environ, PYTHONPATH=str(HERE / "python"))
    proc = subprocess.Popen(
        argv + ["--proto", proto, "--host", host, "--port", "0"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        start_new_session=True,
        env=env,
    )
    port = None
    try:
        deadline = time.monotonic() + 10.0
        while time.monotonic() < deadline:
            line = proc.stdout.readline()
            if not line:
                break
            if line.startswith("READY"):
                port = int(line.split()[1])
                break
        if port is None:
            err = proc.stderr.read() if proc.stderr else ""
            raise RuntimeError(f"server never reported READY: {' '.join(argv)}\n{err}")
        _await_port(host, port, proto)
        yield port
    finally:
        with contextlib.suppress(ProcessLookupError):
            os.killpg(proc.pid, signal.SIGTERM)
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            with contextlib.suppress(ProcessLookupError):
                os.killpg(proc.pid, signal.SIGKILL)
            proc.wait(timeout=3)
        if proc.stdout:
            proc.stdout.close()
        if proc.stderr:
            proc.stderr.close()


def _await_port(host: str, port: int, proto: str, tries: int = 50) -> None:
    for _ in range(tries):
        try:
            if proto == "udp":
                s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                s.settimeout(0.5)
                s.sendto(b"\x00" * 8, (host, port))
                s.recvfrom(64)
                s.close()
                return
            s = socket.create_connection((host, port), timeout=0.5)
            s.close()
            return
        except OSError:
            time.sleep(0.05)
    # not fatal - the client will surface any real problem


def run_client(argv: list[str], *, proto, host, port, connections, payload, pipeline, duration, warmup) -> dict:
    env = dict(os.environ, PYTHONPATH=str(HERE / "python"))
    cmd = argv + [
        "--proto", proto, "--host", host, "--port", str(port),
        "--connections", str(connections), "--payload", str(payload),
        "--pipeline", str(pipeline), "--duration", str(duration), "--warmup", str(warmup),
    ]
    out = subprocess.run(cmd, capture_output=True, text=True, env=env,
                         timeout=warmup + duration + 60)
    lines = [ln for ln in out.stdout.splitlines() if ln.strip().startswith("{")]
    if not lines:
        raise RuntimeError(f"client produced no JSON result\ncmd: {' '.join(cmd)}\n"
                           f"stdout: {out.stdout}\nstderr: {out.stderr}")
    return json.loads(lines[-1])


def fmt_int(x) -> str:
    return f"{int(round(x)):,}"


def markdown_tables(rows: list[dict]) -> str:
    keys = sorted({(r["proto"], r["payload"]) for r in rows})
    out = []
    for proto, payload in keys:
        out.append(f"### {proto.upper()} echo · {payload}-byte payload\n")
        out.append("| stack | conns | pipeline | req/s | MB/s | p50 µs | p99 µs | p99.9 µs | errors |")
        out.append("|---|--:|--:|--:|--:|--:|--:|--:|--:|")
        sub = [r for r in rows if r["proto"] == proto and r["payload"] == payload]
        sub.sort(key=lambda r: (r["connections"], ALL_STACKS.index(r["stack"]) if r["stack"] in ALL_STACKS else 9))
        for r in sub:
            lat = r["lat_us"]
            out.append(
                f"| {r['stack']} | {r['connections']} | {r['pipeline']} | "
                f"{fmt_int(r['rps'])} | {r['throughput_mbps']:.1f} | "
                f"{lat['p50']:.0f} | {lat['p99']:.0f} | {lat['p999']:.0f} | {r['errors']} |"
            )
        out.append("")
    return "\n".join(out)


def environment() -> dict:
    cpu = platform.processor()
    with contextlib.suppress(OSError):
        for line in Path("/proc/cpuinfo").read_text().splitlines():
            if line.startswith("model name"):
                cpu = line.split(":", 1)[1].strip()
                break

    def ver(*cmd):
        with contextlib.suppress(Exception):
            return subprocess.run(cmd, capture_output=True, text=True).stdout.splitlines()[0].strip()
        return None

    return {
        "host": platform.node(),
        "kernel": platform.release(),
        "cpu": cpu,
        "cpu_count": os.cpu_count(),
        "python": platform.python_version(),
        "node": ver("node", "--version"),
        "compiler": ver("c++", "--version"),
        "timestamp": datetime.now(timezone.utc).isoformat(timespec="seconds"),
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build-dir", type=Path, help="CMake build dir holding the C++ bench binaries")
    ap.add_argument("--stacks", default=",".join(ALL_STACKS), help="comma list: lnw,asio,node,python")
    ap.add_argument("--protos", default="tcp,udp", help="comma list: tcp,udp")
    ap.add_argument("--connections", default="1,16,64", help="comma list of connection counts")
    ap.add_argument("--payloads", default="64", help="comma list of payload sizes in bytes")
    ap.add_argument("--pipeline", type=int, default=1, help="in-flight requests per connection")
    ap.add_argument("--duration", type=float, default=10.0, help="measured seconds per run")
    ap.add_argument("--warmup", type=float, default=3.0, help="warmup seconds per run")
    ap.add_argument("--repeat", type=int, default=1, help="runs per combination (median reported)")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--out-dir", type=Path, default=HERE / "results")
    args = ap.parse_args()

    want = [s.strip() for s in args.stacks.split(",") if s.strip()]
    reg = discover(args.build_dir)
    stacks = [s for s in want if s in reg]
    missing = [s for s in want if s not in reg]
    for s in missing:
        hint = " (configure with -DLAMBDATECH_NETWORKING_BUILD_BENCHMARKS=ON and build)" if s in ("lnw", "asio") else ""
        print(f"note: skipping '{s}' - binary/runtime not found{hint}", file=sys.stderr)
    if not stacks:
        print("error: no runnable stacks", file=sys.stderr)
        return 1

    protos = [p.strip() for p in args.protos.split(",") if p.strip()]
    conns = [int(c) for c in args.connections.split(",")]
    payloads = [int(p) for p in args.payloads.split(",")]

    combos = [(pr, pl, c) for pr in protos for pl in payloads for c in conns]
    total = len(combos) * len(stacks) * args.repeat
    print(f"{total} runs: stacks={stacks} protos={protos} conns={conns} payloads={payloads} "
          f"pipeline={args.pipeline} duration={args.duration}s repeat={args.repeat}\n", file=sys.stderr)

    rows: list[dict] = []
    done = 0
    for proto, payload, c in combos:
        for stack in stacks:
            samples = []
            for _ in range(args.repeat):
                done += 1
                print(f"[{done}/{total}] {stack:7} {proto} conns={c:<4} payload={payload}", file=sys.stderr, flush=True)
                try:
                    with echo_server(reg[stack]["server"], proto, args.host) as port:
                        res = run_client(
                            reg[stack]["client"], proto=proto, host=args.host, port=port,
                            connections=c, payload=payload, pipeline=args.pipeline,
                            duration=args.duration, warmup=args.warmup,
                        )
                    samples.append(res)
                except Exception as exc:  # noqa: BLE001 - report and keep going
                    print(f"    FAILED: {exc}", file=sys.stderr)
            if samples:
                samples.sort(key=lambda r: r["rps"])
                rows.append(samples[len(samples) // 2])

    if not rows:
        print("error: every run failed", file=sys.stderr)
        return 1

    env = environment()
    tables = markdown_tables(rows)
    print("\n" + tables)

    args.out_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    (args.out_dir / f"bench-{stamp}.json").write_text(
        json.dumps({"environment": env, "settings": vars(args) | {"build_dir": str(args.build_dir)},
                    "rows": rows}, indent=2, default=str))
    md = [
        f"# Cross-stack echo benchmark — {stamp}", "",
        "| | |", "|---|---|",
        *[f"| {k} | {v} |" for k, v in env.items()],
        "",
        f"Closed loop, pipeline={args.pipeline}, {args.warmup}s warmup + {args.duration}s measured"
        f"{f', median of {args.repeat}' if args.repeat > 1 else ''}. "
        "req/s counts completed round-trips; MB/s counts payload both directions.",
        "", tables,
    ]
    (args.out_dir / f"bench-{stamp}.md").write_text("\n".join(md))
    print(f"\nwrote {args.out_dir}/bench-{stamp}.json and .md", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
