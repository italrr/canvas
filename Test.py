import subprocess

CV_BINARY_PATH = "./cv"

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
    return (( p.stdout.replace('\n', '') == test.expected if test.expEqual else p.stdout != test.expected ), p.stdout.replace('\n', ''))

cases = [
    TestCase("1", "1", True),
    TestCase("[1 2 3]", "[1 2 3]", True),
    TestCase("'string'", "'string'", True),
    TestCase("[[typeof [[~n 10][~k 21]]]]", "'STORE'", True),
    TestCase("[[let test [fn [a b][+ a b]]]]", "[fn [a b] [+ a b]]", True),
    TestCase("+ 2 2", "4", True),
    TestCase("** 4", "16", True),
    TestCase("[[// [** 4]]]", "8", True),
    TestCase("+ [+ 1 2] 3", "6", True),
    TestCase("[[>> 3 [>> [await |[await |2]] 1]]]", "[1 2 3]", True),
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

if failed == 0:
    print("\n\n\n<---------------------ALL TEST PASSED YAY!!!--------------------->")