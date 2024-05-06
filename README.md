# yangnanzi-resource-analysis
Check resources protected by locks
---
`Lock Check` is used to analyze the objects protected by locks in the kernel and the triggering functions that may cause DoS attacks.

# File content description：
There are three files/folders in the compressed package, Resourceanalysis, testllvm folder and test.sh file.
Resourceanalysis is my Pass folder.

testllvm is the folder where I run the pass locally, including a generated vmlinux.bc, a generated vmlinux.ll, the generated preliminary result cmp-finder.txt, and the script cmp-finder.py used to process the results. The out_cmp_fider.txt result file is actually used to find new attacks.

test.sh is the script file needed to generate vmlinux.bc.


# Prepare environment
- wllvm (used to generate bc)
- LLVM 12.0(Core content needs to be pre-installed)
- Linux 5.3.1(Used to compile the kernel)
- cmake 3.18-1(Make it easier to use our tools)

# Usage process
## Pre-install wlvm, llvm 12.0 and cmake 3.18-1
```shell
1.Install cmake 3.18-1
cd cmake
./bootstrap
make
sudo make install
2. Install llvm12.0
cd llvm
mkdir build
cd build
cmake -G "Unix Makefiles" -DLLVM_ENABLE_PROJECTS="clang;libcxx;libcxxabi;lld;compiler-rt;clang-tools-extra;libunwind;lldb;polly" -DCMAKE_BUILD_TYPE=Release ../llvm
make -j4
sudo make install
3.Install wllvm
sudo apt-get install python
sudo apt-get install python3-pip
pip3 install wllvm
pip3 uninstall wllvm #When there is a problem with the installation path
git clone https://github.com/travitch/whole-program-llvm
cd whole-program-llvm
sudo pip3 install -e .
```

## Compile the kernel into IR(`vmlinux.bc`)

For the 5.3.1 kernel, the code needs to be modified manually. There are two errors here:
１.Modify /linux-5.3.1/lib/string.c:

```shell
//younaman change this file manually,add stpcpy declare and fuction,try to export it.
//I reference the https://git.kernel.org/pub/scm/linux/kernel/git/next/linux-next.git/commit/?id=5934637641c863cc2c1765a0d01c5b6f53ecc4fc
char *stpcpy(char *__restrict__ dest, const char *__restrict__ src);
char *stpcpy(char *__restrict__ dest, const char *__restrict__ src)
{
        while ((*dest++ = *src++) != '\0')
                /* nothing */;
        return --dest;
}
EXPORT_SYMBOL(stpcpy);
```
2.Modify /linux-5.3.1/arch/x86/boot/compressed/kaslr_64.c:
```shell
Starting at line 33, add the following:

//younaman change the 'unsigned long' to 'extern unsigned long'
extern unsigned long __force_order;
Change the original unsigned long to extern unsigned long
```
3.Turn off wllvm's optimization and disable inlining:

For details, refer to test.sh in the compressed package. Taking my local as an example, put test.sh into the kernel source folder. My local is ./myownkernel/linux-5.3.1/test.sh. Use this script to generate vmlinux.ll:

```shell
root@younaman-ThinkPad:/home/younaman/Desktop/myownkernel/linux-5.3.1# ./test.sh vmlinux

root@younaman-ThinkPad:/home/younaman/Desktop/myownkernel/linux-5.3.1# ls
arch     CREDITS        fs       Kbuild   LICENSES     modules.builtin          net      security    tools    vmlinux.ll
block    crypto         include  Kconfig  MAINTAINERS  modules.builtin.modinfo  README   sound       usr      vmlinux.llvm.manifest
certs    Documentation  init     kernel   Makefile     modules.order            samples  System.map  virt     vmlinux.opt.bc
COPYING  drivers        ipc      lib      mm           Module.symvers           scripts  test.sh     vmlinux  vmlinux.tmp.manifest
```
The vmlinux.opt.bc in this folder is the generated bc file, just rename it to vmlinux.bc.

## Run the written Pass for the generated `vmlinux.bc`:
First, put Resourceanalysis into your own /llvm-project/llvm/lib/Transforms directory, and modify CMakeLists.txt under Transforms. The final result is as follows.
```shell
root@younaman-ThinkPad:/home/younaman/Desktop/kernel-llvm/llvm-project/llvm/lib/Transforms# cat CMakeLists.txt | grep Locktest
add_subdirectory(Resourceanalysis)
root@younaman-ThinkPad:/home/younaman/Desktop/kernel-llvm/llvm-project/llvm/lib/Transforms/Resourceanalysis# ls
CMakeLists.txt  Resourceanalysis.cpp  Resourceanalysis.exports
```
Afterwards, run the command in the /llvm-project/build directory to build the tool:
``` shell
root@younaman-ThinkPad:/home/younaman/Desktop/kernel-llvm/llvm-project/build# cmake -G "Unix Makefiles" -DLLVM_ENABLE_PROJECTS="clang;libcxx;libcxxabi;lld;compiler-rt;clang-tools-extra;libunwind;lldb;polly" -DCMAKE_BUILD_TYPE=Release ../llvm
root@younaman-ThinkPad:/home/younaman/Desktop/kernel-llvm/llvm-project/build# make -j 8
```

After that, run Pass in the directory where you placed vmlinux.bc (called testllvm here):
``` shell
root@younaman-ThinkPad:/home/younaman/Desktop/testllvm# opt -load /home/younaman/Desktop/kernel-llvm/llvm-project/build/lib/LLVMResourceanalysis.so -resourceanalysis < vmlinux.bc
```
This step will generate a file called cmp-finder.txt. To facilitate reading, call cmp-finder.py in this directory to process cmp-finder.txt:
``` shell
root@younaman-ThinkPad:/home/younaman/Desktop/testllvm# python3.7 cmp-finder.py
root@younaman-ThinkPad:/home/younaman/Desktop/testllvm# ls
cmp-finder.py      out_cpm_finder.txt       vmlinux.bc
cmp-finder.txt vmlinux.ll
```
The generated out_cpm_finder.txt is the result file we want.
# Files
- Resourceanalysis.cpp：This file is all the code files with very detailed comments.
- /Resourceanalysis/CMakeLists.txt：This file is the CMakeLists file corresponding to Locktest.
# Example of results:
