#!/usr/bin/env python3
"""Black-box termination checks against a built MP2 simulator and examples."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("test_dir", type=Path, help="MP2 code/test containing rebuilt examples")
parser.add_argument("output_dir", type=Path)
parser.add_argument("--case", choices=["pair"], help="Run only the baseline reproducer")
args = parser.parse_args()
test_dir = args.test_dir.resolve()
binary = test_dir.parent / "build.linux/nachos"
if not test_dir.is_dir() or not binary.is_file():
    parser.error("A built Linux MP2 test directory and simulator are required")
args.output_dir.mkdir(parents=True, exist_ok=True)
missing = "__mp2_shutdown_missing_executable__"
if (test_dir / missing).exists():
    parser.error("The missing-executable fixture unexpectedly exists")
cases = {
    "pair": (["-e", "consoleIO_test1", "-e", "consoleIO_test2"], 2, False),
    "single": (["-e", "consoleIO_test1"], 1, False),
    "reversed": (["-e", "consoleIO_test2", "-e", "consoleIO_test1"], 2, False),
    "seed_7": (["-rs", "7", "-e", "consoleIO_test1", "-e", "consoleIO_test2"], 2, False),
    "seed_23": (["-rs", "23", "-e", "consoleIO_test1", "-e", "consoleIO_test2"], 2, False),
    "missing": (["-e", missing], 0, True),
    "missing_then_valid": (["-e", missing, "-e", "consoleIO_test2"], 1, True),
}
results = []
for name, (options, completions, expect_missing) in cases.items():
    if args.case and name != args.case:
        continue
    timed_out = False
    try:
        run = subprocess.run([str(binary), *options], cwd=test_dir,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=5)
        output, code = run.stdout, run.returncode
    except subprocess.TimeoutExpired as exc:
        output, code, timed_out = exc.stdout or b"", None, True
    text = output.decode(errors="replace")
    checks = {
        "normal_exit": code == 0 and not timed_out,
        "all_programs_returned": text.count("return value:0") == completions,
        "single_shutdown": text.count("Machine halting!") == 1,
        "missing_reported": ("Unable to open file " + missing in text) == expect_missing,
        "no_assertion": "Assertion failed" not in text,
    }
    (args.output_dir / f"{name}.log").write_bytes(output)
    passed = all(checks.values())
    results.append({"case": name, "passed": passed, "returncode": code,
                    "timed_out": timed_out, "checks": checks})
    print(f"{name}: {'PASS' if passed else 'FAIL'}", flush=True)
(args.output_dir / "results.json").write_text(json.dumps(results, indent=2) + "\n")
sys.exit(0 if all(r["passed"] for r in results) else 1)
