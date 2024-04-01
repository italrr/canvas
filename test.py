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
    # Test function definition
    TestCase("fn [a b c][+ a b c]", "[fn [a b c] [[+ a b c]]]", True),
    
    # Test Context definion and access
    TestCase("[set a [fn [a b c][+ a b c]]][a 1 2 3]", "[[fn [a b c] [[+ a b c]]] 6]", True),
    TestCase("[set test [ct 25~n]][test: set n 10][test]", "[[ct 10~n <TOP>~top] 10 [ct 10~n <TOP>~top]]", True),
    
    # Test `type` trait
    TestCase("5|type", "'NUMBER'", True),
    TestCase("'test'|type", "'STRING'", True),
    TestCase("[1 2 3]|type", "'LIST'", True),
    TestCase("[ct 10]|type", "'CONTEXT'", True),

    # Arithmetic
    TestCase("+ 1 1", "2", True),
    TestCase("- 5 2", "3", True),
    TestCase("* 0.5 100", "50", True),
    TestCase("/ 3.5 3.5", "1", True),
    TestCase("** 2", "2", False),

    # l-range and ANY_NUMBER trait
    TestCase("l-range 1 10", "[1 2 3 4 5 6 7 8 9 10]", True),    
    TestCase("[l-range 1 100]|50", "51", True),

    # Expand
    TestCase("+ [l-range 1 100]^", "5050", True),

    # Complex list definition
    TestCase("[1 2 [3 4 [5 6 [7 8 [9 10 [11 12]]]]]]", "[1 2 [3 4 [5 6 [7 8 [9 10 [11 12]]]]]]", True),

    # Complex list defition and trait stress test
    TestCase("[set k [+|copy 2 'Tres' [ct 10]]][k|0 1 2][k|1][k|2|2][k|3:v-0]", "[[[fn [...] [BINARY]] 2 'Tres' [ct 10~v-0 <TOP>~top]] 3 2 'e' 10]", True),

    # l-flat
    TestCase("l-flat [1 2 [3 4 [5 6 [7 8 [9 10 [11 12]]]]]]", "[1 2 3 4 5 6 7 8 9 10 11 12]", True),

    # Test List `reverse` trait
    TestCase("[1 2 3]|reverse", "[3 2 1]", True),
    TestCase("[l-range 1 10]|reverse", "[10 9 8 7 6 5 4 3 2 1]", True),


    # String definition
    TestCase("'string test'", "'string test'", True),

    # Test String `reverse` trait
    TestCase("'string test'|reverse", "'tset gnirts'", True),

    # Logic operators
    TestCase("gt 10 5", "1", True),
    TestCase("gte 8 8 5", "1", True),
    TestCase("eq 5.2 1.2", "0", True),

    # Copy trait
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