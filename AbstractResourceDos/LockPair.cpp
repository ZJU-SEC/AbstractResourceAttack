#include "LockPair.h"
#include "llvm/IR/InstIterator.h"

std::error_code ec1;
llvm::raw_fd_ostream debugstream("./debug-finder.txt", ec1, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);
llvm::raw_fd_ostream Lockstream("./Namespace-finder.txt", ec1, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);
llvm::raw_fd_ostream RlimitMarkstream("./Rlimit-markfunction.txt", ec1, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);
llvm::raw_fd_ostream NameSpaceMarkstream("./NameSpace-markfunction.txt", ec1, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);
std::set<std::string> Namespace {"%struct.uts_namespace*","%struct.ipc_namespace*","%struct.mnt_namespace*","%struct.pid_namespace*","%struct.net*","%struct.cgroup_namespace*"};

LockPair::LockPair(llvm::Module &module){
    _module_ = &module;
}


void LockPair::Test(){
    for(auto it = RlimitControlFunc.begin();it != RlimitControlFunc.end();it++){
        llvm::Function *F = *it;
        RlimitMarkstream<<F->getName().str()<<"\n";
    }
    for(auto it= NameSpaceControlFunc.begin();it!=NameSpaceControlFunc.end();it++){
        llvm::Function *F= *it;
        NameSpaceMarkstream<<F->getName().str()<<"\n"; 
    }
    std::vector<llvm::StructType *> AllStruct = _module_->getIdentifiedStructTypes();
    for(auto it = AllStruct.begin(); it != AllStruct.end();it ++){
        llvm::StructType* ST = *it;
        if(ST->getName().str() == "struct.nsproxy") {
            int ElementNum = ST->getStructNumElements();
            for(int i = 0; i < ElementNum; i++){
                llvm::Type* NsType = ST->getStructElementType(i);
                if(NsType->isPointerTy()){
                    llvm::Type *RealNsType = NsType->getPointerElementType();
                    if(RealNsType->isStructTy()){
                        std::set<std::string> NsRealm = TravseNamespace(RealNsType);
                        for(auto it = NsRealm.begin();it != NsRealm.end();it ++){
                            NsStruct.insert(*it);
                        }
                    }
                }
            }
        }
    }
    std::cout<<"Total Struct:"<<NsStruct.size()<<"\n";
}

void LockPair::CollectLockAndAtomic(){//This replaces our previous operation of getline from the file.
    FindLockAndAtomic FLA = FindLockAndAtomic(*_module_);   //First set up an instantiated object.
    FLA.FindLock(); //Set everything here.
    spin_lock = FLA.GetSpinLock();
    spin_unlock = FLA.GetSpinUnlock();
    mutex_lock = FLA.GetMutexLock();
    mutex_unlock = FLA.GetMutexUnlock();
    write_lock = FLA.GetWriteLock();
    write_unlock = FLA.GetWriteUnlock();
    percpu_function = FLA.GetPerCPU();
    atomic_function = FLA.GetAtomicFunc();
    percpu_function_not_change=FLA.GetPerCPUNotChange();
    atomic_function_not_change=FLA.GetAtomicFuncNotChange();
    AllocFunctionNames = FLA.GetAllocFunction();
    rlimit_func = FLA.GetRlimitFunction();
    spin_lock.insert(mutex_lock.begin(),mutex_lock.end());
	spin_lock.insert(write_lock.begin(),write_lock.end());
	spin_unlock.insert(mutex_unlock.begin(),mutex_unlock.end());
	spin_unlock.insert(write_unlock.begin(),write_unlock.end());
}

int LockPair::getBBID(llvm::BasicBlock* BB){//If you get a BB, return the BB number.
    std::string ts;
    std::string BBID;
    llvm::raw_string_ostream rso(ts);
    BB->printAsOperand(rso,false);//The function of pinrAsOperand is to type out the BB number %xx and write it into the rso stream.
    BBID = rso.str();
    BBID = BBID.substr(1,BBID.length());//Cut off the first %
    int BBIDint = atoi(BBID.c_str());//Convert the string of xx to int.
    return BBIDint;
}

char* LockPair::GetActualFName(std::string &functionname){   //To obtain the name of the function, intercept the string of numbers after atomic.xxxx, where functionname is the name of the function being called.
	std::string mystr;
	char *FName = (char*)functionname.data();
	char *ActualFName = strtok(FName,".");
    functionname = ActualFName;//What is passed in is a pointer. If you assign a value here, you can directly modify the object of the pointer, which is functionname.
	while(ActualFName != NULL){
		ActualFName = strtok(NULL,".");//The second interception contains xxxx of atomic.xxxxx.
		break;
	}

    return ActualFName;//Here ActualName is actually the number after atomic, and functionname is the real atomic.
}
llvm::Type* LockPair::GetActualStructType(llvm::Instruction *gepInst,std::string funName,llvm::Type *originTy){    //Pass in GEP Instruction and function name (function name is mainly used for debugging), and return the real structure type.
	std::cout<<"-Functiaon Name-:"<<funName<<std::endl;
	for(auto operand = gepInst->operands().begin();operand != gepInst->operands().end();++operand){ //Traverse the operand of Gep Instruction
		if(llvm::CallInst *callInst = llvm::dyn_cast<llvm::CallInst>(operand)){              //If the operand corresponds to a call statement
			if(llvm::Function *voidFunc = llvm::dyn_cast<llvm::Function>(callInst->getCalledOperand()->stripPointerCasts())){//The reason for obtaining the call function name through this method is that in cases like call bitcast, using getName() directly will report an error.
				std::cout<<"void Call to => " << voidFunc ->getName().str() << "\n";
				std::string ActualAllocFuncName  = voidFunc->getName().str();
				if(AllocFunctionNames.find(ActualAllocFuncName) != AllocFunctionNames.end()){ //Determine whether the call is the kmalloc function
					llvm::Value *kmVar = llvm::dyn_cast<llvm::Value>(callInst);
					if(!kmVar->use_empty()){                                                //If it is the kmalloc function, find the user of kmalloc
						for(llvm::Value::use_iterator UB=kmVar->use_begin(),UE=kmVar->use_end();UB!=UE;++UB){
							//Under normal circumstances, there is bitcast in the user immediately following kmalloc to convert the i8* allocated by kamalloc into a real structure. So we only need to find the bitcast statement in the first user
							llvm::User* user=UB->getUser();                                    
							if(llvm::Instruction* userInst = llvm::dyn_cast<llvm::Instruction>(user)){      
								if(userInst->getOpcode() == llvm::Instruction::BitCast){        //Find the bitcast statement
									llvm::Value *userVar = llvm::dyn_cast<llvm::Value>(userInst);
									llvm::Type *userType = userVar->getType();                  //The TypeName of Value corresponding to the bitcast statement is the real structure you are looking for.
                                    return userType; 
								}
							}
						}
					}
				}
			}
		}
		if(llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(operand)){   //For debugging, will there be any other statements between the debugging gep instruction and the call kmalloc function.
			if(Inst->getOpcode() == llvm::Instruction::BitCast){
				std::cout<<"Has a BitCast Middle"<<std::endl;
			}
		}
	}

	return originTy;
}

void LockPair::id_phi_inst(llvm::Function* funcname,llvm::Instruction* I,std::vector<std::string>* Resource){
    std::string FuncName=funcname->getName().str();
	llvm::PHINode *PN= llvm::cast<llvm::PHINode>(I);
	bool found=false;
	for(u_int j=0;j<PN->getNumIncomingValues();j++){
		llvm::Value* V=PN->getIncomingValue(j);
		if(!V)continue;
		if(llvm::GetElementPtrInst* GEP=llvm::dyn_cast<llvm::GetElementPtrInst>(V)){
            if(llvm::GlobalValue* G = llvm::dyn_cast<llvm::GlobalValue>(GEP->getOperand(0))){
                if(!CheckGlobalVariable(G)) {
                    std::string GB = "Global Variable:" + GEP->getOperand(0)->getName().str();
                    Resource->push_back(GB);
                }
            }
            llvm::Type *structType = GEP->getSourceElementType();//Get the struct of the GEP instruction here
			if(ReturnTypeRefine(*structType) == "i8*"){            //If the structure in the GEP instruction is i8*, special processing is required to find out the real structure.
				std::string ActualStructType;
                llvm::Type *ActualTy = GetActualStructType(GEP,FuncName,structType);
				ActualStructType = ReturnTypeRefine(*ActualTy);    //Call GetActualStructType to get the actual structure.
                std::string PS = "ProtectedStruct:" + ActualStructType;
                if(!StructHasNamespace(ActualTy,FuncName)){
                    Resource->push_back(PS);  
                }
			}else{
                std::string PS = "ProtectedStruct:" + ReturnTypeRefine(*structType);  
                if(!StructHasNamespace(structType,FuncName)){
                    Resource->push_back(PS);
                }
			}
            return;             
		}
		if(llvm::LoadInst* LI=llvm::dyn_cast<llvm::LoadInst>(V)){
            llvm::Value* loadValue=LI->getOperand(0);
            if(llvm::GlobalValue* G = llvm::dyn_cast<llvm::GlobalValue>(loadValue)){
                if(!CheckGlobalVariable(G)) {
                    std::string GB = "Global Variable:" + loadValue->getName().str();
                    Resource->push_back(GB);
                }
            } else {
                if(llvm::Instruction *loadVInst=llvm::dyn_cast<llvm::Instruction>(loadValue)){
                    TravseAllocUser(funcname,loadVInst,Resource);
                }
            }
		}
    }
	return;
}

void LockPair::TravseAllocUser(llvm::Function* func,llvm::Instruction* originv,std::vector<std::string>* Resource){  
//The Instruction passed in is the Instruction corresponding to the function parameter Value. It may be a Call statement, a Gep instruction or a Phi instruction, or a load instruction. The Call, Gep, and load statements are specially processed here.
    std::string testfuncName=func->getName().str();
	if(originv->getOpcode() == llvm::Instruction::GetElementPtr){      //The first case corresponds to the GEP instruction, which means we can get the structure from here.
		llvm::GetElementPtrInst *gepinst = llvm::dyn_cast<llvm::GetElementPtrInst>(originv);
        llvm::Value *GepOperand = gepinst->getOperand(0);
        if(llvm::GlobalValue* G=llvm::dyn_cast<llvm::GlobalValue>(GepOperand)){
            if(!CheckGlobalVariable(G)) {
                std::string GB = "Global Variable:" + GepOperand->getName().str();
                Resource->push_back(GB);
                return;
            }
        }
		llvm::Type *structType = gepinst->getSourceElementType();//Get the struct of the GEP instruction here
		if(ReturnTypeRefine(*structType) == "i8*"){            //If the structure in the GEP instruction is i8*, special processing is required to find out the real structure.
			std::string ActualStructType;
            llvm::Type *ActualTy = GetActualStructType(originv,testfuncName,structType);
			ActualStructType = ReturnTypeRefine(*ActualTy);   //Call GetActualStructType to get the actual structure.
            std::string PS = "ProtectedStruct:" + ActualStructType;
            
            if(!StructHasNamespace(ActualTy,testfuncName)){
                Resource->push_back(PS);
            }
		}else{
            std::string PS = "ProtectedStruct:" + ReturnTypeRefine(*structType);
            if(!StructHasNamespace(structType,testfuncName)){
                Resource->push_back(PS); 
            }
		}        
		return ;
	}
    if(llvm::LoadInst * loadIns=llvm::dyn_cast<llvm::LoadInst>(originv)){
        llvm::Value* loadValue=loadIns->getOperand(0);
        if(llvm::GlobalValue* G=llvm::dyn_cast<llvm::GlobalValue>(loadValue)){
            if(!CheckGlobalVariable(G)) {
                std::string GB = "Global Variable:" + loadValue->getName().str();
                Resource->push_back(GB);
                return;
            }
        } else {
            if(llvm::Instruction *loadVInst=llvm::dyn_cast<llvm::Instruction>(loadValue)){
                TravseAllocUser(func,loadVInst,Resource);
            }
        }
    }

	if(llvm::CallInst *callInst = llvm::dyn_cast<llvm::CallInst>(originv)){              //If it corresponds to callInst, then it means that this Value is the result of call. Can I directly type the return value type of the call function here?
        if(llvm::Function *called = callInst->getCalledFunction()){
            std::string CalledName = called->getName().str();
            if(AllocFunctionNames.find(CalledName) != AllocFunctionNames.end()){
                llvm::Value *kmVar = llvm::dyn_cast<llvm::Value>(callInst);
				if(!kmVar->use_empty()){                                                //If it is the kmalloc function, find the user of kmalloc.
					for(llvm::Value::use_iterator UB=kmVar->use_begin(),UE=kmVar->use_end();UB!=UE;++UB){
						//Under normal circumstances, there is bitcast in the user immediately following kmalloc to convert the i8* allocated by kamalloc into a real structure. So we only need to find the bitcast statement in the first user.
						llvm::User* user=UB->getUser();                                    
						if(llvm::Instruction* userInst = llvm::dyn_cast<llvm::Instruction>(user)){      
							if(userInst->getOpcode() == llvm::Instruction::BitCast){        //Find the bitcast statement
								llvm::Value *userVar = llvm::dyn_cast<llvm::Value>(userInst);
								llvm::Type *userType = userVar->getType();                  
                                std::string PS = "ProtectedStruct:" + ReturnTypeRefine(*userType);
                                if(!StructHasNamespace(userType,testfuncName)){
                                    Resource->push_back(PS);
                                }
							}
						}
					}
				}
            }else{
                std::string type_str;
                llvm::Value* callretV=llvm::dyn_cast<llvm::Value>(callInst);
                llvm::Type* tp=callretV->getType();
                llvm::raw_string_ostream rso(type_str);
                tp->print(rso);
                std::string PS = "ProtectedStruct:" + ReturnTypeRefine(*tp);
                if(!StructHasNamespace(tp,testfuncName)){
                    Resource->push_back(PS);
                }
            }
        }
        return;
	} 
    if(llvm::PHINode *testphi=llvm::dyn_cast<llvm::PHINode>(originv)){
        id_phi_inst(func,originv,Resource);
    }

	for (auto operand = originv->operands().begin();operand != originv->operands().end();++operand){  //If the corresponding statement is an ordinary statement, its operands are traversed normally to recurse.
		llvm::Value *opValue = llvm::dyn_cast<llvm::Value>(operand);
		if(llvm::Instruction *opInst = llvm::dyn_cast<llvm::Instruction>(opValue)){
			//This sentence is used to determine whether it is a parameter of the F function. After debugging, we know that if the Value cannot be forced to be converted to an Instruction, it means that it is a function parameter.
			for(auto travop = opInst->operands().begin();travop!=opInst->operands().end();++travop){
				if(llvm::Instruction *travopIns=llvm::dyn_cast<llvm::Instruction>(travop))
				{
					if(travopIns==originv){
						return;
					}
				}
			}
            return;

		}else{
			llvm::Type *StructType = opValue->getType();
            std::string type_str;
            llvm::raw_string_ostream rso(type_str);
            StructType->print(rso);
            std::string PS = "ProtectedStruct:" + ReturnTypeRefine(*StructType);
                Resource->push_back(PS);
			continue ;
		}
	}
		return;
}

bool LockPair::FindChangArg(llvm::Function * funcname, int offset){//This is equivalent to inputting funcB in funcA: call funcB and the offset of the corresponding pointer type function.
    std::cout<<"into FindChangeArg"<<std::endl;
    if(llvm::Value * offsetarg=funcname->getArg(offset)){
        if(!offsetarg->user_empty()){
            for(auto arguser=offsetarg->user_begin();arguser!=offsetarg->user_end();arguser++){
                if(llvm::GetElementPtrInst *argGEP=llvm::dyn_cast<llvm::GetElementPtrInst>(*arguser)){
                    std::cout<<" Transform GEP Instruction To Value"<<std::endl;
                    llvm::Value *gepV=llvm::dyn_cast<llvm::Value>(argGEP);
                    if(!gepV->user_empty()){
                        for(auto gepVuser=gepV->user_begin();gepVuser!=gepV->user_end();gepVuser++){
                            if(llvm::StoreInst * gepVuserStore=llvm::dyn_cast<llvm::StoreInst>(*gepVuser)){
                                std::cout<<"Try to GetStore A to B's B"<<std::endl;
                                llvm::Value * StoreTo=gepVuserStore->getPointerOperand();//This is equivalent to taking B from store A to B.
                                std::cout<<" Got the Bravo"<<std::endl;
                                if(StoreTo==gepV){//If B is the previous GEP result
                                //Here we need to recursively expand A. If A comes from operations such as add/sub, then we think that the modification of this Arg in this place is valid.
                                    bool confirmcount = ConfirmCountotherlayer(gepVuserStore);
                                    if(confirmcount){
                                        return true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return false;
}
void LockPair::FindStoreAndCall(llvm::Instruction *Inst,llvm::Function* funcName,std::vector<std::string> *Resource, std::set<llvm::Instruction *> LockProtectIns){
    std::string funcname=funcName->getName().str();
    if(llvm::CallInst *TestcallInst=llvm::dyn_cast<llvm::CallInst>(Inst)){//If call is caught after lock
        if(llvm::Function *called = TestcallInst->getCalledFunction()){
            std::string calledFuncName = called->getName().str();
            if((spin_lock.find(calledFuncName)!=spin_lock.end())||(spin_unlock.find(calledFuncName)!=spin_unlock.end())||(atomic_function.find(calledFuncName)!=atomic_function.end())||(percpu_function.find(calledFuncName)!=percpu_function.end())){
                return;
            }
            if(called==NULL){
                return;
            }
            if(called->onlyReadsMemory()){
                return;
            }
//            for(auto arglb=TestcallInst->arg_begin(),argle=TestcallInst->arg_end();arglb!=argle;arglb++){
//                if(llvm::Value *argValue = llvm::dyn_cast<llvm::Value>(arglb)){
//                   if(argValue->getType()->isPointerTy()){
//                        int offset=arglb->getOperandNo();
//                        FindChangArg(called, offset);
//                    }
//                }
//            }

            for(auto arglb=TestcallInst->arg_begin(),argle=TestcallInst->arg_end();arglb!=argle;arglb++){
	    //So this corresponds to capturing the call parameters and traversing each parameter of the call statement. If it is a quantitative modification,
            //Then find the resource corresponding to the parameter.
	    //if it is a structure, print the source of the structure, if it is a variable, you need to see where the variable comes from, if it comes from a function parameter, print the variable, if it is a global variable, print the variable name.
                if(llvm::Value *argValue = llvm::dyn_cast<llvm::Value>(arglb)){
                    if(argValue->getType()->isPointerTy()){
                        int offset=arglb->getOperandNo();
                        bool argmodified=FindChangArg(called, offset);
                        if(argmodified){
                            for(auto argF=funcName->arg_begin();argF!=funcName->arg_end();argF++){
				//The function of this judgment is that if arg is the arg of F itself, then the parameters passed in by F have been operated, and we should print the parameter type directly.
                                if(llvm::Value * argnamed=llvm::dyn_cast<llvm::Value>(argF)){  
                                    if(argnamed==argValue){
                                        if(llvm::GlobalValue* AtomicG=llvm::dyn_cast<llvm::GlobalValue>(argF)){//If the parameter has a name, it means it is a global variable and the parameter name is printed.
                                            if(!llvm::dyn_cast<llvm::Function>(argValue)){
                                                if(!CheckGlobalVariable(AtomicG)) {
                                                    std::string GB = "Global Variable:"+argF->getName().str();
                                                    Resource->push_back(GB);
                                                }
                                            }
                                        }
                                        llvm::Type* argtype= argValue->getType();//If the function parameter has no name, it means it is a local variable parameter, so I just print the parameter type here.
                                        std::string PS = "ProtectedStruct:"+ReturnTypeRefine(*argtype);
                                        if(!StructHasNamespace(argtype,funcname)){
                                            Resource->push_back(PS);
                                        }
    						        }
    					        }
                            }
                            if(llvm::GlobalValue* AtomicG=llvm::dyn_cast<llvm::GlobalValue>(arglb)){//If the parameter of the call statement is a global variable, it is printed directly.
                                if(!llvm::dyn_cast<llvm::Function>(argValue)){
                                    if(!CheckGlobalVariable(AtomicG)) {
                                        std::string GB = "Global Variable:"+AtomicG->getName().str();
                                        Resource->push_back(GB);
                                    }
                                }    
                            }
    			            if(llvm::Instruction *argInst=llvm::dyn_cast<llvm::Instruction>(arglb)){//If it is neither a global variable nor a parameter of the F function, nor is it a structure itself, then the source of the parameters of the internal call needs to be analyzed.
                                TravseAllocUser(funcName,argInst,Resource);//Here are the names of the Instruction and F passed into the lock function parameters, because if you call the call statement between lock/unlock, then either the parameters of the call have changed, or where the return value of the call is stored.
                            }
                        }        													
			        }
                }
    		}
        }

        if(llvm::Value * callreturnValue=llvm::dyn_cast<llvm::Value>(TestcallInst)){
            if(!callreturnValue->user_empty()){  //Did not add "!"
                for(auto calluser=callreturnValue->user_begin();calluser!=callreturnValue->user_end();calluser++){
                    if(llvm::StoreInst * returnStore=llvm::dyn_cast<llvm::StoreInst>(*calluser)){
                        if(llvm::Value * returnStoredetination= returnStore->getOperand(1)){
                            if(llvm::GlobalValue* G=llvm::dyn_cast<llvm::GlobalValue>(returnStoredetination)){
                                if(!llvm::dyn_cast<llvm::Function>(returnStoredetination)){
                                    if(!CheckGlobalVariable(G)) {
                                        std::string GB = "Global Variable:"+returnStoredetination->getName().str();
                                        Resource->push_back(GB);
                                    }
                                }
                            }
                            if(llvm::GetElementPtrInst * retstodesInst=llvm::dyn_cast<llvm::GetElementPtrInst>(returnStoredetination)){
                                llvm::Type* retgeptype= retstodesInst->getSourceElementType();
                                std::string PS = "ProtectedStruct:"+ReturnTypeRefine(*retgeptype);
                                if(!StructHasNamespace(retgeptype,funcname)){
                                    Resource->push_back(PS);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if(llvm::StoreInst * teststoreInst=llvm::dyn_cast<llvm::StoreInst>(Inst)){//If the store is caught after lock
        bool confirmcount = ConfirmCount(teststoreInst,LockProtectIns);
        llvm::Value * storeValuet= teststoreInst->getOperand(1);
        if(llvm::GlobalValue* G=llvm::dyn_cast<llvm::GlobalValue>(storeValuet)){//If the store object has a name, it is a global variable and we print it directly.
            if(!llvm::dyn_cast<llvm::Function>(storeValuet)){
                if(!CheckGlobalVariable(G) && confirmcount) {
                    std::string GB = "Global Variable:"+storeValuet->getName().str();
                    Resource->push_back(GB);
                } 
            }
        }
        if(llvm::Instruction * storeInstruction = llvm::dyn_cast<llvm::Instruction>(storeValuet)){//If the store is not a global variable, we need to use GEP to see where the variable comes from.
            if(llvm::GetElementPtrInst * storegep= llvm::dyn_cast<llvm::GetElementPtrInst>(storeInstruction)){
                llvm::Type* gepType= storegep->getSourceElementType();//If the variable is a structure member, we print the structure type. The processing here is flawed. If other statements pass between store and GEP, I cannot handle it here.
                std::string PS = "ProtectedStruct:"+ReturnTypeRefine(*gepType);
                if(!StructHasNamespace(gepType,funcname) && confirmcount){
                    Resource->push_back(PS);
                }
            }
        }
    }

}
bool LockPair::isTypeMatch(const llvm::Instruction *sink, const llvm::Value *source){
    int nFnArg = 0, nCallArg = 0;
    std::vector<const llvm::Type *> fnArgList, callArgList;
    if(llvm::isa<llvm::Function>(source)){
        const llvm::Function *fn= llvm::dyn_cast<llvm::Function>(source);
        const llvm::Type *rTy=fn->getReturnType();
        nFnArg = fn->arg_size();
        for(llvm::Function::const_arg_iterator AI = fn->arg_begin(),AE = fn->arg_end();AI!=AE;++AI){
            const llvm::Value *arg = AI;
            const llvm::Type *argType = arg->getType();
            fnArgList.push_back(argType);
        }
        if(llvm::isa<llvm::CallInst>(sink)){
            const llvm::CallInst *cBase = llvm::dyn_cast<llvm::CallInst>(sink);
            if(cBase->getFunctionType()->getReturnType() != rTy){
                return false;
            }
            nCallArg = cBase->getNumArgOperands();
            for (int i=0; i< cBase->getNumArgOperands(); i++){
                const llvm::Value *arg=cBase->getArgOperand(i);
                const llvm::Type *argType = arg->getType();
                callArgList.push_back(argType);
            }
        } else if(llvm::isa<llvm::InvokeInst>(sink)){
            const llvm::InvokeInst *cBase= llvm::dyn_cast<llvm::InvokeInst>(sink);
            if(cBase->getFunctionType()->getReturnType() != rTy){
                return false;
            }
            nCallArg = cBase->getNumArgOperands();
            for(int i = 0; i < cBase->getNumArgOperands();i++){
                const llvm::Value *arg = cBase->getArgOperand(i);
                const llvm::Type *argType = arg->getType();
                callArgList.push_back(argType);
            }
        }
        if(nFnArg == nCallArg){
            for(int i=0;i < nFnArg; i++){
                if (fnArgList[i] != callArgList[i]){
                    return false;
                }
            }
            return true;
        }
    } else {
        return true;
    }
    return false;
}

bool LockPair::ConfirmCountotherlayer(llvm::Instruction *ins)
{   
    llvm::Value *store_var = ins->getOperand(0);   
    llvm::Instruction *store_ins = llvm::dyn_cast<llvm::Instruction>(store_var);

    if(!store_ins) 
        return false;

    auto Opcode = store_ins->getOpcode();
    std::vector<llvm::Instruction *> TravseStack;

    if(Opcode >= 13 && Opcode <= 16 ) {  //The enumeration value of operation instructions ranges from 13-30
        //llvm::outs()<<"find count op: "<<*store_ins<<"\n";
        return true;
    }

    TravseStack.push_back(store_ins);
    //llvm::outs()<<"store_ins: "<<*store_ins<<"\n";

    while(!TravseStack.empty()) {
        llvm::Instruction *travse_ins = TravseStack.front();
        auto it = TravseStack.begin();
        TravseStack.erase(it);
       //llvm::outs()<<"pop_ins: "<<*travse_ins<<"\n";

        for(auto opd = travse_ins->operands().begin(); opd != travse_ins->operands().end(); opd ++) {
            llvm::Value *op_var = llvm::dyn_cast<llvm::Value>(opd);
            if(llvm::Instruction *op_ins = llvm::dyn_cast<llvm::Instruction>(op_var)) {
                auto travse_op = op_ins->getOpcode();
                if(travse_op >= 13 && travse_op <= 16) {  //The enumeration value of operation instructions ranges from 13-30
                    //llvm::outs()<<"find count op: "<<*op_ins<<"\n";
                    return true;
                }

                //llvm::outs()<<"travse: "<<*op_ins<<"\n";
                if(llvm::dyn_cast<llvm::PHINode>(op_ins))
                    continue;
                if(llvm::dyn_cast<llvm::CallInst>(op_ins))
                    continue;

                TravseStack.push_back(op_ins);
            }
        }
    }

    return false;
}

bool LockPair::ConfirmCount(llvm::Instruction *ins, std::set<llvm::Instruction *> LockProtectIns)
{   
    llvm::Value *store_var = ins->getOperand(0);   
    llvm::Instruction *store_ins = llvm::dyn_cast<llvm::Instruction>(store_var);

    if(!store_ins) 
        return false;

    auto Opcode = store_ins->getOpcode();
    std::vector<llvm::Instruction *> TravseStack;

    if(Opcode >= 13 && Opcode <= 16 &&                     \
            LockProtectIns.find(store_ins) != LockProtectIns.end()) {  //The enumeration value of operation instructions ranges from 13-30
        //llvm::outs()<<"find count op: "<<*store_ins<<"\n";
        return true;
    }

    TravseStack.push_back(store_ins);
    //llvm::outs()<<"store_ins: "<<*store_ins<<"\n";

    while(!TravseStack.empty()) {
        llvm::Instruction *travse_ins = TravseStack.front();
        auto it = TravseStack.begin();
        TravseStack.erase(it);
       //llvm::outs()<<"pop_ins: "<<*travse_ins<<"\n";

        for(auto opd = travse_ins->operands().begin(); opd != travse_ins->operands().end(); opd ++) {
            llvm::Value *op_var = llvm::dyn_cast<llvm::Value>(opd);
            if(llvm::Instruction *op_ins = llvm::dyn_cast<llvm::Instruction>(op_var)) {
                auto travse_op = op_ins->getOpcode();
                if(travse_op >= 13 && travse_op <= 16 &&                              \
                    LockProtectIns.find(travse_ins) != LockProtectIns.end()) {  //The enumeration value of operation instructions ranges from 13-30
                    //llvm::outs()<<"find count op: "<<*op_ins<<"\n";
                    return true;
                }

                //llvm::outs()<<"travse: "<<*op_ins<<"\n";
                if(llvm::dyn_cast<llvm::PHINode>(op_ins))
                    continue;
                if(llvm::dyn_cast<llvm::CallInst>(op_ins))
                    continue;

                TravseStack.push_back(op_ins);
            }
        }
    }

    return false;
}

void LockPair::printLockPairSet(std::vector<std::pair<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>,std::vector<llvm::BasicBlock*>>>  LockPairSet,llvm::Function* funcName,std::vector<std::string>* Resource){//What is passed in is recorded in a function
    std::string funcname=funcName->getName().str();
    for(auto SetIt = LockPairSet.begin();SetIt != LockPairSet.end();SetIt++){//What is taken here is one of the pairs in the vector, which is equivalent to a pair of locks and a passing BB.
        std::pair<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>,std::vector<llvm::BasicBlock*>> LockPairWithBB = *SetIt;//Take out a pair. Or a lock pair traversed by for and the passed BB.
        std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator> LockPair = LockPairWithBB.first;//Because it is a pair, first is the starting and ending point lock pair I want, and it is a pair.
        std::vector<llvm::BasicBlock*> BBroute = LockPairWithBB.second;//second is the set of passing BBs. It is a vector.
        llvm::Instruction * LockIns=&*LockPair.first;
        llvm::Instruction * UnlockIns=&*LockPair.second;
        llvm::BasicBlock * LockBB=LockIns->getParent();
        llvm::BasicBlock * UnlockBB=UnlockIns->getParent();
        std::set<llvm::Instruction*> LockProtectIns;
        for(llvm::BasicBlock::iterator lockbegin=LockPair.first;lockbegin!=LockBB->end();lockbegin++){//This for is used to handle the situation starting from lock in the first BB.
            if(lockbegin==LockPair.first){//LockPiar.first is a lock statement, so we skip it.
                continue;
            }
            if(lockbegin==LockPair.second){//If the unlock statement is found in the first BB, we break to end the loop.
                break;
            }
            llvm::Instruction *lockInst = &* lockbegin;
            //FindStoreAndCall(lockInst,funcName,Resource);  
            LockProtectIns.insert(lockInst); //This is equivalent to using LockPritectIns to collect all lock/unlock protection statements.        
        }
        if(BBroute.size() >2 ){
            for(auto BBit=++BBroute.begin();BBit!=BBroute.end();BBit++){//Used to handle ordinary BB between Lock and unlock.
                llvm::BasicBlock * BB=* BBit;
                auto endBBit = BBit;
                if(++endBBit==BBroute.end()){
                    break;
                }

                for(llvm::BasicBlock::iterator BBInst=BB->begin();BBInst != BB->end();BBInst++){
                    llvm::Instruction *lockInst = &* BBInst;
                    //FindStoreAndCall(lockInst,funcName,Resource);
                    LockProtectIns.insert(lockInst);
                }
            }
        }
                
        if(BBroute.size() > 1){  //Add a judgment here. When locking and unlocking are on the same BB, the first loop has been traversed and there is no need to traverse it again.
            for(llvm::BasicBlock::iterator unlockbegin=UnlockBB->begin();unlockbegin!=LockPair.second;unlockbegin++){
                llvm::Instruction *lockInst = &* unlockbegin;
                //FindStoreAndCall(lockInst,funcName,Resource);
                LockProtectIns.insert(lockInst);
                if(unlockbegin==LockPair.second){
                    break;
                }
            }
        }
        for(auto insit : LockProtectIns) {
            llvm::Instruction *ins = insit;
            FindStoreAndCall(ins,funcName,Resource,LockProtectIns);
        }
    }
}

bool LockPair::findway(std::vector<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>> testPairVector,std::vector<std::pair<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>,std::vector<llvm::BasicBlock*>>>  *LockPairSet, llvm::Function* findwayf){
    std::vector<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>> pairVector=testPairVector;
    for(std::vector<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>>::iterator pairstart=testPairVector.begin();pairstart!=testPairVector.end();pairstart++){
        std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator> testpair=*pairstart;
        llvm::Instruction * LockIns= &*testpair.first;//Take the locked instruction
        llvm::Instruction * UnlockIns=&*testpair.second;//Get the unlocking instructions
        llvm::BasicBlock * LockBB=LockIns->getParent();//Take out the locked BB
        llvm::BasicBlock * UnlockBB=UnlockIns->getParent();//Take the unlocked BB
        std::vector<llvm::BasicBlock*> registerBB;  //Record the vector of BB passed from lock to unlock.
        std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator> LockPair; //A pair consisting of the corresponding iterators for locking and unlocking.
        std::pair<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>,std::vector<llvm::BasicBlock*>> LockPairWithBB;
        llvm::Function::iterator LockIt;
        llvm::Function::iterator UnlockIt;
        std::set<llvm::BasicBlock *> LockDom;
        std::set<llvm::BasicBlock *> UnlockDom;
        std::set<llvm::BasicBlock *> LockUnlock;
        for(llvm::Function::iterator tempLockIt=findwayf->begin();tempLockIt!=findwayf->end();tempLockIt++){
            llvm::BasicBlock *tempBB = &*tempLockIt;
            if(tempBB == LockBB){
                LockIt = tempLockIt;
                break;
            }
        }
        for(llvm::Function::iterator tempUnlockIt=findwayf->begin();tempUnlockIt!=findwayf->end();tempUnlockIt++){
            llvm::BasicBlock *tempBB = &*tempUnlockIt;
            if(tempBB == UnlockBB){
                UnlockIt = tempUnlockIt;
                break;
            }
        }
        auto LockItTemp = LockIt;
        auto UnlockItTemp = UnlockIt;
        for(llvm::Function::iterator It = findwayf->begin();It!=findwayf->end();It++){ //Find the BB of the lock statement dominator in all parts of the function and insert it into the LockDom collection.
            if((&*It==LockBB)||(&*It==UnlockBB)){
                continue;
            }
            if((llvm::isPotentiallyReachable(LockBB,&*It))&&(llvm::isPotentiallyReachable(&*It, UnlockBB))){
                LockDom.insert(&*It);
            }
        }
        registerBB.push_back(LockBB);
        for(auto It: LockDom){
            llvm::BasicBlock * BB = It;
             registerBB.push_back(BB);
        }
        registerBB.push_back(UnlockBB);
        LockPair.first=testpair.first;
        LockPair.second=testpair.second;
        LockPairWithBB.first=LockPair;
        LockPairWithBB.second=registerBB;
        LockPairSet->push_back(LockPairWithBB);
    }
    return true;
}

void LockPair::TravseBB(llvm::BasicBlock* originbb,std::vector<llvm::BasicBlock::iterator> Stack,std::vector<llvm::BasicBlock*> registerBB,std::vector<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>>  *LockPairVector,llvm::Function *findwayf){//
        //What is stored here are the BB to be recursed, the stack of the recorder unlocking pair, registerBB of the record BBvector, and the pointer to the largest record set.
    int originBBID = getBBID(originbb);
    if(pred_empty(originbb)){//BB needs a precursor
        return;
    }
    llvm::pred_iterator SI(pred_begin(originbb)), SE(pred_end(originbb));//The function of this loop is not to traverse all the predecessors of a BB, but to find a pair of unlocks/locks. I can just take one of the paths here.
    for(;SE!=SI;SI++){
        llvm::BasicBlock *SB = llvm::dyn_cast<llvm::BasicBlock>(*SI);
        int SBID = getBBID(SB);
        if(SBID >= originBBID){
	//Because CFG is a directed cyclic graph, the ID of the current BB and the successor BB is used to make the judgment. 
	//If the ID of the successor BB is less than or equal to the ID of the current BB, which is equivalent to the starting point of the cycle, we will continue. Ignore this BB.
            continue;
        }
        std::vector<llvm::BasicBlock::iterator> StackTmp=Stack;
        for(llvm::BasicBlock::iterator bbInstIt = --SB->end(); bbInstIt != --SB->begin();bbInstIt--){//The instructions for traversing the precursor BB are also traversed backwards.
            llvm::Instruction *bbInst = &*bbInstIt;
            if(llvm::CallInst *callInst = llvm::dyn_cast<llvm::CallInst>(bbInst)){
                if(llvm::Function *called = callInst->getCalledFunction()){
                    std::string LockFuncName = called->getName().str();
                    if(spin_unlock.find(LockFuncName) != spin_unlock.end()){//If the unlock statement is found in the precursor BB, push it onto the stack.
                        std::vector<llvm::BasicBlock::iterator> StackTmpush = StackTmp;
                        StackTmpush.push_back(bbInstIt);
                        StackTmp=StackTmpush;
                    }
                    if(spin_lock.find(LockFuncName) != spin_lock.end()){//If a locking statement is found, pop it off the stack.
                        std::vector<llvm::BasicBlock::iterator> StackTmpop = StackTmp;
                        auto UnLockInstIt = StackTmpop.back();
                        StackTmpop.pop_back();
                        StackTmp=StackTmpop;
                        if(StackTmpop.empty()){
			//Because our stack inherits the state of the recursive front stack, the empty stack here means that a branch of the total unlock/lock pair has been found. It must be considered that different total unlock statements exist in different branches.
                            std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator> AllLockPair;
                            AllLockPair.first=bbInstIt;
                            AllLockPair.second=UnLockInstIt;
                            LockPairVector->push_back(AllLockPair);
                            return;
                        }
                    }
                }
            }
        }

        if(!pred_empty(SB)&&!StackTmp.empty()){//Going here means that the current BB traversal is completed and the stack is not empty.
            TravseBB(SB,StackTmp,registerBB,LockPairVector,findwayf);
        }
        return;
                
        if(pred_empty(SB)){
            return;
        }
    }
    return;
}

bool LockPair::LockPairMain(llvm::Function* F){
    std::string FuncName = F->getName().str();
    std::vector<std::string> Resource;
    std::vector<std::pair<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>,std::vector<llvm::BasicBlock*>>>  LockPairSet; //Save all lock/unlock pairs and passed BBs in the current function.
    std::vector<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>> LockPairVector;//Save all add-unlock pairs.
	if((spin_lock.find(FuncName) != spin_lock.end())||(spin_unlock.find(FuncName)!=spin_unlock.end())||(atomic_function.find(FuncName)!=atomic_function.end())||(percpu_function.find(FuncName)!=percpu_function.end())||(AllocFunctionNames.find(FuncName)!=AllocFunctionNames.end())){
		return false;
	}
	if((spin_lock.find(F->getName().str()) == spin_lock.end())&&(spin_unlock.find(F->getName().str()) == spin_unlock.end())&&(atomic_function.find(FuncName)==atomic_function.end())&&(percpu_function.find(FuncName)==percpu_function.end())&&(AllocFunctionNames.find(FuncName)==AllocFunctionNames.end())){
        for(llvm::Function::iterator BBIt = --F->end(); BBIt != --F->begin(); BBIt--){ //Traverse each BB backwards to find the unlocking statement
            std::vector<llvm::BasicBlock*> registerBB;  //Record the vector of BB passed from lock to unlock
            std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator> LockPair; //A pair consisting of the corresponding iterators for locking and unlocking
            std::pair<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>,std::vector<llvm::BasicBlock*>> LockPairWithBB; //A pair consisting of the above pair and vector
            llvm::BasicBlock *BB = &*BBIt;
            for(llvm::BasicBlock::iterator BInstIt = --BB->end(); BInstIt != --BB->begin();BInstIt--){ //Go backwards and look for the unlocking statement in each BB sentence by sentence.
                llvm::Instruction *BInst = &*BInstIt;
                if(llvm::CallInst *callInst = llvm::dyn_cast<llvm::CallInst>(BInst)){
                    if(llvm::Function *called = callInst->getCalledFunction()){
                        std::string LockFuncName = called->getName().str();
                        if(spin_unlock.find(LockFuncName) != spin_unlock.end()){  //Find an unlocking statement and start looking for the real locking statement of this unlocking statement. I don’t care about the locking/unlocking in between.
                            std::vector<llvm::BasicBlock::iterator> Stack;    //Corresponding to a lock-finding behavior, a stack is required, so a local stack is defined here. The life cycle is until the real unlocking statement of the current locking statement is found. What is stored here is the adding and unlocking statements encountered during the traversal process after the lock statement is found.  
                            Stack.push_back(BInstIt);//When an unlock statement is encountered, it is pushed onto the stack. This is equivalent to the total unlock in the total plus unlock pair we found.
                            auto NextIt=BInstIt;
                            for(llvm::BasicBlock::iterator NextUnLockInIt = --NextIt; NextUnLockInIt != --BB->begin(); NextUnLockInIt--){//This loop traverses all statements in this BB.
                                llvm::Instruction *BInstUnLock = &*NextUnLockInIt;
                                if(llvm::CallInst *CallInNextUnLock = llvm::dyn_cast<llvm::CallInst>(BInstUnLock)){
                                    if(llvm::Function *calledNextUnLock = CallInNextUnLock->getCalledFunction()){
                                        std::string NextUnLockFuncName = calledNextUnLock->getName().str();
                                        if(spin_unlock.find(NextUnLockFuncName) != spin_unlock.end()){//If you find that there is another unlock after the total unlock when scanning, just push it into the stack.
                                            Stack.push_back(NextUnLockInIt);
                                        }
                                        if(spin_lock.find(NextUnLockFuncName) != spin_lock.end()){//If the lock is found in this BB,
                                            auto UnLockInstIt = Stack.back();//What is taken is the top of the stack, which is the last element in the vector.
                                            Stack.pop_back();//Pop it out again. This pair of unlock and lock pops up, but it is not necessarily the total unlock statement we care about.               
                                            if(Stack.empty()){//If the stack is empty, it means that the total lock statement has been found.
                                                registerBB.push_back(BB);//Here, the BB where the total unlock is located is recorded in the previous BasicBlock vector.
                                                LockPair.first = NextUnLockInIt;//What is obtained is the total lock, which is the lock statement when we judge that the stack is empty.
                                                LockPair.second = UnLockInstIt;//Get total unlocks.
                                                LockPairWithBB.first = LockPair;
                                                LockPairWithBB.second = registerBB;//This is the BB recorded during the previous traversal of the pair.
                                                LockPairSet.push_back(LockPairWithBB);//This is the outermost large vector.
                                                break;//Jump out of the current total lock/unlock pair and traverse the next total unlock statement in BB.
                                            }   
                                        }
                                    }
                                }
                            }
                            if(!Stack.empty()){ //The current BB traversal is complete, but the stack is not empty, which means that the real locking function has not been found yet, so recurse BB to find it.
                                TravseBB(BB,Stack,registerBB,&LockPairVector,F);//Here the predecessor of BB is traversed recursively.
                                continue;
                            }
                        }
                    }

                }
            }
        }
		if(!LockPairVector.empty()){
            for(std::vector<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>>::iterator pairstarttest=LockPairVector.begin();pairstarttest!=LockPairVector.end();pairstarttest++){
                std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator> testpairtest=*pairstarttest;
                llvm::Instruction * LockInstest= &*testpairtest.first;//Take the locked instruction
                llvm::Instruction * UnlockInstest=&*testpairtest.second;//Get the unlocking instructions
                llvm::BasicBlock * LockBBtest=LockInstest->getParent();//Take out the locked BB
                llvm::BasicBlock * UnlockBBtest=UnlockInstest->getParent();//Take the unlocked BB
            }
            int t=findway(LockPairVector,&LockPairSet,F);
        }
        if(!LockPairSet.empty()){
            std::cout<<"The recursive search is completed and printing begins， Function name: "<<F->getName().str()<<std::endl;
            printLockPairSet(LockPairSet,F,&Resource);
            if(!Resource.empty()){
                FuncResource[F] = Resource;
            }
        }
	} 
    return true;   
}


bool LockPair::LockAtomicResource(llvm::Function* F){
    std::string FuncName = F->getName().str();
    std::vector<std::string> Resource;
    std::vector<std::string> NewResource;
    if(FuncResource.find(F) != FuncResource.end()){
        Resource = FuncResource[F];
    }

    if((spin_lock.find(FuncName) != spin_lock.end())||(spin_unlock.find(FuncName)!=spin_unlock.end())||(atomic_function.find(FuncName)!=atomic_function.end())||(percpu_function.find(FuncName)!=percpu_function.end())||(AllocFunctionNames.find(FuncName)!=AllocFunctionNames.end())){
				return false;
			}
	for(llvm::inst_iterator insb=inst_begin(F),inse=inst_end(F);insb!=inse;insb++) {
	    llvm::Instruction *instem = &*insb;
		if(llvm::CallInst *callInst = llvm::dyn_cast<llvm::CallInst>(instem)){
			if(llvm::Function * called = callInst->getCalledFunction()){
				llvm::StringRef FNameValue=called->getName();
				std::string mystr = FNameValue.str();
				if((spin_lock.find(mystr) != spin_lock.end())||(spin_unlock.find(mystr)!=spin_unlock.end())||(atomic_function.find(mystr)!=atomic_function.end())||(percpu_function.find(mystr)!=percpu_function.end())){//The modification has been completed here. For the case of atomic.xxxx, four judgments are used: 1. Name 2. Parameters
					for(auto arggb=callInst->arg_begin(),argge=callInst->arg_end();arggb!=argge;arggb++){
						if (llvm::GlobalValue* G = llvm::dyn_cast<llvm::GlobalValue>(arggb)){
                            std::string GB = "Global Variable:" + G->getName().str();
                            if(atomic_function.find(mystr)!=atomic_function.end() || percpu_function.find(mystr)!=percpu_function.end()){
                                if(!CheckGlobalVariable(G)) {
                                    NewResource.push_back(GB);
                                }
                            }//An if judgment is added here. If it is the content of atomic/per_cpu, we will insert it into the relevant Global Variable, and we will not insert the content of the lock class.
						}
					}

					std::string FunctionName = mystr;
					for(auto argggb=callInst->arg_begin(),arggge=callInst->arg_end();argggb!=arggge;argggb++){
						if(llvm::Value * argnamed=llvm::dyn_cast<llvm::Value>(argggb)){
							if(argnamed->getType()->isIntOrIntVectorTy()){
								continue;
							}
							if(llvm::Instruction *argInst = llvm::dyn_cast<llvm::Instruction>(argggb)){
								TravseAllocUser(F,argInst,&NewResource);
							}
						}
					}
					continue;

				}
			}
		}
	}

    if(!NewResource.empty()){
        Resource.insert(Resource.end(),NewResource.begin(),NewResource.end());
        FuncResource[F] = Resource;
    }

    return false;

}

void LockPair::MarkRlimitControlFunc(llvm::Function *F){
//The function of this function is to mark functions affected by rlimit. If rlimit is used to participate in comparison in a function, 
//we will mark the function as a function controlled by rlimit, and we will not consider all paths passing through this function.
    for(llvm::inst_iterator inst_it = inst_begin(F);inst_it != inst_end(F);inst_it++){
        llvm::Instruction *instem = &*inst_it;
        if(llvm::CallInst *callInst = llvm::dyn_cast<llvm::CallInst>(instem)){
            if(llvm::Function *called = callInst->getCalledFunction()){
                if(rlimit_func.find(called->getName().str()) != rlimit_func.end()){
                    llvm::Value *ReturnVar = llvm::dyn_cast<llvm::Value>(callInst);
                    if(!ReturnVar->user_empty()){
                        for(auto user_it = ReturnVar->user_begin();user_it != ReturnVar->user_end();user_it++){
                            llvm::Value *ReturnUser = llvm::dyn_cast<llvm::Value>(*user_it);
                            if(llvm::dyn_cast<llvm::ICmpInst>(*user_it)){
                                RlimitControlFunc.insert(F);
                            }else{
                                if(!ReturnUser->user_empty()){
                                    for(auto user_it = ReturnUser->user_begin();user_it != ReturnUser->user_end();user_it++){
                                        if(llvm::dyn_cast<llvm::ICmpInst>(*user_it)){
                                            RlimitControlFunc.insert(F);
                                        }
                                    }
                                }
                            }
                        }
                    }

                }
            }
        }

        if(llvm::ICmpInst *icmpinst = llvm::dyn_cast<llvm::ICmpInst>(instem)){
            int stopsignal = 0;
            TravseIcmpUser(icmpinst,F,&stopsignal);
        }
    }
}


void LockPair::TravseIcmpUser(llvm::Instruction *icmpinst,llvm::Function *F,int *stopsignal){
    if(*stopsignal == 1){
        return;
    }
    if(icmpinst->getOpcode() == llvm::Instruction::GetElementPtr){      //The first case corresponds to the GEP instruction, which means we can get the structure from here.
		llvm::GetElementPtrInst *gepinst = llvm::dyn_cast<llvm::GetElementPtrInst>(icmpinst);
        llvm::Value *GepOperand = gepinst->getOperand(0);
        std::string GepType = ReturnTypeRefine(*GepOperand->getType());
        if(GepType == "%struct.rlimit*"){
            RlimitControlFunc.insert(F);
            *stopsignal = 1;
            return;
        }
    }

    for (auto operand = icmpinst->operands().begin();operand != icmpinst->operands().end();++operand){  //If the corresponding statement is an ordinary statement, its operands are traversed normally to recurse.
		llvm::Value *opValue = llvm::dyn_cast<llvm::Value>(operand);
		if(llvm::Instruction *opInst = llvm::dyn_cast<llvm::Instruction>(opValue)){
            if(llvm::dyn_cast<llvm::PHINode>(opInst)){
                continue;
            }
            if(llvm::dyn_cast<llvm::CallInst>(opInst)){
                continue;
            }
            TravseIcmpUser(opInst,F,stopsignal);
        }
    }
    
    return; 
}


bool LockPair::StructHasNamespace(llvm::Type *Ty, std::string FuncName){//Recursively expand all fields under a sturct
    std::set<std::string> TravsedStruct;//Here is a record of the nested relationships in the structure. As long as I am convenient for this structure, I will not expand it.
    std::string StructName = ReturnTypeRefine(*Ty);
    int hasNamespace = 0;
    std::vector<llvm::Type*> TravseStack;

    if(Ty->isStructTy()){
        TravseStack.push_back(Ty);
    }
    if(Ty->isPointerTy()){
        llvm::Type *PointerTy = Ty->getPointerElementType();
        if(PointerTy->isStructTy()){
            TravseStack.push_back(PointerTy);
        }
    }
    if(Ty->isArrayTy()){
        llvm::Type * ArrayTy = Ty->getArrayElementType();
        if(ArrayTy->isStructTy()){
            TravseStack.push_back(ArrayTy);
        }
    }

    while(!TravseStack.empty()){
        llvm::Type *TravseTy = TravseStack.front();
        auto it = TravseStack.begin();
        TravseStack.erase(it);
        std::string TyName = ReturnTypeRefine(*TravseTy);
        if(TravsedStruct.find(TyName) != TravsedStruct.end() || TyName == "%struct.task_struct*"){
            continue;
        }else{
            if(Namespace.find(TyName) != Namespace.end()){
                Lockstream<<"FunctionName:"<<FuncName<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*Ty)<<"\n";
                std::cout<<"Found Namespace! "<<std::endl;
                return true;
            }
            TravsedStruct.insert(TyName);
            int ElementNum = TravseTy->getStructNumElements();
            for(int i = 0;i < ElementNum; i++){
                llvm::Type *RealmType = TravseTy->getStructElementType(i);
                if(RealmType->isStructTy()){
                    if(TravsedStruct.find(ReturnTypeRefine(*RealmType)) == TravsedStruct.end()){
                        TravseStack.push_back(RealmType);
                    }
                }
                if(RealmType->isPointerTy()){
                    llvm::Type *PointerTy = RealmType->getPointerElementType();
                    if(PointerTy->isStructTy()){
                        if(TravsedStruct.find(ReturnTypeRefine(*PointerTy)) == TravsedStruct.end()){
                            TravseStack.push_back(PointerTy);
                        }
                    }
                }
                if(RealmType->isArrayTy()){
                    llvm::Type *ArrayTy = RealmType->getPointerElementType();
                    if(ArrayTy->isStructTy()){
                        if(TravsedStruct.find(ReturnTypeRefine(*ArrayTy)) == TravsedStruct.end()){
                            TravseStack.push_back(ArrayTy);
                        }
                    }
                }
            }
        }

    }
     
    return false;
}


std::set<std::string> LockPair::TravseNamespace(llvm::Type *Ty){
    std::set<std::string> TravsedStruct;
    std::string StructName = ReturnTypeRefine(*Ty);
    int hasNamespace = 0;
    std::vector<llvm::Type*> TravseStack;
    
    if(Ty->isStructTy()){
        TravseStack.push_back(Ty);
    }
    if(Ty->isPointerTy()){
        llvm::Type *PointerTy = Ty->getPointerElementType();
        if(PointerTy->isStructTy()){
            TravseStack.push_back(PointerTy);
        }
    }
    if(Ty->isArrayTy()){
        llvm::Type * ArrayTy = Ty->getArrayElementType();
        if(ArrayTy->isStructTy()){
            TravseStack.push_back(ArrayTy);
        }
    }

    while(!TravseStack.empty()){
        llvm::Type *TravseTy = TravseStack.front();
        auto it = TravseStack.begin();
        TravseStack.erase(it);
        std::string TyName = ReturnTypeRefine(*TravseTy);
        if(TravsedStruct.find(TyName) != TravsedStruct.end() || TyName == "%struct.task_struct*" || TyName == "%struct.inode*" || TyName == "%struct.dentry*" || TyName == "%struct.device*" || TyName == "%struct.bio*" || TyName == "%struct.bdi_writeback*"){
            continue;
        }else{
            TravsedStruct.insert(TyName);
            int ElementNum = TravseTy->getStructNumElements();
            for(int i = 0;i < ElementNum; i++){
                llvm::Type *RealmType = TravseTy->getStructElementType(i);
                if(RealmType->isStructTy()){
                    if(TravsedStruct.find(ReturnTypeRefine(*RealmType)) == TravsedStruct.end()){
                        TravseStack.push_back(RealmType);
                    }
                }
                if(RealmType->isPointerTy()){
                    llvm::Type *PointerTy = RealmType->getPointerElementType();
                    if(PointerTy->isStructTy()){
                        if(TravsedStruct.find(ReturnTypeRefine(*PointerTy)) == TravsedStruct.end()){
                            TravseStack.push_back(PointerTy);
                        }
                    }
                }
                if(RealmType->isArrayTy()){
                    llvm::Type *ArrayTy = RealmType->getPointerElementType();
                    if(ArrayTy->isStructTy()){
                        if(TravsedStruct.find(ReturnTypeRefine(*ArrayTy)) == TravsedStruct.end()){
                            TravseStack.push_back(ArrayTy);
                        }
                    }
                }
            }
        }

    }
     
    return TravsedStruct;
}

bool LockPair::CheckGlobalVariable(llvm::GlobalValue *G) {
	std::string GB = G->getName().str();
	for (auto c : GB) {
		if (c == '.') {
			debugstream<<"Static Local Variable: "<<GB<<"\n";
			return true;
		}
	}	
	return false;
}

bool LockPair::IsInitFunction(llvm::Function* F) {
    std::string Section = F->getSection().str();
    if (Section == ".init.text") {
        debugstream<<"Init Function: "<<F->getName().str()<<"\n";
        return true;
    }
    return false;
}
