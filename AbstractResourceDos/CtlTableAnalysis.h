#include "Utils.h"
#include "FindLockAndAtomic.h"
#include "LockPair.h"

class CtlTableAnalysis {
    public:
        CtlTableAnalysis(llvm::Module &);
        void CollectCtlGlobal();
        void AllAtomic();
        void AllPerCPU();
        void TravseGlobal(llvm::Value *,llvm::Value * ,std::string,std::vector<llvm::Instruction*> &);
        void IcmpNr(llvm::Instruction *,llvm::Value * ,std::string);
        void CtlTravse(llvm::Instruction *,llvm::Function *,std::vector<std::string> *,std::string);
        void Test();

        std::set<std::pair<std::string,std::string>> *CtlPair;
        std::set<std::string> AtomicFunc;
        std::set<std::string> PerCPUFunc;
        std::set<std::string> AllocFunctionNames;
        std::map<llvm::Function*,std::vector<std::string> > CtlFuncResource;


    private:
        llvm::Module* _module_;
        std::set<llvm::Value*> CtlGlobal;
        void StoreFuncResource(llvm::Function*,llvm::GlobalValue*);
        void StoreResourceVector(llvm::Function*,std::vector<std::string>);
        void FindGlobalIcmp(llvm::Value::use_iterator,llvm::Value *);  
};