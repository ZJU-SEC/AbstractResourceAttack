# Younaman's chages
在这个环境中，我主要修改了/home/younaman/Desktop/kernel-llvm/llvm-project/llvm/lib/Transforms/Hello下的Hello.cpp
在编译时需要先在.../llvm/build下执行
cmake -G "Unix Makefiles" -DLLVM_ENABLE_PROJECTS="clang;libcxx;libcxxabi;lld;compiler-rt;clang-tools-extra;libunwind;lldb;polly" -DCMAKE_BUILD_TYPE=Release ../llvm
再执行make -j 8
之后在/home/younaman/Desktop/testllvm/下，对预先生成的5.3.1的vmlinux.bc文件进行load:
opt -load /home/younaman/Desktop/kernel-llvm/llvm-project/build/lib/LLVMHello.so -hello < vmlinux.bc
这样就能生成我们要的cmp-finder.txt。之后再对cmp-finder.txt进行python脚本的处理

2020-11-21.完成了对atomic/spin_lock两种锁机制的所有处理，bug已修复，代码见Hello.cpp。之后会继续向Hello.cpp中添加其余锁机制。
