#!/usr/bin/python3 -u

import subprocess
import shutil
import time
import atexit


def run_flask(defence_num: int) -> subprocess.Popen:
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
        print("Error, flask setup fails")
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


def bench_flask(defence_num: int):
    print("=== autobench (%d) start ===" % (defence_num))

    flask = run_flask(defence_num)

    time.sleep(10)
    print("start bench...")
    bench = subprocess.run(
        [
            "UB_OUTPUT_FILE_NAME=bench UB_OUTPUT_CSV=true ./Run -c 8 -i 6 dhry2reg whetstone-double syscall pipe context1 spawn execl fstime-r fstime-w shell8"
        ],
        cwd="../host_unixbench/byte-unixbench/UnixBench/",
        shell=True,
    )

    if bench.returncode != 0:
        print("Error, bench fails")
        print(bench.stderr)
        exit(-1)

    print("bench done, kill flask...")
    kill_flask(flask)

    print("processing results...")
    shutil.copy(
        "../host_unixbench/byte-unixbench/UnixBench/results/bench.csv",
        "../host_unixbench/benchmarks/flask%d.csv" % defence_num,
    )
    shutil.copy(
        "../host_unixbench/byte-unixbench/UnixBench/results/bench",
        "../host_unixbench/benchmarks/flask%d" % defence_num,
    )
    shutil.rmtree("../host_unixbench/byte-unixbench/UnixBench/results")

    print("=== autobench (%d) end   ===" % (defence_num))


def baseline():
    print("=== autobench baseline start ===")

    print("start bench...")
    bench = subprocess.run(
        [
            "UB_OUTPUT_FILE_NAME=bench UB_OUTPUT_CSV=true ./Run -c 8 -i 6 dhry2reg whetstone-double syscall pipe context1 spawn execl fstime-r fstime-w shell8"
        ],
        cwd="../host_unixbench/byte-unixbench/UnixBench/",
        shell=True,
    )

    if bench.returncode != 0:
        print("Error, bench fails")
        print(bench.stderr)
        exit(-1)

    print("bench done...")

    print("processing results...")
    shutil.copy(
        "../host_unixbench/byte-unixbench/UnixBench/results/bench.csv",
        "../host_unixbench/benchmarks/baseline.csv",
    )
    shutil.copy(
        "../host_unixbench/byte-unixbench/UnixBench/results/bench",
        "../host_unixbench/benchmarks/baseline",
    )
    shutil.rmtree("../host_unixbench/byte-unixbench/UnixBench/results")

    print("=== autobench baseline end   ===")


if __name__ == "__main__":
    baseline()

    time.sleep(30)
    bench_flask(10)

    time.sleep(30)
    bench_flask(20)

    time.sleep(30)
    bench_flask(30)

    time.sleep(30)
    bench_flask(40)
