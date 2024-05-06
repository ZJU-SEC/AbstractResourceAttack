#include "ResourceCollectPass.h"
#include "llvm/IR/InstIterator.h"
#include "gatlin/include/gatlin_idcs.h"
#include "PexCallGraph.h"
#include "llvm/Support/raw_ostream.h"

extern PexCallGraph gCallGraph;
char ResourceCollectPass::ID = 0;
std::error_code ec;
llvm::raw_fd_ostream resultstream("./cmp-finder.txt", ec, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);
llvm::raw_fd_ostream pathstream("./path-finder.txt", ec, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);
/*
	runOnModule is the main function of pass.
*/
bool ResourceCollectPass::runOnModule(llvm::Module &M) {
	LockPair LP = LockPair(M);                       //Instantiate a LockPair object. We also add the contents of the structure where the lock is located to the LockPair.
	LP.CollectLockAndAtomic();	 				
	auto &functionList = M.getFunctionList();
	bool CanTriger = false;
    for (auto &function : functionList) {                      //Traverse all functions of module
		if (LP.IsInitFunction(&function))
			continue;
		std::string FuncName = function.getName().str();

			LP.LockPairMain(&function);
	    		//The core of this function is to collect the structure where the lock/atomic global parameters & are located. Here, I need to modify it to, for global variables, only collect atomic and not lock.
			LP.LockAtomicResource(&function);
			LP.MarkRlimitControlFunc(&function);	
    }
	LP.Test();

	for (auto &mapit : LP.FuncResource){	//Here we form a map that stores all found sensitive functions without call processing.
		if(SysCallPath(mapit.first,LP.RlimitControlFunc)){
			auto Resource = mapit.second;
			for(auto it = Resource.begin();it != Resource.end();it++){
				resultstream<<"FunctionName:"<<mapit.first->getName().str()<<","<<*it<<"\n";
			}
		}//This corresponds to if(SysCallPath)
	}
	//LP.Test();
//If you need to run ctl_table related content separately, you need to comment out the contents of lines 17-66.
	CtlTableAnalysis CT = CtlTableAnalysis(M);  //Lines 43 and 44 are used to run the newly added CtlTableAnalysis.
	CT.Test();

	std::cout<<"After ResourceCollect "<<std::endl;
	return false;
}

/*
	traceTaskEntry:Obtain the set of all paths from syscall to the target function. The first parameter is passed into the callsite of the target function (the caller of the target function). 
 		       result_path is the obtained result, which contains multiple paths from the target function to the entry.
*/
void ResourceCollectPass::traceTaskEntry(int indirectcount, std::vector<llvm::CallInst*> &ci_stack, std::set<std::vector<llvm::CallInst*> > &result_path, const std::map<llvm::Function*, llvm::CallInst *> &free_sites) {
    auto top_ci = ci_stack.back();
    auto top_fn = top_ci->getFunction();

    if (ci_stack.size() == 13 || free_sites.find(top_fn) != free_sites.end() || result_path.size() >= 100) 	//Here, the recursion depth is set to 7 to limit the scale.
        return;
	if (top_fn == nullptr) {
		llvm::errs() << "Cannot find function holding the CallInst: " << *top_ci << "\n";
		return;
	}
	PexCallGraph::CGNode *n = gCallGraph.getNode(top_fn);
	for (auto link : n->callers) {
		int LayerIndirectCount = indirectcount;
		llvm::CallInst *nextci = link.second->call;
		if (std::find(ci_stack.begin(), ci_stack.end(), nextci) != ci_stack.end()) {
			continue;
		}
		if(nextci->isIndirectCall()){
			LayerIndirectCount++;
			if(LayerIndirectCount >= 2){
				continue;
			}
		}
		ci_stack.push_back(nextci);
		if (isSyscall(nextci->getFunction())) {
	    	result_path.insert(ci_stack);
		}

		else{
	    	traceTaskEntry(LayerIndirectCount,ci_stack, result_path, free_sites);
		}
	  	ci_stack.pop_back();
	}
}

bool ResourceCollectPass::SysCallPath(llvm::Function* F,std::set<llvm::Function*> RlimitControlFunc){
	bool CanTriger = false;
    std::vector<llvm::CallInst*> temp_ci;
    std::set<std::vector<llvm::CallInst*> > result_paths;
	std::set<std::vector<llvm::CallInst*> > resultmark_paths;
	auto gcallsites = gCallGraph.findCallSites(F);
	for(auto *callsite : gcallsites){
		temp_ci.clear();
		result_paths.clear();
		temp_ci.push_back(callsite);
		std::map<llvm::Function*, llvm::CallInst *> emptymap = {};
		int indirectcount=0;
        	traceTaskEntry(indirectcount,temp_ci, result_paths, emptymap);
		if(!result_paths.empty()){
			CanTriger = true;
			for (auto &path : result_paths) {                             //Print all paths from target function to syscall.
            	for (auto &hop : path) {
					if(RlimitControlFunc.find(hop->getFunction()) != RlimitControlFunc.end()){
						goto here;
					}
				}
				resultmark_paths.insert(path);
				here:
				std::cout<<""<<std::endl;
			}
			if(resultmark_paths.empty()){
				CanTriger = false;
			}
		}
		if(CanTriger){
				//pathstream<<"SensitiveFunction:"<<F->getName().str()<<"\n";
				//pathstream<<"path is below"<<"\n";
				for (auto &path : resultmark_paths) {                             //Print all paths from target function to syscall.
					//pathstream<< "@" <<F->getName().str();
            		for (auto &hop : path) {
//						if(RlimitControlFunc.find(hop->getFunction()) != RlimitControlFunc.end()){
//							result_paths.erase(path);
//							pathstream<< " -> ";
//							pathstream<< "@RLIMITF" << hop->getFunction()->getName().str();	
//						} else{
							//pathstream<< " -> ";
							//pathstream<< "@" << hop->getFunction()->getName().str();
//						}
//						pathstream<< " -> ";
//						pathstream<< "@" << hop->getFunction()->getName().str();
//               		std::cout << " -> ";
//                		std::cout << "@" << hop->getFunction()->getName().str();
            		}
				}
			}
		
	}

	return CanTriger;
	
}


static llvm::RegisterPass<ResourceCollectPass>
Z("resourcecollect","Collect Abstract Resource in Kernel");
