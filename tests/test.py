import subprocess

CV_BINARY_PATH = "cv.exe";

class TestCase:
    name = None
    command = None
    expected = None
    expEqual = True
    def __init__(self, command, expected, expEqual, name = ""):
        self.name = name
        self.command = command
        self.expected = expected
        self.expEqual = expEqual

def run_test(test):
    p = subprocess.run([CV_BINARY_PATH, test.command], capture_output=True, text=True)
    return (( p.stdout == test.expected if test.expEqual else p.stdout != test.expected ), p.stdout)

cases = [
    TestCase("+ 1 1", "2", True),
    TestCase("** 2", "2", False),
    TestCase("[1 2 3]|reverse", "[3 2 1]", True),
    TestCase("'string test'", "'string test'", True),
    TestCase("'string test'|reverse", "'tset gnirts'", True),
    TestCase("gt 10 5", "1", True),
    TestCase("gte 8 8 5", "1", True),
    TestCase("eq 5.2 1.2", "0", True),
    TestCase("l-range 1 10", "[1 2 3 4 5 6 7 8 9 10]", True),
    TestCase("[l-range 1 10]|reverse", "[10 9 8 7 6 5 4 3 2 1]", True),
    TestCase("l-sort|copy", "[fn [c l] [BINARY]]", True)

]

print("ABOUT TO START TEST CASES FOR CANVAS")
n = 0
failed = 0
for case in cases:
    n += 1
    result = run_test(case)
    if not result[0]:
        failed += 1
    print(f"[{n}/{len(cases)}] EXPECTED: '{case.command}' {'==' if case.expEqual else '!='} '{case.expected}' | GOT: '{result[1]}' [{'SUCCESS' if result[0] else 'FAILURE'}]")

print(f"DONE. SUCEEDED {len(cases)-failed} | FAILED {failed}")