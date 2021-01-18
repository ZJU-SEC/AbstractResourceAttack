#include "Utils.h"
#include "FindLockAndAtomic.h"

class CtlTableAnalysis {
    public:
        CtlTableAnalysis(llvm::Module &);
        void CollectCtlGlobal();
        void TravseGlobal(llvm::Value *,llvm::Value * ,std::string);
        void IcmpNr(llvm::Instruction *,llvm::Value * ,std::string);
        void Test();

        std::set<std::pair<std::string,std::string>> *CtlPair;
        std::set<std::string> AtomicFunc;
        std::set<std::string> PerCPUFunc;
        std::map<llvm::Function*,std::vector<std::string> > CtlFuncResource;


    private:
        llvm::Module* _module_;
        std::set<llvm::Value*> CtlGlobal;
        void StoreFuncResource(llvm::Function*,llvm::GlobalValue*);
        
};