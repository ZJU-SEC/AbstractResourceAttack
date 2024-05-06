#include "FindLockAndAtomic.h"
#include "llvm/IR/InstIterator.h"

FindLockAndAtomic::FindLockAndAtomic(llvm::Module &module){ //Because our collection is obtained by all functions in the module, we pass the module in here.
    _module_ = &module;
}

void FindLockAndAtomic::Test(){
    AllAtomic();
    AllPerCPU();
    for(auto it = atomic_function.begin();it != atomic_function.end();it ++){
        std::cout<<"atomic_function: "<<*it<<std::endl;
    }
    for(auto it = percpu_function.begin();it != percpu_function.end();it ++){
        std::cout<<"percpu_function: "<<*it<<std::endl;
    }
}

void FindLockAndAtomic::FindLock(){ //This is used to set the total spin/mutex/rw/percpu/atomic/alloc func collection.
    AllSpin();
    AllMutex();
    AllWrite();
    AllPerCPU();
    AllAtomic();
    AllAllocFunction();
    GenerateRlimit();
}

void FindLockAndAtomic::AllSpin(){
//The structure here is the same. GenerateRawSpinLock is used to find the basic lock, the function starting with Set is used to add locks and unlock.
//The BasicAndUpper function is used to find the wrapper and merge the basic lock functions to get the set we want.
    std::set<std::string> SpinLockSet;
    std::set<std::string> SpinUnlockSet;
    std::set<std::string> SpinBasicLockSet;
    GenerateRawSpinLock();      
    GenerateBasicSpinLock();
    SetRawSpin(&SpinLockSet,&SpinUnlockSet);
    SetSpin(&SpinLockSet,&SpinUnlockSet);
    SpinBasicLockSet.insert(basic_spin_lock.begin(),basic_spin_lock.end());
    SpinBasicLockSet.insert(basic_raw_spin_lock.begin(),basic_raw_spin_lock.end());
    BasicAndUpperLockSet(SpinLockSet,SpinUnlockSet,SpinBasicLockSet,&spin_lock,&spin_unlock);
}

void FindLockAndAtomic::AllMutex(){
    std::set<std::string> MutexLockSet;
    std::set<std::string> MutexUnlockSet;
    GenerateBasicMutexLock();
    SetMutex(&MutexLockSet,&MutexUnlockSet);
    BasicAndUpperLockSet(MutexLockSet,MutexUnlockSet,basic_mutex_lock,&mutex_lock,&mutex_unlock);
}

void FindLockAndAtomic::AllWrite(){
    std::set<std::string> WriteLockSet;
    std::set<std::string> WriteUnlockSet;
    GenerateBasicRWLock();
    SetWriteLock(&WriteLockSet,&WriteUnlockSet);
    BasicAndUpperLockSet(WriteLockSet,WriteUnlockSet,basic_write_lock,&write_lock,&write_unlock);
}

void FindLockAndAtomic::AllPerCPU(){
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
                    if(ArgIsInteger==true && (FunName.find("read") == -1) && (FunName.find("sync") == -1) && (FunName.find("compare") == -1)){
                        percpu_function.insert(FunName);
                    }
                    if(ArgIsInteger==true){
                        percpu_function_not_change.insert(FunName);
                    }
				}
            }
        }
    }
}

void FindLockAndAtomic::AllAtomic(){
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
                                ArgIsInteger=false;//for Atomic, the parameters include struct.atomic_t, and the number of parameters does not exceed three. Except for atomi_t, the other parameter types are all integers.
								break;
							}
						}
					}
                    if(ArgIsInteger==true && (F.getName().str().find("read") == -1)){
                        atomic_function.insert(F.getName().str());
                    }
                    if(ArgIsInteger==true){
                        atomic_function_not_change.insert(F.getName().str());
                    }
				}
            }
        }
    }

}

void FindLockAndAtomic::AllAllocFunction(){
    auto &functionlist = _module_->getFunctionList();
    for(auto &F : functionlist){
        for(llvm::Function::arg_iterator argb=F.arg_begin(), arge=F.arg_end();argb!=arge;++argb){
//	bool HasBitCast = false;//If there is a bitcast statement, because the example acpi_os_delete_raw_lock is found, this will convert the passed struct.raw_spin_lock into i8*. We will remove this situation here.
            std::string type_str;
            llvm::Type* tp = argb->getType();
            llvm::raw_string_ostream rso(type_str);
            tp->print(rso);
            if(rso.str() == "%struct.kmem_cache*"){
				std::int32_t argcount=0;
				for(llvm::Function::arg_iterator argcountb=F.arg_begin(),argcounte=F.arg_end();argcountb!=argcounte;++argcountb){
						argcount++;
				}
                AllocFunctionNames.insert(F.getName().str());
            }
        }
    }  
}

void FindLockAndAtomic::GenerateRawSpinLock(){
    auto &functionlist = _module_->getFunctionList();
    for(auto &F : functionlist){
        for(llvm::Function::arg_iterator argb=F.arg_begin(), arge=F.arg_end();argb!=arge;++argb){
		//If there is a bitcast statement, because the example acpi_os_delete_raw_lock is found, this will convert the passed struct.raw_spin_lock into i8*. We will remove this situation here.
		bool HasBitCast = false;
                std::string type_str;
                llvm::Type* tp = argb->getType();
                llvm::raw_string_ostream rso(type_str);
                tp->print(rso);
                if(rso.str() == "%struct.raw_spinlock*"){
					std::int32_t argcount=0;
					for(llvm::Function::arg_iterator argcountb=F.arg_begin(),argcounte=F.arg_end();argcountb!=argcounte;++argcountb){
							argcount++;
					}

					if(llvm::Value *ArgV = llvm::dyn_cast<llvm::Value>(argb)){
						if(!ArgV->user_empty()){
							for(auto ArgUser=ArgV->user_begin();ArgUser!=ArgV->user_end();ArgUser++){
								if(llvm::BitCastInst *bitcastinst = llvm::dyn_cast<llvm::BitCastInst>(*ArgUser)){
									HasBitCast = true;//This is the noise that is targeted here.
								}
							}
						}
					}

					if(argcount<=2 && HasBitCast == false){//The number of parameters of basic functions is less than two.
                    	basic_raw_spin_lock.insert(F.getName().str());
					}else{
						//std::cout<<"Arg > 2: "<<F.getName().str()<<std::endl;
					}
                }
            }
    }
}

/*
    GenerateBasicSpinLock() should be called after GenerateRawSpinlock
*/
void FindLockAndAtomic::GenerateBasicSpinLock(){
    std::set<std::string> raw_spin;
    for(auto it = basic_raw_spin_lock.begin();it != basic_raw_spin_lock.end();it++){
        raw_spin.insert(*it);
    }
    auto &functionlist = _module_->getFunctionList();
    for(auto &F : functionlist){
        for(llvm::Function::arg_iterator argb=F.arg_begin(), arge=F.arg_end();argb!=arge;++argb){
			bool callRaw = false;//spin_lock is an encapsulation of raw_spin_lock, so this is set here, and spin_lock will definitely take the rlock in the spin_lock structure.
			bool IsGep = false;
            std::string type_str;
            llvm::Type* tp = argb->getType();
            llvm::raw_string_ostream rso(type_str);
            tp->print(rso);
            if(rso.str() == "%struct.spinlock*"){
        		if(llvm::Value *ArgV = llvm::dyn_cast<llvm::Value>(argb)){
		//Here we define spin_lock as: the parameter contains struct.spin_lock_t, and the function will take rlock under spin_lock_t as a parameter and pass it to the raw_spin_lock function set we found before.
					if(!ArgV->user_empty()){
						for(auto ArgUser=ArgV->user_begin();ArgUser!=ArgV->user_end();ArgUser++){
							if(llvm::GetElementPtrInst *gepinst = llvm::dyn_cast<llvm::GetElementPtrInst>(*ArgUser)){
								llvm::Value *GepV = llvm::dyn_cast<llvm::Value>(gepinst);
								if(!GepV->user_empty()){
									for(auto GepUser=GepV->user_begin();GepUser!=GepV->user_end();GepUser++){
										if(llvm::CallInst *Callinst = llvm::dyn_cast<llvm::CallInst>(*GepUser)){
											if(llvm::Function *called = Callinst->getCalledFunction()){
												if(raw_spin.find(called->getName().str()) != raw_spin.end()){
													callRaw = true;
												}
											}
										}
									}
								}
							}
						}
					}
				}



				if( callRaw == true){
					basic_spin_lock.insert(F.getName().str());
				}else{
					//std::cout<<" hasGep: "<<F.getName().str()<<std::endl;
				}
            }
        }
    }
}

void FindLockAndAtomic::GenerateBasicMutexLock(){
    auto &functionlist = _module_->getFunctionList();
    for(auto &F : functionlist){
       for(llvm::Function::arg_iterator argb=F.arg_begin(), arge=F.arg_end();argb!=arge;++argb){
	    bool HasBitCast = false;//If there is a bitcast statement, because the example acpi_os_delete_raw_lock is found, this will convert the passed struct.raw_spin_lock into i8*. We will remove this situation here.
            std::string type_str;
            llvm::Type* tp = argb->getType();
            llvm::raw_string_ostream rso(type_str);
            tp->print(rso);
            if(rso.str() == "%struct.mutex*"){
				std::int32_t argcount=0;
				for(llvm::Function::arg_iterator argcountb=F.arg_begin(),argcounte=F.arg_end();argcountb!=argcounte;++argcountb){
						argcount++;
				}

				if(llvm::Value *ArgV = llvm::dyn_cast<llvm::Value>(argb)){
					if(!ArgV->user_empty()){
						for(auto ArgUser=ArgV->user_begin();ArgUser!=ArgV->user_end();ArgUser++){
							if(llvm::BitCastInst *bitcastinst = llvm::dyn_cast<llvm::BitCastInst>(*ArgUser)){
								HasBitCast = true;//This is the noise that is targeted here.
							}
						}
					}
				}

				if(argcount==1 && HasBitCast == false){//The number of parameters of basic functions is less than two.
                    basic_mutex_lock.insert(F.getName().str());
				}else{
					//std::cout<<"Arg > 1: "<<F.getName().str()<<std::endl;
				}
            }
        } 
    }
}

void FindLockAndAtomic::GenerateBasicRWLock(){
    char str1[] = "read";
    char str2[] = "write";
    auto &functionlist = _module_->getFunctionList();
    for(auto &F : functionlist){
       for(llvm::Function::arg_iterator argb=F.arg_begin(), arge=F.arg_end();argb!=arge;++argb){
	    bool HasBitCast = false;//If there is a bitcast statement, because the example acpi_os_delete_raw_lock is found, this will convert the passed struct.raw_spin_lock into i8*. We will remove this situation here.
            std::string type_str;
            llvm::Type* tp = argb->getType();
            llvm::raw_string_ostream rso(type_str);
            tp->print(rso);
            if(rso.str() == "%struct.rwlock_t*"){
				std::int32_t argcount=0;
				for(llvm::Function::arg_iterator argcountb=F.arg_begin(),argcounte=F.arg_end();argcountb!=argcounte;++argcountb){
						argcount++;
				}
				if(argcount<=2 && HasBitCast == false){//The number of parameters of basic functions is less than two.
					std::string fname=F.getName().str();
					const char *p = strstr(fname.c_str(),str1);
					const char *p1 = strstr(fname.c_str(),str2);
					if(p != nullptr && p1==nullptr){
                    	basic_read_lock.insert(F.getName().str());
	                }
    	            if(p==nullptr && p1!=nullptr){
						basic_write_lock.insert(F.getName().str());
					}
				}else{
					//std::cout<<"Arg > 2: "<<F.getName().str()<<std::endl;
				}
            }
        } 
    }
}

std::set<std::string> FindLockAndAtomic::GenerateTaskRlimit(){
    auto &functionlist = _module_->getFunctionList();
    std::set<std::string> TaskRlimit;
    for(auto &F : functionlist){
        int BBcount = 0;
        bool ArgIsInteger=true;
        for(auto BBit = F.begin();BBit != F.end();++BBit){
            BBcount ++;
        }
        for(llvm::Function::arg_iterator arg_it = F.arg_begin();arg_it != F.arg_end();arg_it++){
            llvm::Type* tp = arg_it->getType();
            std::string type_str = ReturnTypeRefine(*tp);
            if(type_str == "%struct.task_struct*" && arg_it == F.arg_begin()){
                if(F.arg_size() == 2 && BBcount <= 1){
                    for(llvm::Function::arg_iterator argbb=F.arg_begin(),argbe=F.arg_end();argbb!=argbe;++argbb){
						if(argbb!=arg_it){
							llvm::Type* tpp=argbb->getType();
                            std::string type_str;
                            llvm::raw_string_ostream rso(type_str);
                            tpp->print(rso);
							if(tpp->isIntegerTy() && rso.str() == "i32"){
								continue;
							}
							else {
                               ArgIsInteger=false; 
								break;
							}
						}
					}
                    if(ArgIsInteger==true){
                        llvm::Type* tpp=F.getReturnType();
                        std::string type_str;
                        llvm::raw_string_ostream rso(type_str);
                        tpp->print(rso);
                        if(tpp->isIntegerTy() && rso.str() == "i64"){
                            //std::cout<<F.getName().str()<<std::endl;
                            TaskRlimit.insert(F.getName().str());
                        }
                    }
                }
            }
        }
    }
    return TaskRlimit;
}

void FindLockAndAtomic::GenerateRlimit(){
    std::set<std::string> TaskRlimitFunc = GenerateTaskRlimit();
    auto &functionlist = _module_->getFunctionList();
    for(auto &F : functionlist){
        if(F.arg_size() == 1 && F.getReturnType()->isIntegerTy()){
            for(llvm::inst_iterator inst_it = inst_begin(F);inst_it != inst_end(F);inst_it++){
                llvm::Instruction *instem = &*inst_it;
                if(llvm::CallInst *callInst = llvm::dyn_cast<llvm::CallInst>(instem)){
                    if(llvm::Function *called = callInst->getCalledFunction()){
                        if(TaskRlimitFunc.find(called->getName().str()) != TaskRlimitFunc.end() && F.arg_begin()->getType()->isIntegerTy()){
                            rlimit_function.insert(F.getName().str());
                        }
                    }
                }
            }
        }
    }
    rlimit_function.insert(TaskRlimitFunc.begin(),TaskRlimitFunc.end());
}

void FindLockAndAtomic::SetSpin(std::set<std::string> *LockSet,std::set<std::string> *UnlockSet){
    char str1[] = "spin_lock";
    char str2[] = "_unlock";
    char str3[] = "_trylock";
    for(auto spinit = basic_spin_lock.begin();spinit != basic_spin_lock.end();++spinit){
        const char *p = strstr(spinit->c_str(),str1);
        const char *p1 = strstr(spinit->c_str(),str2);
        const char *p2 = strstr(spinit->c_str(),str3);
        if(p1 == nullptr && (p != nullptr || p2 != nullptr)){
            LockSet->insert(*spinit);
        }
        if(p1 != nullptr && p == nullptr && p2 == nullptr){
            UnlockSet->insert(*spinit);
        }
    }
}

void FindLockAndAtomic::SetRawSpin(std::set<std::string> *LockSet,std::set<std::string> *UnlockSet){
    char str1[] = "spin_lock";
    char str2[] = "spin_unlock";
    char str3[] = "spin_trylock";
    for(auto rawspinit = basic_raw_spin_lock.begin();rawspinit != basic_raw_spin_lock.end();++rawspinit){
        const char *p = strstr(rawspinit->c_str(),str1);
        const char *p1 = strstr(rawspinit->c_str(),str2);
        const char *p2 = strstr(rawspinit->c_str(),str3);
        if(p1 == nullptr && (p != nullptr || p2 != nullptr)){
            LockSet->insert(*rawspinit);
        }
        if(p1 != nullptr && p == nullptr && p2 == nullptr){
            UnlockSet->insert(*rawspinit);
        }
    }
}

void FindLockAndAtomic::SetMutex(std::set<std::string> *LockSet,std::set<std::string> *UnlockSet){
    char str1[] = "mutex_lock";
    char str2[] = "_unlock";
    char str3[] = "_trylock";
    for(auto spinit = basic_mutex_lock.begin();spinit != basic_mutex_lock.end();++spinit){
        const char *p = strstr(spinit->c_str(),str1);
        const char *p1 = strstr(spinit->c_str(),str2);
        const char *p2 = strstr(spinit->c_str(),str3);
        if(p1 == nullptr && (p != nullptr || p2 != nullptr)){
            LockSet->insert(*spinit);
        }
        if(p1 != nullptr && p == nullptr && p2 == nullptr){
            UnlockSet->insert(*spinit);
        }
    }
}

void FindLockAndAtomic::SetWriteLock(std::set<std::string> *LockSet,std::set<std::string> *UnlockSet){
    char str1[] = "_lock";
    char str2[] = "_unlock";
    char str3[] = "_trylock";
    for(auto spinit = basic_write_lock.begin();spinit != basic_write_lock.end();++spinit){
        const char *p = strstr(spinit->c_str(),str1);
        const char *p1 = strstr(spinit->c_str(),str2);
        const char *p2 = strstr(spinit->c_str(),str3);
        if(p1 == nullptr && (p != nullptr || p2 != nullptr)){
            LockSet->insert(*spinit);
        }
        if(p1 != nullptr && p == nullptr && p2 == nullptr){
            UnlockSet->insert(*spinit);
        }
    }
}

void FindLockAndAtomic::BasicAndUpperLockSet(std::set<std::string> LockSet,std::set<std::string> UnlockSet,std::set<std::string> BasicLock,std::set<std::string> *UpperAndBasicLock,std::set<std::string> *UpperAndBasicUnlock){
    for(auto it = LockSet.begin();it != LockSet.end();it++){
        UpperAndBasicLock->insert(*it);
    }
    for(auto it = UnlockSet.begin();it != UnlockSet.end();it++){
        UpperAndBasicUnlock->insert(*it);
    }

    auto &functionlist = _module_->getFunctionList();
    for(auto &F : functionlist){
        std::string FuncName = F.getName().str();
        bool isupper = false;
        for(llvm::inst_iterator insb = inst_begin(F);insb != inst_end(F);++insb){  //Traverse Instruction
            llvm::Instruction *instem = &*insb;
            if(llvm::CallInst *callInst = llvm::dyn_cast<llvm::CallInst>(instem)){
                if(llvm::Function *called = callInst->getCalledFunction()){
                    std::string CalledFunc = called->getName().str();
                    if(LockSet.find(CalledFunc) != LockSet.end()){ //What I encountered at the beginning was spin_lock (locking)
                        bool hasunlock = false;
                        for(llvm::inst_iterator inafterlock = insb;inafterlock != inst_end(F);++inafterlock){//Traverse downward from the locking statement. If an unlocking statement is encountered, hasunlock is set to true.
                            llvm::Instruction *afterlockInst = &*inafterlock;
                            if(llvm::CallInst *Acalled = llvm::dyn_cast<llvm::CallInst>(afterlockInst)){
                                if(llvm::Function *unlockcalled = Acalled->getCalledFunction()){
                                    std::string aCalledFunc = unlockcalled->getName().str();
                                    if(UnlockSet.find(aCalledFunc) != UnlockSet.end()){
                                        hasunlock = true;//Then it is not the upper package we want
                                    }
                                }
                            }
                        }
                        if(hasunlock == false){   //After traversing, there is no call to unlock, then isupper is set to true.
                            isupper = true;
                            if(isupper=true){
                                if(BasicLock.find(FuncName) != BasicLock.end()){
                                    continue;
                                }else{
                                    UpperAndBasicLock->insert(FuncName); 
                                }   
                            }
                        }
                        break;   //End the analysis of this Function.
                    }//The function here is to use the first captured lock to match. If the first one is a lock, and the following ones are all locks, then we think this is an upper.

                    if(UnlockSet.find(CalledFunc) != UnlockSet.end()){   
		//The first thing I encounter is the unlock statement. If you look down from this sentence, if there is no lock function, it is upper (it feels redundant. If the first one is the unlock function, it means it is upper)
                        bool haslock = false;
                        for(llvm::inst_iterator inafterunlock = insb;inafterunlock != inst_end(F);++inafterunlock){
                            llvm::Instruction *afterUnlockInst = &*inafterunlock;
                            if(llvm::CallInst *Aucalled = llvm::dyn_cast<llvm::CallInst>(afterUnlockInst)){
                                if(llvm::Function *lockcalled = Aucalled->getCalledFunction()){
                                    std::string auCalledFunc = lockcalled->getName().str();
                                    if(LockSet.find(auCalledFunc) != LockSet.end()){
                                        haslock = true;
                                    }
                                }
                            }
                        }
                        if(haslock == false){
                            isupper = true;
                            if(isupper == true){
                                if(BasicLock.find(FuncName) != BasicLock.end()){
                                    continue;
                                }else{
                                    UpperAndBasicUnlock->insert(FuncName);  
                                }
                            }
                        }
                        break;   
                    }//This is similar to the above. Find the unlocked upper.
		        }
            }
        }
    }

}

std::set<std::string> FindLockAndAtomic::GetSpinLock(){
    return spin_lock;
}

std::set<std::string> FindLockAndAtomic::GetSpinUnlock(){
    return spin_unlock;
}

std::set<std::string> FindLockAndAtomic::GetMutexLock(){
    return mutex_lock;
}

std::set<std::string> FindLockAndAtomic::GetMutexUnlock(){
    return mutex_unlock;
}

std::set<std::string> FindLockAndAtomic::GetWriteLock(){
    return write_lock;
}

std::set<std::string> FindLockAndAtomic::GetWriteUnlock(){
    return write_unlock;
}

std::set<std::string> FindLockAndAtomic::GetPerCPU(){
    return percpu_function;
}

std::set<std::string> FindLockAndAtomic::GetAtomicFunc(){
    return atomic_function;
}
std::set<std::string> FindLockAndAtomic::GetAtomicFuncNotChange(){
    return atomic_function_not_change;
}
std::set<std::string> FindLockAndAtomic::GetPerCPUNotChange(){
    return percpu_function_not_change;
}

std::set<std::string> FindLockAndAtomic::GetAllocFunction(){
    return AllocFunctionNames;
}

std::set<std::string> FindLockAndAtomic::GetRlimitFunction(){
    return rlimit_function;
}
