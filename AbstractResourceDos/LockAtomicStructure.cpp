#include "LockAtomicStructure.h"
#include "llvm/IR/InstIterator.h"
std::error_code errorcode;
llvm::raw_fd_ostream fstream("./LockAndAtomicStructure-finder.txt", errorcode, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);


LockAtomicStructure::LockAtomicStructure(llvm::Module &module){
    _module_ = &module;
}

void LockAtomicStructure::CollectLockAndAtomic(){//这里替代了我们之前从文件中getline的操作
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
	spin_lock.insert(mutex_lock.begin(),mutex_lock.end());
	spin_lock.insert(write_lock.begin(),write_lock.end());
	spin_unlock.insert(mutex_unlock.begin(),mutex_unlock.end());
	spin_unlock.insert(write_unlock.begin(),write_unlock.end());
}
/*
void LockAtomicStructure::TravseAllocUser(std::string funcName,llvm::Instruction* originv){   //传进来的Instruction是spin_lock函数参数Value对应的Instruction，有可能是Call语句，Gep指令或者Phi指令或其他，这里对Call,Gep,Phi语句作特殊处理
	if(originv->getOpcode() == llvm::Instruction::GetElementPtr){      //第一种情况，对应的是GEP指令，说明我们可以从这获取结构体了。
		llvm::GetElementPtrInst *gepinst = llvm::dyn_cast<llvm::GetElementPtrInst>(originv);
		llvm::Type *structType = gepinst->getSourceElementType();//此处获取GEP指令的struct
		if(ReturnTypeRefine(*structType) == "i8*"){            //如果GEP指令中的结构体是i8*,需要特殊处理以下来找出真实的结构体。
			std::string ActualStructType;
			ActualStructType = GetActualStructType(originv,funcName);     //调用GetActualStructType来获得真实结构体。
			fstream<<"FunctionName:"<<funcName<<","<<"ProtectedStruct:"<<ActualStructType<<'\n';
		}else{
			fstream<<"FunctionName:"<<funcName<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*structType)<<'\n';   
		}        
		return ;
	}

	if(llvm::CallInst *callInst = llvm::dyn_cast<llvm::CallInst>(originv)){              //如果对应的是 callInst，此处dyn_cast直接拿了call的函数那么不遍历operand，改为遍历它的args
		for(auto arg=callInst->arg_begin(),arge=callInst->arg_end();arg!=arge;arg++){//这里先转成callInst,然后直接可以取call函数的参数
			if(llvm::Value * argnamed=llvm::dyn_cast<llvm::Value>(arg)){
				if(llvm::Instruction *arginst = llvm::dyn_cast<llvm::Instruction>(arg)){         //这一句用来判断是F函数的参数，经过调试知道，如果这个Value不能强制转换为Instruction了，那么代表它是函数参数了。
					for(auto travarg=arginst->operands().begin();travarg!=arginst->operands().end();++travarg){
						if(llvm::Instruction *travargIns=llvm::dyn_cast<llvm::Instruction>(travarg)){
							if(travargIns==originv){
		    					return;
							}
						}		
					}
		    		TravseAllocUser(funcName,arginst);						//如果还能转成Instruction，继续递归
				}else{														//如果是函数参数，打印该结构体。
					llvm::Type *StructType = argnamed->getType();
					std::string type_str;
					llvm::raw_string_ostream rso(type_str);
					StructType->print(rso);
					fstream<<"FunctionName:"<<funcName<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*StructType)<<'\n';
					continue;
				}
			}
		}
		return;
	}	

	for(auto operand = originv->operands().begin();operand != originv->operands().end();++operand){  //如果对应的是普通语句，则正常遍历他的operands来递归。
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
			TravseAllocUser(funcName,opInst);				//如果还能转成Instruction，继续递归
		}else{
			llvm::Type *StructType = opValue->getType();
			fstream<<"FunctionName:"<<funcName<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*StructType)<<'\n';//如果是函数参数，打印该结构体。
			continue ;
		}
    }
	return;
}
*/
void LockAtomicStructure::id_phi_inst(llvm::Function* funcname,llvm::Instruction* I){
    std::string FuncName=funcname->getName().str();
	llvm::PHINode *PN= llvm::cast<llvm::PHINode>(I);
	bool found=false;
	for(u_int j=0;j<PN->getNumIncomingValues();j++){
		llvm::Value* V=PN->getIncomingValue(j);
		if(!V)continue;
		if(llvm::GetElementPtrInst* GEP=llvm::dyn_cast<llvm::GetElementPtrInst>(V)){
            if(GEP->getOperand(0)->hasName()){
                fstream<<"FunctionName:"<<FuncName<<","<<"Global Variable:"<<GEP->getOperand(0)->getName().str()<<'\n';
            }
            llvm::Type *structType = GEP->getSourceElementType();//此处获取GEP指令的struct
			if(ReturnTypeRefine(*structType) == "i8*"){            //如果GEP指令中的结构体是i8*,需要特殊处理以下来找出真实的结构体。
				std::string ActualStructType;
				ActualStructType = GetActualStructType(GEP,FuncName);     //调用GetActualStructType来获得真实结构体。
				fstream<<"FunctionName:"<<FuncName<<","<<"ProtectedStruct:"<<ActualStructType<<'\n';
			}else{
				fstream<<"FunctionName:"<<FuncName<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*structType)<<'\n';   
			}
            return;             
		}
		if(llvm::LoadInst* LI=llvm::dyn_cast<llvm::LoadInst>(V)){
            llvm::Value* loadValue=LI->getOperand(0);
            if(loadValue->hasName()){
                fstream<<"FunctionName:"<<FuncName<<","<<"Global Variable:"<<loadValue->getName().str()<<'\n';
            } else {
                if(llvm::Instruction *loadVInst=llvm::dyn_cast<llvm::Instruction>(loadValue)){
                    TravseAllocUser(funcname,loadVInst);
                }
            }
		}
    }
	return;
}
void LockAtomicStructure::TravseAllocUser(llvm::Function* func,llvm::Instruction* originv){   //传进来的Instruction是函数参数Value对应的Instruction，有可能是Call语句，Gep指令或者Phi指令，load指令，这里对Call,Gep,load语句作特殊处理
    std::string testfuncName=func->getName().str();
	if(originv->getOpcode() == llvm::Instruction::GetElementPtr){      //第一种情况，对应的是GEP指令，说明我们可以从这获取结构体了。
		llvm::GetElementPtrInst *gepinst = llvm::dyn_cast<llvm::GetElementPtrInst>(originv);
        llvm::Value *GepOperand = gepinst->getOperand(0);
        if(const llvm::GlobalValue* G=llvm::dyn_cast<llvm::GlobalValue>(GepOperand)){
            fstream<<"FunctionName:"<<testfuncName<<","<<"Global Variable:"<<GepOperand->getName().str()<<'\n';
            return;
        }
		llvm::Type *structType = gepinst->getSourceElementType();//此处获取GEP指令的struct
		if(ReturnTypeRefine(*structType) == "i8*"){            //如果GEP指令中的结构体是i8*,需要特殊处理以下来找出真实的结构体。
			std::string ActualStructType;
			ActualStructType = GetActualStructType(originv,testfuncName);     //调用GetActualStructType来获得真实结构体。
			fstream<<"FunctionName:"<<testfuncName<<","<<"ProtectedStruct:"<<ActualStructType<<'\n';
		}else{
			fstream<<"FunctionName:"<<testfuncName<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*structType)<<'\n';   
		}        
		return ;
	}
    if(llvm::LoadInst * loadIns=llvm::dyn_cast<llvm::LoadInst>(originv)){
        llvm::Value* loadValue=loadIns->getOperand(0);
        if(const llvm::GlobalValue* G=llvm::dyn_cast<llvm::GlobalValue>(loadValue)){
            fstream<<"FunctionName:"<<testfuncName<<","<<"Global Variable:"<<loadValue->getName().str()<<'\n';
            return;
        } else {
            if(llvm::Instruction *loadVInst=llvm::dyn_cast<llvm::Instruction>(loadValue)){
                TravseAllocUser(func,loadVInst);
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
								llvm::Type *userType = userVar->getType();                  //bitcast语句对应的Value的TypeName就是要找的真实结构体。
								fstream<<"FunctionName:"<<testfuncName<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*userType)<<"\n";//这里获取的是处理之后inode.xxxx中的inode struct name.
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
                fstream<<"FunctionName:"<<testfuncName<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*tp)<<"\n";
            }
        }
        return;
	} 
    if(llvm::PHINode *testphi=llvm::dyn_cast<llvm::PHINode>(originv)){
        id_phi_inst(func,originv);
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
			fstream<<"FunctionName:"<<testfuncName<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*StructType)<<'\n';//如果是函数参数，打印该结构体。
			continue ;
		}
	}
		return;
}


std::string LockAtomicStructure::GetActualStructType(llvm::Instruction *gepInst,std::string funName){    //传入GEP Instruction和函数名（函数名主要用来调试），返回真实的结构体类型。这里用的是LockPair.cpp里面的内容
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

std::string LockAtomicStructure::ReturnTypeRefine(llvm::Type &rt){           //截取掉返回值中%struct.inode.xxxx*后面的这串数字
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
		if(count == 2){//此处用于限定截取的内容块数，我们只需要第1(%struct)和第2(inode)这两个块
			break;
		}
		refined_type_str = strtok(NULL,".");
		if(refined_type_str == nullptr){//该判断用于针对i8*进行处理
            actual_type_str.pop_back();//此处由于之间加.i8*会变成i8*.，因此此处去掉.
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
bool LockAtomicStructure::LockAtomicStructureMain(llvm::Function* F){
    std::string FuncName = F->getName().str();
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
										fstream<<"FunctionName:";
										fstream<<F->getName();
										fstream<<",";
										fstream<<"Global Variable:";
										fstream<<G->getName();
										fstream<<'\n';
									}
								}

								std::string FunctionName = mystr;
								for(auto argggb=callInst->arg_begin(),arggge=callInst->arg_end();argggb!=arggge;argggb++){
									if(llvm::Value * argnamed=llvm::dyn_cast<llvm::Value>(argggb)){
										if(argnamed->getType()->isIntOrIntVectorTy()){
											continue;
										}
										if(llvm::Instruction *argInst = llvm::dyn_cast<llvm::Instruction>(argggb)){
											TravseAllocUser(F,argInst);
										}
									}
								}
								continue;

							}
						}
					}
				}
                return false;

}
