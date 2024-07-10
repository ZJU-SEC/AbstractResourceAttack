import subprocess
import os


def compile_c_program(source_path: str, target_path: str) -> bool:
    compile_process = subprocess.run(
        ["gcc", "-Wall", "-static", "-o", target_path, source_path], capture_output=True
    )
    if compile_process.returncode != 0:
        return False
    return True


def run_executable(executable_path: str, arg: str) -> int:
    run_process = subprocess.run([executable_path, arg], capture_output=True, text=True)
    if run_process.returncode == 0:
        return int(run_process.stdout)
    else:
        return -2


# Extract cg_name's cg id, negative means fails, 
# while -1 means compile fails, and -2 means extract fails
def extract_cgid(cg_path: str) -> int:
    current_file_path = os.path.realpath(__file__)
    current_directory = os.path.dirname(current_file_path)

    executable = current_directory + "/extract"
    source = current_directory + "/extract_cgid.c"

    if not os.path.exists(executable):
        if not compile_c_program(source, executable):
            return -1

    # 运行可执行文件
    return run_executable(executable, cg_path)
