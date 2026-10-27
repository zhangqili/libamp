#!/usr/bin/env python3
"""Compare identical hot-path workloads linked against two libamp revisions."""

import argparse
import json
import platform
import re
import statistics
import subprocess
from pathlib import Path


def positive_int(value):
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return number


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--repeats", type=positive_int, default=7)
    parser.add_argument("--ticks", type=positive_int, default=2000000)
    parser.add_argument("--frames", type=positive_int, default=200000)
    parser.add_argument("--json", type=Path, help="save every raw sample and the run settings")
    args = parser.parse_args()
    binaries = {"baseline": args.baseline.resolve(), "candidate": args.candidate.resolve()}
    samples = {name: {} for name in binaries}
    units = {}
    pattern = re.compile(r"^(.*?)\s*:\s*([0-9.]+)\s+(ns/\w+)$")

    # Alternating the execution order reduces the influence of gradual clock
    # and temperature changes. Run each binary alone, never concurrently.
    for repeat in range(args.repeats):
        order = ("baseline", "candidate") if repeat % 2 == 0 else ("candidate", "baseline")
        for name in order:
            output = subprocess.check_output(
                [str(binaries[name]), str(args.ticks), str(args.frames)], text=True
            )
            measured = {}
            for line in output.splitlines():
                match = pattern.fullmatch(line)
                if match:
                    metric, value, unit = match.groups()
                    metric = metric.strip()
                    if metric in units and units[metric] != unit:
                        raise RuntimeError(f"unit changed for {metric}")
                    units[metric] = unit
                    measured[metric] = float(value)
            if not measured or any(value <= 0 for value in measured.values()):
                raise RuntimeError(f"{name} did not produce positive benchmark measurements")
            if samples["baseline"] and measured.keys() != samples["baseline"].keys():
                raise RuntimeError("benchmark workloads differ; build both with the same benchmark source")
            for metric, value in measured.items():
                samples[name].setdefault(metric, []).append(value)

    print(f"Median of {args.repeats} alternating runs; positive savings mean less time.")
    print("| Workload | Unit | Baseline | Candidate | Time saved |")
    print("| --- | --- | ---: | ---: | ---: |")
    for metric in samples["baseline"]:
        baseline = statistics.median(samples["baseline"][metric])
        candidate = statistics.median(samples["candidate"][metric])
        savings = 100 * (1 - candidate / baseline)
        print(f"| {metric} | {units[metric]} | {baseline:.3f} | {candidate:.3f} | {savings:+.1f}% |")
    if args.json:
        args.json.write_text(json.dumps({
            "platform": platform.platform(),
            "binaries": {name: str(path) for name, path in binaries.items()},
            "ticks": args.ticks,
            "frames": args.frames,
            "repeats": args.repeats,
            "units": units,
            "samples": samples,
        }, indent=2) + "\n")


if __name__ == "__main__":
    main()
