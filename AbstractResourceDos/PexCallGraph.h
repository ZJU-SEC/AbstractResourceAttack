#pragma once

#include "Utils.h"

struct CallUse;
typedef std::vector<CallUse> CallUseSet;

struct CallUse {
	llvm::CallInst *inst;
	llvm::Function *callee;
	static void from_value(llvm::Value *value, llvm::Function *callee, std::unordered_set<llvm::Value *> checked, CallUseSet &set, bool is_indirect = false);
};

class PexCallGraph {
public:
	struct link_info {
	  llvm::CallInst *call;
	  link_info(llvm::CallInst *call = nullptr) : call(call) {}
	};
	struct CGNode {
		llvm::Function *f;
		bool inited;
		CGNode(llvm::Function *f) : f(f), inited(false) {}
		std::unordered_multimap<CGNode *, link_info *> callers;
		std::unordered_multimap<CGNode *, link_info *> callees;
	};
	PexCallGraph() = default;
	~PexCallGraph() = default; // TODO free nodes
	void addLink(llvm::Function *caller, llvm::Function *callee, link_info *info = nullptr);
	void removeLink(llvm::Function *caller, llvm::Function *callee, link_info *info = nullptr);
	std::vector<llvm::Function*> findCallTargets(llvm::CallInst *);
	CGNode *getNode(llvm::Function *f, bool find_caller = true);
	std::set<llvm::CallInst*> findCallSites(llvm::Function *f);
	std::set<llvm::Value *> findActualParams(llvm::Argument *formal_param);
private:
	std::unordered_map<llvm::Function *, CGNode *> nodes;
};
