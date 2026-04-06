#!/usr/bin/env python3

import random
import subprocess
import sys
import time
from dataclasses import dataclass

CV_BINARY_PATH = "./cv"


def norm(s: str) -> str:
    return s.replace("\r\n", "\n").strip()


@dataclass
class SoakCase:
    name: str
    command: str
    expected_stdout: str | None = None
    expected_exit: int = 0
    timeout: float = 5.0
    iterations: int = 100


def run_once(command: str, timeout: float):
    return subprocess.run(
        [CV_BINARY_PATH, command],
        capture_output=True,
        text=True,
        timeout=timeout,
    )


def check_case(case: SoakCase, iteration: int):
    try:
        proc = run_once(case.command, case.timeout)
    except subprocess.TimeoutExpired:
        return False, {
            "reason": "timeout",
            "iteration": iteration,
            "stdout": "",
            "stderr": "",
            "exit": None,
        }
    except Exception as exc:
        return False, {
            "reason": f"runner exception: {exc}",
            "iteration": iteration,
            "stdout": "",
            "stderr": "",
            "exit": None,
        }

    out = norm(proc.stdout)
    err = norm(proc.stderr)
    code = proc.returncode

    if code != case.expected_exit:
        return False, {
            "reason": f"exit mismatch: expected {case.expected_exit}, got {code}",
            "iteration": iteration,
            "stdout": out,
            "stderr": err,
            "exit": code,
        }

    if case.expected_stdout is not None and out != case.expected_stdout:
        return False, {
            "reason": f"stdout mismatch: expected {case.expected_stdout!r}, got {out!r}",
            "iteration": iteration,
            "stdout": out,
            "stderr": err,
            "exit": code,
        }

    return True, {
        "iteration": iteration,
        "stdout": out,
        "stderr": err,
        "exit": code,
    }


SOAK_CASES = [
    SoakCase(
        name="builtin-add",
        command="+ 2 3",
        expected_stdout="5",
        iterations=500,
    ),
    SoakCase(
        name="function-invoke",
        command="[[let add [fn [a b][+ a b]]][add 2 3]]",
        expected_stdout="5",
        iterations=500,
    ),
    SoakCase(
        name="function-identity",
        command="[[let id [fn [x] x]][id 9]]",
        expected_stdout="9",
        iterations=500,
    ),
    SoakCase(
        name="import-json-then-plus",
        command="[[import 'json'][+ 2 3]]",
        expected_stdout="5",
        iterations=500,
    ),
    SoakCase(
        name="import-json-twice-then-plus",
        command="[[import 'json'][import 'json'][+ 2 3]]",
        expected_stdout="5",
        iterations=300,
    ),
    SoakCase(
        name="json-dump-inline-store",
        command="[[import 'json'][json:dump [[~a [1 2 3]] [~b [[~x 1] [~y nil]]]]]]",
        expected_stdout="'{\"a\": [1, 2, 3], \"b\": {\"x\": 1, \"y\": null}}'",
        iterations=300,
    ),
    SoakCase(
        name="anonymous-store-top-level",
        command="[[~a 1] [~b 2]]",
        iterations=300,
    ),
    SoakCase(
        name="thread-await-simple",
        command="[[await |[+ 1 2]]]",
        expected_stdout="3",
        iterations=300,
    ),
    SoakCase(
        name="thread-await-nested",
        command="[[>> 3 [>> [await |[await |2]] 1]]]",
        expected_stdout="[1 2 3]",
        iterations=300,
        timeout=8.0,
    ),
]


def main():
    randomize = "--shuffle" in sys.argv
    stop_fast = "--keep-going" not in sys.argv

    cases = SOAK_CASES[:]
    if randomize:
        random.shuffle(cases)

    total_runs = 0
    total_failures = 0
    started = time.time()

    print("STARTING CANVAS SOAK TESTS")

    for case in cases:
        print(f"\n[{case.name}] running {case.iterations} iterations")
        for i in range(1, case.iterations + 1):
            total_runs += 1
            ok, info = check_case(case, i)

            if not ok:
                total_failures += 1
                print(f"FAIL: {case.name} at iteration {info['iteration']}")
                print(f"  reason: {info['reason']}")
                print(f"  command: {case.command}")
                print(f"  exit: {info['exit']}")
                print(f"  stdout: {info['stdout']!r}")
                print(f"  stderr: {info['stderr']!r}")

                if stop_fast:
                    elapsed = time.time() - started
                    print(f"\nABORTED AFTER {total_runs} RUNS, {total_failures} FAILURE(S), {elapsed:.2f}s")
                    raise SystemExit(1)
            elif i % 50 == 0 or i == case.iterations:
                print(f"  ok through iteration {i}")

    elapsed = time.time() - started
    print(f"\nDONE. TOTAL RUNS: {total_runs} | FAILURES: {total_failures} | TIME: {elapsed:.2f}s")

    if total_failures == 0:
        print("<---------------------SOAK TESTS PASSED--------------------->")
        raise SystemExit(0)

    raise SystemExit(1)


if __name__ == "__main__":
    main()