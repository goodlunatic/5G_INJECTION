#!/usr/bin/env python3
import argparse
import re
from collections import defaultdict
from pathlib import Path


CHANNEL_PATTERNS = {
    "PUSCH": re.compile(
        r"PUSCH\s+(?P<task>\d+)\s+(?P<slot>\d+):\s+c-rnti=0x(?P<rnti>[0-9a-fA-F]+).*CRC=(?P<crc>OK|KO)"
    ),
    "PDSCH": re.compile(
        r"PDSCH\s+(?P<task>\d+)\s+(?P<slot>\d+):\s+(?P<rnti_type>si-rnti|ra-rnti|c-rnti)=0x(?P<rnti>[0-9a-fA-F]+).*CRC=(?P<crc>OK|KO)"
    ),
}


def pct(numerator: int, denominator: int) -> float:
    return (numerator / denominator * 100.0) if denominator else 0.0


def format_ratio(ok: int, total: int) -> str:
    return f"{ok}/{total} ({pct(ok, total):.2f}%)"


def summarize_channel(name: str, matches, top: int) -> None:
    if not matches:
        print(f"{name}")
        print("  No matching CRC lines found")
        print("")
        return

    attempt_ok = 0
    attempts_total = 0
    slot_results = {}
    rnti_attempts = defaultdict(lambda: {"ok": 0, "total": 0, "type": ""})
    rnti_slots = defaultdict(dict)

    for match in matches:
        slot = int(match.group("slot"))
        crc_ok = match.group("crc") == "OK"
        rnti = match.group("rnti").lower()
        rnti_type = match.groupdict().get("rnti_type", "c-rnti")
        rnti_key = f"{rnti_type}:{rnti}"

        attempts_total += 1
        attempt_ok += int(crc_ok)

        rnti_attempts[rnti_key]["total"] += 1
        rnti_attempts[rnti_key]["ok"] += int(crc_ok)
        rnti_attempts[rnti_key]["type"] = rnti_type

        slot_results[slot] = slot_results.get(slot, False) or crc_ok
        rnti_slots[rnti_key][slot] = rnti_slots[rnti_key].get(slot, False) or crc_ok

    unique_slot_ok = sum(slot_results.values())
    unique_slot_total = len(slot_results)

    print(name)
    print(f"  Attempt success rate:     {format_ratio(attempt_ok, attempts_total)}")
    print(f"  Unique-slot success rate: {format_ratio(unique_slot_ok, unique_slot_total)}")
    print(f"  Total {name} lines:        {attempts_total}")
    print(f"  Unique {name} slots:       {unique_slot_total}")
    print("")
    print(f"  Per RNTI for {name}")
    print("    RNTI                 Attempt OK/Total      Slot OK/Total")

    def sort_key(item):
        rnti_key, data = item
        slot_map = rnti_slots[rnti_key]
        return (sum(slot_map.values()), len(slot_map), data["ok"], data["total"], rnti_key)

    for rnti_key, data in sorted(rnti_attempts.items(), key=sort_key, reverse=True)[:top]:
        slot_map = rnti_slots[rnti_key]
        print(
            f"    {rnti_key:<20} "
            f"{format_ratio(data['ok'], data['total']):<20} "
            f"{format_ratio(sum(slot_map.values()), len(slot_map))}"
        )
    print("")


def main() -> int:
    parser = argparse.ArgumentParser(description="Summarize sni5gect PUSCH/PDSCH CRC performance from a log file.")
    parser.add_argument("logfile", help="Path to sni5gect stdout log")
    parser.add_argument("--top", type=int, default=20, help="Max number of RNTIs to print for each channel")
    args = parser.parse_args()

    log_path = Path(args.logfile)
    if not log_path.is_file():
        parser.error(f"log file not found: {log_path}")

    text = log_path.read_text(errors="ignore")

    print(f"Log file: {log_path}")
    print("")
    for channel_name, pattern in CHANNEL_PATTERNS.items():
        summarize_channel(channel_name, list(pattern.finditer(text)), args.top)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
