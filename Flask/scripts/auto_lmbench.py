#!/usr/bin/python3 -u

import threading
import subprocess
import os
import shutil
import signal
import time
import atexit


def run_flask(defence_num: int) -> subprocess.Popen:
    # flask = subprocess.Popen(
    #     ["sudo", "./flask.py", "-i", "2d918a288b8c", "-d", str(defence_num)],
    #     cwd="../ResConfine",
    #     stdout=subprocess.PIPE,
    #     stderr=subprocess.PIPE,
    #     text=True,
    # )

    flask = subprocess.Popen(
        ["sudo", "./FlaskCPP", "-i", "2d918a288b8c", "-d", str(defence_num)],
        # ["sudo", "./FlaskCPP", "-d", str(defence_num)],
        cwd="../ResConfineCPP/build",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )

    atexit.register(flask.kill)

    print("flask process created...")
    if flask.stdout is None:
        print(
            "Error, flask stdout is None, possibly you not run with sudo? Or other erros..."
        )
        exit(-1)
        

    for line in flask.stdout:
        # print("line: %s" % line)
        if "All submonitors init done, start monitoring" in line:
            break
        
    if flask.poll() is not None:
        print(
            "Error, flask setup fails"
        )
        exit(-1)
    print("flask setup done...")
    return flask

def kill_flask(flask: subprocess.Popen):
    flask.kill()
    kill_time = 5
    time.sleep(1)
    while flask.poll() is None:
        time.sleep(1)
        kill_time -= 1
        if kill_time == 0:
            print("Error, flask still alive")
            exit(-1)


# sudo ./flask.py -i 988e1be76718 -d 0
def bench_flask(defence_num: int, times: int):
    print("=== autobench (%d, %d) start ===" % (defence_num, times))

    flask = run_flask(defence_num)
    
    time.sleep(10)
    for i in range(times):
        print("start bench %d..." % i)
        bench = subprocess.run(
            ["make", "rerun"],
            cwd="../host_lmbench/lmbench-3.0-a9",
            capture_output=True,
            text=True,
        )

        if bench.returncode != 0:
            print("Error, bench fails")
            print(bench.stderr)
            exit(-1)
    

    print("bench done, kill flask...")
    kill_flask(flask)

    print("processing results")
    process = subprocess.run(
        ["make", "see"],
        cwd="../host_lmbench/lmbench-3.0-a9",
        capture_output=True,
        text=True
    )

    if process.returncode != 0:
        print("Error, make see results fails")
        print(process.stderr)
        exit(-1)

    shutil.move(
        "../host_lmbench/lmbench-3.0-a9/results/summary.out",
        "../host_lmbench/benchmarks/flask%d.txt" % defence_num,
    )
    shutil.rmtree("../host_lmbench/lmbench-3.0-a9/results/x86_64-linux-gnu")
    os.mkdir("../host_lmbench/lmbench-3.0-a9/results/x86_64-linux-gnu")

    print("=== autobench (%d, %d) end   ===" % (defence_num, times))


def baseline(defence_num: int, times: int):
    print("=== autobench baseline (%d) start ===" % times)

    for i in range(times):
        print("start bench %d..." % i)
        bench = subprocess.run(
            ["make", "rerun"],
            cwd="../host_lmbench/lmbench-3.0-a9",
            capture_output=True,
            text=True,
        )

        if bench.returncode != 0:
            print("Error, bench fails")
            print(bench.stderr)
            exit(-1)
    

    print("processing results")
    process = subprocess.run(
        ["make", "see"],
        cwd="../host_lmbench/lmbench-3.0-a9",
        capture_output=True,
        text=True
    )

    if process.returncode != 0:
        print("Error, make see results fails")
        print(process.stderr)
        exit(-1)

    shutil.move(
        "../host_lmbench/lmbench-3.0-a9/results/summary.out",
        "../host_lmbench/benchmarks/baseline%d.txt" % defence_num
    )
    shutil.rmtree("../host_lmbench/lmbench-3.0-a9/results/x86_64-linux-gnu")
    os.mkdir("../host_lmbench/lmbench-3.0-a9/results/x86_64-linux-gnu")

    print("=== autobench baseline (%d) end   ===" % times)


bench_num = 3

if __name__ == "__main__":
    baseline(0, bench_num)
    
    # time.sleep(30)
    # bench_flask(0, bench_num)

    time.sleep(30)
    bench_flask(10, bench_num)
    
    time.sleep(30)
    bench_flask(20, bench_num)
    
    time.sleep(30)
    bench_flask(30, bench_num)
    
    time.sleep(30)
    bench_flask(40, bench_num)
