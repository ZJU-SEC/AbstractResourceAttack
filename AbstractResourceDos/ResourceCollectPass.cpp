#include "ResourceCollectPass.h"
#include "LockAtomicStructure.h"
#include "llvm/IR/InstIterator.h"
#include "gatlin/include/gatlin_idcs.h"
#include "PexCallGraph.h"
#include "llvm/Support/raw_ostream.h"

extern PexCallGraph gCallGraph;
char ResourceCollectPass::ID = 0;
std::error_code ec;
llvm::raw_fd_ostream resultstream("./location-finder.txt", ec, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);
/*
	runOnModule是pass的主函数。
*/
bool ResourceCollectPass::runOnModule(llvm::Module &M) {
	 
	LockPair LP = LockPair(M);                       //实例化一个LockPair的对象，我们把找锁所在结构体的内容也加到了LockPair里面去
	LP.CollectLockAndAtomic();	
	//LP.Test();					//实例化我们的锁函数集合，在这里就相当于执行了之前的path. 				
	auto &functionList = M.getFunctionList();
	bool CanTriger = false;
    for (auto &function : functionList){                      //遍历module的所有Function 
		std::string FuncName = function.getName().str();
		//if(FuncName == "alloc_pid"){
			LP.LockPairMain(&function);
			LP.LockAtomicResource(&function);
			LP.MarkRlimitControlFunc(&function);
		//} //调试用
		
    }
	//LP.Test();

	for (auto &mapit : LP.FuncResource){	//这里我们形成了一个map,存放的是不经过call处理的所有找出的敏感函数。
		//if(SysCallPath(mapit.first,LP.RlimitControlFunc)){
			auto Resource = mapit.second;
			for(auto it = Resource.begin();it != Resource.end();it++){
				llvm::Function *locf=mapit.first;
				std::string loc;
				for(llvm::inst_iterator Itb=inst_begin(locf);Itb!=inst_end(locf);Itb++){
					llvm::Instruction *itm=&*Itb;
					if(llvm::CallInst *callInst=llvm::dyn_cast<llvm::CallInst>(itm)){
						std::cout<<"getCalled"<<std::endl;
						const llvm::DebugLoc &location=itm->getDebugLoc();
						if(location){
							std::cout<<"getLoc"<<std::endl;
							if(llvm::DIScope *Scope=llvm::dyn_cast<llvm::DIScope>(location.getScope())){
								std::cout<<"dyn_cast right"<<std::endl;
								llvm::StringRef fileName=Scope->getFilename();
								//resultstream<<"Location:"<<fileName.str()<<"\n";
								loc = fileName.str();
								break;
							}
						} else{
							std::cout<<"NO LOCATION message"<<std::endl;
						}
					}
				}
				resultstream<<"FunctionName:"<<mapit.first->getName().str()<<","<<*it<<","<<"Location:"<<loc<<"\n";
			}
		//}
	}
	//LP.Test();*/

	CtlTableAnalysis CT = CtlTableAnalysis(M);  //43和44行用于跑新加的CtlTableAnalysis。
	CT.Test();
	for (auto &mapit : CT.CtlFuncResource){
		auto Resource = mapit.second;
		for(auto it = Resource.begin();it != Resource.end();it++){
				llvm::Function *locf=mapit.first;
				std::string loc;
				for(llvm::inst_iterator Itb=inst_begin(locf);Itb!=inst_end(locf);Itb++){
					llvm::Instruction *itm=&*Itb;
					if(llvm::CallInst *callInst=llvm::dyn_cast<llvm::CallInst>(itm)){
						std::cout<<"getCalled"<<std::endl;
						const llvm::DebugLoc &location=itm->getDebugLoc();
						if(location){
							std::cout<<"getLoc"<<std::endl;
							if(llvm::DIScope *Scope=llvm::dyn_cast<llvm::DIScope>(location.getScope())){
								std::cout<<"dyn_cast right"<<std::endl;
								llvm::StringRef fileName=Scope->getFilename();
								//resultstream<<"Location:"<<fileName.str()<<"\n";
								loc = fileName.str();
								break;
							}
						} else{
							std::cout<<"NO LOCATION message"<<std::endl;
						}
					}
				}	
			resultstream<<"FunctionName:"<<mapit.first->getName().str()<<","<<*it<<","<<"Location:"<<loc<<"\n";
		}
	}

	std::cout<<"After ResourceCollect "<<std::endl;
	return false;
}

/*
	traceTaskEntry:获得所有从syscall到目标函数的路径集合，第一个参数传入目标函数的callsite(目标函数的caller)，result_path是获得的结果，包含多条从目标函数到entry的路径。
*/
void ResourceCollectPass::traceTaskEntry(std::vector<llvm::CallInst*> &ci_stack, std::set<std::vector<llvm::CallInst*> > &result_path, const std::map<llvm::Function*, llvm::CallInst *> &free_sites) {
    auto top_ci = ci_stack.back();
    auto top_fn = top_ci->getFunction();
	/*
	if(top_fn->getName().str() == "nf_conntrack_alloc"){
		std::cout<<"top_fn is : "<<top_fn->getName().str()<<std::endl;
		PexCallGraph::CGNode *n = gCallGraph.getNode(top_fn);
		for (auto &item : n->callers) {
			std::cout<<"caller: "<<item.second->call->getFunction()->getName().str()<<std::endl;
		}
	}
	*/
    if (ci_stack.size() == 10 || free_sites.find(top_fn) != free_sites.end())	//这里设置了递归深度为7,用来限制规模
        return;
	if (top_fn == nullptr) {
		llvm::errs() << "Cannot find function holding the CallInst: " << *top_ci << "\n";
		return;
	}
	PexCallGraph::CGNode *n = gCallGraph.getNode(top_fn);
	for (auto link : n->callers) {
		llvm::CallInst *nextci = link.second->call;
		if (std::find(ci_stack.begin(), ci_stack.end(), nextci) != ci_stack.end()) {
			continue;
		}
		ci_stack.push_back(nextci);
		if (isSyscall(nextci->getFunction())) {
	    	result_path.insert(ci_stack);
		}
//        else if (isInterruptHandler(nextci->getFunction())) {	//只找syscall，所以我们把这里跳过
//            result_path.insert(ci_stack);
//        }
//        else if (isIndirectThreadFunc(nextci->getFunction())) {
//            result_path.insert(ci_stack);
//        }
		else{
	    	traceTaskEntry(ci_stack, result_path, free_sites);
		}
	  	ci_stack.pop_back();
	}
}

/*
	SysCallPath返回路径中是否存在syscall，存在则返回true。
	第二个参数可以返回路径的集合。
*/

bool ResourceCollectPass::SysCallPath(llvm::Function* F,std::set<llvm::Function*> RlimitControlFunc){
	bool CanTriger = false;
    std::vector<llvm::CallInst*> temp_ci;
    std::set<std::vector<llvm::CallInst*> > result_paths;
	auto gcallsites = gCallGraph.findCallSites(F);
	//std::cout<<"SysCallPath*****"<<std::endl;
	for(auto *callsite : gcallsites){
		//std::cout<<"callsite Name: "<<callsite->getFunction()->getName().str()<<std::endl;
		temp_ci.clear();
		result_paths.clear();
		temp_ci.push_back(callsite);
		std::map<llvm::Function*, llvm::CallInst *> emptymap = {};
        traceTaskEntry(temp_ci, result_paths, emptymap);
		if(!result_paths.empty()){
			CanTriger = true;
			for (auto &path : result_paths) {                             //打印所有从目标函数到syscall的路径
            	for (auto &hop : path) {
					if(RlimitControlFunc.find(hop->getFunction()) != RlimitControlFunc.end()){
						CanTriger = false;
					}
				}
			}

			if(CanTriger){
				for (auto &path : result_paths) {                             //打印所有从目标函数到syscall的路径
            		for (auto &hop : path) {
                		std::cout << " -> ";
                		std::cout << "@" << hop->getFunction()->getName().str();
            		}
             		std::cout << "\n";
				}
			}
		}
	}

	return CanTriger;
	
}


static llvm::RegisterPass<ResourceCollectPass>
Z("resourcecollect","Collect Abstract Resource in Kernel");