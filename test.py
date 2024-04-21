import subprocess

CV_BINARY_PATH = "./cv";

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
    # Test function definition
    TestCase("fn [a b c][+ a b c]", "[fn [a b c] [BYTECODE]]", True),
    
    # Test Context definion and access
    TestCase("[set a [fn [a b c][+ a b c]]][a 1 2 3]", "6", True),
    
    # Test `type` trait
    TestCase("5|type", "'NUMBER'", True),
    TestCase("'test'|type", "'STRING'", True),
    TestCase("[1 2 3]|type", "'LIST'", True),
    TestCase("[ct 10]|type", "'CONTEXT'", True),

    # Test loops
    TestCase("[set k 50][do [gt [-- k] 0] [io:out k]]", "49484746454443424140393837363534333231302928272625242322212019181716151413121110987654321nil", True),

    # Arithmetic
    TestCase("+ 1 1", "2", True),
    TestCase("- 5 2", "3", True),
    TestCase("* 0.5 100", "50", True),
    TestCase("/ 3.5 3.5", "1", True),
    TestCase("** 2", "2", False),
    TestCase("/ 1 0", "nil'/': Division by zero.", False),

    # l-gen and ANY_NUMBER trait
    TestCase("l-gen 1 10", "[1 2 3 4 5 6 7 8 9 10]", True),    
    TestCase("[l-gen 1 100]|50", "51", True),

    # Expand
    TestCase("+ [l-gen 1 100]^", "5050", True),

    # Complex list definition
    TestCase("[1 2 [3 4 [5 6 [7 8 [9 10 [11 12]]]]]]", "[1 2 [3 4 [5 6 [7 8 [9 10 [11 12]]]]]]", True),

    # Complex list defition and trait stress test
    TestCase("[set k 10][+ k k][set w [ct 1500~n]][w: [** n]][set m [k w]][+ m|0|inv w:n]", "2249990", True),

    # l-flat
    TestCase("l-flat [1 2 [3 4 [5 6 [7 8 [9 10 [11 12]]]]]]", "[1 2 3 4 5 6 7 8 9 10 11 12]", True),

    # Test List `reverse` trait
    TestCase("[1 2 3]|reverse", "[3 2 1]", True),
    TestCase("[l-gen 1 10]|reverse", "[10 9 8 7 6 5 4 3 2 1]", True),


    # String definition
    TestCase("'string test'", "'string test'", True),

    # Test String `reverse` trait
    TestCase("'string test'|reverse", "'tset gnirts'", True),

    # Logic operators
    TestCase("gt 10 5", "1", True),
    TestCase("gte 8 8 5", "1", True),
    TestCase("eq 5.2 1.2", "0", True),

    # Copy trait
    TestCase("proxy l-sort", "[fn [c l] [BINARY]]", True),

    # with / untether
    TestCase("[with [untether [+ [l-gen 1 1000]^]] [+ 1 inherited]]|await", "500501", True), 
    
    # with / async
    TestCase("[with [with [async [+ 0 1]] [+ inherited 1]] [+ inherited 1]]|await", "3", True)    
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