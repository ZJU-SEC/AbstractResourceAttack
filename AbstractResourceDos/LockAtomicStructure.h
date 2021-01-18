#include "Utils.h"
#include "FindLockAndAtomic.h"
#include <utility>
#include <set>
#include <vector>

class LockAtomicStructure {
    public:
      LockAtomicStructure(llvm::Module &); 
      void CollectLockAndAtomic();
      bool LockAtomicStructureMain(llvm::Function* F);
    
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
        int getBBID(llvm::BasicBlock* BB);
        char* GetActualFName(std::string &functionname);
        std::string GetActualStructType(llvm::Instruction *gepInst,std::string funName);   
        std::string ReturnTypeRefine(llvm::Type &rt);
        void TravseAllocUser(llvm::Function* func,llvm::Instruction* originv);
        void  id_phi_inst(llvm::Function* funcname,llvm::Instruction* I);
};