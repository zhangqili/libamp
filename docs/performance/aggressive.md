# Aggressive performance results

Branch: `codex-aggressive-performance`, based on `codex-fable` at `bcecc2e`.

The scan pipeline and Nexus loop do less repeated work while retaining the
existing state machines, arithmetic, hooks, and public entry points. The priority
workloads are local keyboard scans, calibration, HSV conversion, and Nexus.

## Host measurements

AppleClang 21.0.0, arm64 macOS, CMake Release (`-O3 -DNDEBUG`, no LTO).
Medians of nine alternating baseline/candidate runs, with 2,000,000 ticks and
200,000 frames per run. Both executables use the unchanged benchmark workload
from `codex-fable`. Local scans have 64 keys; the Nexus fixture maps 16 slave keys.
Input feeding and simulated transport are included in the relevant workloads.
The RGB frame rows check for collateral regressions; their algorithms were not
changed in this pass.

These are host CPU timings. AT32F405 cycles, scan deadlines, USB latency, and the
complete board firmware image need measurements on the actual board.

| Workload | Unit | codex-fable | Aggressive | Observed time saved |
| --- | --- | ---: | ---: | ---: |
| keyboard_task, idle keys | ns/tick | 348.700 | 291.900 | +16.3% |
| keyboard_task, travelling keys | ns/tick | 317.800 | 277.200 | +12.8% |
| advanced_key_set_range | ns/key | 0.684 | 0.594 | +13.2%* |
| hsv_to_rgb, saturated colors | ns/color | 1.633 | 1.591 | +2.6% |
| nexus_process, 16 idle slave keys | ns/tick | 10.800 | 7.900 | +26.9% |
| nexus_process, 16 toggling slave keys | ns/tick | 25.400 | 22.200 | +12.6% |
| rgb_process, rainbow + linear | ns/frame | 354.400 | 353.100 | +0.4% |
| rgb_process, wave + fading trigger | ns/frame | 277.100 | 277.300 | -0.1% |
| rgb_process, wave + held trigger | ns/frame | 173.300 | 173.500 | -0.1% |
| rgb_process, 8 live bubble ripples | ns/frame | 465.000 | 464.400 | +0.1% |

*Calibration generates the same instructions in both host executables and still
uses one native divide on Cortex-M4. No additional calibration speedup is claimed.
Its individual samples span 0.564–0.743 ns/key for the baseline and
0.555–0.745 ns/key for the candidate, so the median shift must not be treated as an
algorithm improvement. Sub-percent RGB changes are also within run variation.

Every raw sample is retained in [aggressive-samples.json](aggressive-samples.json).

## Complete change list

1. **Local scan pipeline:** move the existing raw filtering, calibration, and
   normalized state machine into a shared private inline implementation. The
   keyboard wrapper can combine it with debounce/report handling without calling
   through each public layer. Digital, normal, rapid-trigger, and speed modes keep
   their original calculations and order. Public wrappers remain available.
2. **Default raw sampling:** share the ring-buffer average implementation with the
   default sampling hook, removing another call per key. The mapped channel,
   running sum, dirty-window fallback, and non-optimized window scan are unchanged.
3. **Nexus settled keys:** test adjacent physical/report state bytes with an
   alias-safe `memcpy` load; include pending debounce in the same unsettled test.
   A zero bitmap word uses a short loop without per-slot shifts. Every key and
   current binding is still inspected, including keys changed outside Nexus.
4. **Nexus active keys:** share the keyboard report/update implementation and
   inline it in the active-word loop. Keep the zero-word loop compact. Traversal
   remains in slot order, and mapping bounds are refreshed after updates that
   may invoke callbacks. Whole-tick bitmap snapshots and duplicate-map behavior
   are retained.
5. **Nexus receive decoding:** replace the four-iteration byte decoder with
   explicit bounded little-endian assembly. On the usual 16-slot configuration,
   the compiler can use a halfword load. Short frames, unused bytes, and the
   single volatile store per bitmap word keep their previous behavior.
6. **HSV conversion:** use two exact bounded integer reciprocals, share the
   `v * 255` product, and compute interpolation from distance to the sector
   boundary. Every output byte, including invalid hue, grayscale, and clamped
   percentage behavior, is preserved. No lookup table is added.
7. **Validation and measurement tools:** add raw-pipeline/rapid-trigger/ring-buffer/
   debounce/Nexus characterization tests, a wide-bitmap Nexus target, exhaustive
   raw-hue coverage, and a reusable cross-revision pipeline trace runner. Rename
   the benchmark table's baseline heading so it is not tied to `fable`.

Calibration arithmetic and debounce policy are unchanged. No features are
removed or approximated, and board overrides of raw sampling and normalization
remain callable. Calibration smoothing still has one shared history array when
its option is enabled. The implementation uses portable C; no assembly branch or
board HAL rewrite was needed.

## Target code generation and memory tradeoff

GCC 14.2.1 compiled the changed core translation units for Cortex-M0, Cortex-M3,
and Cortex-M4. For M4, the flags include `-mthumb -mfloat-abi=hard
-mfpu=fpv4-sp-d16`, plus function/data sections. These are compilation checks of
the library core, not a complete AT32F405 firmware build.

The M4 HSV function shrank from **232 to 212 bytes**, with **four to two `UMULL`**
instructions and fewer literal loads. The scan path removes intermediate
function boundaries, and the Nexus resting-word loop avoids per-slot bit
extraction. These observations are instruction/code-size evidence, not measured
cycle counts.

Inlining trades some code space for scan speed. Totals below sum `advanced_key`,
`analog`, `keyboard`, `color`, and `nexus` object sections with the test config:

| Cortex-M4 core objects | Baseline text | Aggressive text | Difference |
| --- | ---: | ---: | ---: |
| `-O2` | 7,672 B | 9,140 B | +1,468 B |
| `-O3` | 16,928 B | 17,352 B | +424 B |

Their initialized data remains zero and BSS remains **8,195 B**. Final firmware
flash/RAM usage depends on the board configuration, linker garbage collection,
and LTO. Firmware already using LTO may gain less from removing call boundaries.

## Behavior verification

- **213/213 Release tests passed**, both in the normal build and a complete build
  with `KEY_CALLBACK_ENABLE`.
- **71/71 focused ASan/UBSan tests passed** with halt-on-error enabled. Leak
  detection is disabled because the macOS sanitizer runtime does not support it.
- **80 differential traces matched `codex-fable`**: 20 configurations × two
  normalizers × two compiler flag sets (`-O3`, and `-O3 -flto -ffast-math`). Each
  trace exercises 262,144 samples, including runtime configuration edits,
  callbacks, all key/calibration modes, raw/normalized filters, calibration
  smoothing, debounce policies, different averaging windows, nonzero analog
  minimum, and large LUTs. The custom normalizer changes mode during a call to
  check override ordering.
- Calibration matched the wide signed definition across every nonzero uint16
  span in both directions and zero span, including forced-32-bit builds with
  LUT lengths 32,768 and 65,535.
- HSV matched its scalar reference for all **3,672,360 valid inputs** and
  **2,359,296 additional cases** covering every uint16 hue with representative
  percentages, clamping, and grayscale.
- Nexus checks cover external state/keymap edits, pending debounce, duplicate
  and invalid mappings, callbacks that change later keys or mapping lengths,
  frames arriving during dispatch, timeout release, and bitmap word boundaries.
  Extra ARM compilations cover callbacks, disabled debounce, raw/slave modes,
  byte values, and 1/32/33/64/128-slot limits.
- Independent source reviews found no critical or important issues.

The full sanitizer suite is not claimed clean: pre-existing packed-member
reference alignment failures and the out-of-range packet-index defect at
`src/packet.c:156` remain outside this performance change. The focused run excludes
the existing packed-member analog-copy assertion in the Nexus tests.

## Reproduce

Build both checkouts with the same compiler and the Release commands in the
[README](../../README.md). Then run:

```sh
python3 test/bench/compare.py path/to/codex-fable/libamp_bench path/to/aggressive/libamp_bench --repeats 9 --ticks 2000000 --frames 200000 --json comparison.json
python3 test/advanced_key/check_pipeline.py path/to/codex-fable-checkout .
python3 test/advanced_key/check_pipeline.py path/to/codex-fable-checkout . --cflags='-flto -ffast-math'
```

The trace runner needs only a host C compiler and the two source trees; it does
not depend on the firmware SDK or the CMake test fixture.

## Rejected experiments

A different debounce branch arrangement helped local host scans but increased
Nexus toggling time by 5.6% against the same candidate in five alternating runs,
so the original debounce implementation was restored. Inlining every Nexus
update reduced toggling time but enlarged the idle loop; retaining a compact
zero-word loop preserved the idle improvement. A specialized release helper and
HSV table/branch alternatives did not justify keeping them.
