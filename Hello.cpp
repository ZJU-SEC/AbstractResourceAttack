#include <iostream>
#include <set>
#include <cstring>
#include <string>
#include <map>
#include <vector>
#include "llvm/IR/Use.h"
#include "llvm/IR/User.h"
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
std::error_code ec;
const std::set<std::string> AllocFunctionNames = {"kmem_cache_alloc", "__kmalloc", "__kmalloc_node", "kmem_cache_alloc_node", "kmalloc_array", "kcalloc", "kzalloc",  "__alloc_percpu_gfp", "__alloc_percpu", "mempool_alloc", "mempool_alloc_slab", "mempool_kmalloc", "mempool_alloc_pages"};
std::map<std::string,std::vector<std::string>> AllocFunctionArg;
llvm::raw_fd_ostream fstream("./cmp-finder.txt", ec, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);



namespace {
		
	struct Hello : public FunctionPass {
		static char ID; // Pass identification, replacement for typeid
		Hello() : FunctionPass(ID) {}
		
		void AllocFunctionArgInit(){                 //初始化一个map,key为内存分配函数函数名，value为一个存放函数参数类型的vector，这十三个函数和对应的参数类型是事先用pass找到的存在定义的内存分配函数和它们的参数。
			std::vector<std::string> kzalloc = {"i64"};
			AllocFunctionArg["kzalloc"] = kzalloc;
			std::vector<std::string> kmalloc_array = {"i64","i64","i32"};
			AllocFunctionArg["kmalloc_array"] = kmalloc_array;
			std::vector<std::string> kcalloc = {"i64"};
			AllocFunctionArg["kcalloc"] = kcalloc;
			std::vector<std::string> __kmalloc = {"i64","i32"};
			AllocFunctionArg["__kmalloc"] = __kmalloc;
			std::vector<std::string> kmem_cache_alloc_node = {"%struct.kmem_cache*","i32","i32"};
			AllocFunctionArg["kmem_cache_alloc_node"] = kmem_cache_alloc_node;
			std::vector<std::string> kmem_cache_alloc = {"%struct.kmem_cache*","i32","i32"};
			AllocFunctionArg["kmem_cache_alloc"] = kmem_cache_alloc;
			std::vector<std::string> __kmalloc_node ={"i64","i32","i32"};
			AllocFunctionArg["__kmalloc_node"] = __kmalloc_node;
			std::vector<std::string> mempool_kmalloc = {"i32","i8*"};
			AllocFunctionArg["mempool_kmalloc"] = mempool_kmalloc;
			std::vector<std::string> mempool_alloc_slab = {"i32","i8*"};
			AllocFunctionArg["mempool_alloc_slab"] = mempool_alloc_slab;
			std::vector<std::string> mempool_alloc_pages = {"i32","i8*"};
			AllocFunctionArg["mempool_alloc_pages"] = mempool_alloc_pages;
			std::vector<std::string> mempool_alloc = {"%struct.mempool_s*","i32"};
			AllocFunctionArg["mempool_alloc"] = mempool_alloc;
			std::vector<std::string> __alloc_percpu_gfp = {"i64","i64","i32"};
			AllocFunctionArg["__alloc_percpu_gfp"] = __alloc_percpu_gfp;
			std::vector<std::string> __alloc_percpu = {"i64","i64"};
			AllocFunctionArg["__alloc_percpu"] = __alloc_percpu;
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
		void TravseAllocUser(Value* recurvalue,int *isRet,Value* originv){
			if(recurvalue->getNumUses()!=0){
				if(recurvalue->getName()==originv->getName()){//此处AAA代表reutn %407中的%407变量名,若相等，说明找到了这个变量
					*isRet=1;
					return;
				}
				for(auto UB=recurvalue->user_begin();UB!=recurvalue->user_end();++UB){
					Value * useb = *UB;
					TravseAllocUser(useb,isRet,originv);
				}
				if(recurvalue->getNumUses()==0){
					return;
				}
			}
//			Value* valueret= dyn_cast_or_null<Value>(instrecur);
		}
		int AllocUser(Instruction* &instrecur, Value* &testvalue){
			int isRet=0;
			Value* originv=testvalue;
			TravseAllocUser(instrecur,&isRet, originv);
			return isRet;
		}
		char* GetActualFName(std::string &functionname){   //获得函数的名字即截取掉kmalloc.xxxx后面这串数字,此处functionname为被call的函数名.
			std::string mystr;
			char *FName = (char*)functionname.data();
			char *ActualFName = strtok(FName,".");
    			functionname = ActualFName;//传进来的是个指针，在此处赋值，直接可以修改指针的对象，就是functionname
				while(ActualFName != NULL){
					ActualFName = strtok(NULL,".");//第二次截取，内容为kmalloc.xxxxx的xxxx.
					break;
				}
    			if(functionname == "llvm"){
//        			functionname = ActualFName;
    			}
    			return ActualFName;//此处ActualName实际是kmalloc后面的数字,functionname为真实的kmalloc
		}

		std::string ReturnTypeRefine(Type &rt){           //截取掉返回值中%struct.inode.xxxx*后面的这串数字
			std::string ts;
			std::string type_str;
			std::string actual_type_str = "";
			int count = 1;
			llvm::raw_string_ostream rso(ts);
			rt.print(rso);
			type_str = rso.str();
			char *refined_type_str = strtok((char*)type_str.data(),".");
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
			Type* rt = F.getReturnType();
			if(rt->isPointerTy()) {	//在此处我进入每一个instruction中，找到call语句中调用kmem_cache_alloc的语句
				for(inst_iterator insb=inst_begin(F),inse=inst_end(F);insb!=inse;insb++) {
					Instruction *instem = &*insb;
					if(CallInst *callInst = dyn_cast<CallInst>(instem)){
						if(Function * called = callInst->getCalledFunction()){
							StringRef FNameValue=called->getName();
							std::string mystr = FNameValue.str();
							char *ActualFName; 
							ActualFName = GetActualFName(mystr);//mystr是个指针，所以这个地方直接修改了指针内容，返回出来的ActualFName为一串数字
							if(AllocFunctionNames.find(mystr) != AllocFunctionNames.end()){//此处已修改完成，针对kmalloc.xxxx的情况，使用了四个判断1.名字2.参数
								//个数3.参数类型4.返回值类型。此处完成的是第一个对名字的判断
//								fstream<<"filtered function name"<<":";
//								fstream<<FNameValue<<'\n';
								if(ActualFName != nullptr){
									if((int)ActualFName[0] >= 48 && (int)ActualFName[0] <= 57){   //如果是内存分配函数且后面带不同数字，则检查参数是否一样。此处取裁剪得到的第一个数，如果是个数，则对函数参数进行判断
										if(AllocFunctionArgs(mystr,*called) == false){//此处判断函数参数个数和类型是否与模板相同,相同返回true,不同返回false.
											continue;
										}
									}
								}

//								Value* atomicreturn=dyn_cast_or_null<Value>(instem);//如果抓到了kmem_cache_alloc，则需要看整个函数return
								//和kmem_cache_alloc这一句的结果的关系，如果函数返回值是从这里拿的，则没问题，如果不是，则跳过
//								for(inst_iterator insbb=inst_begin(F),insbe=inst_end(F);insbb!=insbe;insbb++){
//									Instruction * insret = &* insbb;
//									if(insret->getOpcode()==llvm::Instruction::Ret){
//										if(insret->getOperand(0)->getName()==atomicreturn->getName()){
//											fstream<<F.getName()<<":";
//											fstream<<ReturnTypeRefine(*rt);//ReturnTypeRefine函数为自定义函数，目的是为了截取返回值类型中
//											//inode.xxxx中的xxxx.只得到inode
//											fstream<<'\n';
//											return false;
//										} else if(atomicreturn->getType()==rt){
//											fstream<<F.getName()<<":";
//											fstream<<ReturnTypeRefine(*rt);
//											fstream<<'\n';
//											return false;
//										}
//									}
//								}
//								continue;
								//此处需要获取return的value name，没写完
								Value* returnv; 
								for(inst_iterator insrb=inst_begin(F),insre=inst_end(F);insrb!=insre;insrb++)
								{
									if(insrb->getOpcode()==llvm::Instruction::Ret)
										returnv=insrb->getOperand(0);
								}
								if(AllocUser(instem,returnv)==1)
								{
									fstream<<F.getName()<<":";
									fstream<<ReturnTypeRefine(*rt);
									fstream<<'\n';
									return false;
								}
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
			}
		return false;
		}
	};
}


char Hello::ID = 0;
static RegisterPass<Hello> X("hello", "my hello world pass");

