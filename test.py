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
    # Test let & mut
    TestCase("[let n 5][+ n n]", "10", True),
    TestCase("[let n 5][mut n [+ n 16]][n]", "21", True),
    
    # Test Arithmetic & sub chained expressions
    TestCase("[+ 2 2]", "4", True),
    TestCase("[+ 2 [+ 2 2]]", "6", True),
    TestCase("[+ 2 [+ 2 [+ 2 2]]]", "8", True),
    TestCase("[+ 2 [+ 2 [+ 2 2]]]", "8", True),

    # Test embdded binary functions & namespaces
    TestCase("[io:out 'This is a TEST']", "This is a TESTnil", True),
    TestCase("[math:cos math:PI]", "-1", True),
    TestCase("[namespace Lib lb [var:5]][lb:var]", "5", True),
    TestCase("[namespace Lib lb [var:[fn [][25]]]][lb:var]", "25", True),
    
    # Test Store
    TestCase("[let t [store [test1:[store a:[store z:1200]]][test2:[store b:3]]]][+ t:test1:a:z t:test2:b]", "1203", True),

    # Test lists
    TestCase("[1 2 3 4]", "[1 2 3 4]", True),
    TestCase("[1 2 3 4]", "[1 2 3 4]", True),
    TestCase(">> [1 2 3 4] 2", "[2 [1 2 3 4]]", True),
    TestCase("<< [1 2 3 4] 2", "[1 2 3 4 2]", True),
    TestCase("<< 1 2 3", "[1 2 3]", True),

    # Test dynamic library
    TestCase("[import 'lib/bm.cv'][let img [bm:create 3 25 25]]", "[height:25 channels:3 width:25 pixels:[0 0 0 0 0 0 0 0 0 0 ...(1865 hidden)]]", True),
    TestCase("[import 'lib/bm.cv'][let img [bm:create 3 25 25]][img:pixels]", "[0 0 0 0 0 0 0 0 0 0 ...(1865 hidden)]", True),

    # Test loops & interrupts
    TestCase("[for i from 1 to 10 [io:out i]]", "12345678910nil", True),
    TestCase("[import 'lib/core.cv'][let n 5][do [gt n 0][-- n]]", "0", True),
    TestCase("[let list [1 2 3 4 5]][iter i in list [io:out [<< i i]]]", "[1 1][2 2][3 3][4 4][5 5]nil", True),
    TestCase("[import 'lib/core.cv'][let n 5][do [gt n 0][[-- n][if [eq n 2][skip][io:out n]]]]", "4310nil", True)
]

print("<--------------------ABOUT TO START TEST CASES FOR CANVAS-------------------->")
n = 0
failed = 0
for case in cases:
    n += 1
    result = run_test(case)
    if not result[0]:
        failed += 1
    got = (f" | GOT: '{result[1]}'" if not result[0] else "")
    print(f"[{n}/{len(cases)}]\t[{'SUCCESS' if result[0] else 'FAILURE'}]\tEXPECTED: '{case.command}' {'==' if case.expEqual else '!='} '{case.expected}'"+got)

print(f"\nDONE. SUCEEDED {len(cases)-failed} | FAILED {failed}")

if failed == 0:
    print("\n\n\n<---------------------------ALL TEST PASSED YAY!!!--------------------------->")