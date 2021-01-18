#ifndef _LOCKPAIR_H_
#define _LOCKPAIR_H_

#include "Utils.h"
#include "FindLockAndAtomic.h"
#include <utility>
#include <set>
#include <vector>

class LockPair {
    public:
      LockPair(llvm::Module &); 
      void CollectLockAndAtomic();
      bool LockPairMain(llvm::Function* );
      bool LockAtomicResource(llvm::Function* );
      void MarkRlimitControlFunc(llvm::Function*);
      void Test();
      std::map<llvm::Function*,std::vector<std::string> > FuncResource; //这里存放的就是我们之前使用LockPair/LockAtomicStructure找出的结果，key是function,value是敏感函数对应的多个资源
      std::set<llvm::Function*> RlimitControlFunc;
    
    private:
        llvm::Module* _module_;
        std::set<std::string> spin_lock;
        std::set<std::string> spin_unlock;
        std::set<std::string> atomic_function;
        std::set<std::string> percpu_function;
        std::set<std::string> write_lock;
        std::set<std::string> write_unlock;
        std::set<std::string> mutex_lock;
        std::set<std::string> mutex_unlock;
        std::set<std::string> AllocFunctionNames;
        std::set<std::string> rlimit_func;
        int getBBID(llvm::BasicBlock* BB);
        char* GetActualFName(std::string &functionname);
        std::string GetActualStructType(llvm::Instruction *gepInst,std::string funName);   
        void id_phi_inst(llvm::Function* funcname,llvm::Instruction* I,std::vector<std::string>*);
        void TravseIcmpUser(llvm::Instruction*,llvm::Function* ,int* );
        void TravseAllocUser(llvm::Function* func,llvm::Instruction* originv,std::vector<std::string>*);
        void FindStoreAndCall(llvm::Instruction *Inst,llvm::Function* funcName,std::vector<std::string>*);
        void printLockPairSet(std::vector<std::pair<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>,std::vector<llvm::BasicBlock*>>> ,llvm::Function*,std::vector<std::string>*);
        bool findway(std::vector<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>> ,std::vector<std::pair<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>,std::vector<llvm::BasicBlock*>>>  *, llvm::Function* );
        void TravseBB(llvm::BasicBlock* ,std::vector<llvm::BasicBlock::iterator> ,std::vector<llvm::BasicBlock*> ,std::vector<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>>  *,llvm::Function *); 
};


#endif