#include "LockPair.h"
#include "llvm/IR/InstIterator.h"

std::error_code ec1;
//llvm::raw_fd_ostream resultstream("./cmp-finder.txt", ec, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);
llvm::raw_fd_ostream Lockstream("./Lock-finder.txt", ec1, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);
//llvm::raw_fd_ostream Unlockstream("./Unlock-finder.txt", ec, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);

LockPair::LockPair(llvm::Module &module){
    _module_ = &module;
}


void LockPair::Test(){
    for(auto it = RlimitControlFunc.begin();it != RlimitControlFunc.end();it++){
        llvm::Function *F = *it;
        Lockstream<<F->getName().str()<<"\n";
    }
}

void LockPair::CollectLockAndAtomic(){//这里替代了我们之前从文件中getline的操作
    FindLockAndAtomic FLA = FindLockAndAtomic(*_module_);   //先设置一个实例化的对象。
    FLA.FindLock(); //这里设置所有的东西
    spin_lock = FLA.GetSpinLock();
    spin_unlock = FLA.GetSpinUnlock();
    mutex_lock = FLA.GetMutexLock();
    mutex_unlock = FLA.GetMutexUnlock();
    write_lock = FLA.GetWriteLock();
    write_unlock = FLA.GetWriteUnlock();
    percpu_function = FLA.GetPerCPU();
    atomic_function = FLA.GetAtomicFunc();
    AllocFunctionNames = FLA.GetAllocFunction();
    rlimit_func = FLA.GetRlimitFunction();
    spin_lock.insert(mutex_lock.begin(),mutex_lock.end());
	spin_lock.insert(write_lock.begin(),write_lock.end());
	spin_unlock.insert(mutex_unlock.begin(),mutex_unlock.end());
	spin_unlock.insert(write_unlock.begin(),write_unlock.end());
}

int LockPair::getBBID(llvm::BasicBlock* BB){//拿到一个BB的，返回BB的编号。
    std::string ts;
    std::string BBID;
    llvm::raw_string_ostream rso(ts);
    BB->printAsOperand(rso,false);//pinrAsOperand的作用是打出BB的编号％xx，写到rso stream里面
    BBID = rso.str();
    BBID = BBID.substr(1,BBID.length());//截掉第一个%
    //std::cout<<"BBID: "<<BBID<<std::endl;
    int BBIDint = atoi(BBID.c_str());//把xx的string转为int.
    //std::cout<<"BBIDint: "<<BBIDint<<std::endl;
    return BBIDint;
}

char* LockPair::GetActualFName(std::string &functionname){   //获得函数的名字即截取掉atomic.xxxx后面这串数字,此处functionname为被call的函数名.
	std::string mystr;
	char *FName = (char*)functionname.data();
	char *ActualFName = strtok(FName,".");
    functionname = ActualFName;//传进来的是个指针，在此处赋值，直接可以修改指针的对象，就是functionname
	while(ActualFName != NULL){
		ActualFName = strtok(NULL,".");//第二次截取，内容为atomic.xxxxx的xxxx.
		break;
	}
    if(functionname == "llvm"){
//        functionname = ActualFName;
    }
    return ActualFName;//此处ActualName实际是atomic后面的数字,functionname为真实的atomic
}

std::string LockPair::GetActualStructType(llvm::Instruction *gepInst,std::string funName){    //传入GEP Instruction和函数名（函数名主要用来调试），返回真实的结构体类型。
	std::string ActualStructType = "i8*";
	std::cout<<"-Functiaon Name-:"<<funName<<std::endl;
	for(auto operand = gepInst->operands().begin();operand != gepInst->operands().end();++operand){ //遍历Gep Instruction的operand
		if(llvm::CallInst *callInst = llvm::dyn_cast<llvm::CallInst>(operand)){              //如果该operand对应的是一句call语句
			if(llvm::Function *voidFunc = llvm::dyn_cast<llvm::Function>(callInst->getCalledOperand()->stripPointerCasts())){//通过这个方法获取call函数名是因为会遇到call bitcast这种case，直接用getName()会报错。
				std::cout<<"void Call to => " << voidFunc ->getName().str() << "\n";
				std::string ActualAllocFuncName  = voidFunc->getName().str();
				if(AllocFunctionNames.find(ActualAllocFuncName) != AllocFunctionNames.end()){ //判断call的是不是kmalloc函数
					llvm::Value *kmVar = llvm::dyn_cast<llvm::Value>(callInst);
					if(!kmVar->use_empty()){                                                //如果是kmalloc函数，找kmalloc的user
						for(llvm::Value::use_iterator UB=kmVar->use_begin(),UE=kmVar->use_end();UB!=UE;++UB){
							llvm::User* user=UB->getUser();                                    //一般情况下kmalloc的紧接的user中就有bitcast将kamalloc分配的i8*转换为真实的结构体。所以我们只需要在第一次user里找bitcast语句就行
							if(llvm::Instruction* userInst = llvm::dyn_cast<llvm::Instruction>(user)){      
								if(userInst->getOpcode() == llvm::Instruction::BitCast){        //找出bitcast语句
									llvm::Value *userVar = llvm::dyn_cast<llvm::Value>(userInst);
									llvm::Type *userType = userVar->getType();                  //bitcast语句对应的Value的TypeName就是要找的真实结构体。
									std::cout<<"User Type:"<<ReturnTypeRefine(*userType)<<std::endl; 
									ActualStructType = ReturnTypeRefine(*userType);//这里获取的是处理之后inode.xxxx中的inode struct name.
								}
							}
						}
					}
				}
			}
		}
		if(llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(operand)){   //调试用，调试gep指令和call kmalloc函数之间还会不会有别的语句。
			if(Inst->getOpcode() == llvm::Instruction::BitCast){
				std::cout<<"Has a BitCast Middle"<<std::endl;
			}
		}
	}

	return ActualStructType;
}

void LockPair::id_phi_inst(llvm::Function* funcname,llvm::Instruction* I,std::vector<std::string>* Resource){
    std::string FuncName=funcname->getName().str();
	llvm::PHINode *PN= llvm::cast<llvm::PHINode>(I);
	bool found=false;
	for(u_int j=0;j<PN->getNumIncomingValues();j++){
		llvm::Value* V=PN->getIncomingValue(j);
		if(!V)continue;
		if(llvm::GetElementPtrInst* GEP=llvm::dyn_cast<llvm::GetElementPtrInst>(V)){
            if(GEP->getOperand(0)->hasName()){
                //resultstream<<"FunctionName:"<<FuncName<<","<<"Global Variable:"<<GEP->getOperand(0)->getName().str()<<'\n';
                std::string GB = "Global Variable:" + GEP->getOperand(0)->getName().str();
                Resource->push_back(GB);
            }
            llvm::Type *structType = GEP->getSourceElementType();//此处获取GEP指令的struct
			if(ReturnTypeRefine(*structType) == "i8*"){            //如果GEP指令中的结构体是i8*,需要特殊处理以下来找出真实的结构体。
				std::string ActualStructType;
				ActualStructType = GetActualStructType(GEP,FuncName);     //调用GetActualStructType来获得真实结构体。
				//resultstream<<"FunctionName:"<<FuncName<<","<<"ProtectedStruct:"<<ActualStructType<<'\n';
                std::string PS = "ProtectedStruct:" + ActualStructType;
                Resource->push_back(PS);
			}else{
				//resultstream<<"FunctionName:"<<FuncName<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*structType)<<'\n'; 
                std::string PS = "ProtectedStruct:" + ReturnTypeRefine(*structType);
                Resource->push_back(PS);  
			}
            return;             
		}
		if(llvm::LoadInst* LI=llvm::dyn_cast<llvm::LoadInst>(V)){
            llvm::Value* loadValue=LI->getOperand(0);
            if(loadValue->hasName()){
                //resultstream<<"FunctionName:"<<FuncName<<","<<"Global Variable:"<<loadValue->getName().str()<<'\n';
                std::string GB = "Global Variable:" + loadValue->getName().str();
                Resource->push_back(GB);
            } else {
                if(llvm::Instruction *loadVInst=llvm::dyn_cast<llvm::Instruction>(loadValue)){
                    TravseAllocUser(funcname,loadVInst,Resource);
                }
            }
		}
    }
	return;
}

void LockPair::TravseAllocUser(llvm::Function* func,llvm::Instruction* originv,std::vector<std::string>* Resource){   //传进来的Instruction是函数参数Value对应的Instruction，有可能是Call语句，Gep指令或者Phi指令，load指令，这里对Call,Gep,load语句作特殊处理
    std::string testfuncName=func->getName().str();
	if(originv->getOpcode() == llvm::Instruction::GetElementPtr){      //第一种情况，对应的是GEP指令，说明我们可以从这获取结构体了。
		llvm::GetElementPtrInst *gepinst = llvm::dyn_cast<llvm::GetElementPtrInst>(originv);
        llvm::Value *GepOperand = gepinst->getOperand(0);
        if(const llvm::GlobalValue* G=llvm::dyn_cast<llvm::GlobalValue>(GepOperand)){
            //resultstream<<"FunctionName:"<<testfuncName<<","<<"Global Variable:"<<GepOperand->getName().str()<<'\n';
            std::string GB = "Global Variable:" + GepOperand->getName().str();
            Resource->push_back(GB);
            return;
        }
		llvm::Type *structType = gepinst->getSourceElementType();//此处获取GEP指令的struct
		if(ReturnTypeRefine(*structType) == "i8*"){            //如果GEP指令中的结构体是i8*,需要特殊处理以下来找出真实的结构体。
			std::string ActualStructType;
			ActualStructType = GetActualStructType(originv,testfuncName);     //调用GetActualStructType来获得真实结构体。
			//resultstream<<"FunctionName:"<<testfuncName<<","<<"ProtectedStruct:"<<ActualStructType<<'\n';
            std::string PS = "ProtectedStruct:" + ActualStructType;
            Resource->push_back(PS);
		}else{
			//resultstream<<"FunctionName:"<<testfuncName<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*structType)<<'\n';
            std::string PS = "ProtectedStruct:" + ReturnTypeRefine(*structType);
            Resource->push_back(PS);   
		}        
		return ;
	}
    if(llvm::LoadInst * loadIns=llvm::dyn_cast<llvm::LoadInst>(originv)){
        llvm::Value* loadValue=loadIns->getOperand(0);
        if(const llvm::GlobalValue* G=llvm::dyn_cast<llvm::GlobalValue>(loadValue)){
            //resultstream<<"FunctionName:"<<testfuncName<<","<<"Global Variable:"<<loadValue->getName().str()<<'\n';
            std::string GB = "Global Variable:" + loadValue->getName().str();
            Resource->push_back(GB);
            return;
        } else {
            if(llvm::Instruction *loadVInst=llvm::dyn_cast<llvm::Instruction>(loadValue)){
                TravseAllocUser(func,loadVInst,Resource);
            }
        }
    }

	if(llvm::CallInst *callInst = llvm::dyn_cast<llvm::CallInst>(originv)){              //如果对应的是 callInst，那么说明这个Value就是call的结果，我这里直接把call函数的返回值类型打出来？
        if(llvm::Function *called = callInst->getCalledFunction()){
            std::string CalledName = called->getName().str();
            if(AllocFunctionNames.find(CalledName) != AllocFunctionNames.end()){
                llvm::Value *kmVar = llvm::dyn_cast<llvm::Value>(callInst);
				if(!kmVar->use_empty()){                                                //如果是kmalloc函数，找kmalloc的user
					for(llvm::Value::use_iterator UB=kmVar->use_begin(),UE=kmVar->use_end();UB!=UE;++UB){
						llvm::User* user=UB->getUser();                                    //一般情况下kmalloc的紧接的user中就有bitcast将kamalloc分配的i8*转换为真实的结构体。所以我们只需要在第一次user里找bitcast语句就行
						if(llvm::Instruction* userInst = llvm::dyn_cast<llvm::Instruction>(user)){      
							if(userInst->getOpcode() == llvm::Instruction::BitCast){        //找出bitcast语句
								llvm::Value *userVar = llvm::dyn_cast<llvm::Value>(userInst);
								llvm::Type *userType = userVar->getType();                  
								//resultstream<<"FunctionName:"<<testfuncName<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*userType)<<"\n";
                                std::string PS = "ProtectedStruct:" + ReturnTypeRefine(*userType);
                                Resource->push_back(PS);
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
                //resultstream<<"FunctionName:"<<testfuncName<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*tp)<<"\n";
                std::string PS = "ProtectedStruct:" + ReturnTypeRefine(*tp);
                Resource->push_back(PS);
            }
        }
        return;
	} 
    if(llvm::PHINode *testphi=llvm::dyn_cast<llvm::PHINode>(originv)){
        id_phi_inst(func,originv,Resource);
    }

	for (auto operand = originv->operands().begin();operand != originv->operands().end();++operand){  //如果对应的是普通语句，则正常遍历他的operands来递归。
		llvm::Value *opValue = llvm::dyn_cast<llvm::Value>(operand);
		if(llvm::Instruction *opInst = llvm::dyn_cast<llvm::Instruction>(opValue)){//这一句用来判断是F函数的参数，经过调试知道，如果这个Value不能强制转换为Instruction了，那么代表它是函数参数了。
			for(auto travop = opInst->operands().begin();travop!=opInst->operands().end();++travop){
				if(llvm::Instruction *travopIns=llvm::dyn_cast<llvm::Instruction>(travop))
				{
					if(travopIns==originv){
						return;
					}
				}
			}
            return;
//			TravseAllocUser(func,opInst);				//如果还能转成Instruction，继续递归
		}else{
			llvm::Type *StructType = opValue->getType();
            std::string type_str;
            llvm::raw_string_ostream rso(type_str);
            StructType->print(rso);
			//resultstream<<"FunctionName:"<<testfuncName<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*StructType)<<'\n';//如果是函数参数，打印该结构体。
            std::string PS = "ProtectedStruct:" + ReturnTypeRefine(*StructType);
            Resource->push_back(PS);
			continue ;
		}
	}
		return;
}

void LockPair::FindStoreAndCall(llvm::Instruction *Inst,llvm::Function* funcName,std::vector<std::string> *Resource){
    std::string funcname=funcName->getName().str();
    if(llvm::CallInst *TestcallInst=llvm::dyn_cast<llvm::CallInst>(Inst)){//如果在lock之后抓住了call
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

            for(auto arglb=TestcallInst->arg_begin(),argle=TestcallInst->arg_end();arglb!=argle;arglb++){
                if(llvm::Value *argValue = llvm::dyn_cast<llvm::Value>(arglb)){
                    if(argValue->getType()->isPointerTy()){
                        for(auto argF=funcName->arg_begin();argF!=funcName->arg_end();argF++){//这个判断的作用是，如果arg为Ｆ本身的arg,那么就是对Ｆ传进来的参数做了操作，我们就应该直接打印参数类型。
                            if(llvm::Value * argnamed=llvm::dyn_cast<llvm::Value>(argF)){  
                                if(argnamed==argValue){
                                    if(const llvm::GlobalValue* AtomicG=llvm::dyn_cast<llvm::GlobalValue>(argF)){//如果参数有名字，说明是个全局变量，打印参数名字。
                                        if(!llvm::dyn_cast<llvm::Function>(argValue)){
                                            //resultstream<<"FunctionName:"<<funcname<<","<<"Global Variable:"<<argF->getName().str()<<"\n";
                                            std::string GB = "Global Variable:"+argF->getName().str();
                                            Resource->push_back(GB);
                                        }
                                    }
                                    llvm::Type* argtype= argValue->getType();//如果函数参数没名字，说明是个局部变量参数，那我这里就把参数类型打印出来就好。
                                    //resultstream<<"FunctionName:"<<funcname<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*argtype)<<"\n";
                                    std::string PS = "ProtectedStruct:"+ReturnTypeRefine(*argtype);
                                    Resource->push_back(PS);
    						    }
    					    }
                        }
                        if(const llvm::GlobalValue* AtomicG=llvm::dyn_cast<llvm::GlobalValue>(arglb)){//如果call语句的参数为全局变量，直接打印。
                            if(!llvm::dyn_cast<llvm::Function>(argValue)){
                                //resultstream<<"FunctionName:"<<funcname<<","<<"Global Variable:"<<AtomicG->getName().str()<<"\n";
                                std::string GB = "Global Variable:"+AtomicG->getName().str();
                                Resource->push_back(GB);
                            }    
                        }
    			        if(llvm::Instruction *argInst=llvm::dyn_cast<llvm::Instruction>(arglb)){//如果既不是全局变量，又不是Ｆ函数的参数，本身又不是个结构体，那么需要分析内部call的参数来源。
//    			                  resultstream<<"testforTravseAllocUser"<<"\n";
                            TravseAllocUser(funcName,argInst,Resource);//这里是传入lock函数参数的Instruction和F的名字，因为你如果在lock/unlock中间调用了call语句，那么要不是call的参数产生了变化，要不就是call的返回值存储到了哪里。
                        }                                    //所以这里对应抓到call参数，遍历call语句的每个参数，找到参数对应的资源（如果是结构体，打印结构体来源，如果是变量，需要去看变量来自哪里，如果来自函数参数，打印变量，如果是全局变量，打印变量名)													
			        }
                }
    		}
        }

        if(llvm::Value * callreturnValue=llvm::dyn_cast<llvm::Value>(TestcallInst)){
            if(!callreturnValue->user_empty()){  //少加了！号
                for(auto calluser=callreturnValue->user_begin();calluser!=callreturnValue->user_end();calluser++){
                    if(llvm::StoreInst * returnStore=llvm::dyn_cast<llvm::StoreInst>(*calluser)){
                        if(llvm::Value * returnStoredetination= returnStore->getOperand(1)){
                            if(const llvm::GlobalValue* G=llvm::dyn_cast<llvm::GlobalValue>(returnStoredetination)){
                                if(!llvm::dyn_cast<llvm::Function>(returnStoredetination)){
                                    //resultstream<<"FunctionName:"<<funcname<<","<<"Global Variable:"<<returnStoredetination->getName().str()<<"\n";
                                    std::string GB = "Global Variable:"+returnStoredetination->getName().str();
                                    Resource->push_back(GB);
                                }
                            }
                            if(llvm::GetElementPtrInst * retstodesInst=llvm::dyn_cast<llvm::GetElementPtrInst>(returnStoredetination)){
                                llvm::Type* retgeptype= retstodesInst->getSourceElementType();
                                //resultstream<<"FunctionName:"<<funcname<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*retgeptype)<<"\n";
                                std::string PS = "ProtectedStruct:"+ReturnTypeRefine(*retgeptype);
                                Resource->push_back(PS);
                            }
                        }
                    }
                }
            }
        }
    }

    if(llvm::StoreInst * teststoreInst=llvm::dyn_cast<llvm::StoreInst>(Inst)){//如果在lock之后抓到了store
        llvm::Value * storeValuet= teststoreInst->getOperand(1);
        if(const llvm::GlobalValue* G=llvm::dyn_cast<llvm::GlobalValue>(storeValuet)){//如果store的对象有名字，这是个全局变量，我们直接打印。
            if(!llvm::dyn_cast<llvm::Function>(storeValuet)){
                //resultstream<<"FunctionName:"<<funcname<<","<<"Global Variable:"<<storeValuet->getName().str()<<"\n";
                std::string GB = "Global Variable:"+storeValuet->getName().str();
                Resource->push_back(GB); 
            }
        }
        if(llvm::Instruction * storeInstruction = llvm::dyn_cast<llvm::Instruction>(storeValuet)){//如果store的不是个全局变量，我们要用GEP看这个变量来自哪里
            if(llvm::GetElementPtrInst * storegep= llvm::dyn_cast<llvm::GetElementPtrInst>(storeInstruction)){
                llvm::Type* gepType= storegep->getSourceElementType();//如果这个变量是结构体成员，我们打印结构体类型。这里的处理是有缺陷的，如果store到GEP之间经过了别的语句，我这里就处理不出来了。
                //resultstream<<"FunctionName:"<<funcname<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*gepType)<<"\n";
                std::string PS = "ProtectedStruct:"+ReturnTypeRefine(*gepType);
                Resource->push_back(PS);
            }
        }
    }

}

void LockPair::printLockPairSet(std::vector<std::pair<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>,std::vector<llvm::BasicBlock*>>>  LockPairSet,llvm::Function* funcName,std::vector<std::string>* Resource){//传进来的就是记录一个函数中
        //所有加锁解锁对的集合。就是最长的vector.
    std::string funcname=funcName->getName().str();
    for(auto SetIt = LockPairSet.begin();SetIt != LockPairSet.end();SetIt++){//这里取的是vector中的其中一个pair,相当于一对锁和经过的BB
        std::pair<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>,std::vector<llvm::BasicBlock*>> LockPairWithBB = *SetIt;//取出一对pair.还是for遍历的一个锁对和经过的BB
        std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator> LockPair = LockPairWithBB.first;//因为是个pair,first就是我要的起点终点锁对，是个pair.
        std::vector<llvm::BasicBlock*> BBroute = LockPairWithBB.second;//second就是经过的BB的集合。是个vector
        llvm::Instruction * LockIns=&*LockPair.first;
        llvm::Instruction * UnlockIns=&*LockPair.second;
        llvm::BasicBlock * LockBB=LockIns->getParent();
        llvm::BasicBlock * UnlockBB=UnlockIns->getParent();
        for(llvm::BasicBlock::iterator lockbegin=LockPair.first;lockbegin!=LockBB->end();lockbegin++){//这个for用于处理第一个BB中从lock出发的情况
            if(lockbegin==LockPair.first){//LockPiar.first为lock语句，所以我们跳过。
                continue;
            }
            if(lockbegin==LockPair.second){//如果在第一个BB里面找到了unlock语句，我们就break结束循环。
                break;
            }
            llvm::Instruction *lockInst = &* lockbegin;
            FindStoreAndCall(lockInst,funcName,Resource);                  
        }
        if(BBroute.size() >2 ){
            for(auto BBit=++BBroute.begin();BBit!=BBroute.end();BBit++){//用于处理Lock和unlock之间的普通BB。
                llvm::BasicBlock * BB=* BBit;
                auto endBBit = BBit;
                if(++endBBit==BBroute.end()){
                    break;
                }
                //std::cout<<"iterate BBID: "<<getBBID(BB)<<std::endl;
                for(llvm::BasicBlock::iterator BBInst=BB->begin();BBInst != BB->end();BBInst++){
                    llvm::Instruction *lockInst = &* BBInst;
                    FindStoreAndCall(lockInst,funcName,Resource);
                }
            }
        }
                
        if(BBroute.size() > 1){  //这里加一个判断，加锁和解锁在同一个BB的情况，第一个循环已经遍历完了，这里不需要再遍历了。
            for(llvm::BasicBlock::iterator unlockbegin=UnlockBB->begin();unlockbegin!=LockPair.second;unlockbegin++){
                llvm::Instruction *lockInst = &* unlockbegin;
                FindStoreAndCall(lockInst,funcName,Resource);
                if(unlockbegin==LockPair.second){
                    break;
                }
            }
        }
    }
}

bool LockPair::findway(std::vector<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>> testPairVector,std::vector<std::pair<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>,std::vector<llvm::BasicBlock*>>>  *LockPairSet, llvm::Function* findwayf){
    std::vector<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>> pairVector=testPairVector;
    for(std::vector<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>>::iterator pairstart=testPairVector.begin();pairstart!=testPairVector.end();pairstart++){
        std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator> testpair=*pairstart;
        llvm::Instruction * LockIns= &*testpair.first;//取一下上锁的instruction
        llvm::Instruction * UnlockIns=&*testpair.second;//取一下解锁的instruction
        llvm::BasicBlock * LockBB=LockIns->getParent();//取一下上锁的BB
        llvm::BasicBlock * UnlockBB=UnlockIns->getParent();//取一下解锁的BB
        std::vector<llvm::BasicBlock*> registerBB;  //纪录加锁到解锁所经过的BB的vector
        std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator> LockPair; //加锁和解锁对应的iterator组成的pair
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
        for(llvm::Function::iterator It = findwayf->begin();It!=findwayf->end();It++){ //在函数的所有部分找加锁语句dominator的BB,并插入到LockDom集合中
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
        //这里存进来的有要递归的BB,记录家解锁对的stack,以及记录BBvector的registerBB,还有最大的记录集合的指针。
    int originBBID = getBBID(originbb);
    if(pred_empty(originbb)){//BB要有前驱
        return;
    }
    llvm::pred_iterator SI(pred_begin(originbb)), SE(pred_end(originbb));//这个循环的作用现在不是遍历一个BB所有的前驱，是找到一对成对的解锁/锁，我在这里只取其中的一个路径就好了
    for(;SE!=SI;SI++){
        llvm::BasicBlock *SB = llvm::dyn_cast<llvm::BasicBlock>(*SI);
        int SBID = getBBID(SB);
        if(SBID >= originBBID){//因为CFG是一个有向有环图，所以用当前BB和后继BB的ID做判断，如果后继BB的ID小于等于当前BB的ID，相当于循环的起始点，我们就continue.不管这个BB了。
            continue;
        }
        std::vector<llvm::BasicBlock::iterator> StackTmp=Stack;
        for(llvm::BasicBlock::iterator bbInstIt = --SB->end(); bbInstIt != --SB->begin();bbInstIt--){//遍历前驱BB的instruction，同样是倒着遍历。
            llvm::Instruction *bbInst = &*bbInstIt;
            if(llvm::CallInst *callInst = llvm::dyn_cast<llvm::CallInst>(bbInst)){
                if(llvm::Function *called = callInst->getCalledFunction()){
                    std::string LockFuncName = called->getName().str();
                    if(spin_unlock.find(LockFuncName) != spin_unlock.end()){//如果在前驱BB中找到了解锁语句，入栈
                        std::vector<llvm::BasicBlock::iterator> StackTmpush = StackTmp;
                        StackTmpush.push_back(bbInstIt);
                        StackTmp=StackTmpush;
                    }
                    if(spin_lock.find(LockFuncName) != spin_lock.end()){//如果找到了上锁语句，出栈
                        std::vector<llvm::BasicBlock::iterator> StackTmpop = StackTmp;
                        auto UnLockInstIt = StackTmpop.back();
                        StackTmpop.pop_back();
                        StackTmp=StackTmpop;
                        if(StackTmpop.empty()){//因为我们的栈是继承递归前栈状态的，所以这里栈空，就能够说明找到了总解锁/加锁对的一个分支，这里必须要考虑不同分支存在不同的总解锁语句。
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

        if(!pred_empty(SB)&&!StackTmp.empty()){//走到这里说明当前ＢＢ遍历完成且栈不空
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
    std::vector<std::pair<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>,std::vector<llvm::BasicBlock*>>>  LockPairSet; //保存当前函数中所有加锁/解锁对以及经过的BB,
    std::vector<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>> LockPairVector;//保存所有加解锁对
	if((spin_lock.find(FuncName) != spin_lock.end())||(spin_unlock.find(FuncName)!=spin_unlock.end())||(atomic_function.find(FuncName)!=atomic_function.end())||(percpu_function.find(FuncName)!=percpu_function.end())||(AllocFunctionNames.find(FuncName)!=AllocFunctionNames.end())){
		return false;
	}
	if((spin_lock.find(F->getName().str()) == spin_lock.end())&&(spin_unlock.find(F->getName().str()) == spin_unlock.end())&&(atomic_function.find(FuncName)==atomic_function.end())&&(percpu_function.find(FuncName)==percpu_function.end())&&(AllocFunctionNames.find(FuncName)==AllocFunctionNames.end())){
        for(llvm::Function::iterator BBIt = --F->end(); BBIt != --F->begin(); BBIt--){ //倒着遍历每个BB找解锁语句
            std::vector<llvm::BasicBlock*> registerBB;  //纪录加锁到解锁所经过的BB的vector
            std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator> LockPair; //加锁和解锁对应的iterator组成的pair
            std::pair<std::pair<llvm::BasicBlock::iterator,llvm::BasicBlock::iterator>,std::vector<llvm::BasicBlock*>> LockPairWithBB; //由上述pair和vector组成的pair
            llvm::BasicBlock *BB = &*BBIt;
            for(llvm::BasicBlock::iterator BInstIt = --BB->end(); BInstIt != --BB->begin();BInstIt--){ //倒着在每个BB中逐句寻找解锁语句。
                llvm::Instruction *BInst = &*BInstIt;
                if(llvm::CallInst *callInst = llvm::dyn_cast<llvm::CallInst>(BInst)){
                    if(llvm::Function *called = callInst->getCalledFunction()){
                        std::string LockFuncName = called->getName().str();
                        if(spin_unlock.find(LockFuncName) != spin_unlock.end()){  //找到一个解锁语句，开始找这个解锁语句的真实加锁语句。中间经过的加锁/解锁我不关心
                            std::vector<llvm::BasicBlock::iterator> Stack;    //对应一次找锁行为需要一个栈，所以在此定义一个局部栈，生命周期为找到当前加锁语句的真实解锁语句为止。这里存的是找到锁语句之后，遍历过程中遇到的加解锁语句。  
                            Stack.push_back(BInstIt);//遇到了解锁语句，入栈，这里相当于是我们找到的总加解锁对中的总解锁。
                            auto NextIt=BInstIt;
                            for(llvm::BasicBlock::iterator NextUnLockInIt = --NextIt; NextUnLockInIt != --BB->begin(); NextUnLockInIt--){//这个循环是本BB中的所有语句遍历。
                                llvm::Instruction *BInstUnLock = &*NextUnLockInIt;
                                if(llvm::CallInst *CallInNextUnLock = llvm::dyn_cast<llvm::CallInst>(BInstUnLock)){
                                    if(llvm::Function *calledNextUnLock = CallInNextUnLock->getCalledFunction()){
                                        std::string NextUnLockFuncName = calledNextUnLock->getName().str();
                                        if(spin_unlock.find(NextUnLockFuncName) != spin_unlock.end()){//如果在扫的时候发现总解锁之后还有解锁，就把它入栈
                                            Stack.push_back(NextUnLockInIt);
                                        }
                                        if(spin_lock.find(NextUnLockFuncName) != spin_lock.end()){//如果在本BB中找到了lock,
                                            auto UnLockInstIt = Stack.back();//取的是栈中的栈顶，就是vector中的最后一个元素
                                            Stack.pop_back();//再把它pop出来。这一对解锁和锁就弹出了，但是不一定是我们关心的总解锁语句。                
                                            if(Stack.empty()){//如果栈空，说明找到了总加锁锁语句。
                                                registerBB.push_back(BB);//这里把总解锁所在的ＢＢ记录到之前的BasicBlock vector中
                                                LockPair.first = NextUnLockInIt;//获取的是总加锁,就是我们判断栈空时候的上锁语句
                                                LockPair.second = UnLockInstIt;//获取总解锁。
                                                LockPairWithBB.first = LockPair;
                                                LockPairWithBB.second = registerBB;//这是之前遍历pair过程中记录的经过的BB
                                                LockPairSet.push_back(LockPairWithBB);//这是最外层的大vector.
                                                break;//跳出当前的总加锁/解锁对，遍历BB中的下一个总解锁语句。
                                            }   
                                        }
                                    }
                                }
                            }
                            if(!Stack.empty()){ //当前BB遍历完了，但是栈非空，说明还没有找到真实的加锁函数，递归BB去寻找。
                                TravseBB(BB,Stack,registerBB,&LockPairVector,F);//这里递归遍历BB的前驱.
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
                llvm::Instruction * LockInstest= &*testpairtest.first;//取一下上锁的instruction
                llvm::Instruction * UnlockInstest=&*testpairtest.second;//取一下解锁的instruction
                llvm::BasicBlock * LockBBtest=LockInstest->getParent();//取一下上锁的BB
                llvm::BasicBlock * UnlockBBtest=UnlockInstest->getParent();//取一下解锁的BB  
            }
            int t=findway(LockPairVector,&LockPairSet,F);
        }
        if(!LockPairSet.empty()){
            std::cout<<"递归查找完成，开始打印，Function name"<<F->getName().str()<<std::endl;
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
				if((spin_lock.find(mystr) != spin_lock.end())||(spin_unlock.find(mystr)!=spin_unlock.end())||(atomic_function.find(mystr)!=atomic_function.end())||(percpu_function.find(mystr)!=percpu_function.end())){//此处已修改完成，针对atomic.xxxx的情况，使用了四个判断1.名字2.参数
					for(auto arggb=callInst->arg_begin(),argge=callInst->arg_end();arggb!=argge;arggb++){
						if (const llvm::GlobalValue* G = llvm::dyn_cast<llvm::GlobalValue>(arggb)){//这个地方可能遇到别名分析?假设全局传给临时？临时传给atomic?
                            std::string GB = "Global Variable:" + G->getName().str();
                            NewResource.push_back(GB);
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
                                //std::cout<<" ICMP: "<<F->getName().str()<<std::endl;
                                RlimitControlFunc.insert(F);
                            }else{
                                if(!ReturnUser->user_empty()){
                                    for(auto user_it = ReturnUser->user_begin();user_it != ReturnUser->user_end();user_it++){
                                        if(llvm::dyn_cast<llvm::ICmpInst>(*user_it)){
                                            //std::cout<<"two layer ICMP: "<<F->getName().str()<<std::endl;
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
    if(icmpinst->getOpcode() == llvm::Instruction::GetElementPtr){      //第一种情况，对应的是GEP指令，说明我们可以从这获取结构体了。
		llvm::GetElementPtrInst *gepinst = llvm::dyn_cast<llvm::GetElementPtrInst>(icmpinst);
        llvm::Value *GepOperand = gepinst->getOperand(0);
        std::string GepType = ReturnTypeRefine(*GepOperand->getType());
        //std::cout<<"GetType: "<<GepType<<std::endl;
        if(GepType == "%struct.rlimit*"){
            RlimitControlFunc.insert(F);
            *stopsignal = 1;
            return;
        }
    }

    for (auto operand = icmpinst->operands().begin();operand != icmpinst->operands().end();++operand){  //如果对应的是普通语句，则正常遍历他的operands来递归。
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