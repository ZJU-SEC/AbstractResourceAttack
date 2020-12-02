# yangnanzi-llvm-pass
This repository is used to analysis the shared resources of different containers
检查锁保护的资源
---
`锁检查` 用来分析kernel中锁保护的对象，以及可能造成DoS攻击的触发函数。 

# 文件内容说明：
在压缩包中存在三个文件/文件夹，Resourceanalysis，testllvm文件夹和test.sh文件。
Resourceanalysis为我的Pass文件夹。

testllvm为我本地运行pass的文件夹，包含一份生成好的vmlinux.bc，一份生成好的vmlinux.ll，生成的初步结果cmp-finder.txt，处理结果用的脚本cmp-finder.py和真正用来找新攻击的out_cmp_fider.txt结果文件。

test.sh为生成vmlinux.bc时需要用到的脚本文件。


# 预备环境
- wllvm (用于生成bc)
- LLVM 12.0(核心内容，需要预先装好)
- Linux 5.3.1(编译内核用)
- cmake 3.18-1(方便我们工具使用)

# 使用流程
## 预先安装好wllvm,llvm 12.0和cmake 3.18-1
```shell
1.安装cmake 3.18-1
cd cmake
./bootstrap
make
sudo make install
2.安装llvm12.0
cd llvm
mkdir build
cd build
cmake -G "Unix Makefiles" -DLLVM_ENABLE_PROJECTS="clang;libcxx;libcxxabi;lld;compiler-rt;clang-tools-extra;libunwind;lldb;polly" -DCMAKE_BUILD_TYPE=Release ../llvm
make -j4
sudo make install
3.安装wllvm
sudo apt-get install python
sudo apt-get install python3-pip
pip3 install wllvm
pip3 uninstall wllvm #安装路径有问题时
git clone https://github.com/travitch/whole-program-llvm
cd whole-program-llvm
sudo pip3 install -e .
```

## 把内核编译成IR(`vmlinux.bc`)

对于5.3.1的内核，需要手工修改代码，这里存在两个坑:
１.修改/linux-5.3.1/lib/string.c:

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
2.修改/linux-5.3.1/arch/x86/boot/compressed/kaslr_64.c:
```shell
从第33行开始，添加以下内容:

//younaman change the 'unsigned long' to 'extern unsigned long'
extern unsigned long __force_order;
将原来的unsigned long改为extern unsigned long
```
3.关掉wllvm的优化，禁止内联：

具体参考压缩包中的test.sh。以我本地为例，将test.sh放入内核源码文件夹下，我本地为./myownkernel/linux-5.3.1/test.sh，使用该脚本进行vmlinux.ll的生成:

```shell
root@younaman-ThinkPad:/home/younaman/Desktop/myownkernel/linux-5.3.1# ./test.sh vmlinux

root@younaman-ThinkPad:/home/younaman/Desktop/myownkernel/linux-5.3.1# ls
arch     CREDITS        fs       Kbuild   LICENSES     modules.builtin          net      security    tools    vmlinux.ll
block    crypto         include  Kconfig  MAINTAINERS  modules.builtin.modinfo  README   sound       usr      vmlinux.llvm.manifest
certs    Documentation  init     kernel   Makefile     modules.order            samples  System.map  virt     vmlinux.opt.bc
COPYING  drivers        ipc      lib      mm           Module.symvers           scripts  test.sh     vmlinux  vmlinux.tmp.manifest
```
该文件夹下的vmlinux.opt.bc就是生成的bc文件，重命名为vmlinux.bc即可。

## 对于生成的`vmlinux.bc`运行编写的Pass：
首先，将Resourceanalysis放入你自己的/llvm-project/llvm/lib/Transforms目录下，并修改Transforms下的CMakeLists.txt，最终结果如下。
```shell
root@younaman-ThinkPad:/home/younaman/Desktop/kernel-llvm/llvm-project/llvm/lib/Transforms# cat CMakeLists.txt | grep Locktest
add_subdirectory(Resourceanalysis)
root@younaman-ThinkPad:/home/younaman/Desktop/kernel-llvm/llvm-project/llvm/lib/Transforms/Resourceanalysis# ls
CMakeLists.txt  Resourceanalysis.cpp  Resourceanalysis.exports
```
之后，在/llvm-project/build目录下运行命令来构建工具：
``` shell
root@younaman-ThinkPad:/home/younaman/Desktop/kernel-llvm/llvm-project/build# cmake -G "Unix Makefiles" -DLLVM_ENABLE_PROJECTS="clang;libcxx;libcxxabi;lld;compiler-rt;clang-tools-extra;libunwind;lldb;polly" -DCMAKE_BUILD_TYPE=Release ../llvm
root@younaman-ThinkPad:/home/younaman/Desktop/kernel-llvm/llvm-project/build# make -j 8
```

之后，在你放置vmlinux.bc的目录下(我这里叫testllvm) 运行Pass:
``` shell
root@younaman-ThinkPad:/home/younaman/Desktop/testllvm# opt -load /home/younaman/Desktop/kernel-llvm/llvm-project/build/lib/LLVMResourceanalysis.so -resourceanalysis < vmlinux.bc
```
这一步会生成一个叫cmp-finder.txt的文件，为了方便阅读，调用该目录下的cmp-finder.py对cmp-finder.txt进行处理：
``` shell
root@younaman-ThinkPad:/home/younaman/Desktop/testllvm# python3.7 cmp-finder.py
root@younaman-ThinkPad:/home/younaman/Desktop/testllvm# ls
cmp-finder.py      out_cpm_finder.txt       vmlinux.bc
cmp-finder.txt vmlinux.ll
```
生成的out_cpm_finder.txt就是我们想要的结果文件。
# Files
- Resourceanalysis.cpp：该文件就是所有的代码文件，里面有非常详细的注释。
- /Resourceanalysis/CMakeLists.txt：该文件为Locktest对应的CMakeLists文件。
# 结果示例：
