#!/usr/bin/env python3
"""Compare raw-pipeline traces against an unchanged source checkout.

Example: python3 test/advanced_key/check_pipeline.py /tmp/codex-fable .
Uses the same harness/configurations for both revisions; fails on any mismatch.
"""
import argparse
import shlex
import subprocess
import tempfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--cflags", default="", help="additional compiler/linker flags, e.g. '-flto -ffast-math'")
    args = parser.parse_args()
    harness = Path(__file__).with_name("trace_pipeline.c").resolve()
    configs = {
        "plain": {},
        "callbacks": {"KEY_CALLBACK_ENABLE": 1},
        "no-debounce": {"DEBOUNCE_PRESS": 0, "DEBOUNCE_RELEASE": 0},
        "deferred-press-eager-release": {"DEBOUNCE_PRESS_EAGER": 0, "DEBOUNCE_RELEASE_EAGER": 1},
        "both-deferred": {"DEBOUNCE_PRESS_EAGER": 0, "DEBOUNCE_RELEASE_EAGER": 0},
        "both-eager": {"DEBOUNCE_PRESS_EAGER": 1, "DEBOUNCE_RELEASE_EAGER": 1},
        "raw-lowpass": {"FILTER_ENABLE": 1},
        "normalized-lowpass": {"FILTER_ENABLE": 1, "FILTER_DOMAIN": 1},
        "raw-kalman": {"FILTER_ENABLE": 1, "FILTER_TYPE": 1},
        "normalized-kalman": {"FILTER_ENABLE": 1, "FILTER_TYPE": 1, "FILTER_DOMAIN": 1},
        "raw-hysteresis": {"FILTER_HYSTERESIS_ENABLE": 1},
        "normalized-hysteresis": {"FILTER_HYSTERESIS_ENABLE": 1, "FILTER_DOMAIN": 1},
        "calibration-lowpass": {"CALIBRATION_LPF_ENABLE": 1},
        "all-raw-filters": {"FILTER_ENABLE": 1, "FILTER_TYPE": 1, "FILTER_HYSTERESIS_ENABLE": 1, "CALIBRATION_LPF_ENABLE": 1, "KEY_CALLBACK_ENABLE": 1},
        "all-normalized-filters": {"FILTER_ENABLE": 1, "FILTER_TYPE": 1, "FILTER_DOMAIN": 1, "FILTER_HYSTERESIS_ENABLE": 1, "CALIBRATION_LPF_ENABLE": 1, "KEY_CALLBACK_ENABLE": 1},
        "maximum-lut": {"LUT_LENGTH": 65535},
        "signed-lut-boundary": {"LUT_LENGTH": 32768},
        "single-sample-window": {"RING_BUF_LEN": 1},
        "scanned-window": {"RING_BUF_LEN": 8, "OPTIMIZE_MOVING_AVERAGE_FOR_RINGBUF": None},
        "nonzero-analog-minimum": {"ANALOG_VALUE_MIN": 100, "ANALOG_VALUE_MAX": 60000},
    }
    defaults = dict(ADVANCED_KEY_NUM=8, KEY_NUM=0, LAYER_NUM=1, LUT_LENGTH=8192,
                    DEFAULT_ESTIMATED_RANGE=500, RING_BUF_LEN=2,
                    DEBOUNCE_PRESS=10, DEBOUNCE_RELEASE=10,
                    DEBOUNCE_PRESS_EAGER=1, DEBOUNCE_RELEASE_EAGER=0,
                    FILTER_TYPE=0, FILTER_DOMAIN=0, OPTIMIZE_MOVING_AVERAGE_FOR_RINGBUF=1)
    with tempfile.TemporaryDirectory(prefix="libamp-pipeline-") as directory:
        build = Path(directory)
        for name, overrides in configs.items():
            definitions = defaults | overrides
            (build / "keyboard_config.h").write_text("#pragma once\n" + "".join(
                f"#define {key} {value}\n" for key, value in definitions.items() if value is not None))
            for custom in (False, True):
                traces = []
                for label, root in (("reference", args.reference), ("candidate", args.candidate)):
                    source = root.resolve() / "src"
                    binary = build / label
                    inline = label == "candidate"
                    command = [args.cc, "-std=c11", "-O3", "-Wall", "-Wextra", "-Werror",
                               "-I", str(build), "-I", str(source),
                               f"-DLIBAMP_CHECK_INLINE_PIPELINE={int(inline)}"]
                    if custom:
                        command.append("-DCHECK_CUSTOM_NORMALIZER=1")
                    command += shlex.split(args.cflags)
                    command += [str(harness), str(source / "advanced_key.c"),
                                str(source / "analog.c"), "-o", str(binary)]
                    subprocess.run(command, check=True)
                    traces.append(subprocess.check_output([str(binary)], text=True).strip())
                if traces[0] != traces[1]:
                    raise RuntimeError(f"{name}, custom={custom}: {traces}")
            print(f"PASS {name}: default and overridden normalizer", flush=True)
    print(f"{len(configs) * 2} matching traces, 262144 sample updates each")


if __name__ == "__main__":
    main()
