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
	runOnModule是pass的主函数。
*/
bool ResourceCollectPass::runOnModule(llvm::Module &M) {
	LockPair LP = LockPair(M);                       //实例化一个LockPair的对象，我们把找锁所在结构体的内容也加到了LockPair里面去
	LP.CollectLockAndAtomic();	 				
	auto &functionList = M.getFunctionList();
	bool CanTriger = false;
    for (auto &function : functionList) {                      //遍历module的所有Function 
		if (LP.IsInitFunction(&function))
			continue;
		std::string FuncName = function.getName().str();

			LP.LockPairMain(&function);
			LP.LockAtomicResource(&function);//这个函数的核心是收集lock/atomic全局参数&所在的结构体,在这里,我需要修改为,对于全局变量,只收集atomic,不收集lock
			LP.MarkRlimitControlFunc(&function);	
    }
	LP.Test();

	for (auto &mapit : LP.FuncResource){	//这里我们形成了一个map,存放的是不经过call处理的所有找出的敏感函数。
		if(SysCallPath(mapit.first,LP.RlimitControlFunc)){
			auto Resource = mapit.second;
			for(auto it = Resource.begin();it != Resource.end();it++){
				resultstream<<"FunctionName:"<<mapit.first->getName().str()<<","<<*it<<"\n";
			}
		}//这里对应if(SysCallPath)
	}
	//LP.Test();
//如果需要单独运行ctl_table相关内容,需要将17-66行的内容注释掉	
	CtlTableAnalysis CT = CtlTableAnalysis(M);  //43和44行用于跑新加的CtlTableAnalysis。
	CT.Test();

	std::cout<<"After ResourceCollect "<<std::endl;
	return false;
}

/*
	traceTaskEntry:获得所有从syscall到目标函数的路径集合，第一个参数传入目标函数的callsite(目标函数的caller)，result_path是获得的结果，包含多条从目标函数到entry的路径。
*/
void ResourceCollectPass::traceTaskEntry(int indirectcount, std::vector<llvm::CallInst*> &ci_stack, std::set<std::vector<llvm::CallInst*> > &result_path, const std::map<llvm::Function*, llvm::CallInst *> &free_sites) {
    auto top_ci = ci_stack.back();
    auto top_fn = top_ci->getFunction();

    if (ci_stack.size() == 13 || free_sites.find(top_fn) != free_sites.end() || result_path.size() >= 100) 	//这里设置了递归深度为7,用来限制规模
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
			for (auto &path : result_paths) {                             //打印所有从目标函数到syscall的路径
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
				for (auto &path : resultmark_paths) {                             //打印所有从目标函数到syscall的路径
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