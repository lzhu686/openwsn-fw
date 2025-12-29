#!/usr/bin/env python3
"""Aggregate packet log files and plot PDR/RSSI vs distance.

Expected log format (one line per packet):
    <len> <seq> <crc_ok> <rssi>

Usage:
    python3 plot_pdr_rssi.py --log-dir ./ --save plots.png

Outputs a two-panel plot (PDR and RSSI) either saved to disk or displayed.
"""
import argparse
import os
import re
from statistics import mean, stdev
from typing import Dict, List, Optional, Tuple

import matplotlib.pyplot as plt

LOG_PATTERN = re.compile(r"log_(\d+)cm_pkt\.log$")
LINE_SPLIT_RE = re.compile(r"\s+")
SEQ_MODULO = 256


class DistanceStats:
    __slots__ = ("distance", "received", "missing", "crc_pass", "rssis")

    def __init__(self, distance: int) -> None:
        self.distance = distance
        self.received = 0
        self.missing = 0
        self.crc_pass = 0
        self.rssis: List[int] = []

    @property
    def expected(self) -> int:
        return self.received + self.missing

    @property
    def pdr(self) -> float:
        if self.expected == 0:
            return 0.0
        return self.received / self.expected

    @property
    def avg_rssi(self) -> float:
        return mean(self.rssis) if self.rssis else float("nan")

    @property
    def std_rssi(self) -> float:
        return stdev(self.rssis) if len(self.rssis) > 1 else 0.0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Plot PDR and RSSI vs distance from packet logs")
    parser.add_argument("--log-dir", default=".", help="Directory containing log_XXcm_pkt.log files")
    parser.add_argument("--save", default=None, help="Output path to save the plot instead of showing it")
    parser.add_argument("--show", action="store_true", help="Force displaying the plot even if --save is used")
    parser.add_argument("--min-crc", type=int, choices=[0, 1], default=1,
                        help="Minimum CRC value to treat packet as valid (default: 1)")
    parser.add_argument("--verbose", action="store_true", help="Print per-file statistics")
    return parser.parse_args()


def iter_log_files(log_dir: str):
    for entry in os.listdir(log_dir):
        match = LOG_PATTERN.match(entry)
        if match:
            yield int(match.group(1)), os.path.join(log_dir, entry)


def analyse_file(path: str, min_crc: int) -> Tuple[int, int, int, List[int]]:
    received = 0
    missing = 0
    rssis: List[int] = []
    crc_pass = 0
    prev_seq = None

    with open(path, "r", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if not line:
                continue
            parts = LINE_SPLIT_RE.split(line)
            if len(parts) != 4:
                continue
            try:
                length, seq, crc_ok, rssi = (int(parts[0]), int(parts[1]), int(parts[2]), int(parts[3]))
            except ValueError:
                continue

            if crc_ok < min_crc:
                continue

            received += 1
            if crc_ok:
                crc_pass += 1
            rssis.append(rssi)

            if prev_seq is not None:
                diff = (seq - prev_seq) % SEQ_MODULO
                if diff == 0:
                    # duplicate sequence; ignore for missing computation
                    continue
                if diff > 1:
                    missing += diff - 1
            prev_seq = seq
        # For overall expected count, assume start of log is first packet => no assumption on leading losses
    return received, missing, crc_pass, rssis


def gather_stats(log_dir: str, min_crc: int, verbose: bool) -> Dict[int, DistanceStats]:
    stats: Dict[int, DistanceStats] = {}
    for distance, path in sorted(iter_log_files(log_dir)):
        received, missing, crc_pass, rssis = analyse_file(path, min_crc)
        entry = DistanceStats(distance)
        entry.received = received
        entry.missing = missing
        entry.crc_pass = crc_pass
        entry.rssis = rssis
        stats[distance] = entry
        if verbose:
            print(f"{distance}cm: received={received}, missing={missing}, PDR={entry.pdr:.3f}, avg RSSI={entry.avg_rssi:.1f} dBm")
    return stats


def plot_stats(stats: Dict[int, DistanceStats], save_path: Optional[str], show_plot: bool) -> None:
    if not stats:
        print("No log files found matching pattern log_*cm_pkt.log")
        return

    distances = sorted(stats.keys())
    pdr_values = [stats[d].pdr for d in distances]
    rssi_values = [stats[d].avg_rssi for d in distances]
    rssi_std = [stats[d].std_rssi for d in distances]

    fig, (ax_pdr, ax_rssi) = plt.subplots(2, 1, figsize=(8, 8), sharex=True)

    ax_pdr.plot(distances, pdr_values, marker="o", linestyle="-", color="tab:blue")
    ax_pdr.set_ylabel("PDR")
    ax_pdr.set_ylim(0, 1.05)
    ax_pdr.grid(True, linestyle="--", alpha=0.4)

    ax_rssi.errorbar(distances, rssi_values, yerr=rssi_std, marker="s", linestyle="-", color="tab:orange",
                     ecolor="tab:orange", capsize=4)
    ax_rssi.set_xlabel("Distance (cm)")
    ax_rssi.set_ylabel("Average RSSI (dBm)")
    ax_rssi.grid(True, linestyle="--", alpha=0.4)

    fig.suptitle("Packet Delivery Rate and RSSI vs Distance")
    fig.tight_layout(rect=[0, 0.03, 1, 0.95])

    if save_path:
        fig.savefig(save_path, dpi=150)
        print(f"Plot saved to {save_path}")
        if not show_plot:
            plt.close(fig)
            return
    plt.show()


def main() -> None:
    args = parse_args()
    stats = gather_stats(args.log_dir, args.min_crc, args.verbose)
    plot_stats(stats, args.save, args.show)


if __name__ == "__main__":
    main()
