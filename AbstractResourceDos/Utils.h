#pragma once
#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/CFG.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Utils/UnifyFunctionExitNodes.h"
#include "llvm/IR/DebugInfoMetadata.h"



#include <map>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <utility>
#include <cassert>
#include <set>
#include <cstring>
#include <vector>
#include <iostream>
#include <fstream>

llvm::Value *stripConstCastExpr(llvm::Value *);

llvm::Value *stripAllConstExpr(llvm::Value *);

std::vector<llvm::ReturnInst *> getReturnInsts(llvm::Function *f);

llvm::Function *getCallTarget(llvm::CallInst *call);

std::vector<int> get_indices(llvm::GetElementPtrInst *gep);

std::string to_function_name(std::string llvm_name);

std::string to_struct_name(std::string llvm_name);

void backtrace_syscall_and_print(int hop, llvm::Function* f);

bool isFunctionPointerType(llvm::Value *v);

bool isSyscall(llvm::Function *);
bool isInterruptHandler(llvm::Function *);
bool isIndirectThreadFunc(llvm::Function *);

std::string print_debugloc(llvm::Instruction *);

std::string ReturnTypeRefine(llvm::Type &rt);

template<class T> std::vector<T *> find_phi_select_user(llvm::Value *vin)
{
	std::vector<T *> result;
	std::unordered_set<llvm::Value *> waiting, checked;
	waiting.insert(vin);
	while (!waiting.empty()) {
	    auto vit = waiting.begin();
	    llvm::Value *vin = *vit;
	    waiting.erase(vit);
	    if (checked.find(vin) != checked.end())
	    	continue;
	    checked.insert(vin);
	
	    for (llvm::Value *v : vin->users()) {
	        if (auto t_value = llvm::dyn_cast<T>(v))
	        	result.push_back(t_value);
	        else if (auto phi = llvm::dyn_cast<llvm::PHINode>(v)) {
	            waiting.insert(phi);
	    	}
	        else if (auto ci = llvm::dyn_cast<llvm::CastInst>(v)) {
	            waiting.insert(ci);
	        }
	        else if (auto select = llvm::dyn_cast<llvm::SelectInst>(v)) {
	            waiting.insert(select);
	        }
	    }
	}
	return result;
}



template<class T> std::vector<T *> find_user(llvm::Value *vin)
{
  std::vector<T *> result;
  for (llvm::Value *v : vin->users())
    {
      if (auto t_value = llvm::dyn_cast<T>(v))
        result.push_back(t_value);

      /*
      if (ignore_condition)
        {
          if (auto t_value = llvm::dyn_cast<llvm::SelectInst>(v))
            {
              auto sub_result = find_user<T>(t_value, true);
              result.insert(result.end(), sub_result.begin(), sub_result.end());
            }
        }
      */
    }
  return result;
}

class typeJudger {
public:
  typeJudger(llvm::Module* M) : _M_(M) {}
  bool isTwoStructTyEqual(llvm::Type *ty_a, llvm::Type *ty_b);

private:
  std::set<llvm::Type*> _typeSet_;
  llvm::Module *_M_;
  std::string __get_prefix(llvm::StructType *st);
};