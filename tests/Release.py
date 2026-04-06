#!/usr/bin/env python3

import argparse
import json
import os
import random
import re
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Optional


ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")


def strip_ansi(s: str) -> str:
    return ANSI_RE.sub("", s or "")


def norm(s: str) -> str:
    return strip_ansi((s or "").replace("\r\n", "\n")).strip()


def unwrap_canvas_string(s: str) -> str:
    s = norm(s)
    if len(s) >= 2 and s[0] == "'" and s[-1] == "'":
        return s[1:-1]
    return s


@dataclass
class RunResult:
    ok: bool
    exit_code: Optional[int]
    stdout: str
    stderr: str
    duration: float
    reason: str = ""


@dataclass
class Case:
    name: str
    mode: str  # inline | file | project
    payload: object
    validator: Callable[[RunResult], tuple[bool, str]]
    tags: set[str] = field(default_factory=set)
    repeat: int = 1
    timeout: float = 8.0


def exact(expected_stdout: str, exit_code: int = 0):
    def _check(r: RunResult):
        if not r.ok:
            return False, r.reason
        ok = r.exit_code == exit_code and r.stdout == expected_stdout
        detail = (
            f"expected stdout={expected_stdout!r}, exit={exit_code}; "
            f"got stdout={r.stdout!r}, stderr={r.stderr!r}, exit={r.exit_code}"
        )
        return ok, detail
    return _check


def contains(needle: str, exit_code: Optional[int] = None):
    def _check(r: RunResult):
        if not r.ok:
            return False, r.reason
        blob = (r.stdout + "\n" + r.stderr).strip()
        ok = needle in blob and (exit_code is None or r.exit_code == exit_code)
        detail = (
            f"expected combined output to contain {needle!r}; "
            f"got stdout={r.stdout!r}, stderr={r.stderr!r}, exit={r.exit_code}"
        )
        return ok, detail
    return _check


def one_of(options: list[str], exit_code: int = 0):
    def _check(r: RunResult):
        if not r.ok:
            return False, r.reason
        ok = r.exit_code == exit_code and r.stdout in options
        detail = (
            f"expected one of {options!r}; "
            f"got stdout={r.stdout!r}, stderr={r.stderr!r}, exit={r.exit_code}"
        )
        return ok, detail
    return _check


def json_eq(expected_obj, exit_code: int = 0):
    def _check(r: RunResult):
        if not r.ok:
            return False, r.reason
        try:
            parsed = json.loads(unwrap_canvas_string(r.stdout))
        except Exception as exc:
            return False, (
                f"failed to parse stdout as JSON string: {r.stdout!r}, "
                f"stderr={r.stderr!r}, exit={r.exit_code}, exc={exc}"
            )
        ok = r.exit_code == exit_code and parsed == expected_obj
        detail = (
            f"expected JSON={expected_obj!r}; got JSON={parsed!r}, "
            f"raw stdout={r.stdout!r}, stderr={r.stderr!r}, exit={r.exit_code}"
        )
        return ok, detail
    return _check


def run_subprocess(cmd, timeout: float, cwd: Optional[str] = None, env: Optional[dict] = None) -> RunResult:
    started = time.time()
    try:
        proc = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            cwd=cwd,
            env=env,
        )
        return RunResult(
            ok=True,
            exit_code=proc.returncode,
            stdout=norm(proc.stdout),
            stderr=norm(proc.stderr),
            duration=time.time() - started,
        )
    except subprocess.TimeoutExpired:
        return RunResult(
            ok=False,
            exit_code=None,
            stdout="",
            stderr="",
            duration=time.time() - started,
            reason=f"timeout after {timeout}s",
        )
    except Exception as exc:
        return RunResult(
            ok=False,
            exit_code=None,
            stdout="",
            stderr="",
            duration=time.time() - started,
            reason=f"runner exception: {exc}",
        )


class Runner:
    def __init__(self, binary: str, file_flag: str):
        self.binary = binary
        self.file_flag = file_flag

    def run_inline(self, command: str, timeout: float, cwd: Optional[str] = None, env: Optional[dict] = None) -> RunResult:
        return run_subprocess([self.binary, command], timeout, cwd=cwd, env=env)

    def run_file(self, source: str, timeout: float, cwd: Optional[str] = None, env: Optional[dict] = None) -> RunResult:
        with tempfile.TemporaryDirectory(prefix="canvas-file-") as td:
            workdir = cwd or td
            p = Path(td) / "test.cv"
            p.write_text(source, encoding="utf-8")
            if self.file_flag:
                cmd = [self.binary, self.file_flag, str(p)]
            else:
                cmd = [self.binary, str(p)]
            return run_subprocess(cmd, timeout, cwd=workdir, env=env)

    def run_project(self, payload: dict, timeout: float) -> RunResult:
        """
        payload = {
            "entry_name": "main.cv",
            "files": {"main.cv": "...", "math.cv": "..."},
            "cwd_subdir": "",   # optional
            "env": {...}        # optional
        }
        """
        with tempfile.TemporaryDirectory(prefix="canvas-proj-") as td:
            root = Path(td)
            files = payload["files"]
            for rel, content in files.items():
                p = root / rel
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_text(content, encoding="utf-8")

            env = os.environ.copy()
            env.update(payload.get("env", {}))
            # Make imports work from lib fallback if desired
            env.setdefault("CANVAS_LIB_HOME", str(root / "lib"))

            entry = root / payload["entry_name"]
            cwd = str(root / payload.get("cwd_subdir", "")) if payload.get("cwd_subdir") else str(root)

            if self.file_flag:
                cmd = [self.binary, self.file_flag, str(entry)]
            else:
                cmd = [self.binary, str(entry)]
            return run_subprocess(cmd, timeout, cwd=cwd, env=env)

    def run_case(self, case: Case) -> RunResult:
        if case.mode == "inline":
            return self.run_inline(case.payload, case.timeout)
        if case.mode == "file":
            return self.run_file(case.payload, case.timeout)
        if case.mode == "project":
            return self.run_project(case.payload, case.timeout)
        return RunResult(False, None, "", "", 0.0, f"unknown mode {case.mode}")


def build_cases(binary_path: str) -> list[Case]:
    cases: list[Case] = []

    # Basics
    cases += [
        Case("literal:number", "inline", "1", exact("1"), {"core"}),
        Case("literal:string", "inline", "'hello'", exact("'hello'"), {"core"}),
        Case("literal:list", "inline", "[1 2 3]", exact("[1 2 3]"), {"core"}),
        Case("literal:store", "inline", "[[~a 1] [~b 2]]",
             one_of(["[[~a 1] [~b 2]]", "[[~b 2] [~a 1]]"]), {"core"}),
        Case("let:basic", "inline", "[let a 5] [a]", exact("5"), {"core"}),
        Case("mut:number", "inline", "[let a 5] [mut a 7] [a]", exact("7"), {"core"}),
        Case("cc:copy-number", "inline", "[let a 5] [let b [cc a]] [mut a 9] [b]", exact("5"), {"core"}),
    ]

    # Arithmetic / boolean / conditionals
    cases += [
        Case("arith:add", "inline", "+ 2 3", exact("5"), {"core", "arith"}),
        Case("arith:sub", "inline", "- 10 3 2", exact("5"), {"core", "arith"}),
        Case("arith:mul", "inline", "* 2 3 4", exact("24"), {"core", "arith"}),
        Case("arith:div", "inline", "/ 20 2 2", exact("5"), {"core", "arith"}),
        Case("bool:and-true", "inline", "and 1 2 3", exact("1"), {"core", "bool"}),
        Case("bool:and-false", "inline", "and 1 0 3", exact("0"), {"core", "bool"}),
        Case("bool:not", "inline", "not 0", exact("1"), {"core", "bool"}),
        Case("cmp:eq", "inline", "eq 3 3", exact("1"), {"core", "cmp"}),
        Case("cmp:lt", "inline", "< 2 7", exact("1"), {"core", "cmp"}),
        Case("if:true", "inline", "[if 1 'yes' 'no']", exact("'yes'"), {"core", "flow"}),
        Case("if:false", "inline", "[if 0 'yes' 'no']", exact("'no'"), {"core", "flow"}),
    ]

    # Lists / stores
    cases += [
        Case("length:list", "inline", "length [1 2 3 4]", exact("4"), {"core", "list"}),
        Case("length:store", "inline", "length [[~a 1] [~b 2]]", exact("2"), {"core", "store"}),
        Case("nth", "inline", "nth [1 2 3] 1", exact("2"), {"core", "list"}),
        Case("splice:list", "inline", "l-splice 1 2 3", exact("[1 2 3]"), {"core", "list"}),
        Case("splice:store", "inline", "s-splice [~a 1] [~b 2]",
             one_of(["[[~a 1] [~b 2]]", "[[~b 2] [~a 1]]"]), {"core", "store"}),
        Case("list:push", "inline", ">> 4 [1 2 3]", exact("[1 2 3 4]"), {"core", "list"}),
        Case("list:pop", "inline", "<< [1 2 3]", exact("3"), {"core", "list"}),
        Case("list:sub", "inline", "l-sub [1 2 3 4 5] 1 3", exact("[2 3 4]"), {"core", "list"}),
        Case("store:access", "inline", "[[let user [b:store [~name 'Italo'] [~role 'builder']]] [user ~name]]",
             exact("'Italo'"), {"core", "store"}),
        Case("store:multi-access", "inline",
             "[[let user [b:store [~name 'Italo'] [~role 'builder'] [~years 10]]] [user ~name ~years]]",
             one_of(["['Italo' 10]", "[10 'Italo']"]), {"core", "store"}),
    ]

    # Proxies / named args / functions
    cases += [
        Case("fn:basic", "inline", "[[let add [fn [a b] [+ a b]]] [add 2 3]]", exact("5"), {"core", "fn"}),
        Case("fn:named-args", "inline", "[[let pair [fn [a b] [b:list a b]]] [pair [~b 9] [~a 3]]]",
             exact("[3 9]"), {"core", "fn"}),
        Case("fn:variadic", "inline", "[[let id [fn [@] @]] [id 1 2 3]]", exact("[1 2 3]"), {"core", "fn"}),
        Case("typeof:number", "inline", "typeof 5", exact("'NUMBER'"), {"core", "util"}),
        Case("typeof:store", "inline", "typeof [[~a 1] [~b 2]]", exact("'STORE'"), {"core", "util"}),
    ]

    # Mutators
    cases += [
        Case("mutator:inc", "inline", "[let a 5] [++ a] [a]", exact("6"), {"core", "mut"}),
        Case("mutator:dec", "inline", "[let a 5] [-- a] [a]", exact("4"), {"core", "mut"}),
        Case("mutator:half", "inline", "[let a 8] [// a] [a]", exact("4"), {"core", "mut"}),
        Case("mutator:square", "inline", "[let a 7] [** a] [a]", exact("49"), {"core", "mut"}),
    ]

    # while / for / foreach
    cases += [
        Case("while:count", "inline",
             "[[let n 0] [while [< n 5] [++ n]] [n]]",
             exact("5"), {"core", "loop"}),
        Case("for:sum", "inline",
             "[[let sum 0] [for [~x [0 5]] [mut sum [+ sum x]]] [sum]]",
             exact("10"), {"core", "loop"}),
        Case("for:step", "inline",
             "[[let sum 0] [for [~x [0 10 2]] [mut sum [+ sum x]]] [sum]]",
             exact("20"), {"core", "loop"}),
        Case("foreach:list", "inline",
             "[[let sum 0] [foreach [~x [1 2 3 4]] [mut sum [+ sum x]]] [sum]]",
             exact("10"), {"core", "loop"}),
        Case("foreach:store-values", "inline",
             "[[let total 0] [let s [b:store [~a 1] [~b 2] [~c 3]]] [foreach [~v s] [mut total [+ total v]]] [total]]",
             exact("6"), {"core", "loop"}),
    ]

    # print smoke
    cases += [
        Case("print:basic", "inline", "print 'hello'", contains("hello"), {"core", "util"}),
    ]

    # File-mode tests
    cases += [
        Case(
            "file:basic-program",
            "file",
            "[let a 5]\n[let b 7]\n[print [+ a b]]\n",
            contains("12"), {"file"}
        ),
        Case(
            "file:loops",
            "file",
            "[let sum 0]\n[for [~x [0 5]] [mut sum [+ sum x]]]\n[print sum]\n",
            contains("10"), {"file", "loop"}
        ),
        Case(
            "file:functions",
            "file",
            "[let add [fn [a b] [+ a b]]]\n[print [add 9 4]]\n",
            contains("13"), {"file", "fn"}
        ),
    ]

    # Project import tests
    cases += [
        Case(
            "project:import-local-cv",
            "project",
            {
                "entry_name": "main.cv",
                "files": {
                    "main.cv": "[import 'math']\n[print [double 6]]\n",
                    "math.cv": "[let double [fn [x] [+ x x]]]\n"
                },
            },
            contains("12"), {"project", "import"}
        ),
        Case(
            "project:import-chain",
            "project",
            {
                "entry_name": "main.cv",
                "files": {
                    "main.cv": "[import 'a']\n[print [triple 4]]\n",
                    "a.cv": "[import 'b']\n[let triple [fn [x] [+ [double x] x]]]\n",
                    "b.cv": "[let double [fn [x] [+ x x]]]\n",
                },
            },
            contains("12"), {"project", "import"}
        ),
    ]

    # Error cases
    cases += [
        Case("error:undefined-name", "inline", "does_not_exist", contains("Undefined imperative", exit_code=1), {"error"}),
        Case("error:bad-nth", "inline", "nth [1 2 3] 9", contains("Invalid Index", exit_code=1), {"error"}),
        Case("error:bad-let-name", "inline", "[let 123 5]", contains("Misused Constructor", exit_code=1), {"error"}),
        Case("error:bad-for", "inline", "[for [0 5] [print 1]]", contains("Illegal Iterator", exit_code=1), {"error"}),
        Case("error:bad-foreach", "inline", "[foreach [~x 5] [print x]]", contains("Illegal Iterator", exit_code=1), {"error"}),
        Case("error:div-zero", "inline", "/ 10 0", contains("Division by zero", exit_code=1), {"error"}),
        Case("error:mismatching-brackets", "inline", "[[let a 1]", contains("Syntax Error", exit_code=1), {"error"}),
    ]

    # Soak-ish repeats
    cases += [
        Case("soak:inline-add", "inline", "+ 2 3", exact("5"), {"soak"}, repeat=200),
        Case("soak:inline-fn", "inline", "[[let add [fn [a b] [+ a b]]] [add 2 3]]", exact("5"), {"soak"}, repeat=200),
        Case("soak:for-sum", "inline", "[[let sum 0] [for [~x [0 20]] [mut sum [+ sum x]]] [sum]]", exact("190"), {"soak", "loop"}, repeat=100),
        Case("soak:file", "file", "[let a 5]\n[let b 7]\n[print [+ a b]]\n", contains("12"), {"soak", "file"}, repeat=50),
    ]

    # Optional dynamic json tests
    maybe_json = detect_json_library(binary_path)
    if maybe_json:
        cases += [
            Case("dynlib:json-dump", "inline",
                 "[[import 'json'][json:dump [[~a [1 2 3]] [~b [[~x 1] [~y nil]]]]]]",
                 json_eq({"a": [1, 2, 3], "b": {"x": 1, "y": None}}),
                 {"dynlib", "json"}),
            Case("dynlib:json-parse", "inline",
                 "[[import 'json'][json:parse '{\"a\":1,\"b\":[2,3],\"ok\":true}']]",
                 one_of([
                     "[[~a 1] [~b [2 3]] [~ok 1]]",
                     "[[~a 1] [~ok 1] [~b [2 3]]]",
                     "[[~b [2 3]] [~a 1] [~ok 1]]",
                     "[[~b [2 3]] [~ok 1] [~a 1]]",
                     "[[~ok 1] [~a 1] [~b [2 3]]]",
                     "[[~ok 1] [~b [2 3]] [~a 1]]",
                 ]),
                 {"dynlib", "json"}),
        ]

    return cases


def detect_json_library(binary_path: str) -> bool:
    root = Path(binary_path).resolve().parent
    candidates = [
        root / "lib" / "libjson.so",
        root / "lib" / "json.so",
        root / "libjson.so",
        root / "json.so",
        root / "lib" / "json.dll",
        root / "lib" / "libjson.dll",
        root / "json.dll",
        root / "libjson.dll",
        root / "lib" / "libjson.dylib",
        root / "lib" / "json.dylib",
    ]
    return any(p.exists() for p in candidates)


def main():
    ap = argparse.ArgumentParser(description="Canvas direct-interpreter test suite")
    ap.add_argument("--bin", default="./cv", help="Path to Canvas binary")
    ap.add_argument("--file-flag", default="-f", help="Flag used to run a .cv file, e.g. -f or --file or empty")
    ap.add_argument("--keep-going", action="store_true", help="Keep running after failures")
    ap.add_argument("--shuffle", action="store_true", help="Shuffle case order")
    ap.add_argument("--repeats", type=int, default=1, help="Multiply repeat counts")
    ap.add_argument("--tags", nargs="*", default=[], help="Run only cases matching any of these tags")
    ap.add_argument("--only", nargs="*", default=[], help="Run only cases whose names contain one of these fragments")
    args = ap.parse_args()

    file_flag = args.file_flag
    if file_flag.lower() in {"none", "empty", "null"}:
        file_flag = ""

    from pathlib import Path

    binary_path = str(Path(args.bin).resolve())
    runner = Runner(binary_path, file_flag)
    cases = build_cases(binary_path)

    if args.tags:
        wanted = set(args.tags)
        cases = [c for c in cases if c.tags & wanted]

    if args.only:
        frags = args.only
        cases = [c for c in cases if any(f in c.name for f in frags)]

    if args.shuffle:
        random.shuffle(cases)

    if not cases:
        print("No cases selected.")
        return 1

    print("STARTING CANVAS TEST SUITE")
    total = 0
    failed = 0
    started = time.time()

    for case in cases:
        repeats = max(1, case.repeat * max(1, args.repeats))
        for i in range(1, repeats + 1):
            total += 1
            result = runner.run_case(case)
            ok, detail = case.validator(result)

            if not ok:
                failed += 1
                suffix = f" [{i}/{repeats}]" if repeats > 1 else ""
                print(f"[{total}] {case.name}{suffix} | FAILURE")
                print(f"    mode: {case.mode}")
                print(f"    tags: {sorted(case.tags)}")
                print(f"    {detail}")
                if result.reason:
                    print(f"    runner: {result.reason}")
                if not args.keep_going:
                    elapsed = time.time() - started
                    print(f"\nABORTED. SUCCEEDED {total - failed} | FAILED {failed} | TOTAL {total} | TIME {elapsed:.2f}s")
                    return 1

    elapsed = time.time() - started
    print(f"\nDONE. SUCCEEDED {total - failed} | FAILED {failed} | TOTAL {total} | TIME {elapsed:.2f}s")
    if failed == 0:
        print("\n<---------------------ALL TESTS PASSED--------------------->")
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())