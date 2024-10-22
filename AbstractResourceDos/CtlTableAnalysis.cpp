#include "CtlTableAnalysis.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "Utils.h"
#include "llvm/IR/InstIterator.h"

std::error_code ec2;
llvm::raw_fd_ostream cstream("./CtlTable-finder.txt", ec2, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);
llvm::raw_fd_ostream ctlstream("./CtlTable-Variable.txt", ec2, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);
llvm::raw_fd_ostream ctlinsstream("./CtlTable-Instruction.txt", ec2, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);
//llvm::raw_fd_ostream resultstream("./CtlTable-Result.txt", ec2, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);
CtlTableAnalysis::CtlTableAnalysis(llvm::Module &module){
    _module_ = &module;
}

/*
  Test() is currently equivalent to the main function.
*/
void CtlTableAnalysis::Test(){
    FindLockAndAtomic FLA = FindLockAndAtomic(*_module_);
    FLA.FindLock();                
    AllocFunctionNames = FLA.GetAllocFunction();
    AllAtomic();//Get Atomic and PerCPU function collections.
    AllPerCPU();

    CollectCtlGlobal();                 //Collect CtlGlobal          

    for(auto it = CtlGlobal.begin();it != CtlGlobal.end();it++){              //Traverse CtlGlobal
        llvm::Value * G = *it;
//        if(G->getName().str()=="files_stat"){
//          std::cout<<"find files_stat's user"<<std::endl;
        //cstream<<"*********"<<G->getName().str()<<"**********"<<"\n";
        if(!G->use_empty()){
          for(llvm::Value::use_iterator UB=G->use_begin(),UE=G->use_end();UB!=UE;++UB){  //Traverse Global users
            llvm::User* user=UB->getUser();
		        llvm::Value *UserValue = llvm::dyn_cast<llvm::Value>(*UB);
//            llvm::Instruction* TestInstruction = llvm::dyn_cast<llvm::Instruction>(user);
//            std::cout<<"the user of "<<G->getName().str()<<" is "<<&TestInstruction<<"\n";
            FindGlobalIcmp(UB,G);
            if(llvm::ConstantExpr *userexp=llvm::dyn_cast<llvm::ConstantExpr>(user)){ //Here we add the judgment that the user is an Expr and find the missing files.stat through the user of expr.
              llvm::Value * testu=llvm::dyn_cast<llvm::Value>(userexp);
              for(llvm::Value::use_iterator UBB=testu->use_begin(),UBE=testu->use_end();UBB!=UBE;++UBB){//find the user of constantExpr
             // cstream<<"Traverse each user of constantExpr"<<"\n";
                  FindGlobalIcmp(UBB,G);
              }
            }
            //cstream<<*UserValue<<"\n";
          }       
        }
//    }//This corresponds to the above if(G.getName().str()=="files_stat"), used for debugging.
    }
}
void CtlTableAnalysis::AllAtomic(){
  auto &functionlist = _module_->getFunctionList();
  for(auto &F : functionlist){
    std::int32_t t=0;
    bool ArgIsInteger=true;
	  for(llvm::Function::arg_iterator argb=F.arg_begin(),arge=F.arg_end();argb!=arge;++argb){
		  ++t;
	  }
    for(llvm::Function::arg_iterator argb=F.arg_begin(), arge=F.arg_end();argb!=arge;++argb){
      std::string type_str;
      llvm::Type* tp = argb->getType();
      llvm::raw_string_ostream rso(type_str);
      tp->print(rso);
      if((rso.str() == "%struct.atomic_t*")||(rso.str()=="%struct.atomic64_t*")){
		    if(t<=3){
		      for(llvm::Function::arg_iterator argbb=F.arg_begin(),argbe=F.arg_end();argbb!=argbe;++argbb){
				    if(argbb!=argb){
				      llvm::Type* tpp=argbb->getType();
					    if(tpp->isIntegerTy()){
						    continue;
					    }
				      else {
					//The logic here is that for Atomic, the parameters include struct.atomic_t, and the number of parameters does not exceed three. 
					//Except for atomi_t, the other parameter types are all integers.
                			ArgIsInteger=false;
					break;
				      }
				    }
			    }
          if(ArgIsInteger==true){
            AtomicFunc.insert(F.getName().str());
          }
		    }
      }
    }
  }
}
void CtlTableAnalysis::AllPerCPU(){
  auto &functionlist = _module_->getFunctionList();
  for(auto &F : functionlist){
    std::string FunName = F.getName().str();
    std::int32_t t=0;
    bool ArgIsInteger=true;
		for(llvm::Function::arg_iterator argb=F.arg_begin(),arge=F.arg_end();argb!=arge;++argb){
			++t;
		}
    for(llvm::Function::arg_iterator argb=F.arg_begin(), arge=F.arg_end();argb!=arge;++argb){
      std::string type_str;
      llvm::Type* tp = argb->getType();
      llvm::raw_string_ostream rso(type_str);
      tp->print(rso);
      if(rso.str() == "%struct.percpu_counter*"){
				if(t<=3){
					for(llvm::Function::arg_iterator argbb=F.arg_begin(),argbe=F.arg_end();argbb!=argbe;++argbb){
						if(argbb!=argb){
							llvm::Type* tpp=argbb->getType();
							if(tpp->isIntegerTy()){
								continue;
							}
							else {
                ArgIsInteger=false; 
								break;
							}
						}
					}
          if(ArgIsInteger==true){
            PerCPUFunc.insert(FunName);
          }
				}
      }
    }
  }
}

/*
  The CollectCtlGlobal() function collects all sysctl Global and stores them in CtlGlobal.
*/
void CtlTableAnalysis::CollectCtlGlobal(){
	  for(auto &GlobalV : _module_->getGlobalList()){
//    if(GlobalV.getName().str()=="fs_table"){//for debugging
      if(GlobalV.getValueType()->isArrayTy()){
        llvm::Type* t=GlobalV.getValueType()->getArrayElementType();
        std::string type_str;
        llvm::raw_string_ostream rso(type_str);
        t->print(rso);
        //fstream<<"The type of the global variables"<<GlobalV.getName().str()<<"is"<<type_str<<"\n";
//        std::cout<<t->getStructName().str()<<std::endl;
        if(t->isStructTy()){
          if(GlobalV.hasInitializer()){ //If fs_table is initialized
            llvm::Constant *GlobalVIni=GlobalV.getInitializer();//To get the initialized content on the right side of it.
            llvm::ArrayType *Ty=llvm::dyn_cast<llvm::ArrayType>(GlobalVIni->getType());
            if(!Ty){
//              std::cout<<"No ArrayType Global Variable"<<std::endl;
              continue;
            }
            if(Ty->getNumElements()==0){
//              std::cout<<"getNumElements is zero"<<std::endl;
              continue;
            }
//            std::cout<<Ty->getNumElements()<<std::endl;
            llvm::ConstantArray *GlobalVInitList=llvm::dyn_cast<llvm::ConstantArray>(GlobalVIni);
            if(!GlobalVInitList){
//              std::cout<<"No ConstArray"<<std::endl;
              continue;
            }
            for(unsigned Index=0;Index < GlobalVInitList->getNumOperands();++Index){
              std::cout<<"start to travse each yuansu in the main array"<<std::endl;
              std::cout<<"the num of main array is"<<GlobalVInitList->getNumOperands()<<std::endl;
              llvm::ConstantStruct *CS=llvm::cast<llvm::ConstantStruct>(GlobalVInitList->getOperand(Index));
              if (!CS) continue;
              bool isCtl_Table=false;
              for(unsigned IndexStruct=0;IndexStruct< CS->getNumOperands();++IndexStruct){
                llvm::Value *test=CS->getOperand(IndexStruct);
                llvm::Type *StructType = test->getType();
									std::string type_str;
									llvm::raw_string_ostream rso(type_str);
									StructType->print(rso);
                  char *t="%struct.ctl_table";
                  if(strstr(type_str.c_str(), t)!=NULL){
                    isCtl_Table=true;
                    break;
                  }
//                std::cout<<type_str<<std::endl;
              }
              if(isCtl_Table==true){
                if(llvm::Value * test=CS->getOperand(1)){
                  if(test->hasName()){
                    CtlGlobal.insert(test);
                    ctlstream<<"the Global Variable inside ctl_table is"<<test->getName().str()<<"\n";
                    continue;
                  }
                  if(llvm::dyn_cast<llvm::Value>(test->stripPointerCasts())){
                    llvm::Value * stripV=test->stripPointerCasts();
                    if(stripV->hasName()){
                      CtlGlobal.insert(stripV);
                      ctlstream<<"the Global Variable inside ctl_table is"<<stripV->getName().str()<<"\n";
                      continue;
                    }
                  }
                  if(llvm::ConstantExpr * gepexp=llvm::dyn_cast<llvm::ConstantExpr>(test)){
                    if(gepexp->getOpcode()==llvm::Instruction::GetElementPtr){
                      llvm::Value * gepv=gepexp->getOperand(0);
                      if(gepv->hasName()){
                        CtlGlobal.insert(gepv);
                        ctlstream<<"the Global Variable inside constantexpr ctl_table is"<<gepv->getName().str()<<"\n";
                      }
                      if(llvm::dyn_cast<llvm::Value>(gepv->stripPointerCasts())){
                        llvm::Value * gepvstrip=gepv->stripPointerCasts();
                        if(gepvstrip->hasName()){
                          CtlGlobal.insert(gepvstrip);
                          ctlstream<<"the Global Variable inside ctl_table is"<<gepvstrip->getName().str()<<"\n";
                        } 
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
//    }//This corresponds to the above tablename=fs_table
	}


   /* for(auto &GlobalV : _module_->getGlobalList()){
//		std::cout<<"the global variable of module is"<<GlobalV.getName().str()<<std::endl;
//    if(GlobalV.getName().str()=="fs_table"){//for debugging
      if(GlobalV.getValueType()->isArrayTy()){
        llvm::Type* t=GlobalV.getValueType()->getArrayElementType();
//        std::cout<<t->getStructName().str()<<std::endl;
        if(t->isStructTy()){
          if(GlobalV.hasInitializer()){ //If fs_table is initialized
            llvm::Constant *GlobalVIni=GlobalV.getInitializer();//To get the initialized content on the right side of it.
            llvm::ArrayType *Ty=llvm::dyn_cast<llvm::ArrayType>(GlobalVIni->getType());
            if(!Ty){
              continue;
            }
            if(Ty->getNumElements()==0){
//              std::cout<<"getNumElements is zero"<<std::endl;
              continue;
            }
//            std::cout<<Ty->getNumElements()<<std::endl;
            llvm::ConstantArray *GlobalVInitList=llvm::dyn_cast<llvm::ConstantArray>(GlobalVIni);
            if(!GlobalVInitList){
//              std::cout<<"No ConstArray"<<std::endl;
              continue;
            }
            for(unsigned Index=0;Index < GlobalVInitList->getNumOperands();++Index){
              llvm::ConstantStruct *CS=llvm::cast<llvm::ConstantStruct>(GlobalVInitList->getOperand(Index));
              bool isCtl_Table=false;
              for(unsigned IndexStruct=0;IndexStruct< CS->getNumOperands();++IndexStruct){
                llvm::Value *test=CS->getOperand(IndexStruct);
                llvm::Type *StructType = test->getType();
									std::string type_str;
									llvm::raw_string_ostream rso(type_str);
									StructType->print(rso);
                  char *t="%struct.ctl_table";
                  if(strstr(type_str.c_str(), t)!=NULL){
                    isCtl_Table=true;
                    break;
                  }
//                std::cout<<type_str<<std::endl;
              }
              if(isCtl_Table==true){
                for(unsigned IndexStruct=0;IndexStruct< CS->getNumOperands();++IndexStruct){
//                std::cout<<"the num of each ctl_table struct is"<<CS->getNumOperands()<<std::endl;
                  llvm::Value * test=CS->getOperand(IndexStruct);
                  if(test->hasName()){  //Make sure it is a global variable.                
                    //std::cout<<test->getName().str()<<std::endl;//Print out global variable names.
                    CtlGlobal.insert(test);
                  }
                  if(llvm::dyn_cast<llvm::Value>(test->stripPointerCasts())){
                    llvm::Value * stripV=test->stripPointerCasts();
                    if(stripV->hasName()){
                      char *b=".str";
                      if(strstr(stripV->getName().str().c_str(),b)!=NULL){
                        continue;
                      }
                      //std::cout<<stripV->getName().str()<<std::endl;//Here we basically capture all the global variables in ctl_table.
                      CtlGlobal.insert(stripV);
                    }
                  }
                }
              }
            }
          }
        }
      }
    }*/
}
//CollectCtlGlobal ends here. There is nothing wrong with this function. It is used to collect variables.
/*
  After finding the Load statement, recursively find its User. If there is an Icmp statement, call IcmpNr.
*/
void CtlTableAnalysis::TravseGlobal(llvm::Value *LoadValue,llvm::Value *OldValue,std::string GlobalName,std::vector<llvm::Instruction*> &travseglobal_stack){//This may be written simply, find the comparison that ctl_table participates in
//It won't be that simple. How about rewriting this function?
//The first problem is found here. What is passed in here is a value. I expand it back here, so the previous processing of gep/load/store is equivalent to useless work. 
//I modify it here and pass the user before calling TravseGlobal.
    std::cout<<"coming inside TravseGlobal!"<<std::endl;
    if(!(llvm::dyn_cast<llvm::Instruction>(LoadValue))){
      return;
    }
    llvm::Instruction *tmpInst=llvm::dyn_cast<llvm::Instruction>(LoadValue);
    std::cout<<"the Function is"<<tmpInst->getFunction()->getName().str()<<std::endl;
    std::cout<<"the related instruction is"<<std::endl;
    //cstream<<"coming inside TravseGlobal!"<<"\n";
    //cstream<<"the Function is"<<tmpInst->getFunction()->getName().str()<<"\n";
    //cstream<<"the related instruction is"<<"\n";
    //cstream<<*tmpInst<<"\n";
    if (travseglobal_stack.size() == 20){//The recursion depth is set to 10 to limit the scale and infinite loop.
      travseglobal_stack.pop_back();
      return;
    }
    travseglobal_stack.push_back(tmpInst);
    if(llvm::ICmpInst *Icmp = llvm::dyn_cast<llvm::ICmpInst>(LoadValue)){
      if(Icmp->isEquality()==false){
        std::cout<<"Inside ICMP SOLVE"<<std::endl;
      IcmpNr(Icmp,OldValue,GlobalName);//The OldValue processing here is questionable, so I will change it after running.
      //cstream<<"Travse:"<<*Icmp<<"\n";
      return;
      }
    }
    if(tmpInst->getOpcode()==llvm::Instruction::Add||tmpInst->getOpcode()==llvm::Instruction::FAdd||tmpInst->getOpcode()==llvm::Instruction::Sub||tmpInst->getOpcode()==llvm::Instruction::FSub||tmpInst->getOpcode()==llvm::Instruction::Mul||tmpInst->getOpcode()==llvm::Instruction::FMul||tmpInst->getOpcode()==llvm::Instruction::UDiv||tmpInst->getOpcode()==llvm::Instruction::SDiv||tmpInst->getOpcode()==llvm::Instruction::FDiv||tmpInst->getOpcode()==llvm::Instruction::URem||tmpInst->getOpcode()==llvm::Instruction::SRem||tmpInst->getOpcode()==llvm::Instruction::FRem||tmpInst->getOpcode()==llvm::Instruction::Shl||tmpInst->getOpcode()==llvm::Instruction::LShr||tmpInst->getOpcode()==llvm::Instruction::AShr||tmpInst->getOpcode()==llvm::Instruction::And||tmpInst->getOpcode()==llvm::Instruction::Or||tmpInst->getOpcode()==llvm::Instruction::Xor){
//What this means here is that if the operators of llvm Instruction are arithmetic operations of addition, subtraction, multiplication, and division, 
//the results of the arithmetic operations are traversed here, which is the user of the arithmetic operations.
      if(!LoadValue->user_empty()){
        //
        std::cout<<"the Function is "<<tmpInst->getFunction()->getName().str()<<", inside addition, subtraction, multiplication and division"<<std::endl;
        for(auto user_it=LoadValue->user_begin();user_it!=LoadValue->user_end();++user_it){
          TravseGlobal(*user_it,LoadValue,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::CallInst *Callins=llvm::dyn_cast<llvm::CallInst>(LoadValue)){
      std::cout<<"comming inside CALL resolve"<<std::endl;
      if(llvm::Function *calledFunction = Callins->getCalledFunction()){
        if((AtomicFunc.find(Callins->getCalledFunction()->getName().str())!=AtomicFunc.end())||(PerCPUFunc.find(Callins->getCalledFunction()->getName().str())!=PerCPUFunc.end())){
          if(!LoadValue->user_empty()){
            for(auto user_it=LoadValue->user_begin();user_it!=LoadValue->user_end();++user_it){
              TravseGlobal(*user_it,LoadValue,GlobalName,travseglobal_stack);
            }
          } else {
            std::cout<<"the CallSite of percpu/atomic's user is NULL"<<std::endl;
            return;
          }
        } else {
          std::cout<<"the CallSite is not atomic/percpu function, we need to solve it"<<std::endl;
          return;
        }
      } else if(llvm::Function *voidFunc = llvm::dyn_cast<llvm::Function>(Callins->getCalledOperand()->stripPointerCasts())){
        if((AtomicFunc.find(voidFunc->getName().str())!=AtomicFunc.end())||(PerCPUFunc.find(voidFunc->getName().str())!=PerCPUFunc.end())){
          if(!LoadValue->user_empty()){
            for(auto user_it=LoadValue->user_begin();user_it!=LoadValue->user_end();++user_it){
              TravseGlobal(*user_it,LoadValue,GlobalName,travseglobal_stack);
            }
          } else {
            std::cout<<"the CallSite of percpu/atomic's user is NULL"<<std::endl;
            return;
          }
        } else {
          std::cout<<"the CallSite is not atomic/percpu function, we need to solve it"<<std::endl;
          return;
        }
      } else{
        return;
      }
    }
    if(llvm::StoreInst *teststore = llvm::dyn_cast<llvm::StoreInst>(LoadValue)){
        if(OldValue==teststore->getOperand(0)){
            llvm::Value* storeV=teststore->getOperand(1);
            for(auto user_it=storeV->user_begin();user_it!=storeV->user_end();++user_it){
                std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideSTORE solve"<<std::endl;
                TravseGlobal(*user_it,storeV,GlobalName,travseglobal_stack);
            }
        }
    }
    if(llvm::PHINode *testphi = llvm::dyn_cast<llvm::PHINode>(LoadValue)){//I observed here that if it is a phi command, I should not take the incoming value of phi, but the left side of phi.
      if(llvm::Value* phiV=llvm::dyn_cast<llvm::Value>(testphi)){
        for(auto user_it= phiV->user_begin();user_it!=phiV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insidePHI solve"<<std::endl;
          TravseGlobal(*user_it,phiV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::SelectInst *testselect=llvm::dyn_cast<llvm::SelectInst>(LoadValue)){//It was previously discovered that the processing of select was lost.
      if(llvm::Value *selectV=llvm::dyn_cast<llvm::Value>(testselect)){
        for(auto user_it=selectV->user_begin();user_it!=selectV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideSELECT solve"<<std::endl;
          TravseGlobal(*user_it,selectV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::SExtInst *testsext=llvm::dyn_cast<llvm::SExtInst>(LoadValue)){//It was previously discovered that the processing of sext was lost.
      if(llvm::Value *sextV=llvm::dyn_cast<llvm::Value>(testsext)){
        for(auto user_it=sextV->user_begin();user_it!=sextV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideSEXT solve"<<std::endl;
          TravseGlobal(*user_it,sextV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::TruncInst *testtrunc=llvm::dyn_cast<llvm::TruncInst>(LoadValue)){//It was previously discovered that the processing of trunc was lost.
      if(llvm::Value *truncV=llvm::dyn_cast<llvm::Value>(testtrunc)){
        for(auto user_it=truncV->user_begin();user_it!=truncV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideTRUNC solve"<<std::endl;
          TravseGlobal(*user_it,truncV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::ZExtInst *testzext=llvm::dyn_cast<llvm::ZExtInst>(LoadValue)){//It was previously discovered that the processing of zext was lost.
      if(llvm::Value *zextV=llvm::dyn_cast<llvm::Value>(testzext)){
        for(auto user_it=zextV->user_begin();user_it!=zextV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", inside ZEXT solve"<<std::endl;
          TravseGlobal(*user_it,zextV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::FPTruncInst *testfptrunc=llvm::dyn_cast<llvm::FPTruncInst>(LoadValue)){//It was previously discovered that the processing of fptrunc was lost.
      if(llvm::Value *fptruncV=llvm::dyn_cast<llvm::Value>(testfptrunc)){
        for(auto user_it=fptruncV->user_begin();user_it!=fptruncV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideFPTRUNC solve"<<std::endl;
          TravseGlobal(*user_it,fptruncV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::FPExtInst *testfpext=llvm::dyn_cast<llvm::FPExtInst>(LoadValue)){//It was previously discovered that the processing of fpext was lost.
      if(llvm::Value *fpextV=llvm::dyn_cast<llvm::Value>(testfpext)){
        for(auto user_it=fpextV->user_begin();user_it!=fpextV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideFPEXT solve"<<std::endl;
          TravseGlobal(*user_it,fpextV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::FPToUIInst *testfpoui=llvm::dyn_cast<llvm::FPToUIInst>(LoadValue)){//It was previously discovered that the processing of fptoui was lost.
      if(llvm::Value *fptouiV=llvm::dyn_cast<llvm::Value>(testfpoui)){
        for(auto user_it=fptouiV->user_begin();user_it!=fptouiV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideFPTOUI solve"<<std::endl;
          TravseGlobal(*user_it,fptouiV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::FPToSIInst *testfptosi=llvm::dyn_cast<llvm::FPToSIInst>(LoadValue)){//It was previously discovered that the processing of fptosi was lost.
      if(llvm::Value *fptosiV=llvm::dyn_cast<llvm::Value>(testfptosi)){
        for(auto user_it=fptosiV->user_begin();user_it!=fptosiV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideFPTOSI solve"<<std::endl;
          TravseGlobal(*user_it,fptosiV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::UIToFPInst *testuitofp=llvm::dyn_cast<llvm::UIToFPInst>(LoadValue)){//It was previously discovered that the processing of uitofp was lost.
      if(llvm::Value *uitofpV=llvm::dyn_cast<llvm::Value>(testuitofp)){
        for(auto user_it=uitofpV->user_begin();user_it!=uitofpV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideUITOFP solve"<<std::endl;
          TravseGlobal(*user_it,uitofpV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::SIToFPInst *testsitofp=llvm::dyn_cast<llvm::SIToFPInst>(LoadValue)){//It was previously discovered that the processing of sitofp was lost.
      if(llvm::Value *sitofpV=llvm::dyn_cast<llvm::Value>(testsitofp)){
        for(auto user_it=sitofpV->user_begin();user_it!=sitofpV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideSITOFP solve"<<std::endl;
          TravseGlobal(*user_it,sitofpV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::PtrToIntInst *testptrtoint=llvm::dyn_cast<llvm::PtrToIntInst>(LoadValue)){//It was previously discovered that the processing of ptrtoint was lost.
      if(llvm::Value *ptrtointV=llvm::dyn_cast<llvm::Value>(testptrtoint)){
        for(auto user_it=ptrtointV->user_begin();user_it!=ptrtointV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insidePTRTOINT solve"<<std::endl;
          TravseGlobal(*user_it,ptrtointV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::IntToPtrInst *testinttoptr=llvm::dyn_cast<llvm::IntToPtrInst>(LoadValue)){//It was previously discovered that the processing of inttoptr was lost.
      if(llvm::Value *inttoptrV=llvm::dyn_cast<llvm::Value>(testinttoptr)){
        for(auto user_it=inttoptrV->user_begin();user_it!=inttoptrV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideINTTOPTR solve"<<std::endl;
          TravseGlobal(*user_it,inttoptrV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::BitCastInst *testbitcast=llvm::dyn_cast<llvm::BitCastInst>(LoadValue)){//It was previously discovered that the processing of bitcast was lost.
      if(llvm::Value *bitcastV=llvm::dyn_cast<llvm::Value>(testbitcast)){
        for(auto user_it=bitcastV->user_begin();user_it!=bitcastV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideBITCAST solve"<<std::endl;
          TravseGlobal(*user_it,bitcastV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::AddrSpaceCastInst *testaddrspacecast=llvm::dyn_cast<llvm::AddrSpaceCastInst>(LoadValue)){//It was previously discovered that the processing of addrspacecast was lost.
      if(llvm::Value *truncV=llvm::dyn_cast<llvm::Value>(testaddrspacecast)){
        for(auto user_it=truncV->user_begin();user_it!=truncV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideADDRSPACECAST solve"<<std::endl;
          TravseGlobal(*user_it,truncV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::LoadInst *testload=llvm::dyn_cast<llvm::LoadInst>(LoadValue)){//Add another load processing.
      if(llvm::Value *loadV=llvm::dyn_cast<llvm::Value>(testload)){
        for(auto user_it=loadV->user_begin();user_it!=loadV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideLOAD solve"<<std::endl;
          TravseGlobal(*user_it,loadV,GlobalName,travseglobal_stack);
        }
      }
    }
    /*if(!LoadValue->user_empty()){
      std::cout<<"TravseGlobalBegin"<<std::endl;
      for(auto user_it = LoadValue->user_begin();user_it != LoadValue->user_end();++user_it){
          TravseGlobal(*user_it,LoadValue,GlobalName);
      }
    }*/
    //travseglobal_stack.pop_back();//If none of them match, then the stack will be decremented by 1. I haven’t thought about it yet.
    return;
}

/*
  For a CtlGlobal, call the IcmpNr function after finding the Icmp comparison statement where it is located.
  The first parameter of the function is the imcp statement, the second parameter is used to avoid not looking for yourself when looking for the operand from Icmp, and the third is your name.
  The basic idea is to find another operand of Icmp. If it is Global, take it out as a pair. 
  If the atomic or perCPU function is called, check whether the parameters of this function are global variables. If so, take it out and make it a pair.
  Only one layer has been processed so far.
*/
/*
3-28:Take a closer look at the IcmpNr of ctl_table. This function actually only handles two situations. 
(1) The other end is a global variable G. 
(2) The other end is a local variable. The source of this local variable is the percpu/atomic function. 
it does not handle complex situations from structures at all. This comparison function deserves an overhaul.
*/
void CtlTableAnalysis::IcmpNr(llvm::Instruction *icmpinst,llvm::Value *OldValue,std::string GlobalName){//
    if(llvm::ICmpInst * icmpinsti=llvm::dyn_cast<llvm::ICmpInst>(icmpinst)){
      if(icmpinsti->isEquality()==true){
        return;
      }
    //std::set<std::pair<std::string,std::string>> PairSet;  
    for(auto operand = icmpinst->operands().begin();operand != icmpinst->operands().end();++operand){
      llvm::Value *opValue = llvm::dyn_cast<llvm::Value>(operand);
      if(opValue == OldValue){
        continue;
      }
//      if(llvm::Instruction* icmpInst = llvm::dyn_cast<llvm::Instruction>(operand)){//If the result at the other end of the comparison comes from another comparison
//            std::vector<std::string> ResourceVector;
//            //LockPair LP = LockPair(*_module_);
//            //LP.TravseAllocUser(icmpInst->getFunction(),icmpInst,&ResourceVector);
//            CtlTravse(icmpInst,icmpInst->getFunction(),&ResourceVector,GlobalName);
//            StoreResourceVector(icmpInst->getFunction(),ResourceVector);
//        }
//This shows that the second global variable has been found from icmp. Now we need to find its user from the global variable, and then see if the user is a store statement.
      if(llvm::GlobalValue* G = llvm::dyn_cast<llvm::GlobalValue>(operand)){
     // cstream<<"The global variable found is named"<<G->getName().str()<<"\n";
          /*if(!G->use_empty()){
        //cstream<<"The global variable found is named"<<G->getName().str()<<"\n";
       // cstream<<"G's use is not empty"<<"\n";
              for(llvm::Value::use_iterator UBB=G->use_begin(),UBE=G->use_end();UBB!=UBE;++UBB){//To find the user of the global variable participating in the comparison.
          //cstream<<"Traverse each user of the comparison object"<<"\n";
                  if(llvm::User* icmpuser=UBB->getUser()){
                      if(llvm::StoreInst* testStore=llvm::dyn_cast<llvm::StoreInst>(icmpuser)){
                          if(testStore->getOperand(1)==G){
			  //If the user of the compared global variable is a store instruction, we will consider it to have modified this, 
     			  //and we will mark the function that modifies this global variable as a sensitive function.
                              llvm::Function* minganFunc=testStore->getFunction();  
//                              StoreFuncResource(minganFunc,G);
                              cstream<<"FunctionName:"<<minganFunc->getName().str()<<",Global Variable"<<G->getName().str()<<",corressponding ctl_variable:"<<GlobalName<<"\n";

                          }
                      }
                      if(llvm::CallInst* testCall=llvm::dyn_cast<llvm::CallInst>(icmpuser)){
                          llvm::Function* called=testCall->getCalledFunction();
                          llvm::Function* minganFunc=testCall->getFunction();
                          if(called->onlyReadsMemory()==false){
//                              StoreFuncResource(minganFunc,G);
                              cstream<<"FunctionName:"<<minganFunc->getName().str()<<",Global Variable"<<G->getName().str()<<",corressponding ctl_variable:"<<GlobalName<<"\n";
                          } 
                      }
                  }
              }
          }*/
          //StoreFuncResource(icmpinst->getFunction(),G);
          cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<",Global Variable:"<<G->getName().str()<<",corresponding ctl_variable:"<<GlobalName<<"\n";
          //cstream<<",Global Variable:"<<G->getName().str()<<"\n";//I am trying to count the total number of counters captured using these cstreams.
          //cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<"\n";//I am trying to count the number of sensitive functions captured in these cstreams.
      }

      if(llvm::CallInst *callinst = llvm::dyn_cast<llvm::CallInst>(operand)){
          if(llvm::Function *called = callinst->getCalledFunction()){
              std::string FuncName = called->getName().str();
              if(AtomicFunc.find(FuncName) != AtomicFunc.end() || PerCPUFunc.find(FuncName) != PerCPUFunc.end()){
                  for(auto argit = callinst->arg_begin();argit != callinst->arg_end();++argit){
                      if(llvm::GlobalValue *G = llvm::dyn_cast<llvm::GlobalValue>(argit)){
                        //cstream<<"The global variable found is named"<<G->getName().str()<<"\n";
                        if(G->hasName()){
                          //cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<"\n";//I am trying to count the number of sensitive functions captured in these cstreams.
                          //cstream<<",Global Variable:"<<G->getName().str()<<"\n";//I am trying to count the total number of counters captured using these cstreams.
                          cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<",GlobalVariable"<<G->getName().str()<<",corressponding ctl_variable:"<<GlobalName<<"\n";
                          /*
                          if(!G->use_empty()){
                            //cstream<<"G's user is not empty "<<"\n";
                            for(llvm::Value::use_iterator UBB=G->use_begin(),UBE=G->use_end();UBB!=UBE;++UBB){//To find the user of the global variable participating in the comparison.
                              //cstream<<"Traverse each user of the comparison object"<<"\n";
                              if(llvm::User* icmpuser=UBB->getUser()){
                                if(llvm::StoreInst* testStore=llvm::dyn_cast<llvm::StoreInst>(icmpuser)){
                                  if(testStore->getOperand(1)==G){
				  //If the user of the compared global variable is a store instruction, we will consider it to have modified this, 
      				//and we will mark the function that modifies this global variable as a sensitive function.
                                    llvm::Function* minganFunc=testStore->getFunction();      
//                                      StoreFuncResource(minganFunc,G);
                                      cstream<<"FunctionName:"<<minganFunc->getName().str()<<",Global Variable"<<G->getName().str()<<",corressponding ctl_variable:"<<GlobalName<<"\n";
                                  }
                                }
                                if(llvm::CallInst* testCall=llvm::dyn_cast<llvm::CallInst>(icmpuser)){
                                  llvm::Function* called=testCall->getCalledFunction();
                                  llvm::Function* minganFunc=testCall->getFunction();
                                  if(called->onlyReadsMemory()==false){
//                                    StoreFuncResource(minganFunc,G);
                                    cstream<<"FunctionName:"<<minganFunc->getName().str()<<",Global Variable"<<G->getName().str()<<",corressponding ctl_variable:"<<GlobalName<<"\n";
                                  }
                                }
                              }
                            }
                          }                       
//                          std::pair<std::string,std::string> Pair;
//                          Pair.first = GlobalName;
//                          Pair.second = G->getName().str().c_str();
//                          llvm::outs()<<GlobalName<<" compare with  "<<G->getName().str().c_str()<<"\n";
//                          CtlPair->insert(Pair);
*/
                        }
                      } else{//What is missing here is that if the found atomic/percpu parameter is not a global variable, but comes from a structure, it will be added now.
                        for(llvm::inst_iterator instuser_it= llvm::inst_begin(icmpinst->getFunction());instuser_it!=llvm::inst_end(icmpinst->getFunction());instuser_it++){
                        llvm::Instruction *instgepm= &*instuser_it;
                        if(llvm::GetElementPtrInst *GepInst=llvm::dyn_cast<llvm::GetElementPtrInst>(instgepm)){
                          llvm::Type* userType=GepInst->getSourceElementType();
                          llvm::Value *GepVar=llvm::dyn_cast<llvm::Value>(GepInst);
                          if(!GepVar->user_empty()){
                            for(auto gepuser_it=GepVar->user_begin();gepuser_it!=GepVar->user_end();gepuser_it++){
                              if(llvm::Instruction* tmpInst=llvm::dyn_cast<llvm::Instruction>(*gepuser_it)){
                                if(tmpInst==callinst){
                                  //cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<"\n";//I am trying to count the number of sensitive functions captured in these cstreams.
                                  //cstream<<"ProtectedStruct:"<<ReturnTypeRefine(*userType)<<":";
                                  //GepInst->getOperand(GepInst->getNumIndices())->printAsOperand(cstream);
                                  //cstream<<"\n";

                                  cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<",ProtectedStruct:"<<ReturnTypeRefine(*userType)<<",corressponding ctl_variable:"<<GlobalName<<"\n";
                                  cstream<<"Try to print the offset of Gep:";
                                  GepInst->getOperand(GepInst->getNumIndices())->printAsOperand(cstream);
                                  cstream<<"\n";
                                  cstream<<"The corressponding GEP Instruction is:"<<"\n";
                                  cstream<<*GepInst<<"\n";
                                } else {
                                  if(llvm::Value* tmpVar1=llvm::dyn_cast<llvm::Value>(*gepuser_it)){
                                    if(!tmpVar1->user_empty()){
                                      for(auto tmpuser_it1=tmpVar1->user_begin();tmpuser_it1!=tmpVar1->user_end();tmpuser_it1++){
                                        if(llvm::Instruction* tmpInst1=llvm::dyn_cast<llvm::Instruction>(*tmpuser_it1)){
                                          if(tmpInst1==callinst){
                                            //cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<"\n";//I am trying to count the number of sensitive functions captured in these cstreams.

                                            //cstream<<"ProtectedStruct:"<<ReturnTypeRefine(*userType)<<":";
                                            //GepInst->getOperand(GepInst->getNumIndices())->printAsOperand(cstream);
                                            //cstream<<"\n";
                                            cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<",ProtectedStruct:"<<ReturnTypeRefine(*userType)<<",corressponding ctl_variable:"<<GlobalName<<"\n";
                                            cstream<<"Try to print the offset of Gep:";
                                            GepInst->getOperand(GepInst->getNumIndices())->printAsOperand(cstream);
                                            cstream<<"\n";
                                            cstream<<"The corressponding GEP Instruction is:"<<"\n";
                                            cstream<<*GepInst<<"\n";
                                          } else{
                                            if(llvm::Value * tmpVar2=llvm::dyn_cast<llvm::Value>(*tmpuser_it1)){
                                              if(!tmpVar2->user_empty()){
                                                for(auto tmpuser_it2=tmpVar2->user_begin();tmpuser_it2!=tmpVar2->user_end();tmpuser_it2++){
                                                  if(llvm::Instruction* tmpInst2=llvm::dyn_cast<llvm::Instruction>(*tmpuser_it2)){
                                                    if(tmpInst2==callinst){
                                                      cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<"\n";//I am trying to count the number of sensitive functions captured in these cstreams.

                                                      //cstream<<"ProtectedStruct:"<<ReturnTypeRefine(*userType)<<":";
                                                      //GepInst->getOperand(GepInst->getNumIndices())->printAsOperand(cstream);
                                                      //cstream<<"\n";
                                                      cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<",ProtectedStruct:"<<ReturnTypeRefine(*userType)<<",corressponding ctl_variable:"<<GlobalName<<"\n";
                                                      cstream<<"Try to print the offset of Gep:";
                                                      GepInst->getOperand(GepInst->getNumIndices())->printAsOperand(cstream);
                                                      cstream<<"\n";
                                                      cstream<<"The corressponding GEP Instruction is:"<<"\n";
                                                      cstream<<*GepInst<<"\n";
                                                    }
                                                  }
                                                }
                                              }
                                            }
                                          }
                                        }   
                                      }
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }  

                      }
                      
                  }
              }
          }
      } else{//The result of this else is that if it does not come from callInst (typically atomic/percpu), then it will come from which offset of which structure,
      //Three layers of user judgment are applied here. Let’s see the effect.
          for(llvm::inst_iterator instuser_it= llvm::inst_begin(icmpinst->getFunction());instuser_it!=llvm::inst_end(icmpinst->getFunction());instuser_it++){
            llvm::Instruction *instgepm= &*instuser_it;
            if(llvm::GetElementPtrInst *GepInst=llvm::dyn_cast<llvm::GetElementPtrInst>(instgepm)){
              llvm::Type* userType=GepInst->getSourceElementType();
              llvm::Value *GepVar=llvm::dyn_cast<llvm::Value>(GepInst);
                if(!GepVar->user_empty()){
                  for(auto gepuser_it=GepVar->user_begin();gepuser_it!=GepVar->user_end();gepuser_it++){
                    if(llvm::Instruction* tmpInst=llvm::dyn_cast<llvm::Instruction>(*gepuser_it)){
                      if(tmpInst==icmpinst){
                        cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<"\n";//I am trying to count the number of sensitive functions captured in these cstreams.

                        //cstream<<"ProtectedStruct:"<<ReturnTypeRefine(*userType)<<":";
                        //GepInst->getOperand(GepInst->getNumIndices())->printAsOperand(cstream);
                        //cstream<<"\n";
                        cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<",ProtectedStruct:"<<ReturnTypeRefine(*userType)<<",corressponding ctl_variable:"<<GlobalName<<"\n";
                        cstream<<"Try to print the offset of Gep:";
                        GepInst->getOperand(GepInst->getNumIndices())->printAsOperand(cstream);
                        cstream<<"\n";
                        cstream<<"The corressponding GEP Instruction is:"<<"\n";
                        cstream<<*GepInst<<"\n";
                      } else {
                        if(llvm::Value* tmpVar1=llvm::dyn_cast<llvm::Value>(*gepuser_it)){
                          if(!tmpVar1->user_empty()){
                            for(auto tmpuser_it1=tmpVar1->user_begin();tmpuser_it1!=tmpVar1->user_end();tmpuser_it1++){
                              if(llvm::Instruction* tmpInst1=llvm::dyn_cast<llvm::Instruction>(*tmpuser_it1)){
                                if(tmpInst1==icmpinst){
                                  cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<"\n";//I am trying to count the number of sensitive functions captured in these cstreams.

                                  //cstream<<"ProtectedStruct:"<<ReturnTypeRefine(*userType)<<":";
                                  //GepInst->getOperand(GepInst->getNumIndices())->printAsOperand(cstream);
                                  //cstream<<"\n";
                                  cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<",ProtectedStruct:"<<ReturnTypeRefine(*userType)<<",corressponding ctl_variable:"<<GlobalName<<"\n";
                                  cstream<<"Try to print the offset of Gep:";
                                  GepInst->getOperand(GepInst->getNumIndices())->printAsOperand(cstream);
                                  cstream<<"\n";
                                  cstream<<"The corressponding GEP Instruction is:"<<"\n";
                                  cstream<<*GepInst<<"\n";
                                } else{
                                  if(llvm::Value * tmpVar2=llvm::dyn_cast<llvm::Value>(*tmpuser_it1)){
                                    if(!tmpVar2->user_empty()){
                                      for(auto tmpuser_it2=tmpVar2->user_begin();tmpuser_it2!=tmpVar2->user_end();tmpuser_it2++){
                                        if(llvm::Instruction* tmpInst2=llvm::dyn_cast<llvm::Instruction>(*tmpuser_it2)){
                                          if(tmpInst2==icmpinst){
                                            cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<"\n";//I am trying to count the number of sensitive functions captured in these cstreams.

                                            //cstream<<"ProtectedStruct:"<<ReturnTypeRefine(*userType)<<":";
                                            //GepInst->getOperand(GepInst->getNumIndices())->printAsOperand(cstream);
                                            //cstream<<"\n";
                                            cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<",ProtectedStruct:"<<ReturnTypeRefine(*userType)<<",corressponding ctl_variable:"<<GlobalName<<"\n";
                                            cstream<<"Try to print the offset of Gep:";
                                            GepInst->getOperand(GepInst->getNumIndices())->printAsOperand(cstream);
                                            cstream<<"\n";
                                            cstream<<"The corressponding GEP Instruction is:"<<"\n";
                                            cstream<<*GepInst<<"\n";
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }   
                            }
                          }
                        }
                      }
                    }
                  }
                }
            }
          }
        }
    }
    /*
    std::set<llvm::BasicBlock *> IcmpReachableSet;
    llvm::Function *IcmpFunction=icmpinst->getFunction();
    llvm::BasicBlock *IcmpBB=icmpinst->getParent();
//    if(IcmpFunction->getName().str()!="alloc_fdtable"){
//      return;
//    }
//    cstream<<"the Function that use CTL_TABLE ICMP is"<<IcmpFunction->getName().str()<<"\n";
    IcmpReachableSet.insert(IcmpBB);
    for(llvm::Function::iterator testReach=IcmpFunction->begin();testReach!=IcmpFunction->end();testReach++){
      if(llvm::isPotentiallyReachable(IcmpBB,&*testReach)){
        IcmpReachableSet.insert(&*testReach);
      }
    }
//    for(llvm::BasicBlock *ReachableBB : llvm::successors(IcmpBB)){
//      IcmpReachableSet.insert(ReachableBB);
//    }
    if(!IcmpReachableSet.empty()){
//      cstream<<"the ICMPREACHABLESET of"<<"\n"<<*icmpinst<<" is not empty"<<"\n";
      for(auto It: IcmpReachableSet){
        for(llvm::BasicBlock::iterator BBInst=It->begin();BBInst != It->end();BBInst++){
          llvm::Instruction *ReachableIns=&*BBInst;
//          cstream<<*ReachableIns<<"\n";
          if(llvm::CallInst * ReachableCall=llvm::dyn_cast<llvm::CallInst>(ReachableIns)){
            if(llvm::Function *called = ReachableCall->getCalledFunction()){
              std::string calledFuncName = called->getName().str();
//              if((spin_lock.find(calledFuncName)!=spin_lock.end())||(spin_unlock.find(calledFuncName)!=spin_unlock.end())||(atomic_function.find(calledFuncName)!=atomic_function.end())||(percpu_function.find(calledFuncName)!=percpu_function.end())){
//                  return;
//              }
              if(called==NULL){
                return;
              }
              if(called->onlyReadsMemory()){
                return;
              }
              for(auto arglb=ReachableCall->arg_begin(),argle=ReachableCall->arg_end();arglb!=argle;arglb++){
                if(llvm::Value *argValue = llvm::dyn_cast<llvm::Value>(arglb)){
                  if(argValue->getType()->isPointerTy()){
                    for(auto argF=IcmpFunction->arg_begin();argF!=IcmpFunction->arg_end();argF++){
		    //The function of this judgment is that if arg is the arg of F itself, then the parameters passed in by F have been operated, and we should print the parameter type directly.
                      if(llvm::Value * argnamed=llvm::dyn_cast<llvm::Value>(argF)){  
                        if(argnamed==argValue){
                          if(const llvm::GlobalValue* AtomicG=llvm::dyn_cast<llvm::GlobalValue>(argF)){//If the parameter has a name, it means it is a global variable and the parameter name is printed.
                            if(!llvm::dyn_cast<llvm::Function>(argValue)){
                              //resultstream<<"FunctionName:"<<funcname<<","<<"Global Variable:"<<argF->getName().str()<<"\n";
                              std::string GB = "Global Variable:"+argF->getName().str();
                              cstream<<"FunctionName:"<<IcmpFunction->getName().str()<<","<<GB<<",corressponding ctl_variable:"<<GlobalName<<"\n";
//                            StoreFuncResource();
//                            Resource->push_back(GB);
                            }
                          }
                          llvm::Type* argtype= argValue->getType();//If the function parameter has no name, it means it is a local variable parameter, so I just print the parameter type here.
                          //resultstream<<"FunctionName:"<<funcname<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*argtype)<<"\n";
                          std::string PS = "ProtectedStruct:"+ReturnTypeRefine(*argtype);
//                        if(!StructHasNamespace(argtype,funcname)){
                          cstream<<"FunctionName:"<<IcmpFunction->getName().str()<<","<<PS<<",corressponding ctl_variable:"<<GlobalName<<"\n";
//                        Resource->push_back(PS);
//                        }
    						        }
    					        }
                    }
                    if(const llvm::GlobalValue* AtomicG=llvm::dyn_cast<llvm::GlobalValue>(arglb)){//If the parameter of the call statement is a global variable, it is printed directly.
                      if(!llvm::dyn_cast<llvm::Function>(argValue)){
                        //resultstream<<"FunctionName:"<<funcname<<","<<"Global Variable:"<<AtomicG->getName().str()<<"\n";
                        std::string GB = "Global Variable:"+AtomicG->getName().str();
                        cstream<<"FunctionName:"<<IcmpFunction->getName().str()<<","<<GB<<",corressponding ctl_variable:"<<GlobalName<<"\n";
                        //Resource->push_back(GB);
                      }    
                    }
		    //If it is neither a global variable nor a parameter of the F function, nor is it a structure itself, 
      		   //then the source of the parameters of the internal call needs to be analyzed. It's still being revised here, and there is no final plan.
    			        if(llvm::Instruction *argInst=llvm::dyn_cast<llvm::Instruction>(arglb)){
//    			        resultstream<<"testforTravseAllocUser"<<"\n";
//Here are the names of the Instruction and F passed into the lock function parameters, 
//because if you call the call statement between lock/unlock, then either the parameters of the call have changed, or where the return value of the call is stored.
//So this corresponds to catching the call parameter, traversing each parameter of the call statement, and finding the resource corresponding to the parameter.
//If it is a structure, print the source of the structure, if it is a variable, you need to see where the variable comes from.
//If it comes from a function parameter, print the variable, if it is a global variable, print the variable name.
//                    TravseAllocUser(funcName,argInst,Resource);
                  }                                   												
			        }
          }
    		}
        }

        if(llvm::Value * callreturnValue=llvm::dyn_cast<llvm::Value>(ReachableCall)){
            if(!callreturnValue->user_empty()){  //Did not add "!"
                for(auto calluser=callreturnValue->user_begin();calluser!=callreturnValue->user_end();calluser++){
                  if(llvm::BitCastInst * returnBitCast=llvm::dyn_cast<llvm::BitCastInst>(*calluser)){
                    llvm::Type* rettype=callreturnValue->getType();
                    std::string RS= "ProtectedStruct:"+ReturnTypeRefine(*rettype);
                    cstream<<"FunctionName:"<<IcmpFunction->getName().str()<<","<<RS<<",corressponding ctl_variable:"<<GlobalName<<"\n";
                  }
                    if(llvm::StoreInst * returnStore=llvm::dyn_cast<llvm::StoreInst>(*calluser)){
                        if(llvm::Value * returnStoredetination= returnStore->getOperand(1)){
                            if(const llvm::GlobalValue* G=llvm::dyn_cast<llvm::GlobalValue>(returnStoredetination)){
                                if(!llvm::dyn_cast<llvm::Function>(returnStoredetination)){
                                    //resultstream<<"FunctionName:"<<funcname<<","<<"Global Variable:"<<returnStoredetination->getName().str()<<"\n";
                                    std::string GB = "Global Variable:"+returnStoredetination->getName().str();
                                    cstream<<"FunctionName:"<<IcmpFunction->getName().str()<<","<<GB<<",corressponding ctl_variable:"<<GlobalName<<"\n";
//                                    Resource->push_back(GB);
                                }
                            }
                            if(llvm::GetElementPtrInst * retstodesInst=llvm::dyn_cast<llvm::GetElementPtrInst>(returnStoredetination)){
                                llvm::Type* retgeptype= retstodesInst->getSourceElementType();
                                //resultstream<<"FunctionName:"<<funcname<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*retgeptype)<<"\n";
                                std::string PS = "ProtectedStruct:"+ReturnTypeRefine(*retgeptype);
//                                if(!StructHasNamespace(retgeptype,funcname)){
                                    cstream<<"FunctionName:"<<IcmpFunction->getName().str()<<","<<PS<<",corressponding ctl_variable:"<<GlobalName<<"\n";
//                                    Resource->push_back(PS);
//                                }
                            }
                        }
                    }
                }
            }
        }
        }
        if(llvm::StoreInst * ReachableStore=llvm::dyn_cast<llvm::StoreInst>(ReachableIns)){
          llvm::Value * storeValuet= ReachableStore->getOperand(1);
          if(const llvm::GlobalValue* G=llvm::dyn_cast<llvm::GlobalValue>(storeValuet)){//If the store object has a name, it is a global variable and we print it directly.
            if(!llvm::dyn_cast<llvm::Function>(storeValuet)){
                //resultstream<<"FunctionName:"<<funcname<<","<<"Global Variable:"<<storeValuet->getName().str()<<"\n";
              std::string GB = "Global Variable:"+storeValuet->getName().str();
              cstream<<"FunctionName:"<<IcmpFunction->getName().str()<<","<<GB<<",corressponding ctl_variable:"<<GlobalName<<"\n";
              //Resource->push_back(GB); 
            }
          }
          if(llvm::Instruction * storeInstruction = llvm::dyn_cast<llvm::Instruction>(storeValuet)){//If the store is not a global variable, we need to use GEP to see where the variable comes from.
            if(llvm::GetElementPtrInst * storegep= llvm::dyn_cast<llvm::GetElementPtrInst>(storeInstruction)){
	    //If the variable is a structure member, we print the structure type. The processing here is flawed. If other statements pass between store and GEP, I cannot handle it here.
              llvm::Type* gepType= storegep->getSourceElementType();
              //resultstream<<"FunctionName:"<<funcname<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*gepType)<<"\n";
              std::string PS = "ProtectedStruct:"+ReturnTypeRefine(*gepType);
                cstream<<"FunctionName:"<<IcmpFunction->getName().str()<<","<<PS<<",corressponding ctl_variable:"<<GlobalName<<"\n";
//              if(!StructHasNamespace(gepType,funcname)){
//                Resource->push_back(PS);
//              }
            }
          }
        }
        }
      }
    }*/

  }
  
}


void CtlTableAnalysis::StoreFuncResource(llvm::Function* minganFunc,llvm::GlobalValue* G){
    if(CtlFuncResource.find(minganFunc) != CtlFuncResource.end()){
        std::vector<std::string> Resource = CtlFuncResource[minganFunc];
        std::string GB = "Global Variable:" + G->getName().str();
        Resource.push_back(GB);
        CtlFuncResource[minganFunc] = Resource;
    }else{
        std::vector<std::string> NewResource;
        std::string GB = "Global Variable:" + G->getName().str();
        NewResource.push_back(GB);
        CtlFuncResource[minganFunc] = NewResource;
    }
}

void CtlTableAnalysis::StoreResourceVector(llvm::Function* minganFunc,std::vector<std::string> ResourceVector){
    if(CtlFuncResource.find(minganFunc) != CtlFuncResource.end()){
        std::vector<std::string> Resource = CtlFuncResource[minganFunc];
        for(auto it = ResourceVector.begin();it != ResourceVector.end(); it++){
            Resource.push_back(*it);
        }    
        CtlFuncResource[minganFunc] = Resource;
    }else{
        std::vector<std::string> NewResource;
        for(auto it = ResourceVector.begin();it != ResourceVector.end(); it++){
            NewResource.push_back(*it);
        } 
        CtlFuncResource[minganFunc] = NewResource;
    }
}

void CtlTableAnalysis::CtlTravse(llvm::Instruction *icmpinst,llvm::Function *F,std::vector<std::string>* Resource,std::string GlobalName){
    std::string testfuncName = F->getName().str();
    if(icmpinst->getOpcode() == llvm::Instruction::GetElementPtr){      //The first case corresponds to the GEP instruction, which means we can get the structure from here.
		    llvm::GetElementPtrInst *gepinst = llvm::dyn_cast<llvm::GetElementPtrInst>(icmpinst);
        llvm::Value *GepOperand = gepinst->getOperand(0);
        if(const llvm::GlobalValue* G=llvm::dyn_cast<llvm::GlobalValue>(GepOperand)){
            cstream<<"FunctionName:"<<testfuncName<<","<<"Global Variable:"<<GepOperand->getName().str()<<",corressponding ctl_variable:"<<GlobalName<<'\n';
            std::string GB = "Global Variable:" + GepOperand->getName().str();
            Resource->push_back(GB);
            return;
        }else{
            llvm::Type *structType = gepinst->getSourceElementType();
            std::string PS = "ProtectedStruct:" + ReturnTypeRefine(*structType);
            cstream<<"FunctionName:"<<testfuncName<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*structType)<<",corressponding ctl_variable:"<<GlobalName<<"\n";
            //if(!StructHasNamespace(structType,testfuncName) && NsStruct.find(ReturnTypeRefine(*structType)) == NsStruct.end()){
                Resource->push_back(PS); 
            //}
        }
        return;
    }

    if(llvm::CallInst *callInst = llvm::dyn_cast<llvm::CallInst>(icmpinst)){              //If it corresponds to callInst, then it means that this Value is the result of call. Can I directly type the return value type of the call function here?
        if(llvm::Function *called = callInst->getCalledFunction()){
            std::string CalledName = called->getName().str();
            if(AllocFunctionNames.find(CalledName) != AllocFunctionNames.end()){
                llvm::Value *kmVar = llvm::dyn_cast<llvm::Value>(callInst);
				        if(!kmVar->use_empty()){                                                //If it is the kmalloc function, find the user of kmalloc
					          for(llvm::Value::use_iterator UB=kmVar->use_begin(),UE=kmVar->use_end();UB!=UE;++UB){
							  //Under normal circumstances, there is bitcast in the user immediately following kmalloc to convert the i8* allocated by kamalloc into a real structure. So we only need to find the bitcast statement in the first user
						            llvm::User* user=UB->getUser();                                    
						            if(llvm::Instruction* userInst = llvm::dyn_cast<llvm::Instruction>(user)){      
							              if(userInst->getOpcode() == llvm::Instruction::BitCast){        //Find the bitcast statement
								                llvm::Value *userVar = llvm::dyn_cast<llvm::Value>(userInst);
								                llvm::Type *userType = userVar->getType();                  
								                cstream<<"FunctionName:"<<testfuncName<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*userType)<<",corressponding ctl_variable:"<<GlobalName<<"\n";
                                std::string PS = "ProtectedStruct:" + ReturnTypeRefine(*userType);
                                //if(!StructHasNamespace(userType,testfuncName) && NsStruct.find(ReturnTypeRefine(*userType)) == NsStruct.end()){
                                    Resource->push_back(PS);
                               // }
							              }
						            }
					          }
				        }
            }else{
                for(auto argit = callInst->arg_begin();argit != callInst->arg_end(); argit++){
                    llvm::Value *argValue = llvm::dyn_cast<llvm::Value>(argit);
                    if(llvm::Instruction *argInst = llvm::dyn_cast<llvm::Instruction>(argValue)){
                        if(llvm::dyn_cast<llvm::PHINode>(argInst)){
                          continue;
                        }
                        CtlTravse(argInst,F,Resource,GlobalName);
                    }
                }
            }
        }
        return;
	  }  



    for (auto operand = icmpinst->operands().begin();operand != icmpinst->operands().end();++operand){  //If the corresponding statement is an ordinary statement, its operands are traversed normally to recurse.
		    llvm::Value *opValue = llvm::dyn_cast<llvm::Value>(operand);
		    if(llvm::Instruction *opInst = llvm::dyn_cast<llvm::Instruction>(opValue)){
            if(llvm::dyn_cast<llvm::PHINode>(opInst)){
                continue;
            }
            /*if(llvm::dyn_cast<llvm::CallInst>(opInst)){
                continue;
            }*/
            CtlTravse(opInst,F,Resource,GlobalName);
        }
    }
    
    return; 
}

void CtlTableAnalysis::FindGlobalIcmp(llvm::Value::use_iterator UB,llvm::Value* G){
    llvm::User* user=UB->getUser();
    std::vector<llvm::Instruction*> travseglobal_stack;
		llvm::Value *UserValue = llvm::dyn_cast<llvm::Value>(*UB);
    if(llvm::Instruction *testInst=llvm::dyn_cast<llvm::Instruction>(user)){
      ctlinsstream<<"the User of "<<G->getName().str()<<" is an Instruction"<<"\n";
      ctlinsstream<<*testInst<<"\n";
      ctlinsstream<<"the function name of User is"<<testInst->getFunction()->getName().str()<<"\n";
    }
    if(llvm::ICmpInst* userInst = llvm::dyn_cast<llvm::ICmpInst>(user)){     //Directly icmp, call icmpNr
      if(!userInst->isEquality()){
//        cstream<<"the user is IcmpInstruction"<<"\n";
//        std::cout<<"Find the files_stat's user alloc_empty_file"<<std::endl;
        IcmpNr(userInst,G,G->getName().str());//。
      }
//      if(userInst->isEquality()){
//        cstream<<"the user is Equal ICMP Instruction"<<"\n";
//      }
//      if((!userInst->isEquality())&&(userInst->getFunction()->getName().str()=="alloc_empty_file")){
//        std::cout<<"Find the files_stat's user alloc_empty_file"<<std::endl;
//        IcmpNr(userInst,G,G->getName().str());//change it to search in the function where userInst is located, find all the instructions that have dependencies with it, and collect them.
//      }
		}
    if(llvm::LoadInst* userInst = llvm::dyn_cast<llvm::LoadInst>(user)){   //It is loadinst, recursively looking for icmp.
//      cstream<<"the user is LoadInstruction"<<"\n";
//      if(userInst->getFunction()->getName().str()=="alloc_empty_file"){
        //llvm::outs()<<*userInst<<"\n";
        llvm::Value* InstValue = llvm::dyn_cast<llvm::Value>(userInst);
        if(!InstValue->user_empty()){
          for(auto user_it=InstValue->user_begin();user_it!=InstValue->user_end();user_it++){
            TravseGlobal(*user_it,InstValue,G->getName().str(),travseglobal_stack);
          }
        }
//        TravseGlobal(InstValue,G,G->getName().str());
//      }
    }
    if(llvm::StoreInst* userInst = llvm::dyn_cast<llvm::StoreInst>(user)){
//      cstream<<"the user is StoreInstruction"<<"\n";
//      if(userInst->getFunction()->getName().str()=="alloc_empty_file"){
        if(userInst->getOperand(0) == UserValue){
            llvm::Value* InstValue = llvm::dyn_cast<llvm::Value>(userInst);
            if(!InstValue->user_empty()){
              for(auto user_it=InstValue->user_begin();user_it!=InstValue->user_end();user_it++){
                TravseGlobal(*user_it,InstValue,G->getName().str(),travseglobal_stack);
              } 
            }
//            TravseGlobal(userInst,G,G->getName().str());
        }
//    }
    }
    if(llvm::GetElementPtrInst* gepInst = llvm::dyn_cast<llvm::GetElementPtrInst>(user)){
//      cstream<<"the user is GepInstruction"<<"\n";
        llvm::Value* InstValue = llvm::dyn_cast<llvm::Value>(gepInst);
//        if(gepInst->getFunction()->getName().str()=="alloc_empty_file"){
        /*llvm::outs()<<"FuncName: "<<gepInst->getFunction()->getName().str()<<"\n"<<*gepInst<<"\n";
        if(gepInst->getFunction()->getName().str() == "xt_find_table_lock"){
          llvm::outs()<<"UserValue: "<<*InstValue<<"\n";
          if(!InstValue->user_empty()){
              for(auto user_it = InstValue->user_begin();user_it != InstValue->user_end();++user_it){
                  llvm::Value* UU = *user_it;
                  llvm::outs()<<"TravseValue: "<<*UU<<"\n";
              }
          }
        }*/
        if(!InstValue->user_empty()){
          for(auto user_it=InstValue->user_begin();user_it!=InstValue->user_end();user_it++){
            TravseGlobal(*user_it,InstValue,G->getName().str(),travseglobal_stack);
          } 
        }
//        TravseGlobal(InstValue,G,G->getName().str());
//    }
    }
    if(llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(user)){
//      cstream<<"We don't resolve CallInst Now"<<"\n";
//      if(callInst->getFunction()->getName().str()=="alloc_empty_file"){
        //llvm::outs()<<"FuncName: "<<callInst->getFunction()->getName().str()<<"\n"<<*callInst<<"\n";
        if(llvm::Value *ReturnValue = llvm::dyn_cast<llvm::CallInst>(callInst)){
          if(!ReturnValue->user_empty()){
            for(auto user_it=ReturnValue->user_begin();user_it!=ReturnValue->user_end();user_it++){
              TravseGlobal(*user_it,ReturnValue,G->getName().str(),travseglobal_stack);
            } 
          }  
//            TravseGlobal(ReturnValue,G,G->getName().str());
        }
//    }
    }
    if(llvm::ReturnInst* retInst = llvm::dyn_cast<llvm::ReturnInst>(user)){
//      cstream<<"We don't resolve ReturnInst Now"<<"\n";
    }
}
