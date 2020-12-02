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
const std::set<std::string> AtomicFunctionNames = {"_raw_read_trylock","_raw_write_trylock","_raw_write_lock","_raw_read_lock","_raw_read_lock_irqsave","_raw_write_lock_irqsave","_raw_read_lock_irq","_raw_read_lock_bh","_raw_write_lock_irq","_raw_write_lock_bh","__raw_read_unlock","__raw_write_unlock","__raw_read_unlock_irq","__raw_write_unlock_irq","__raw_read_unlock_irqrestore","__raw_write_unlock_irqrestore","__raw_write_unlock_bh", "percpu_counter_set","percpu_counter_add_batch","percpu_counter_compare","percpu_counter_add","percpu_counter_sum_positive","percpu_counter_sum","percpu_counter_read","percpu_counter_read_positive","__percpu_counter_compare","percpu_counter_inc","percpu_counter_sub","atomic_add", "atomic_sub", "atomic_or", "atomic_and", "atomic_read", "atomic_set","atomic_inc_return", "atomic_dec_return", "atomic_add_return", "atomic_sub_return", "atomic_xchg", "atomic_cmpxchg", "atomic_fetch_add","atomic_fetch_and","atomic_fetch_or","atomic_fetch_xor", "atomic64_read", "atomic64_set", "atomic64_add", "atomic64_sub","atomic64_and","atomic64_or","atomic64_add_return","atomic64_sub_return","atomic64_fetch_add","atomic64_fetch_add_unless", "atomic64_xchg","atomic64_cmpxchg", "do_raw_spin_lock", "do_raw_spin_lock_flags", "do_raw_spin_trylock", "do_raw_spin_unlock", "spin_lock", "spin_lock_bh", "spin_trylock", "spin_lock_irq", "spin_lock_irqsave", "spin_unlock" ,"spin_unlock_bh", "spin_unlock_irq", "spin_unlock_irqrestore", "spin_trylock_bh", "spin_is_locked", "spin_is_contended"};
const std::set<std::string> AllocFunctionNames = {"kmem_cache_alloc", "__kmalloc", "__kmalloc_node", "kmem_cache_alloc_node", "kmalloc","kmalloc_node", "kmalloc_array","kmalloc_array_node","kcalloc","kcalloc_node","kmem_cache_zalloc","kzalloc_node", "kzalloc",  "__alloc_percpu_gfp", "__alloc_percpu", "mempool_alloc", "mempool_alloc_pages"};
std::map<std::string,std::vector<std::string>> AllocFunctionArg;
llvm::raw_fd_ostream fstream("./cmp-finder.txt", ec, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);



namespace {
		
	struct Resourceanalysis : public FunctionPass {
		static char ID; // Pass identification, replacement for typeid
		Resourceanalysis() : FunctionPass(ID) {}
		
		void AllocFunctionArgInit(){                 //初始化一个map,key为内存分配函数函数名，value为一个存放函数参数类型的vector，这十三个函数和对应的参数类型是事先用pass找到的存在定义的内存分配函数和它们的参数。


			std::vector<std::string> _raw_read_trylock = {"%struct.rwlock_t*"};
			AllocFunctionArg["_raw_read_trylock"] = _raw_read_trylock;
			std::vector<std::string> _raw_write_trylock = {"%struct.rwlock_t*"};
			AllocFunctionArg["_raw_write_trylock"] = _raw_write_trylock;
			std::vector<std::string> _raw_write_lock = {"%struct.rwlock_t*"};
			AllocFunctionArg["_raw_write_lock"] = _raw_write_lock;
			std::vector<std::string> _raw_read_lock = {"%struct.rwlock_t*"};
			AllocFunctionArg["_raw_read_lock"] = _raw_read_lock;
			std::vector<std::string> _raw_read_lock_irqsave = {"%struct.rwlock_t*"};
			AllocFunctionArg["_raw_read_lock_irqsave"] = _raw_read_lock_irqsave;
			std::vector<std::string> _raw_write_lock_irqsave = {"%struct.rwlock_t*"};
			AllocFunctionArg["_raw_write_lock_irqsave"] = _raw_write_lock_irqsave;
			std::vector<std::string> _raw_read_lock_irq = {"%struct.rwlock_t*"};
			AllocFunctionArg["_raw_read_lock_irq"] = _raw_read_lock_irq;
			std::vector<std::string> _raw_read_lock_bh = {"%struct.rwlock_t*"};
			AllocFunctionArg["_raw_read_lock_bh"] = _raw_read_lock_bh;
			std::vector<std::string> _raw_write_lock_irq = {"%struct.rwlock_t*"};
			AllocFunctionArg["_raw_write_lock_irq"] = _raw_write_lock_irq;
			std::vector<std::string> _raw_write_lock_bh = {"%struct.rwlock_t*"};
			AllocFunctionArg["_raw_write_lock_bh"] = _raw_write_lock_bh;
			std::vector<std::string> __raw_read_unlock = {"%struct.rwlock_t*"};
			AllocFunctionArg["__raw_read_unlock"] = __raw_read_unlock;
			std::vector<std::string> __raw_write_unlock = {"%struct.rwlock_t*"};
			AllocFunctionArg["__raw_write_unlock"] = __raw_write_unlock;
			std::vector<std::string> __raw_read_unlock_irq = {"%struct.rwlock_t*"};
			AllocFunctionArg["__raw_read_unlock_irq"] = __raw_read_unlock_irq;
			std::vector<std::string> __raw_write_unlock_irq = {"%struct.rwlock_t*"};
			AllocFunctionArg["__raw_write_unlock_irq"] = __raw_write_unlock_irq;
			std::vector<std::string> __raw_read_unlock_irqrestore = {"%struct.rwlock_t*", "i64"};
			AllocFunctionArg["__raw_read_unlock_irqrestore"] = __raw_read_unlock_irqrestore;
			std::vector<std::string> __raw_write_unlock_irqrestore = {"%struct.rwlock_t*", "i64"};
			AllocFunctionArg["__raw_write_unlock_irqrestore"] = __raw_write_unlock_irqrestore;
			std::vector<std::string> __raw_write_unlock_bh = {"%struct.rwlock_t*"};
			AllocFunctionArg["__raw_write_unlock_bh"] = __raw_write_unlock_bh;
			std::vector<std::string> percpu_counter_set = {"%struct.percpu_counter*", "i64"};
			AllocFunctionArg["percpu_counter_set"] = percpu_counter_set;
			std::vector<std::string> percpu_counter_add_batch = {"%struct.percpu_counter*", "i64", "i32"};
			AllocFunctionArg["percpu_counter_add_batch"] = percpu_counter_add_batch;
			std::vector<std::string> percpu_counter_compare = {"%struct.percpu_counter*", "i64"};
			AllocFunctionArg["percpu_counter_compare"] = percpu_counter_compare;
			std::vector<std::string> percpu_counter_add = {"%struct.percpu_counter*", "i64"};
			AllocFunctionArg["percpu_counter_add"] = percpu_counter_add;
			std::vector<std::string> percpu_counter_sum_positive = {"%struct.percpu_counter*"};
			AllocFunctionArg["percpu_counter_sum_positive"] = percpu_counter_sum_positive;
			std::vector<std::string> percpu_counter_sum = {"%struct.percpu_counter*"};
			AllocFunctionArg["percpu_counter_sum"] = percpu_counter_sum;
			std::vector<std::string> percpu_counter_read = {"%struct.percpu_counter*"};
			AllocFunctionArg["percpu_counter_read"] = percpu_counter_read;
			std::vector<std::string> percpu_counter_read_positive = {"%struct.percpu_counter*"};
			AllocFunctionArg["percpu_counter_read_positive"] = percpu_counter_read_positive;
			std::vector<std::string> __percpu_counter_compare = {"%struct.percpu_counter*", "i64","i32"};
			AllocFunctionArg["__percpu_counter_compare"] = __percpu_counter_compare;
			std::vector<std::string> percpu_counter_inc = {"%struct.percpu_counter*"};
			AllocFunctionArg["percpu_counter_inc"] = percpu_counter_inc;
			std::vector<std::string> percpu_counter_sub = {"%struct.percpu_counter*", "i64"};
			AllocFunctionArg["percpu_counter_sub"] = percpu_counter_sub;//percpu相关函数到此为止，此处去掉了初始化init/destroy，因为我们的核心是争用，是对锁的修改
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
			std::vector<std::string> atomic64_cmpxchg = {"%struct.anon.1*","i64","i64"};//atomic相关操作到此结束,同样去掉了一些初始化/删除atomic的操作
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
			AllocFunctionArg["spin_is_contended"] = spin_is_contended;//spin_lock操作到此为止
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


		void TravseAllocUser(std::string funcName,Instruction* originv){   //传进来的Instruction是spin_lock函数参数Value对应的Instruction，有可能是Call语句，Gep指令或者Phi指令或其他，这里对Call,Gep,Phi语句作特殊处理
				if(originv->getOpcode() == Instruction::GetElementPtr){      //第一种情况，对应的是GEP指令，说明我们可以从这获取结构体了。
					GetElementPtrInst *gepinst = dyn_cast<GetElementPtrInst>(originv);
					Type *structType = gepinst->getSourceElementType();//此处获取GEP指令的struct
					if(ReturnTypeRefine(*structType) == "i8*"){            //如果GEP指令中的结构体是i8*,需要特殊处理以下来找出真实的结构体。
						std::string ActualStructType;
						ActualStructType = GetActualStructType(originv,funcName);     //调用GetActualStructType来获得真实结构体。
						fstream<<"FunctionName:"<<funcName<<","<<"ProtectedStruct:"<<ActualStructType<<'\n';
					}else{
						fstream<<"FunctionName:"<<funcName<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*structType)<<'\n';   
					}        
					return ;
				}

				if(CallInst *callInst = dyn_cast<CallInst>(originv)){              //如果对应的是 callInst，此处dyn_cast直接拿了call的函数那么不遍历operand，改为遍历它的args
					for(auto arg=callInst->arg_begin(),arge=callInst->arg_end();arg!=arge;arg++){//这里先转成callInst,然后直接可以取call函数的参数
						if(Value * argnamed=dyn_cast<Value>(arg)){
								if(Instruction *arginst = dyn_cast<Instruction>(arg)){         //这一句用来判断是F函数的参数，经过调试知道，如果这个Value不能强制转换为Instruction了，那么代表它是函数参数了。
									for(auto travarg=arginst->operands().begin();travarg!=arginst->operands().end();++travarg){
										if(Instruction *travargIns=dyn_cast<Instruction>(travarg)){
											if(travargIns==originv){
											return;
											}
										}
									}
									TravseAllocUser(funcName,arginst);						//如果还能转成Instruction，继续递归
								}else{														//如果是函数参数，打印该结构体。
									Type *StructType = argnamed->getType();
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

				for (auto operand = originv->operands().begin();operand != originv->operands().end();++operand){  //如果对应的是普通语句，则正常遍历他的operands来递归。
					Value *opValue = dyn_cast<Value>(operand);
						if(Instruction *opInst = dyn_cast<Instruction>(opValue)){//这一句用来判断是F函数的参数，经过调试知道，如果这个Value不能强制转换为Instruction了，那么代表它是函数参数了。
							for(auto travop = opInst->operands().begin();travop!=opInst->operands().end();++travop){
								if(Instruction *travopIns=dyn_cast<Instruction>(travop))
								{
									if(travopIns==originv){
										return;
									}
								}
							}
							TravseAllocUser(funcName,opInst);				//如果还能转成Instruction，继续递归
						}else{
							Type *StructType = opValue->getType();
							fstream<<"FunctionName:"<<funcName<<","<<"ProtectedStruct:"<<ReturnTypeRefine(*StructType)<<'\n';//如果是函数参数，打印该结构体。
							continue ;
						}
				}
				return;
		}

		std::string GetActualStructType(Instruction *gepInst,std::string funName){    //传入GEP Instruction和函数名（函数名主要用来调试），返回真实的结构体类型。
			std::string ActualStructType = "i8*";
			std::cout<<"-Functiaon Name-:"<<funName<<std::endl;
			for(auto operand = gepInst->operands().begin();operand != gepInst->operands().end();++operand){ //遍历Gep Instruction的operand
				if(CallInst *callInst = dyn_cast<CallInst>(operand)){              //如果该operand对应的是一句call语句
					if(llvm::Function *voidFunc = llvm::dyn_cast<llvm::Function>(callInst->getCalledOperand()->stripPointerCasts())){//通过这个方法获取call函数名是因为会遇到call bitcast这种case，直接用getName()会报错。
						std::cout<<"void Call to => " << voidFunc ->getName().str() << "\n";
						std::string ActualAllocFuncName  = voidFunc->getName().str();
						GetActualFName(ActualAllocFuncName);
						if(AllocFunctionNames.find(ActualAllocFuncName) != AllocFunctionNames.end()){ //判断call的是不是kmalloc函数
							Value *kmVar = dyn_cast<Value>(callInst);
							if(!kmVar->use_empty()){                                                //如果是kmalloc函数，找kmalloc的user
								for(llvm::Value::use_iterator UB=kmVar->use_begin(),UE=kmVar->use_end();UB!=UE;++UB){
									User* user=UB->getUser();                                    //一般情况下kmalloc的紧接的user中就有bitcast将kamalloc分配的i8*转换为真实的结构体。所以我们只需要在第一次user里找bitcast语句就行
									if(Instruction* userInst = dyn_cast<Instruction>(user)){      
										if(userInst->getOpcode() == Instruction::BitCast){        //找出bitcast语句
											Value *userVar = dyn_cast<Value>(userInst);
											Type *userType = userVar->getType();                  //bitcast语句对应的Value的TypeName就是要找的真实结构体。
											std::cout<<"User Type:"<<ReturnTypeRefine(*userType)<<std::endl; 
											ActualStructType = ReturnTypeRefine(*userType);//这里获取的是处理之后inode.xxxx中的inode struct name.
										}
									}
								}
							}
						}
					}
				}
				if(Instruction *Inst = dyn_cast<Instruction>(operand)){   //调试用，调试gep指令和call kmalloc函数之间还会不会有别的语句。
					if(Inst->getOpcode() == Instruction::BitCast){
						std::cout<<"Has a BitCast Middle"<<std::endl;
					}
				}
			}

			return ActualStructType;
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
							if(AtomicFunctionNames.find(mystr) != AtomicFunctionNames.end()){//此处已修改完成，针对atomic.xxxx的情况，使用了四个判断1.名字2.参数
								//个数3.参数类型4.返回值类型。此处完成的是第一个对名字的判断
//								fstream<<"filtered function name"<<":";
//								fstream<<FNameValue<<'\n';
								if(ActualFName != nullptr){
									if((int)ActualFName[0] >= 48 && (int)ActualFName[0] <= 57){   //这个地方有可能遇到testatomicx是atomic的情况?会漏？，如果是锁函数且后面带不同数字，则检查参数是否一样。此处取裁剪得到的第一个数，如果是个数，则对函数参数进行判断
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
									if (const GlobalValue* G = dyn_cast<GlobalValue>(arggb)){//这个地方可能遇到别名分析?假设全局传给临时？临时传给atomic?
										fstream<<"FunctionName:";
										fstream<<F.getName();
										fstream<<",";
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
										TravseAllocUser(F.getName().str(),argInst);//这里传入lock函数参数的Instruction和F的名字，F用作调试
									}
									std::vector<std::string>::iterator k = FunctionArgs.begin();
									FunctionArgs.erase(k);
								}
								continue;

							}
						}
					}
				}
		return false;
		}
	};
}


char Resourceanalysis::ID = 0;
static RegisterPass<Resourceanalysis> X("resourceanalysis", "resource analysis pass");

