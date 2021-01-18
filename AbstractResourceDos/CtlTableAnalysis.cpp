#include "CtlTableAnalysis.h"

std::error_code ec2;
llvm::raw_fd_ostream cstream("./CtlTable-finder.txt", ec2, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);

CtlTableAnalysis::CtlTableAnalysis(llvm::Module &module){
    _module_ = &module;
}

/*
  Test()目前相当于main函数
*/
void CtlTableAnalysis::Test(){
    FindLockAndAtomic FLA = FindLockAndAtomic(*_module_);
    FLA.FindLock();
    AtomicFunc = FLA.GetAtomicFunc();
    PerCPUFunc = FLA.GetPerCPU();                  //获得Atomic和PerCPU函数集合

    CollectCtlGlobal();                 //收集CtlGlobal            

    for(auto it = CtlGlobal.begin();it != CtlGlobal.end();it++){              //遍历CtlGlobal
        llvm::Value * G = *it;
        //cstream<<"*********"<<G->getName().str()<<"**********"<<"\n";
        if(!G->use_empty()){
          for(llvm::Value::use_iterator UB=G->use_begin(),UE=G->use_end();UB!=UE;++UB){  //遍历Global的user
            llvm::User* user=UB->getUser();
					  llvm::Value *UserValue = llvm::dyn_cast<llvm::Value>(*UB);
            if(llvm::ICmpInst* userInst = llvm::dyn_cast<llvm::ICmpInst>(user)){     //直接是icmp,调用icmpNr
              if(!userInst->isEquality()){
                IcmpNr(userInst,G,G->getName().str());
              }
						}
            if(llvm::LoadInst* userInst=llvm::dyn_cast<llvm::LoadInst>(user)){   //是loadinst，递归找icmp
                //llvm::outs()<<*userInst<<"\n";
                TravseGlobal(UserValue,G,G->getName().str());
            }
            if(llvm::ConstantExpr *userexp=llvm::dyn_cast<llvm::ConstantExpr>(user)){ //这里加上了它user是个Expr的判断，通过expr的user找到丢失的files.stat
              llvm::Value * testu=llvm::dyn_cast<llvm::Value>(userexp);
             // cstream<<"isConstantExpr"<<"\n";
              //std::cout<<""<<testu->getNumUses()<<std::endl;
              for(llvm::Value::use_iterator UBB=testu->use_begin(),UBE=testu->use_end();UBB!=UBE;++UBB){//这里的作用是找constantExpr的user
             // cstream<<"便利constantexpr的每个user"<<"\n";
                if(llvm::User* expuser=UBB->getUser()){
                  if(llvm::ICmpInst *userInst=llvm::dyn_cast<llvm::ICmpInst>(expuser)){
                    if(!userInst->isEquality()){
                     // cstream<<"通过constExpr找到的，icmp比较语句为"<<*userInst<<"\n";
                      IcmpNr(userInst,G,G->getName().str());
                    }
                  }
                  if(llvm::LoadInst* userInst=llvm::dyn_cast<llvm::LoadInst>(expuser)){
                    //cstream<<"通过constExpr找到的,LoadInst为"<<*userInst<<"\n";
                    llvm::Value * exploadV=llvm::dyn_cast<llvm::Value>(userInst);
                    TravseGlobal(exploadV,G,G->getName().str());
                  }
                }
              }
            }
            //cstream<<*UserValue<<"\n";
          }       
        }
    }
}

/*
  CollectCtlGlobal()函数收集所有sysctl Global，存入CtlGlobal中
*/
void CtlTableAnalysis::CollectCtlGlobal(){
    for(auto &GlobalV : _module_->getGlobalList()){
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
    }
}

/*
  找到Load语句后，递归找他的User,如果存在Icmp语句，则调用IcmpNr
*/
void CtlTableAnalysis::TravseGlobal(llvm::Value *LoadValue,llvm::Value *OldValue,std::string GlobalName){
    if(llvm::ICmpInst *Icmp = llvm::dyn_cast<llvm::ICmpInst>(LoadValue)){
      if(Icmp->isEquality()==false){
      IcmpNr(Icmp,OldValue,GlobalName);
      //cstream<<"Travse:"<<*Icmp<<"\n";
      return ;
      }
    }

    if(llvm::dyn_cast<llvm::PHINode>(LoadValue)){
      return ;
    }

    if(!LoadValue->user_empty()){
      for(auto user_it = LoadValue->user_begin();user_it != LoadValue->user_end();++user_it){
          TravseGlobal(*user_it,LoadValue,GlobalName);
      }
    }

    return;
}

/*
  对于一个CtlGlobal,找到他所在的Icmp比较语句后调用IcmpNr函数，函数第一个参数为imcp语句，第二个参数用于避免从Icmp找operand时不找自己，第三个是自己的名字。
  基本思路为找Icmp的另一个operand，如果是Global，取出成pair，如果Call了atomic或perCPU函数，则看这个函数的参数是否为全局变量，是则取出，成pair.
  目前只处理了一层。
*/
void CtlTableAnalysis::IcmpNr(llvm::Instruction *icmpinst,llvm::Value *OldValue,std::string GlobalName){
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
      if(llvm::GlobalValue* G = llvm::dyn_cast<llvm::GlobalValue>(operand)){//这里说明从icmp中找到了第二个全局变量，现在需要从全局变量里面找它的user,再看user是否是个store语句。
     // cstream<<"找到的全局变量名为"<<G->getName().str()<<"\n";
      if(!G->use_empty()){
        //cstream<<"找到的全局变量名为"<<G->getName().str()<<"\n";
       // cstream<<"G的use不为空"<<"\n";
        for(llvm::Value::use_iterator UBB=G->use_begin(),UBE=G->use_end();UBB!=UBE;++UBB){//这里的作用是找参与比较的全局变量的user
          //cstream<<"便利比较对象的每个user"<<"\n";
          if(llvm::User* icmpuser=UBB->getUser()){
            if(llvm::StoreInst* testStore=llvm::dyn_cast<llvm::StoreInst>(icmpuser)){
              if(testStore->getOperand(1)==G){
                llvm::Function* minganFunc=testStore->getFunction();  //如果比较的全局变量的user是一个store instruction,我们就认为它对这个做了修改，修改这个全局变量的函数我们就标记为
                //敏感函数。
                StoreFuncResource(minganFunc,G);
                cstream<<"FunctionName:"<<minganFunc->getName().str()<<",Global Variable"<<G->getName().str()<<"\n";

              }
            }
            if(llvm::CallInst* testCall=llvm::dyn_cast<llvm::CallInst>(icmpuser)){
              llvm::Function* called=testCall->getCalledFunction();
              llvm::Function* minganFunc=testCall->getFunction();
              if(called->onlyReadsMemory()==false){
                StoreFuncResource(minganFunc,G);
                cstream<<"FunctionName:"<<minganFunc->getName().str()<<",Global Variable"<<G->getName().str()<<"\n";
              }
            }
          }
        }
      }
//          std::pair<std::string,std::string> Pair;
//          Pair.first = GlobalName;
//          Pair.second = G->getName().str();
//          CtlPair->insert(Pair);
//          llvm::outs()<<GlobalName<<" compare with  "<<G->getName().str()<<"\n";
      }
      if(llvm::CallInst *callinst = llvm::dyn_cast<llvm::CallInst>(operand)){
          if(llvm::Function *called = callinst->getCalledFunction()){
              std::string FuncName = called->getName().str();
              if(AtomicFunc.find(FuncName) != AtomicFunc.end() || PerCPUFunc.find(FuncName) != PerCPUFunc.end()){
                  for(auto argit = callinst->arg_begin();argit != callinst->arg_end();++argit){
                      if(llvm::GlobalValue *G = llvm::dyn_cast<llvm::GlobalValue>(argit)){
                        //cstream<<"找到的全局变量名为"<<G->getName().str()<<"\n";
                        if(G->hasName()){
                          if(!G->use_empty()){
                            //cstream<<"G的user不为空1"<<"\n";
                            for(llvm::Value::use_iterator UBB=G->use_begin(),UBE=G->use_end();UBB!=UBE;++UBB){//这里的作用是找参与比较的全局变量的user
                              //cstream<<"便利比较对象的每个user"<<"\n";
                              if(llvm::User* icmpuser=UBB->getUser()){
                                if(llvm::StoreInst* testStore=llvm::dyn_cast<llvm::StoreInst>(icmpuser)){
                                  if(testStore->getOperand(1)==G){
                                    llvm::Function* minganFunc=testStore->getFunction();  //如果比较的全局变量的user是一个store instruction,我们就认为它对这个做了修改，修改这个全局变量的函数我们就标记为
                                      //敏感函数。
                                      StoreFuncResource(minganFunc,G);
                                      cstream<<"FunctionName:"<<minganFunc->getName().str()<<",Global Variable"<<G->getName().str()<<"\n";
                                  }
                                }
                                if(llvm::CallInst* testCall=llvm::dyn_cast<llvm::CallInst>(icmpuser)){
                                  llvm::Function* called=testCall->getCalledFunction();
                                  llvm::Function* minganFunc=testCall->getFunction();
                                  if(called->onlyReadsMemory()==false){
                                    StoreFuncResource(minganFunc,G);
                                    cstream<<"FunctionName:"<<minganFunc->getName().str()<<",Global Variable"<<G->getName().str()<<"\n";
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
                        }
                      }
                  }
              }
          }
      }

    }
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
