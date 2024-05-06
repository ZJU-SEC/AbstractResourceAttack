#include "Utils.h"
#include "PexCallGraph.h"
extern PexCallGraph gCallGraph;

std::string get_prefix(llvm::StructType *st) {
	std::string Name = st->getName().str();
	size_t DotPos = Name.find_last_of('.');
	return (DotPos == 0 || DotPos == std::string::npos || Name.back() == '.' ||
			!isdigit(static_cast<unsigned char>(Name[DotPos + 1])))
				? Name
				: Name.substr(0, DotPos);
}

llvm::Value *stripConstCastExpr(llvm::Value *V) {
	llvm::Value *v = V;
	assert(v);
	while(auto cexpr = llvm::dyn_cast<llvm::ConstantExpr>(v)){
		if (llvm::Instruction::CastOpsBegin <= cexpr->getOpcode() && cexpr->getOpcode() < llvm::Instruction::CastOpsEnd)
			v = cexpr->getOperand(0);
		else break;
	}
	return v;
}

llvm::Value *stripAllConstExpr(llvm::Value *V) {
	llvm::Value *v = V;
	assert(v);
	while(auto cexpr = llvm::dyn_cast<llvm::ConstantExpr>(v)){
		if (llvm::Instruction::CastOpsBegin <= cexpr->getOpcode() && cexpr->getOpcode() < llvm::Instruction::CastOpsEnd)
			v = cexpr->getOperand(0);
		else if (llvm::Instruction::GetElementPtr == cexpr->getOpcode())
			v = cexpr->getOperand(0);
		else break;
	}
	return v;
}

std::vector<llvm::ReturnInst *> getReturnInsts(llvm::Function *f) {
  	std::vector<llvm::ReturnInst *> insts;
  	for (llvm::BasicBlock &bb : f->getBasicBlockList()) {
		llvm::Instruction *i = bb.getTerminator();
      	if (auto retinst = llvm::dyn_cast<llvm::ReturnInst>(i))
        	insts.push_back(retinst);
      	/*
      	if (llvm::isa<llvm::UnreachableInst>(i))
        	insts.push_back(i);
      	*/
	}
  	return insts;
}

llvm::Function *getCallTarget(llvm::CallInst *call)
{
  	llvm::Function *f = call->getCalledFunction();
  	if (!f) {
      	f = llvm::dyn_cast<llvm::Function>(call->getCalledOperand()->stripPointerCastsAndAliases());
    }
  	return f;
}

// std::vector<int> get_indices(llvm::GetElementPtrInst *gep)
// {
//   std::vector<int> indices;
//   for (llvm::Value *v : gep->indices())
//     {
//       if (auto c = llvm::dyn_cast<llvm::ConstantInt>(v))
//         {
//           indices.push_back(c->getSExtValue());
//         }
//       else
//         {
//           indices.push_back(0);
//         }
//     }
//   return indices;
// }

/* handle FuncName.Number */
std::string to_function_name(std::string llvm_name) {
  	size_t dot;
 	dot = llvm_name.find_first_of('.');
  	return llvm_name.substr(0, dot);
}

/* handle StructName.Number */
std::string to_struct_name(std::string llvm_name) {
	size_t dot_first, dot_last;
	dot_first = llvm_name.find_first_of('.');
	dot_last = llvm_name.find_last_of('.');
	if (dot_first == dot_last)
		return llvm_name;
	else
		return llvm_name.substr(0, dot_last);
}

bool isFunctionPointerType(llvm::Value *v) {
	llvm::Type *ty = v->getType();
    return ty->isPointerTy() && ty->getPointerElementType()->isFunctionTy();
}

bool isSyscall(llvm::Function *F) {
	const char *const arch_prefixes[] = {"__ia32", "__ia32", "__x32", "__x64", "__arm64"};
	std::string name = F->getName().str();
	for (int i = 0; i < sizeof(arch_prefixes) / sizeof(const char *); i++) {
	    std::string prefix_sys = std::string(arch_prefixes[i]) + "_sys_";
	    std::string prefix_compat_sys = std::string(arch_prefixes[i]) + "_compat_sys_";
	    if (name.rfind(prefix_sys, 0) == 0 || name.rfind(prefix_compat_sys, 0) == 0) {
			if (name.find("reboot") != name.npos)
				return false;
			return true;
		}
	}
	return false;
}

bool isInterruptHandler(llvm::Function *F) {
	return false;
}

bool isIndirectThreadFunc(llvm::Function *F) {
	auto name = F->getName().str();
	if (name == "worker_thread" || name == "__do_softirq")
		return true;
	return false;
}


bool typeJudger::isTwoStructTyEqual(llvm::Type *ty_a, llvm::Type *ty_b) {
	if(ty_a->isStructTy() && ty_b->isStructTy()) {
		llvm::StructType* st_a = llvm::dyn_cast<llvm::StructType>(ty_a);
		llvm::StructType* st_b = llvm::dyn_cast<llvm::StructType>(ty_b);

		if (get_prefix(st_a) == get_prefix(st_b)) {
			return true;
		}
	}
	return false;
}

void print_func_vec(std::vector<llvm::Function*> &vec) {
	for(auto it : vec) {
		llvm::errs() << it->getName() << "--";
	}
	llvm::errs() << "\n";
}

void __recursive_check_syscall(int max_hop, int hop, llvm::Function* f, std::vector<llvm::Function*> vec) {
	if(hop > max_hop) {
		return;
	}

	vec.push_back(f);

	if(isSyscall(f)) {
		llvm::errs() << "find syscall chain:\n";
		print_func_vec(vec);
	} else {
		PexCallGraph::CGNode *temp_node = gCallGraph.getNode(f);
		for(auto it : temp_node->callers) {
			__recursive_check_syscall(max_hop, hop+1, it.first->f, vec);
		}
	}

	vec.pop_back();
}

void backtrace_syscall_and_print(int hop, llvm::Function* f) {
	__recursive_check_syscall(hop, 0, f, std::vector<llvm::Function*>());
}

std::string print_debugloc(llvm::Instruction *instruction) {
//	os << "{";
	const llvm::DebugLoc &locationLoc = instruction->getDebugLoc();
	if (!locationLoc) {
		//locationLoc.dump();
		std::string location;
		std::string ts;
		llvm::raw_string_ostream rso(ts);	 
		locationLoc.print(rso);
		location=rso.str();
		std::cout<<"Function's location is"<<location<<std::endl;
		return location;
	}
	else{
		return "NOLOC";
	}
//		os << "NF";
//	os << "}";
}

std::string ReturnTypeRefine(llvm::Type &rt){           //Intercept the string of numbers after %struct.inode.xxxx* in the return value.
	std::string ts;
	std::string type_str;
	std::string actual_type_str = "";
	std::string test_type_str = "";
	int count = 1;
	llvm::raw_string_ostream rso(ts);
	rt.print(rso);
	type_str = rso.str();
	if(rt.isIntOrIntVectorTy()){
        return type_str;
    }
    if(rt.isVoidTy()){
    }
    if(*type_str.begin() == '['){
        char *refined_space_str = strtok((char*)type_str.data()," ");
        int count1 = 1;
        while(refined_space_str){
            if(count1 == 3){
                break;
            }
            refined_space_str = strtok(NULL," ");
            count1++;
        }
        type_str = std::string(refined_space_str);
		if(strstr(type_str.c_str(),"]")){
			for(std::string::iterator it=type_str.begin();it!=type_str.end();it++){
				if(*it==']'){
					type_str.erase(it);
					it--;
				}
				if(*it=='*'){
					type_str.erase(it);
					it--;
				}
			}
			return type_str;
		}

	}
	char *refined_space_str = strtok((char*)type_str.data()," ");
	test_type_str=actual_type_str+ std::string(refined_space_str);
	refined_space_str = strtok(NULL," ");
	char *refined_type_str = strtok((char*)test_type_str.data(),".");
	actual_type_str = actual_type_str + std::string(refined_type_str) + ".";
	while(refined_type_str != NULL){
		if(count == 2){//This is used to limit the number of intercepted content blocks. We only need the 1st (%struct) and 2nd (inode) blocks.
			break;
		}
		refined_type_str = strtok(NULL,".");
		if(refined_type_str == nullptr){//This judgment is used to process i8*.
            actual_type_str.pop_back();//Here, adding .i8* will become i8*., so it is removed here.
			break;
		}
		actual_type_str = actual_type_str + std::string(refined_type_str);
		count ++;
	}
	if(actual_type_str[actual_type_str.length()-1] != '*'){
		actual_type_str = actual_type_str + "*";
	}
	return actual_type_str;
}

void PrintSet(std::set<std::string> StringSet,std::vector<llvm::Type*> TypeStack){
	std::cout<<"---TravsedStruct---"<<std::endl;
	for(auto it = StringSet.begin();it != StringSet.end();it++){
		std::cout<<*it<<std::endl;
	}
	std::cout<<"---Stack---"<<std::endl;
	for(auto it = TypeStack.begin();it != TypeStack.end();it++){
		llvm::Type* Ty = *it;
		std::cout<<ReturnTypeRefine(*Ty)<<std::endl;
	}
}

void FindLocation(llvm::Module &M){
	llvm::DebugInfoFinder Finder;
	Finder.processModule(M);
	int count = 0;
	for (auto &Typeit : Finder.types()){
		llvm::DIType *DT = Typeit;
		if(DT->isForwardDecl()){
			count ++;
			std::cout<<count<<": Is ForwardDecl: "<<DT->getName().str()<<std::endl;
			llvm::DIScope *DS = DT->getScope();
			if(DS){
				std::cout<<"has DS!"<<std::endl;
				DS->print(llvm::outs(),&M,false);
			}	
		}
		if(DT->isTypePassByReference()){
			std::cout<<"Is TypePassByReference: "<<DT->getName().str()<<std::endl;
			llvm::DIScope *DS = DT->getScope();
			if(DS){
				std::cout<<"has DS!"<<std::endl;
				DS->print(llvm::outs(),&M,false);	
			}
		}
		if(DT->isRValueReference()){
			std::cout<<"Is RValueReference: "<<DT->getName().str()<<std::endl;
			llvm::DIScope *DS = DT->getScope();
			if(DS){
				std::cout<<"has DS!"<<std::endl;
				DS->print(llvm::outs(),&M,false);
			}	
		}
		//std::cout<<"DIType Name: "<<DT->getName().str()<<std::endl;
	}
	/*for(auto &ScopeIt : Finder.scopes()){
		llvm::DIScope *DS = ScopeIt;
		DS->print(llvm::outs(),&M,false);
	}*/
}

