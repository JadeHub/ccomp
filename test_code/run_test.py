from glob import glob
import sys
import os
import subprocess

def run_command(cmd):
    result = subprocess.run(cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return result.returncode

def run_test(path):
    test_name, ext = os.path.splitext(os.path.basename(path))
    print(test_name.ljust(50-4, '.'), end = '')
    
    if run_command('gcc ' + path + ' -o ~/temp/test.app') != 0:
        print('GCC FAIL')
        return False
#    cmd_result = subprocess.run('gcc ' + path + ' -o ~/temp/test.app', shell=True)
    #if cmd_result.returncode != 0:
        #return False
    gcc_result = run_command('~/temp/test.app')
       
    #compile
    if run_command('~/grayj/code/compiler/build/compiler/jcc ' + path + ' > temp.s') != 0:
        print('JCC FAIL')
        return False
    #link
    if run_command('gcc -m32 temp.s -o ~/temp/jcc_' + test_name) != 0:
        print('LINK FAIL')
        return False;
        
    jcc_result = run_command('~/temp/jcc_' + test_name);

    if gcc_result == jcc_result:
        print('PASS')
    else:
        print('FAIL')
   
    return gcc_result == jcc_result

def run_tests(path):
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
            
    print('=' * 50)
    print(f'{successes} PASS')
    print(f'{failures} FAIL')


if len(sys.argv) == 2:
    selection = sys.argv[1]
else:
    selection = '*/'
paths = glob(selection)
for path in paths:
    run_tests(path)
    