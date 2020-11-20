#include <iostream>
#include <set>
#include <cstring>
#include <string>
#include <map>
#include <vector>
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/IR/User.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IR/InstIterator.h"
using namespace llvm;
using namespace llvm;
std::error_code ec;
const std::set<std::string> AllocFunctionNames = {"atomic_add", "atomic_sub", "atomic_or", "atomic_and", "atomic_read", "atomic_set","atomic_inc_return", "atomic_dec_return", "atomic_add_return", "atomic_sub_return", "atomic_xchg", "atomic_cmpxchg", "atomic_fetch_add","atomic_fetch_and","atomic_fetch_or","atomic_fetch_xor", "atomic64_read", "atomic64_set", "atomic64_add", "atomic64_sub","atomic64_and","atomic64_or","atomic64_add_return","atomic64_sub_return","atomic64_fetch_add","atomic64_fetch_add_unless", "atomic64_xchg","atomic64_cmpxchg", "do_raw_spin_lock", "do_raw_spin_lock_flags", "do_raw_spin_trylock", "do_raw_spin_unlock", "spin_lock", "spin_lock_bh", "spin_trylock", "spin_lock_irq", "spin_lock_irqsave", "spin_unlock" ,"spin_unlock_bh", "spin_unlock_irq", "spin_unlock_irqrestore", "spin_trylock_bh", "spin_is_locked", "spin_is_contended"};
std::map<std::string,std::vector<std::string>> AllocFunctionArg;
llvm::raw_fd_ostream fstream("./cmp-finder.txt", ec, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);



namespace {
		
	struct Hello : public FunctionPass {
		static char ID; // Pass identification, replacement for typeid
		Hello() : FunctionPass(ID) {}
		
		void AllocFunctionArgInit(){                 //初始化一个map,key为内存分配函数函数名，value为一个存放函数参数类型的vector，这十三个函数和对应的参数类型是事先用pass找到的存在定义的内存分配函数和它们的参数。
			std::vector<std::string> atomic_add = {"i32", "%struct.kuid_t*"};
			AllocFunctionArg["atomic_add"] = atomic_add;
			std::vector<std::string> atomic_sub = {"i32","%struct.kuid_t*"};
			AllocFunctionArg["atomic_sub"] = atomic_sub;
			std::vector<std::string> atomic_or = {"i32", "%struct.kuid_t*"};
			AllocFunctionArg["atomic_or"] = atomic_or;
			std::vector<std::string> atomic_and = {"i32","%struct.kuid_t*"};
			AllocFunctionArg["atomic_and"] = atomic_and;
			std::vector<std::string> atomic_read = {"%struct.kuid_t*"};
			AllocFunctionArg["atomic_read"] = atomic_read;
			std::vector<std::string> atomic_set = {"%struct.kuid_t*","i32"};
			AllocFunctionArg["atomic_set"] = atomic_set;
			std::vector<std::string> atomic_inc_return ={"%struct.kuid_t*"};
			AllocFunctionArg["atomic_inc_return"] = atomic_inc_return;
			std::vector<std::string> atomic_dec_return = {"%struct.kuid_t*"};
			AllocFunctionArg["atomic_dec_return"] = atomic_dec_return;
			std::vector<std::string> atomic_add_return = {"i32","%struct.kuid_t*"};
			AllocFunctionArg["atomic_add_return"] = atomic_add_return;
			std::vector<std::string> atomic_sub_return = {"i32","%struct.kuid_t*"};
			AllocFunctionArg["atomic_sub_return"] = atomic_sub_return;
			std::vector<std::string> atomic_xchg = {"%struct.kuid_t*","i32"};
			AllocFunctionArg["atomic_xchg"] = atomic_xchg;
			std::vector<std::string> atomic_cmpxchg = {"%struct.kuid_t*","i32","i32"};
			AllocFunctionArg["atomic_cmpxchg"] = atomic_cmpxchg;
			std::vector<std::string> atomic_fetch_add = {"i32","%struct.kuid_t*"};
			AllocFunctionArg["atomic_fetch_add"] = atomic_fetch_add;
			std::vector<std::string> atomic_fetch_and = {"i32","%struct.kuid_t*"};
			AllocFunctionArg["atomic_fetch_and"] = atomic_fetch_and;
			std::vector<std::string> atomic_fetch_or = {"i32","%struct.kuid_t*"};
			AllocFunctionArg["atomic_fetch_or"] = atomic_fetch_or;
			std::vector<std::string> atomic_fetch_xor = {"i32","%struct.kuid_t*"};
			AllocFunctionArg["atomic_fetch_xor"] = atomic_fetch_xor;
			std::vector<std::string> atomic64_read = {"%struct.anon.1*"};
			AllocFunctionArg["atomic64_read"] = atomic64_read;
			std::vector<std::string> atomic64_set = {"%struct.anon.1*","i64"};
			AllocFunctionArg["atomic64_set"] = atomic64_set;
			std::vector<std::string> atomic64_add = {"i64","%struct.anon.1*"};
			AllocFunctionArg["atomic64_add"] = atomic64_add;
			std::vector<std::string> atomic64_sub = {"i64","%struct.anon.1*"};
			AllocFunctionArg["atomic64_sub"] = atomic64_sub;
			std::vector<std::string> atomic64_and = {"i64","%struct.anon.1*"};
			AllocFunctionArg["atomic64_and"] = atomic64_and;
			std::vector<std::string> atomic64_or = {"i64","%struct.anon.1*"};
			AllocFunctionArg["atomic64_or"] = atomic64_or;
			std::vector<std::string> atomic64_add_return = {"i64","%struct.anon.1*"};
			AllocFunctionArg["atomic64_add_return"] = atomic64_add_return;
			std::vector<std::string> atomic64_sub_return = {"i64","%struct.anon.1*"};
			AllocFunctionArg["atomic64_sub_return"] = atomic64_sub_return;
			std::vector<std::string> atomic64_fetch_add = {"i64","%struct.anon.1*"};
			AllocFunctionArg["atomic64_fetch_add"] = atomic64_fetch_add;
			std::vector<std::string> atomic64_fetch_add_unless = {"%struct.anon.1*","i64","i64"};
			AllocFunctionArg["atomic64_fetch_add_unless"] = atomic64_fetch_add_unless;
			std::vector<std::string> atomic64_xchg = {"%struct.anon.1*","i64"};
			AllocFunctionArg["atomic64_xchg"] = atomic64_xchg;
			std::vector<std::string> atomic64_cmpxchg = {"%struct.anon.1*","i64","i64"};
			AllocFunctionArg["atomic64_cmpxchg"] = atomic64_cmpxchg;
			std::vector<std::string> do_raw_spin_lock = {"%struct.raw_spinlock*"};
			AllocFunctionArg["do_raw_spin_lock"] = do_raw_spin_lock;
			std::vector<std::string> do_raw_spin_lock_flags = {"%struct.raw_spinlock*","i64"};
			AllocFunctionArg["do_raw_spin_lock_flags"] = do_raw_spin_lock_flags;
			std::vector<std::string> do_raw_spin_trylock = {"%struct.raw_spinlock*"};
			AllocFunctionArg["do_raw_spin_trylock"] = do_raw_spin_trylock;
			std::vector<std::string> do_raw_spin_unlock = {"%struct.raw_spinlock*"};
			AllocFunctionArg["do_raw_spin_unlock"] = do_raw_spin_unlock;
			std::vector<std::string> spin_lock = {"%struct.spinlock*"};
			AllocFunctionArg["spin_lock"] = spin_lock;
			std::vector<std::string> spin_lock_bh = {"%struct.spinlock*"};
			AllocFunctionArg["spin_lock_bh"] = spin_lock_bh;
			std::vector<std::string> spin_trylock = {"%struct.spinlock*"};
			AllocFunctionArg["spin_trylock"] = spin_trylock;
			std::vector<std::string> spin_lock_irq = {"%struct.spinlock*"};
			AllocFunctionArg["spin_lock_irq"] = spin_lock_irq;
			std::vector<std::string> spin_lock_irqsave = {"%struct.spinlock*","i64"};
			AllocFunctionArg["spin_lock_irqsave"] = spin_lock_irqsave;
			std::vector<std::string> spin_unlock = {"%struct.spinlock*"};
			AllocFunctionArg["spin_unlock"] = spin_unlock;
			std::vector<std::string> spin_unlock_bh = {"%struct.spinlock*"};
			AllocFunctionArg["spin_unlock_bh"] = spin_unlock_bh;
			std::vector<std::string> spin_unlock_irq = {"%struct.spinlock*"};
			AllocFunctionArg["spin_unlock_irq"] = spin_unlock_irq;
			std::vector<std::string> spin_unlock_irqrestore = {"%struct.spinlock*","i64"};
			AllocFunctionArg["spin_unlock_irqrestore"] = spin_unlock_irqrestore;
			std::vector<std::string> spin_trylock_bh = {"%struct.spinlock*"};
			AllocFunctionArg["spin_trylock_bh"] = spin_trylock_bh;
			std::vector<std::string> spin_is_locked = {"%struct.spinlock*"};
			AllocFunctionArg["spin_is_locked"] = spin_is_locked;
			std::vector<std::string> spin_is_contended = {"%struct.spinlock*"};
			AllocFunctionArg["spin_is_contended"] = spin_is_contended;
		}//该函数用于写死函数名-参数的数组

		bool AllocFunctionArgs(std::string functionname,Function &F){        //检查参数是否一样,functionname是经过处理之后的函数名,Function &F是call之后调用的function.
			std::map<std::string,std::vector<std::string>>::iterator it;
			std::vector<std::string> FunctionArgs = AllocFunctionArg.find(functionname)->second;//此处找到模板函数的参数value.
			std::string str;
			int argcount = FunctionArgs.size();//此处argcount为模板函数的参数个数
			int count = 0;
			for(Function::arg_iterator argb=F.arg_begin(), arge=F.arg_end();argb!=arge;++argb) {
				count++;
			}//获取裁剪后函数的参数个数
			if (argcount != count){
				return false;//看参数个数是否相同
			}
			for(Function::arg_iterator argb=F.arg_begin(), arge=F.arg_end();argb!=arge;++argb) {
				std::string type_str;
				Type* tp = argb->getType();
				llvm::raw_string_ostream rso(type_str);
				tp->print(rso);//把type流转为string.
				if(FunctionArgs.front() != rso.str()){
					return false;
				}
				std::vector<std::string>::iterator k = FunctionArgs.begin();//得到局部变量FunctionArgs的第一个参数
				FunctionArgs.erase(k);//释放掉第一个参数，好进行后续比较。相当于依次比较函数,这里erase,那里++argb,两个就可以同时比较了。
			}
			return true;
		}
		void TravseAllocUser(int *isRet,std::string &LockStruct,Instruction* originv,Function* testfunction){
			if(*isRet == 0){
				if(originv->getOpcode() == Instruction::GetElementPtr){//这个地方去看传进来的参数是否是gep指令直接获取的(因为取结构体里的东西需要用到gep)
					std::string type_str;
					GetElementPtrInst *gepinst = dyn_cast<GetElementPtrInst>(originv);
					Type *structType = gepinst->getSourceElementType();           //将Instruction转换为GetElemtPtrInst，然后使用其中的getSourceElementType()方法获得结构体的类型。
					std::string teststrr=ReturnTypeRefine(*structType);
					LockStruct = teststrr;
//					llvm::raw_string_ostream rso(type_str);
//					structType->print(rso);
//					LockStruct = rso.str();
					*isRet = 1;
					return ;
				}

				if(originv->getOpcode() == Instruction::PHI){//如果这里的东西不是gep直接获取的，那有可能是通过phi指令走到这里的。
					return;//如果没有GEP指令，可能是从之前的phi指令走到这里的，这里我们忽略phi指令?会丢掉一些结构体，在改
				}

				for (auto operand = originv->operands().begin();operand != originv->operands().end();++operand){//如果传进来的参数既不是gep拿到的，也不是phi走到这里的，去看atomic修改的是不是函数传进来的参数，如果是以某种方式修改了函数传进来的参数，那我们就认为它保护的资源就是这个参数资源。这里要改的就是获取F的参数列表
					Value *opValue = dyn_cast<Value>(operand);
					for(auto argu=testfunction->arg_begin();argu!=testfunction->arg_end();++argu){
						if(opValue->getName()==argu->getName()){  
							Type *StructType = opValue->getType();
							if(StructType->isStructTy()){
								std::string teststr=ReturnTypeRefine(*StructType);
//								std::string type_str;
//								llvm::raw_string_ostream rso(type_str);
//								StructType->print(rso);
//								LockStruct = rso.str();
								LockStruct = teststr;						
								*isRet = 1;
								return ;
							}
						}
					}
					if(Instruction *opInst = dyn_cast<Instruction>(operand)){
						TravseAllocUser(isRet,LockStruct,opInst,testfunction);
					}
				}
			}
		}
		int AllocUser(std::string &LockStruct,Instruction* &testvalue,Function* testfunction){
			int isRet = 0;
			Instruction* originv = testvalue;
			TravseAllocUser(&isRet,LockStruct,originv,testfunction);
			return isRet;
		}


		char* GetActualFName(std::string &functionname){   //获得函数的名字即截取掉atomic.xxxx后面这串数字,此处functionname为被call的函数名.
			std::string mystr;
			char *FName = (char*)functionname.data();
			char *ActualFName = strtok(FName,".");
    			functionname = ActualFName;//传进来的是个指针，在此处赋值，直接可以修改指针的对象，就是functionname
				while(ActualFName != NULL){
					ActualFName = strtok(NULL,".");//第二次截取，内容为atomic.xxxxx的xxxx.
					break;
				}
    			if(functionname == "llvm"){
//        			functionname = ActualFName;
    			}
    			return ActualFName;//此处ActualName实际是atomic后面的数字,functionname为真实的atomic
		}

		std::string ReturnTypeRefine(Type &rt){           //截取掉返回值中%struct.inode.xxxx*后面的这串数字
			std::string ts;
			std::string type_str;
			std::string actual_type_str = "";
			std::string test_type_str = "";
			int count = 1;
			llvm::raw_string_ostream rso(ts);
			rt.print(rso);
			type_str = rso.str();
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

		bool runOnFunction(Function &F) override {
			AllocFunctionArgInit();
				for(inst_iterator insb=inst_begin(F),inse=inst_end(F);insb!=inse;insb++) {
					Instruction *instem = &*insb;
					if(CallInst *callInst = dyn_cast<CallInst>(instem)){
						if(Function * called = callInst->getCalledFunction()){
							StringRef FNameValue=called->getName();
							std::string mystr = FNameValue.str();
							char *ActualFName; 
							ActualFName = GetActualFName(mystr);//mystr是个指针，所以这个地方直接修改了指针内容，返回出来的ActualFName为一串数字
							if(AllocFunctionNames.find(mystr) != AllocFunctionNames.end()){//此处已修改完成，针对atomic.xxxx的情况，使用了四个判断1.名字2.参数
								//个数3.参数类型4.返回值类型。此处完成的是第一个对名字的判断
//								fstream<<"filtered function name"<<":";
//								fstream<<FNameValue<<'\n';
								if(ActualFName != nullptr){
									if((int)ActualFName[0] >= 48 && (int)ActualFName[0] <= 57){   //如果是锁函数且后面带不同数字，则检查参数是否一样。此处取裁剪得到的第一个数，如果是个数，则对函数参数进行判断
										if(AllocFunctionArgs(mystr,*called) == false){//此处判断函数参数个数和类型是否与模板相同,相同返回true,不同返回false.
											continue;
										}
									}
								}
								//这里需要确认atomic函数中参数的来源，暂时分为两种情况:1.来自全局变量，则参数有名字。2.来自结构体，需要进行递归调用分析，这个结构体要不来自于Function F中的
								//定义，要不来自于函数传进来的参数，不过都可以用GEP指令来进行判断，从F的GEP指令开始，进行user的递归分析，如果其中一个user是atomic的参数，那就建立了结构体和
								//atomic的关系
								//fstream<<"Actual Name:"<<mystr<<'\n';
								for(auto arggb=callInst->arg_begin(),argge=callInst->arg_end();arggb!=argge;arggb++){
									if (const GlobalValue* G = dyn_cast<GlobalValue>(arggb)){
										fstream<<"FunctionName:";
										fstream<<F.getName();
										fstream<<'\n';
										fstream<<"Global Variable:";
										fstream<<G->getName();
										fstream<<'\n';

									}
								}

								std::string FunctionName = mystr;
								std::vector<std::string> FunctionArgs = AllocFunctionArg.find(FunctionName)->second;
								for(auto argggb=callInst->arg_begin(),arggge=callInst->arg_end();argggb!=arggge;argggb++){
									if(FunctionArgs.front() == "i64" || FunctionArgs.front() == "i32"){
										std::vector<std::string>::iterator k = FunctionArgs.begin();
										FunctionArgs.erase(k);
										continue;
									}
									if(Instruction *argInst = dyn_cast<Instruction>(argggb)){
										std::string LockStruct;
										fstream<<"Struct:";
										if(AllocUser(LockStruct,argInst,&F)==1){
											fstream<<"FunctionName:";
											fstream<<F.getName();
											fstream<<'\n';
											fstream<<"ProtectedStruct:";
											fstream<<LockStruct<<'\n';
										}
									}
									std::vector<std::string>::iterator k = FunctionArgs.begin();
									FunctionArgs.erase(k);
								}
//								if(AllocUser(instem,returnv)==1)
//								{
//									fstream<<F.getName()<<":";
//									fstream<<ReturnTypeRefine(*rt);
//									fstream<<'\n';
//									return false;
//								}
								continue;

							}
//							std::string strfname;
//							llvm::raw_string_ostream ostreamf(strfname);//ostreamf是个自己定义的变量，用来将strfname转换为流
//							called->getName()->print(ostreamf);
//							std::cout << strfname<<'\n';
//							return false;
						}
					}
				}
		return false;
		}
	};
}


char Hello::ID = 0;
static RegisterPass<Hello> X("hello", "hello world pass");

