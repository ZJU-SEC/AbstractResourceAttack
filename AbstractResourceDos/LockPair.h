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
      bool IsInitFunction(llvm::Function* F);
      bool LockPairMain(llvm::Function* );
      bool FindChangArg(llvm::Function * funcname, int offset);//这个用来便利funcA->callfuncB中的funcB函数
      bool LockAtomicResource(llvm::Function* );
      void MarkRlimitControlFunc(llvm::Function*);
      void MarkNameSpaceControlFunc(llvm::Function*);
      void FindStoreAndCall(llvm::Instruction *Inst,llvm::Function* funcName,std::vector<std::string>*, std::set<llvm::Instruction *>);
      void Test();
      bool isTypeMatch(const llvm::Instruction *Inst, const llvm::Value *source);
      void TravseUserStruct(llvm::Value *,llvm::Value * ,llvm::Function* funcName,std::vector<llvm::Instruction*> &);
      std::map<llvm::Function*,std::vector<std::string> > FuncResource; //这里存放的就是我们之前使用LockPair/LockAtomicStructure找出的结果，key是function,value是敏感函数对应的多个资源
      std::set<llvm::Function*> RlimitControlFunc;
      std::set<llvm::Function*> NameSpaceControlFunc;
    
    private:
        llvm::Module* _module_;
        std::set<std::string> spin_lock;
        std::set<std::string> spin_unlock;
        std::set<std::string> atomic_function;
        std::set<std::string> percpu_function;
        std::set<std::string> atomic_function_not_change;
        std::set<std::string> percpu_function_not_change;
        std::set<std::string> write_lock;
        std::set<std::string> write_unlock;
        std::set<std::string> mutex_lock;
        std::set<std::string> mutex_unlock;
        std::set<std::string> AllocFunctionNames;
        std::set<std::string> rlimit_func;
        std::set<std::string> NsStruct;
        int getBBID(llvm::BasicBlock* BB);
        char* GetActualFName(std::string &functionname);
        llvm::Type* GetActualStructType(llvm::Instruction *gepInst,std::string funName,llvm::Type* );   
        void id_phi_inst(llvm::Function* funcname,llvm::Instruction* I,std::vector<std::string>*);
        void TravseIcmpNUser(llvm::Instruction*,llvm::Function* ,int* );
        void TravseIcmpUser(llvm::Instruction*,llvm::Function* ,int* );
        void TravseAllocUser(llvm::Function* func,llvm::Instruction* originv,std::vector<std::string>*);
        void printLockPairSet(std::vector<std::pair<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>,std::vector<llvm::BasicBlock*>>> ,llvm::Function*,std::vector<std::string>*);
        bool findway(std::vector<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>> ,std::vector<std::pair<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>,std::vector<llvm::BasicBlock*>>>  *, llvm::Function* );
        void TravseBB(llvm::BasicBlock* ,std::vector<llvm::BasicBlock::iterator> ,std::vector<llvm::BasicBlock*> ,std::vector<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>>  *,llvm::Function *); 
        bool StructHasNamespace(llvm::Type* ,std::string FuncName);
        bool CheckGlobalVariable(llvm::GlobalValue *G);
        bool ConfirmCount(llvm::Instruction *, std::set<llvm::Instruction *>);
        bool ConfirmCountotherlayer(llvm::Instruction *);
        std::set<std::string> TravseNamespace(llvm::Type* );
};


#endif