from glob import glob
import sys
import os
import subprocess



class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def run_command(cmd):
    result = subprocess.run(cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return result.returncode

def run_test(path):
    test_name, ext = os.path.splitext(os.path.basename(path))
    print(test_name.ljust(50-4, '.'), end = '')

    #build with gcc
    if run_command('gcc ' + path + ' -o ~/temp/test.app') != 0:
        print(f'{bcolors.FAIL}GCC FAIL')
        return False

    #run gcc version
    gcc_result = run_command('~/temp/test.app')
       
    #compile
    if run_command('../build/compiler/jcc ' + path + ' > temp.s') != 0:
        print(f'{bcolors.FAIL}JCC FAIL')
        return False
    #link
    if run_command('gcc -m32 temp.s -o ~/temp/jcc_' + test_name) != 0:
        print(f'{bcolors.FAIL}LINK FAIL')
#        run_command('rm temp.s')
        return False;
        
#    run_command('rm temp.s')
    #run our version
    jcc_result = run_command('~/temp/jcc_' + test_name);

    if gcc_result == jcc_result:
        print('PASS')
    else:
        print(f'{bcolors.FAIL}FAIL')
   
    return gcc_result == jcc_result

def run_tests(path):
    global total_failures
    dir_name = path.rstrip('/')
    print('=' * 50)
    print(dir_name.center(50))
    print('=' * 50)
    
    tests = glob(path + '*.c')
    failures = 0
    successes = 0
    for test in tests:
        if (run_test(test)):
            successes = successes + 1
        else:
            failures = failures + 1
        print(bcolors.ENDC, end = '')
            
    print('=' * 50)
    if successes > 0:
        print(f'{successes} PASS')
    if failures > 0:
        print(f'{bcolors.FAIL}{failures} FAIL{bcolors.ENDC}')
        total_failures +=  failures

total_failures = 0

if len(sys.argv) == 2:
    selection = sys.argv[1]
else:
    selection = '*/'
paths = glob(selection)
for path in paths:
    run_tests(path)

if total_failures > 0:
    print(f'{bcolors.FAIL}{total_failures} TOTAL FAILURES{bcolors.ENDC}')
    
