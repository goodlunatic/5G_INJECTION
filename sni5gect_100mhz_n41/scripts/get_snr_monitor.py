#!/usr/bin/env python3
import argparse
import re
import statistics
import subprocess
import sys
import time
from collections import defaultdict, deque
from pathlib import Path


LINE_RE = re.compile(
    r"(?P<channel>PDSCH|PUSCH)\s+\d+\s+(?P<slot>\d+):.*CRC=(?P<crc>OK|KO).*?snr=(?P<snr>[+-]?\d+\.\d+)"
)


def tail_file(path: Path):
    with path.open("r", errors="ignore") as f:
        f.seek(0, 2)
        while True:
            line = f.readline()
            if line:
                yield line
            else:
                time.sleep(0.2)


def stream_docker_logs(container: str):
    proc = subprocess.Popen(
        ["docker", "logs", "-f", container],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        errors="ignore",
        bufsize=1,
    )
    try:
        assert proc.stdout is not None
        for line in proc.stdout:
            yield line
    finally:
        proc.terminate()


def fmt_stats(values):
    if not values:
        return "n/a"
    vals = list(values)
    return f"avg={statistics.mean(vals):+6.1f} med={statistics.median(vals):+6.1f} min={min(vals):+6.1f} max={max(vals):+6.1f}"


def print_report(stats, window_size):
    print("\033[2J\033[H", end="")
    print(f"Rolling SNR monitor, window={window_size} frames")
    print("")
    for channel in ("PDSCH", "PUSCH"):
        channel_stats = stats[channel]
        print(channel)
        print(
            f"  last: {channel_stats['last_snr'] if channel_stats['last_snr'] is not None else 'n/a':>7}   "
            f"OK={channel_stats['ok_total']:>5} KO={channel_stats['ko_total']:>5}"
        )
        print(f"  all:  {fmt_stats(channel_stats['all'])}")
        print(f"  OK:   {fmt_stats(channel_stats['ok'])}")
        print(f"  KO:   {fmt_stats(channel_stats['ko'])}")
        print("")
    print("Ctrl+C to stop")


def main():
    parser = argparse.ArgumentParser(description="Real-time PDSCH/PUSCH SNR monitor for sni5gect logs.")
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--file", help="Follow a local log file")
    source.add_argument("--docker-container", help="Follow docker logs from a container, e.g. sni5gect")
    parser.add_argument("--window", type=int, default=200, help="Rolling sample window size")
    parser.add_argument("--refresh", type=float, default=1.0, help="Refresh interval in seconds")
    args = parser.parse_args()

    stats = defaultdict(
        lambda: {
            "all": deque(maxlen=args.window),
            "ok": deque(maxlen=args.window),
            "ko": deque(maxlen=args.window),
            "ok_total": 0,
            "ko_total": 0,
            "last_snr": None,
        }
    )

    if args.file:
        stream = tail_file(Path(args.file))
    else:
        stream = stream_docker_logs(args.docker_container)

    next_refresh = time.monotonic() + args.refresh
    for line in stream:
        match = LINE_RE.search(line)
        if match:
            channel = match.group("channel")
            crc = match.group("crc")
            snr = float(match.group("snr"))
            channel_stats = stats[channel]
            channel_stats["all"].append(snr)
            channel_stats["last_snr"] = f"{snr:+.1f} dB"
            if crc == "OK":
                channel_stats["ok"].append(snr)
                channel_stats["ok_total"] += 1
            else:
                channel_stats["ko"].append(snr)
                channel_stats["ko_total"] += 1

        now = time.monotonic()
        if now >= next_refresh:
            print_report(stats, args.window)
            next_refresh = now + args.refresh


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(0)
