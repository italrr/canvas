#!/usr/bin/env python3

import json
import re
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Optional, Union

CV_BINARY_PATH = "./cv"


def normalize_output(s: str) -> str:
    return s.replace("\r\n", "\n").strip()


def unwrap_canvas_string(s: str) -> str:
    s = normalize_output(s)
    if len(s) >= 2 and s[0] == "'" and s[-1] == "'":
        return s[1:-1]
    return s


def run_canvas(command: str, timeout: float = 5.0):
    return subprocess.run(
        [CV_BINARY_PATH, command],
        capture_output=True,
        text=True,
        timeout=timeout,
    )


@dataclass
class TestCase:
    name: str
    command: Union[str, Callable[[Path], str]]
    validator: Callable[[int, str, str], tuple[bool, str]]
    repeat: int = 1
    timeout: float = 5.0


def exact(expected: str, exit_code: int = 0):
    def _check(code: int, out: str, err: str):
        out_n = normalize_output(out)
        ok = (code == exit_code) and (out_n == expected)
        detail = f"expected stdout={expected!r}, got stdout={out_n!r}, exit={code}, stderr={normalize_output(err)!r}"
        return ok, detail
    return _check


def contains(needle: str, exit_code: Optional[int] = None):
    def _check(code: int, out: str, err: str):
        blob = normalize_output(out + "\n" + err)
        ok = (needle in blob) and (exit_code is None or code == exit_code)
        detail = f"expected combined output to contain {needle!r}, got stdout={normalize_output(out)!r}, stderr={normalize_output(err)!r}, exit={code}"
        return ok, detail
    return _check


def regex(pattern: str, exit_code: Optional[int] = None):
    rx = re.compile(pattern, re.DOTALL)

    def _check(code: int, out: str, err: str):
        blob = normalize_output(out + "\n" + err)
        ok = bool(rx.search(blob)) and (exit_code is None or code == exit_code)
        detail = f"expected combined output to match /{pattern}/, got stdout={normalize_output(out)!r}, stderr={normalize_output(err)!r}, exit={code}"
        return ok, detail
    return _check


def json_string_eq(expected_obj, exit_code: int = 0):
    def _check(code: int, out: str, err: str):
        out_n = normalize_output(out)
        try:
            parsed = json.loads(unwrap_canvas_string(out_n))
            ok = (code == exit_code) and (parsed == expected_obj)
            detail = f"expected JSON={expected_obj!r}, got JSON={parsed!r}, raw stdout={out_n!r}, exit={code}, stderr={normalize_output(err)!r}"
            return ok, detail
        except Exception as exc:
            return False, f"failed to parse stdout as JSON string: {out_n!r}, exc={exc}, exit={code}, stderr={normalize_output(err)!r}"
    return _check


def one_of(options: list[str], exit_code: int = 0):
    def _check(code: int, out: str, err: str):
        out_n = normalize_output(out)
        ok = (code == exit_code) and (out_n in options)
        detail = f"expected one of {options!r}, got stdout={out_n!r}, exit={code}, stderr={normalize_output(err)!r}"
        return ok, detail
    return _check


def no_crash_nonempty(exit_code: int = 0):
    def _check(code: int, out: str, err: str):
        out_n = normalize_output(out)
        ok = (code == exit_code) and (out_n != "")
        detail = f"expected non-empty stdout, got stdout={out_n!r}, exit={code}, stderr={normalize_output(err)!r}"
        return ok, detail
    return _check


cases = [
    # --- literals / basic runtime ---
    TestCase("literal:number", "1", exact("1")),
    TestCase("literal:list", "[1 2 3]", exact("[1 2 3]")),
    TestCase("literal:string", "'string'", exact("'string'")),
    TestCase("typeof:store", "[[typeof [[~n 10][~k 21]]]]", exact("'STORE'")),

    # --- arithmetic ---
    TestCase("arith:add", "+ 2 2", exact("4")),
    TestCase("arith:add-nested", "+ [+ 1 2] 3", exact("6")),
    TestCase("arith:square", "** 4", exact("16")),
    TestCase("arith:half-of-square", "[[// [** 4]]]", exact("8")),
    TestCase("arith:variadic-add", "+ 2 2 2", exact("6")),
    TestCase("mut:plusplus", "[[++ 2]]", exact("3")),
    TestCase("mut:minusminus", "[[-- 5]]", exact("4")),

    # --- variables / functions ---
    TestCase("let:variable", "[[let x 5][+ x 2]]", exact("7")),
    TestCase("fn:definition-returns-function", "[[let test [fn [a b][+ a b]]]]", exact("[fn [a b] [+ a b]]")),
    TestCase("fn:invoke", "[[let add [fn [a b][+ a b]]][add 2 3]]", exact("5")),
    TestCase("fn:identity", "[[let id [fn [x] x]][id 9]]", exact("9")),

    # --- lists / stores ---
    TestCase("list:nth", "[[nth [10 20 30] 1]]", exact("20")),
    TestCase("length:list", "[[length [1 2 3]]]", exact("3")),
    TestCase("length:store", "[[length [[~a 1] [~b 2]]]]", exact("2")),
    TestCase("push:list", "[[>> 3 [1 2]]]", exact("[1 2 3]")),
    TestCase("pop:list", "[[<< [1 2 3]]]", exact("3")),
    TestCase("anonymous-store-top-level", "[[~a 1] [~b 2]]", one_of([
        "[[~a 1] [~b 2]]",
        "[[~b 2] [~a 1]]",
    ])),

    # --- import / plugin regression ---
    TestCase("import:json-then-plus", "[[import 'json'][+ 2 3]]", exact("5"), repeat=25),
    TestCase("import:json-twice-then-plus", "[[import 'json'][import 'json'][+ 2 3]]", exact("5"), repeat=10),

    # --- json semantic tests ---
    TestCase(
        "json:parse-dump-roundtrip",
        "[[import 'json'][json:dump [json:parse '{\"a\":[1,2,3],\"b\":{\"x\":true,\"y\":null}}']]]",
        json_string_eq({"a": [1, 2, 3], "b": {"x": 1, "y": None}})
    ),
    TestCase(
        "json:dump-inline-store",
        "[[import 'json'][json:dump [[~a [1 2 3]] [~b [[~x 1] [~y nil]]]]]]",
        json_string_eq({"a": [1, 2, 3], "b": {"x": 1, "y": None}})
    ),
    TestCase(
        "json:dump-simple-store",
        "[[import 'json'][json:dump [[~a 1] [~b 2]]]]",
        json_string_eq({"a": 1, "b": 2})
    ),

    # --- json file tests ---
    TestCase(
        "json:write-load-roundtrip",
        lambda td: f"[[import 'json'][json:write [[~a 1] [~b [1 2 3]]] '{td / 'rt.json'}'][json:dump [json:load '{td / 'rt.json'}']]]",
        json_string_eq({"a": 1, "b": [1, 2, 3]})
    ),

    # --- regression cases from bugs you already hit ---
    TestCase(
        "regression:inline-anonymous-store-arg",
        "[[import 'json'][json:dump [[~a [1 2 3]] [~b [[~x 1] [~y nil]]]]]]",
        json_string_eq({"a": [1, 2, 3], "b": {"x": 1, "y": None}}),
        repeat=10
    ),
    TestCase(
        "regression:top-level-anonymous-store",
        "[[import 'json'][json:dump [[~a 1] [~b 2]]]]",
        json_string_eq({"a": 1, "b": 2}),
        repeat=10
    ),
    TestCase(
        "regression:import-does-not-kill-builtins",
        "[[import 'json'][+ 1 2]]",
        exact("3"),
        repeat=25
    ),

    # --- expected failures / diagnostics ---
    TestCase(
        "error:json-dump-missing-arg",
        "[[import 'json'][json:dump]]",
        contains("invalid number of arguments", exit_code=1)
    ),
    TestCase(
        "error:bad-json",
        "[[import 'json'][json:parse '{bad json}']]",
        contains("Bad JSON Input", exit_code=1)
    ),
    TestCase(
        "error:missing-library",
        "import 'this_library_should_not_exist_12345'",
        contains("No canvas library", exit_code=1)
    ),
    TestCase(
        "error:mismatching-brackets",
        "[[+ 1 2]",
        contains("Syntax Error", exit_code=1)
    ),
]


def run_test(case: TestCase, tempdir: Path):
    command = case.command(tempdir) if callable(case.command) else case.command
    try:
        proc = run_canvas(command, timeout=case.timeout)
        return case.validator(proc.returncode, proc.stdout, proc.stderr), command
    except subprocess.TimeoutExpired:
        return (False, f"command timed out after {case.timeout}s"), command
    except Exception as exc:
        return (False, f"runner exception: {exc}"), command


def main():
    print("ABOUT TO START TEST CASES FOR CANVAS")
    total = 0
    failed = 0

    with tempfile.TemporaryDirectory(prefix="canvas-tests-") as td:
        tempdir = Path(td)

        for case in cases:
            for attempt in range(case.repeat):
                total += 1
                (ok, detail), command = run_test(case, tempdir)
                if not ok:
                    failed += 1

                repeat_suffix = f" [{attempt + 1}/{case.repeat}]" if case.repeat > 1 else ""
                print(
                    f"[{total}] {case.name}{repeat_suffix} | "
                    f"CMD: {command!r} | "
                    f"{'SUCCESS' if ok else 'FAILURE'}"
                )
                if not ok:
                    print(f"    {detail}")

    succeeded = total - failed
    print(f"\nDONE. SUCCEEDED {succeeded} | FAILED {failed} | TOTAL {total}")

    if failed == 0:
        print("\n<---------------------ALL TESTS PASSED--------------------->")
        raise SystemExit(0)
    else:
        raise SystemExit(1)


if __name__ == "__main__":
    main()