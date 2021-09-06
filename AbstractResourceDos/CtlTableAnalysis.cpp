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
  Test()目前相当于main函数
*/
void CtlTableAnalysis::Test(){
    FindLockAndAtomic FLA = FindLockAndAtomic(*_module_);
    FLA.FindLock();                
    AllocFunctionNames = FLA.GetAllocFunction();
    AllAtomic();//获得Atomic和PerCPU函数集合
    AllPerCPU();

    CollectCtlGlobal();                 //收集CtlGlobal            

    for(auto it = CtlGlobal.begin();it != CtlGlobal.end();it++){              //遍历CtlGlobal
        llvm::Value * G = *it;
//        if(G->getName().str()=="files_stat"){
//          std::cout<<"find files_stat's user"<<std::endl;
        //cstream<<"*********"<<G->getName().str()<<"**********"<<"\n";
        if(!G->use_empty()){
          for(llvm::Value::use_iterator UB=G->use_begin(),UE=G->use_end();UB!=UE;++UB){  //遍历Global的user
            llvm::User* user=UB->getUser();
		        llvm::Value *UserValue = llvm::dyn_cast<llvm::Value>(*UB);
//            llvm::Instruction* TestInstruction = llvm::dyn_cast<llvm::Instruction>(user);
//            std::cout<<"the user of "<<G->getName().str()<<" is "<<&TestInstruction<<"\n";
            FindGlobalIcmp(UB,G);
            if(llvm::ConstantExpr *userexp=llvm::dyn_cast<llvm::ConstantExpr>(user)){ //这里加上了它user是个Expr的判断，通过expr的user找到丢失的files.stat
              llvm::Value * testu=llvm::dyn_cast<llvm::Value>(userexp);
              for(llvm::Value::use_iterator UBB=testu->use_begin(),UBE=testu->use_end();UBB!=UBE;++UBB){//这里的作用是找constantExpr的user
             // cstream<<"便利constantexpr的每个user"<<"\n";
                  FindGlobalIcmp(UBB,G);
              }
            }
            //cstream<<*UserValue<<"\n";
          }       
        }
//    }//这里对应上面的if(G.getName().str()=="files_stat")，调试用
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
                ArgIsInteger=false;//这里的逻辑是，对于Atomic,一方面参数中包含struct.atomic_t,另外参数个数不超过三个，除了atomi_t之外，其他参数类型都是整数
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
  CollectCtlGlobal()函数收集所有sysctl Global，存入CtlGlobal中
*/
void CtlTableAnalysis::CollectCtlGlobal(){
	  for(auto &GlobalV : _module_->getGlobalList()){
//    if(GlobalV.getName().str()=="fs_table"){//调试用
      if(GlobalV.getValueType()->isArrayTy()){
        llvm::Type* t=GlobalV.getValueType()->getArrayElementType();
        std::string type_str;
        llvm::raw_string_ostream rso(type_str);
        t->print(rso);
        //fstream<<"全局变量"<<GlobalV.getName().str()<<"的类型为"<<type_str<<"\n";
//        std::cout<<t->getStructName().str()<<std::endl;
        if(t->isStructTy()){
          if(GlobalV.hasInitializer()){ //如果比如fs_table被初始化了
            llvm::Constant *GlobalVIni=GlobalV.getInitializer();//我就去拿一下它右边初始化的内容
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
//    }//这里对应上面的tablename=fs_table
	}


   /* for(auto &GlobalV : _module_->getGlobalList()){
//		std::cout<<"the global variable of module is"<<GlobalV.getName().str()<<std::endl;
//    if(GlobalV.getName().str()=="fs_table"){//调试用
      if(GlobalV.getValueType()->isArrayTy()){
        llvm::Type* t=GlobalV.getValueType()->getArrayElementType();
//        std::cout<<t->getStructName().str()<<std::endl;
        if(t->isStructTy()){
          if(GlobalV.hasInitializer()){ //如果比如fs_table被初始化了
            llvm::Constant *GlobalVIni=GlobalV.getInitializer();//我就去拿一下它右边初始化的内容
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
                  if(test->hasName()){  //这里确认它是个全局变量                 
                    //std::cout<<test->getName().str()<<std::endl;//打印出全局变量名
                    CtlGlobal.insert(test);
                  }
                  if(llvm::dyn_cast<llvm::Value>(test->stripPointerCasts())){
                    llvm::Value * stripV=test->stripPointerCasts();
                    if(stripV->hasName()){
                      char *b=".str";
                      if(strstr(stripV->getName().str().c_str(),b)!=NULL){
                        continue;
                      }
                      //std::cout<<stripV->getName().str()<<std::endl;//这里基本抓出了ctl_table中的所有全局变量
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
//CollectCtlGlobal到此为止，这个函数没有问题，是用来收集变量的
/*
  找到Load语句后，递归找他的User,如果存在Icmp语句，则调用IcmpNr
*/
void CtlTableAnalysis::TravseGlobal(llvm::Value *LoadValue,llvm::Value *OldValue,std::string GlobalName,std::vector<llvm::Instruction*> &travseglobal_stack){//这个可能写的简单了,找到ctl_table参与的比较
//不会这么简单,这个函数要不重新写一下.
//这里找到了第一个问题,这里传进来的是个value,我在这里又把它展开回去了,所以之前对gep/load/store的处理等于做了无用功,这里修改一下,在调用TravseGlobal之前传user看看
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
    if (travseglobal_stack.size() == 20){//这里设置了递归深度为10,用来限制规模和死循环
      travseglobal_stack.pop_back();
      return;
    }
    travseglobal_stack.push_back(tmpInst);
    if(llvm::ICmpInst *Icmp = llvm::dyn_cast<llvm::ICmpInst>(LoadValue)){
      if(Icmp->isEquality()==false){
        std::cout<<"Inside ICMP SOLVE"<<std::endl;
      IcmpNr(Icmp,OldValue,GlobalName);//这里的OldValue处理存疑,跑步回来接着改
      //cstream<<"Travse:"<<*Icmp<<"\n";
      return;
      }
    }
    if(tmpInst->getOpcode()==llvm::Instruction::Add||tmpInst->getOpcode()==llvm::Instruction::FAdd||tmpInst->getOpcode()==llvm::Instruction::Sub||tmpInst->getOpcode()==llvm::Instruction::FSub||tmpInst->getOpcode()==llvm::Instruction::Mul||tmpInst->getOpcode()==llvm::Instruction::FMul||tmpInst->getOpcode()==llvm::Instruction::UDiv||tmpInst->getOpcode()==llvm::Instruction::SDiv||tmpInst->getOpcode()==llvm::Instruction::FDiv||tmpInst->getOpcode()==llvm::Instruction::URem||tmpInst->getOpcode()==llvm::Instruction::SRem||tmpInst->getOpcode()==llvm::Instruction::FRem||tmpInst->getOpcode()==llvm::Instruction::Shl||tmpInst->getOpcode()==llvm::Instruction::LShr||tmpInst->getOpcode()==llvm::Instruction::AShr||tmpInst->getOpcode()==llvm::Instruction::And||tmpInst->getOpcode()==llvm::Instruction::Or||tmpInst->getOpcode()==llvm::Instruction::Xor){
//这里的意思是,如果llvm Instruction的操作符是加减乘除的算数运算,这里就便利算数运算的结果,就是算数运算的user
      if(!LoadValue->user_empty()){
        //
        std::cout<<"the Function is "<<tmpInst->getFunction()->getName().str()<<", inside 加减乘除"<<std::endl;
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
    if(llvm::PHINode *testphi = llvm::dyn_cast<llvm::PHINode>(LoadValue)){//这里我观察了一下,如果是个phi指令,我不应该拿phi的incoming value,应该拿的是phi的左边
      if(llvm::Value* phiV=llvm::dyn_cast<llvm::Value>(testphi)){
        for(auto user_it= phiV->user_begin();user_it!=phiV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insidePHI solve"<<std::endl;
          TravseGlobal(*user_it,phiV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::SelectInst *testselect=llvm::dyn_cast<llvm::SelectInst>(LoadValue)){//之前发现丢失了对select的处理
      if(llvm::Value *selectV=llvm::dyn_cast<llvm::Value>(testselect)){
        for(auto user_it=selectV->user_begin();user_it!=selectV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideSELECT solve"<<std::endl;
          TravseGlobal(*user_it,selectV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::SExtInst *testsext=llvm::dyn_cast<llvm::SExtInst>(LoadValue)){//之前发现丢失了对sext的处理
      if(llvm::Value *sextV=llvm::dyn_cast<llvm::Value>(testsext)){
        for(auto user_it=sextV->user_begin();user_it!=sextV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideSEXT solve"<<std::endl;
          TravseGlobal(*user_it,sextV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::TruncInst *testtrunc=llvm::dyn_cast<llvm::TruncInst>(LoadValue)){//之前发现丢失了对trunc的处理
      if(llvm::Value *truncV=llvm::dyn_cast<llvm::Value>(testtrunc)){
        for(auto user_it=truncV->user_begin();user_it!=truncV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideTRUNC solve"<<std::endl;
          TravseGlobal(*user_it,truncV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::ZExtInst *testzext=llvm::dyn_cast<llvm::ZExtInst>(LoadValue)){//之前发现丢失了对zext的处理
      if(llvm::Value *zextV=llvm::dyn_cast<llvm::Value>(testzext)){
        for(auto user_it=zextV->user_begin();user_it!=zextV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", inside ZEXT solve"<<std::endl;
          TravseGlobal(*user_it,zextV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::FPTruncInst *testfptrunc=llvm::dyn_cast<llvm::FPTruncInst>(LoadValue)){//之前发现丢失了对fptrunc的处理
      if(llvm::Value *fptruncV=llvm::dyn_cast<llvm::Value>(testfptrunc)){
        for(auto user_it=fptruncV->user_begin();user_it!=fptruncV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideFPTRUNC solve"<<std::endl;
          TravseGlobal(*user_it,fptruncV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::FPExtInst *testfpext=llvm::dyn_cast<llvm::FPExtInst>(LoadValue)){//之前发现丢失了对fpext的处理
      if(llvm::Value *fpextV=llvm::dyn_cast<llvm::Value>(testfpext)){
        for(auto user_it=fpextV->user_begin();user_it!=fpextV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideFPEXT solve"<<std::endl;
          TravseGlobal(*user_it,fpextV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::FPToUIInst *testfpoui=llvm::dyn_cast<llvm::FPToUIInst>(LoadValue)){//之前发现丢失了对fptoui的处理
      if(llvm::Value *fptouiV=llvm::dyn_cast<llvm::Value>(testfpoui)){
        for(auto user_it=fptouiV->user_begin();user_it!=fptouiV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideFPTOUI solve"<<std::endl;
          TravseGlobal(*user_it,fptouiV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::FPToSIInst *testfptosi=llvm::dyn_cast<llvm::FPToSIInst>(LoadValue)){//之前发现丢失了对fptosi的处理
      if(llvm::Value *fptosiV=llvm::dyn_cast<llvm::Value>(testfptosi)){
        for(auto user_it=fptosiV->user_begin();user_it!=fptosiV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideFPTOSI solve"<<std::endl;
          TravseGlobal(*user_it,fptosiV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::UIToFPInst *testuitofp=llvm::dyn_cast<llvm::UIToFPInst>(LoadValue)){//之前发现丢失了对uitofp的处理
      if(llvm::Value *uitofpV=llvm::dyn_cast<llvm::Value>(testuitofp)){
        for(auto user_it=uitofpV->user_begin();user_it!=uitofpV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideUITOFP solve"<<std::endl;
          TravseGlobal(*user_it,uitofpV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::SIToFPInst *testsitofp=llvm::dyn_cast<llvm::SIToFPInst>(LoadValue)){//之前发现丢失了对sitofp的处理
      if(llvm::Value *sitofpV=llvm::dyn_cast<llvm::Value>(testsitofp)){
        for(auto user_it=sitofpV->user_begin();user_it!=sitofpV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideSITOFP solve"<<std::endl;
          TravseGlobal(*user_it,sitofpV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::PtrToIntInst *testptrtoint=llvm::dyn_cast<llvm::PtrToIntInst>(LoadValue)){//之前发现丢失了对ptrtoint的处理
      if(llvm::Value *ptrtointV=llvm::dyn_cast<llvm::Value>(testptrtoint)){
        for(auto user_it=ptrtointV->user_begin();user_it!=ptrtointV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insidePTRTOINT solve"<<std::endl;
          TravseGlobal(*user_it,ptrtointV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::IntToPtrInst *testinttoptr=llvm::dyn_cast<llvm::IntToPtrInst>(LoadValue)){//之前发现丢失了对inttoptr的处理
      if(llvm::Value *inttoptrV=llvm::dyn_cast<llvm::Value>(testinttoptr)){
        for(auto user_it=inttoptrV->user_begin();user_it!=inttoptrV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideINTTOPTR solve"<<std::endl;
          TravseGlobal(*user_it,inttoptrV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::BitCastInst *testbitcast=llvm::dyn_cast<llvm::BitCastInst>(LoadValue)){//之前发现丢失了对bitcast的处理
      if(llvm::Value *bitcastV=llvm::dyn_cast<llvm::Value>(testbitcast)){
        for(auto user_it=bitcastV->user_begin();user_it!=bitcastV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideBITCAST solve"<<std::endl;
          TravseGlobal(*user_it,bitcastV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::AddrSpaceCastInst *testaddrspacecast=llvm::dyn_cast<llvm::AddrSpaceCastInst>(LoadValue)){//之前发现丢失了对addrspacecast的处理
      if(llvm::Value *truncV=llvm::dyn_cast<llvm::Value>(testaddrspacecast)){
        for(auto user_it=truncV->user_begin();user_it!=truncV->user_end();++user_it){
          std::cout<<"the function is"<<tmpInst->getFunction()->getName().str()<<", insideADDRSPACECAST solve"<<std::endl;
          TravseGlobal(*user_it,truncV,GlobalName,travseglobal_stack);
        }
      }
    }
    if(llvm::LoadInst *testload=llvm::dyn_cast<llvm::LoadInst>(LoadValue)){//再加一个load的处理
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
    //travseglobal_stack.pop_back();//如果都不符合，那么栈减１,这里还没想好
    return;
}

/*
  对于一个CtlGlobal,找到他所在的Icmp比较语句后调用IcmpNr函数，函数第一个参数为imcp语句，第二个参数用于避免从Icmp找operand时不找自己，第三个是自己的名字。
  基本思路为找Icmp的另一个operand，如果是Global，取出成pair，如果Call了atomic或perCPU函数，则看这个函数的参数是否为全局变量，是则取出，成pair.
  目前只处理了一层。
*/
/*
3-28:仔细看了看ctl_table的IcmpNr,这个函数其实只处理了两种情况,(1),另一端是一个全局变量G.(2),另一端是个局部变量,这个局部变量的来源是percpu/atomic函数,根本没有处理复杂的,来自
结构体的情况,这个比较函数值得大修一波.
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
//      if(llvm::Instruction* icmpInst = llvm::dyn_cast<llvm::Instruction>(operand)){//比较另一端的结果如果来自于另一个比较
//            std::vector<std::string> ResourceVector;
//            //LockPair LP = LockPair(*_module_);
//            //LP.TravseAllocUser(icmpInst->getFunction(),icmpInst,&ResourceVector);
//            CtlTravse(icmpInst,icmpInst->getFunction(),&ResourceVector,GlobalName);
//            StoreResourceVector(icmpInst->getFunction(),ResourceVector);
//        }
      if(llvm::GlobalValue* G = llvm::dyn_cast<llvm::GlobalValue>(operand)){//这里说明从icmp中找到了第二个全局变量，现在需要从全局变量里面找它的user,再看user是否是个store语句。
     // cstream<<"找到的全局变量名为"<<G->getName().str()<<"\n";
          /*if(!G->use_empty()){
        //cstream<<"找到的全局变量名为"<<G->getName().str()<<"\n";
       // cstream<<"G的use不为空"<<"\n";
              for(llvm::Value::use_iterator UBB=G->use_begin(),UBE=G->use_end();UBB!=UBE;++UBB){//这里的作用是找参与比较的全局变量的user
          //cstream<<"便利比较对象的每个user"<<"\n";
                  if(llvm::User* icmpuser=UBB->getUser()){
                      if(llvm::StoreInst* testStore=llvm::dyn_cast<llvm::StoreInst>(icmpuser)){
                          if(testStore->getOperand(1)==G){
                              llvm::Function* minganFunc=testStore->getFunction();  //如果比较的全局变量的user是一个store instruction,我们就认为它对这个做了修改，修改这个全局变量的函数我们就标记为
                //敏感函数。
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
          //cstream<<",Global Variable:"<<G->getName().str()<<"\n";//这些cstream我是尝试着统计一下一共抓出来多少个counter用的
          //cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<"\n";//这些cstream我是尝试统计一下一共抓出来多少个敏感函数用的
      }

      if(llvm::CallInst *callinst = llvm::dyn_cast<llvm::CallInst>(operand)){
          if(llvm::Function *called = callinst->getCalledFunction()){
              std::string FuncName = called->getName().str();
              if(AtomicFunc.find(FuncName) != AtomicFunc.end() || PerCPUFunc.find(FuncName) != PerCPUFunc.end()){
                  for(auto argit = callinst->arg_begin();argit != callinst->arg_end();++argit){
                      if(llvm::GlobalValue *G = llvm::dyn_cast<llvm::GlobalValue>(argit)){
                        //cstream<<"找到的全局变量名为"<<G->getName().str()<<"\n";
                        if(G->hasName()){
                          //cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<"\n";//这些cstream我是尝试统计一下一共抓出来多少个敏感函数用的
                          //cstream<<",Global Variable:"<<G->getName().str()<<"\n";//这些cstream我是尝试着统计一下一共抓出来多少个counter用的
                          cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<",GlobalVariable"<<G->getName().str()<<",corressponding ctl_variable:"<<GlobalName<<"\n";
                          /*
                          if(!G->use_empty()){
                            //cstream<<"G的user不为空1"<<"\n";
                            for(llvm::Value::use_iterator UBB=G->use_begin(),UBE=G->use_end();UBB!=UBE;++UBB){//这里的作用是找参与比较的全局变量的user
                              //cstream<<"便利比较对象的每个user"<<"\n";
                              if(llvm::User* icmpuser=UBB->getUser()){
                                if(llvm::StoreInst* testStore=llvm::dyn_cast<llvm::StoreInst>(icmpuser)){
                                  if(testStore->getOperand(1)==G){
                                    llvm::Function* minganFunc=testStore->getFunction();  //如果比较的全局变量的user是一个store instruction,我们就认为它对这个做了修改，修改这个全局变量的函数我们就标记为
                                      //敏感函数。
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
                      } else{//这里缺少了如果找到的atomic/percpu参数不是全局变量,而是来自于结构体的情况,现在补上
                        for(llvm::inst_iterator instuser_it= llvm::inst_begin(icmpinst->getFunction());instuser_it!=llvm::inst_end(icmpinst->getFunction());instuser_it++){
                        llvm::Instruction *instgepm= &*instuser_it;
                        if(llvm::GetElementPtrInst *GepInst=llvm::dyn_cast<llvm::GetElementPtrInst>(instgepm)){
                          llvm::Type* userType=GepInst->getSourceElementType();
                          llvm::Value *GepVar=llvm::dyn_cast<llvm::Value>(GepInst);
                          if(!GepVar->user_empty()){
                            for(auto gepuser_it=GepVar->user_begin();gepuser_it!=GepVar->user_end();gepuser_it++){
                              if(llvm::Instruction* tmpInst=llvm::dyn_cast<llvm::Instruction>(*gepuser_it)){
                                if(tmpInst==callinst){
                                  //cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<"\n";//这些cstream我是尝试统计一下一共抓出来多少个敏感函数用的
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
                                            //cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<"\n";//这些cstream我是尝试统计一下一共抓出来多少个敏感函数用的

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
                                                      cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<"\n";//这些cstream我是尝试统计一下一共抓出来多少个敏感函数用的

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
      } else{//这个else的结果是,如果它不是来自于callInst(典型的是atomic/percpu),那么它会来自于哪个结构体的哪个偏移量,
      //这里套用了三层user判断,看看效果再说
          for(llvm::inst_iterator instuser_it= llvm::inst_begin(icmpinst->getFunction());instuser_it!=llvm::inst_end(icmpinst->getFunction());instuser_it++){
            llvm::Instruction *instgepm= &*instuser_it;
            if(llvm::GetElementPtrInst *GepInst=llvm::dyn_cast<llvm::GetElementPtrInst>(instgepm)){
              llvm::Type* userType=GepInst->getSourceElementType();
              llvm::Value *GepVar=llvm::dyn_cast<llvm::Value>(GepInst);
                if(!GepVar->user_empty()){
                  for(auto gepuser_it=GepVar->user_begin();gepuser_it!=GepVar->user_end();gepuser_it++){
                    if(llvm::Instruction* tmpInst=llvm::dyn_cast<llvm::Instruction>(*gepuser_it)){
                      if(tmpInst==icmpinst){
                        cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<"\n";//这些cstream我是尝试统计一下一共抓出来多少个敏感函数用的

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
                                  cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<"\n";//这些cstream我是尝试统计一下一共抓出来多少个敏感函数用的

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
                                            cstream<<"FunctionName:"<<icmpinst->getFunction()->getName().str()<<"\n";//这些cstream我是尝试统计一下一共抓出来多少个敏感函数用的

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
                    for(auto argF=IcmpFunction->arg_begin();argF!=IcmpFunction->arg_end();argF++){//这个判断的作用是，如果arg为Ｆ本身的arg,那么就是对Ｆ传进来的参数做了操作，我们就应该直接打印参数类型。
                      if(llvm::Value * argnamed=llvm::dyn_cast<llvm::Value>(argF)){  
                        if(argnamed==argValue){
                          if(const llvm::GlobalValue* AtomicG=llvm::dyn_cast<llvm::GlobalValue>(argF)){//如果参数有名字，说明是个全局变量，打印参数名字。
                            if(!llvm::dyn_cast<llvm::Function>(argValue)){
                              //resultstream<<"FunctionName:"<<funcname<<","<<"Global Variable:"<<argF->getName().str()<<"\n";
                              std::string GB = "Global Variable:"+argF->getName().str();
                              cstream<<"FunctionName:"<<IcmpFunction->getName().str()<<","<<GB<<",corressponding ctl_variable:"<<GlobalName<<"\n";
//                            StoreFuncResource();
//                            Resource->push_back(GB);
                            }
                          }
                          llvm::Type* argtype= argValue->getType();//如果函数参数没名字，说明是个局部变量参数，那我这里就把参数类型打印出来就好。
                          //resultstream<<"FunctionName:"<<funcname<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*argtype)<<"\n";
                          std::string PS = "ProtectedStruct:"+ReturnTypeRefine(*argtype);
//                        if(!StructHasNamespace(argtype,funcname)){
                          cstream<<"FunctionName:"<<IcmpFunction->getName().str()<<","<<PS<<",corressponding ctl_variable:"<<GlobalName<<"\n";
//                        Resource->push_back(PS);
//                        }
    						        }
    					        }
                    }
                    if(const llvm::GlobalValue* AtomicG=llvm::dyn_cast<llvm::GlobalValue>(arglb)){//如果call语句的参数为全局变量，直接打印。
                      if(!llvm::dyn_cast<llvm::Function>(argValue)){
                        //resultstream<<"FunctionName:"<<funcname<<","<<"Global Variable:"<<AtomicG->getName().str()<<"\n";
                        std::string GB = "Global Variable:"+AtomicG->getName().str();
                        cstream<<"FunctionName:"<<IcmpFunction->getName().str()<<","<<GB<<",corressponding ctl_variable:"<<GlobalName<<"\n";
                        //Resource->push_back(GB);
                      }    
                    }
    			        if(llvm::Instruction *argInst=llvm::dyn_cast<llvm::Instruction>(arglb)){//如果既不是全局变量，又不是Ｆ函数的参数，本身又不是个结构体，那么需要分析内部call的参数来源。这里还在改，没有最终确定方案
//    			        resultstream<<"testforTravseAllocUser"<<"\n";
//                    TravseAllocUser(funcName,argInst,Resource);//这里是传入lock函数参数的Instruction和F的名字，因为你如果在lock/unlock中间调用了call语句，那么要不是call的参数产生了变化，要不就是call的返回值存储到了哪里。
                  }                                    //所以这里对应抓到call参数，遍历call语句的每个参数，找到参数对应的资源（如果是结构体，打印结构体来源，如果是变量，需要去看变量来自哪里，如果来自函数参数，打印变量，如果是全局变量，打印变量名)													
			        }
          }
    		}
        }

        if(llvm::Value * callreturnValue=llvm::dyn_cast<llvm::Value>(ReachableCall)){
            if(!callreturnValue->user_empty()){  //少加了！号
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
          if(const llvm::GlobalValue* G=llvm::dyn_cast<llvm::GlobalValue>(storeValuet)){//如果store的对象有名字，这是个全局变量，我们直接打印。
            if(!llvm::dyn_cast<llvm::Function>(storeValuet)){
                //resultstream<<"FunctionName:"<<funcname<<","<<"Global Variable:"<<storeValuet->getName().str()<<"\n";
              std::string GB = "Global Variable:"+storeValuet->getName().str();
              cstream<<"FunctionName:"<<IcmpFunction->getName().str()<<","<<GB<<",corressponding ctl_variable:"<<GlobalName<<"\n";
              //Resource->push_back(GB); 
            }
          }
          if(llvm::Instruction * storeInstruction = llvm::dyn_cast<llvm::Instruction>(storeValuet)){//如果store的不是个全局变量，我们要用GEP看这个变量来自哪里
            if(llvm::GetElementPtrInst * storegep= llvm::dyn_cast<llvm::GetElementPtrInst>(storeInstruction)){
              llvm::Type* gepType= storegep->getSourceElementType();//如果这个变量是结构体成员，我们打印结构体类型。这里的处理是有缺陷的，如果store到GEP之间经过了别的语句，我这里就处理不出来了。
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
    if(icmpinst->getOpcode() == llvm::Instruction::GetElementPtr){      //第一种情况，对应的是GEP指令，说明我们可以从这获取结构体了。
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

    if(llvm::CallInst *callInst = llvm::dyn_cast<llvm::CallInst>(icmpinst)){              //如果对应的是 callInst，那么说明这个Value就是call的结果，我这里直接把call函数的返回值类型打出来？
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



    for (auto operand = icmpinst->operands().begin();operand != icmpinst->operands().end();++operand){  //如果对应的是普通语句，则正常遍历他的operands来递归。
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
    if(llvm::ICmpInst* userInst = llvm::dyn_cast<llvm::ICmpInst>(user)){     //直接是icmp,调用icmpNr
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
//        IcmpNr(userInst,G,G->getName().str());//这里需要改成去userInst所在的function里面找，找到所有和它存在dependency的instruction，收集起来。
//      }
		}
    if(llvm::LoadInst* userInst = llvm::dyn_cast<llvm::LoadInst>(user)){   //是loadinst，递归找icmp
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
