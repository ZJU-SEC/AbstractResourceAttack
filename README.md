# Resource Analysis
We use `Lock Check` to analyze the objects protected by locks in the kernel and the triggering functions that may cause DoS attacks.

File contents:
- AbstractResourceDos: LLVM Pass folder.
- cmp-finder.py: Result process scripts.
- test.sh: the script file needed to generate vmlinux.bc file.

## Environment Requirement
- LLVM 12.0: Core analysis support.
- wllvm: To generate bc files for analysis.
- cmake 3.18.1: Build tools.
- Linux 5.3.1: Analysis target kernel.


To install:
```shell
# 1. Install cmake 3.18.1
$ cd cmake-3.18.1
$ ./bootstrap
$ make -j$(nproc)
$ sudo make install

# 2. Install LLVM 12.0
$ cd llvm-project-llvmorg-12.0.0
$ mkdir build
$ cd build
$ cmake -G "Unix Makefiles" -DLLVM_ENABLE_PROJECTS="clang;libcxx;libcxxabi;lld;compiler-rt;clang-tools-extra;libunwind;lldb;polly" -DCMAKE_BUILD_TYPE=Release ../llvm
$ make -j$(nproc)
$ sudo make install

# 3. Install wllvm
$ sudo apt install python3-pip
$ pip3 install wllvm
```

**Note:** You may install some tools from package manager and other versions for convenience, but we won't that guarantee they can work correctly.


## Compile the Kernel into IR
In our evaluation, we use the 5.3.1 kernel. To compile, you need to modify the code manually. There are 3 errors here:
1. linux-5.3.1/lib/string.c
    ```c
    /*
    * Modify: add `stpcpy` declare and function here, and export it.
    * Ref: https://git.kernel.org/pub/scm/linux/kernel/git/next/linux-next.git/commit/?id=5934637641c863cc2c1765a0d01c5b6f53ecc4fc
    */
    char *stpcpy(char *__restrict__ dest, const char *__restrict__ src);
    char *stpcpy(char *__restrict__ dest, const char *__restrict__ src)
    {
            while ((*dest++ = *src++) != '\0')
                    /* nothing */;
            return --dest;
    }
    EXPORT_SYMBOL(stpcpy);
    ```

2. linux-5.3.1/arch/x86/boot/compressed/kaslr_64.c:
    ```c
    /*
    * Modify: At line 33, change the 'unsigned long' to 'extern unsigned long'.
    */
    extern unsigned long __force_order;
    ```

3. tools/objtool/elf.c:
    ```c
    static int read_symbols(struct elf *elf)
    {
        // ...
        /*
        * Modify: Change the return value from '-1' to '0'.
        * Ref: https://github.com/torvalds/linux/commit/1d489151e9f9d1647110277ff77282fe4d96d09b.patch
        */
        symtab = find_section_by_name(elf, ".symtab");
        if (!symtab) {
                return 0;
        }
        // ...
    }
    ```

Then you can config the kernel as you need, and compile it. In here, we compile it in default config:
```bash
$ export LLVM_COMPILER=clang
$ make CC=wllvm HOSTCC=wllvm defconfig
$ make CC=wllvm HOSTCC=wllvm -j$(nproc)
```

Now we can generate the IR file from `vmlinux`. We provide a script `test.sh` to help you generate the `vmlinux.opt.bc` file. Please first place the script in the kernel source directory, and then run it:
```bash
$ cp test.sh linux-5.3.1
$ cd linux-5.3.1
$ ./test.sh vmlinux
# You will get the vmlinux.opt.bc file.
```

## Run the Analysis
First we have to compile the LLVM Pass.
Please put the `AbstractResourceDos` folder into the `llvm-project/llvm/lib/Transforms` directory, and modify the `CMakeLists.txt` file under `Transforms` directory:
```bash
~/llvm-project-llvmorg-12.0.0/llvm/lib/Transforms$ cat CMakeLists.txt | grep Abstract
add_subdirectory(AbstractResourceDos)

~/llvm-project-llvmorg-12.0.0/llvm/lib/Transforms$ ls
AbstractResourceDos  CMakeLists.txt  ...
```
And then, build the tool by running the following commands in the `llvm-project/build` directory:
```bash
~/llvm-project-llvmorg-12.0.0/build$ cmake -G "Unix Makefiles" -DLLVM_ENABLE_PROJECTS="clang;libcxx;libcxxabi;lld;compiler-rt;clang-tools-extra;libunwind;lldb;polly" -DCMAKE_BUILD_TYPE=Release ../llvm

~/llvm-project-llvmorg-12.0.0/build$ make -j$(nproc)
```
Now you can run Pass in the directory where you place your `vmlinux.opt.bc`:
```bash
$ opt -load /path/to/llvm-project/build/lib/libresource_collect.so -resourcecollect -gatlin < vmlinux.opt.bc
```

This step will generate a file called `cmp-finder.txt`. To facilitate reading, use `cmp-finder.py` in this repo to process the `cmp-finder.txt`:
``` bash
# The cmp-finder.txt file should be in the same directory.
~/AbstractResourceAttack$ python3.7 cmp-finder.py
~/AbstractResourceAttack$ ls
cmp-finder.py       out_cpm_finder.txt
cmp-finder.txt      location-finder.txt
```

The generated `out_cpm_finder.txt` is the result file we want.