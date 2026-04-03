#!/usr/bin/env python3
import argparse
import math
import re
from pathlib import Path


CHANNEL_PATTERNS = {
    "PUSCH": re.compile(
        r"PUSCH\s+\d+\s+\d+:\s+c-rnti=0x(?P<rnti>[0-9a-fA-F]+).*CRC=(?P<crc>OK|KO).*?snr=(?P<snr>[+-]?\d+(?:\.\d+)?)"
    ),
    "PDSCH": re.compile(
        r"PDSCH\s+\d+\s+\d+:\s+(?P<rnti_type>si-rnti|ra-rnti|c-rnti)=0x(?P<rnti>[0-9a-fA-F]+).*CRC=(?P<crc>OK|KO).*?snr=(?P<snr>[+-]?\d+(?:\.\d+)?)"
    ),
}


def percentile(values, q):
    if not values:
        return math.nan
    if len(values) == 1:
        return values[0]
    pos = (len(values) - 1) * q
    lower = math.floor(pos)
    upper = math.ceil(pos)
    if lower == upper:
        return values[lower]
    weight = pos - lower
    return values[lower] * (1.0 - weight) + values[upper] * weight


def describe(values):
    if not values:
        return None
    ordered = sorted(values)
    count = len(ordered)
    avg = sum(ordered) / count
    return {
        "count": count,
        "avg": avg,
        "min": ordered[0],
        "p10": percentile(ordered, 0.10),
        "median": percentile(ordered, 0.50),
        "p90": percentile(ordered, 0.90),
        "max": ordered[-1],
    }


def print_stats(channel_name, bucket_name, stats):
    if stats is None:
        print(f"  {bucket_name:<6} no matching lines")
        return
    print(
        f"  {bucket_name:<6} "
        f"count={stats['count']:<4} "
        f"avg={stats['avg']:+6.2f} dB "
        f"min={stats['min']:+6.2f} "
        f"p10={stats['p10']:+6.2f} "
        f"med={stats['median']:+6.2f} "
        f"p90={stats['p90']:+6.2f} "
        f"max={stats['max']:+6.2f}"
    )


def summarize_channel(name, pattern, text):
    by_crc = {"OK": [], "KO": []}
    for match in pattern.finditer(text):
        by_crc[match.group("crc")].append(float(match.group("snr")))

    print(name)
    ok_stats = describe(by_crc["OK"])
    ko_stats = describe(by_crc["KO"])
    print_stats(name, "OK", ok_stats)
    print_stats(name, "KO", ko_stats)

    if ok_stats is not None and ko_stats is not None:
        print(f"  delta  avg(OK)-avg(KO) = {ok_stats['avg'] - ko_stats['avg']:+.2f} dB")
    print("")


def main():
    parser = argparse.ArgumentParser(description="Summarize SNR distributions for PUSCH/PDSCH CRC results in a sni5gect log.")
    parser.add_argument("logfile", help="Path to sni5gect stdout log")
    args = parser.parse_args()

    log_path = Path(args.logfile)
    if not log_path.is_file():
        parser.error(f"log file not found: {log_path}")

    text = log_path.read_text(errors="ignore")

    print(f"Log file: {log_path}")
    print("")
    for channel_name, pattern in CHANNEL_PATTERNS.items():
        summarize_channel(channel_name, pattern, text)


if __name__ == "__main__":
    raise SystemExit(main())
