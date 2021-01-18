#include "Utils.h"
#include "LockPair.h"
#include "CtlTableAnalysis.h"

class ResourceCollectPass : public llvm::ModulePass
{
    public:
        static char ID;
        ResourceCollectPass() : ModulePass(ID) {}
        virtual bool runOnModule(llvm::Module &) override;
        bool SysCallPath(llvm::Function* ,std::set<llvm::Function*>);
        void DeBug();
        //void PassMain(LockPair LP);
    
    private:
        void traceTaskEntry(std::vector<llvm::CallInst*> &, std::set<std::vector<llvm::CallInst*> > &, const std::map<llvm::Function*, llvm::CallInst *> &);
};