#include "PexCallGraph.h"

#include "gatlin_idcs.h"

PexCallGraph gCallGraph;

std::vector<llvm::Function*> PexCallGraph::findCallTargets(llvm::CallInst *call) {
	auto *target_func = getCallTarget(call);
	if (target_func != nullptr) {
		return {target_func};
	}
	std::vector<llvm::Function*> &id_targets = gatlin_idcs.idcs2callee[call];
	if (!id_targets.empty()) {
		return id_targets;
	}
	return {};
}

void PexCallGraph::addLink(llvm::Function *caller, llvm::Function *callee, link_info *info) {
	CGNode *caller_node = getNode(caller, false);
	CGNode *callee_node = getNode(callee, false);
	caller_node->callees.insert(std::make_pair(callee_node, info));
	callee_node->callers.insert(std::make_pair(caller_node, info));
}

void PexCallGraph::removeLink(llvm::Function *caller, llvm::Function *callee, link_info *info) {
	assert(false);
}


PexCallGraph::CGNode *PexCallGraph::getNode(llvm::Function *f, bool find_caller) {
  	CGNode *n;
	auto ni = nodes.find(f);
	if (ni == nodes.end()) {
	    n = new CGNode(f);
	    nodes.insert(std::make_pair(f, n));
	}
	else {
		n = (*ni).second;
	}

	if (find_caller && !n->inited) {
	    CallUseSet call_uses;
	    CallUse::from_value(f, f, std::unordered_set<llvm::Value *>(), call_uses);
	    for (auto use : call_uses) {
	        assert(use.callee == f);
	        llvm::Function *caller = use.inst->getFunction();
	        addLink(caller, f, new PexCallGraph::link_info(use.inst));
	    }
	    n->inited = true;
	}	
	return n;
}


void CallUse::from_value(llvm::Value *value, llvm::Function *callee, std::unordered_set<llvm::Value *> checked, CallUseSet &set, bool is_indirect) {
  	// find all calls to the callee
  	// by iterating through use-def chain
  	// parameter value is the temp value in the use-def chain
  	if (checked.find(value) != checked.end())
    	return ;
  	checked.insert(value);

	if (value == callee) {
    	std::vector<llvm::Instruction *> &calls = gatlin_idcs.callee2idcs[callee];
    	for (llvm::Instruction *inst : calls) {
        	if (auto call = llvm::dyn_cast<llvm::CallInst>(inst)) {
            	CallUse use;
            	use.callee = callee;
            	use.inst = call;
        		set.push_back(use);
			}
          	else{
              	llvm::errs() << "not callinst: " << *inst << "\n";
            }
        }
	}
  
  	for (auto &use : value->uses()) {
    	auto *user = use.getUser();
    	llvm::Value *v = llvm::dyn_cast<llvm::Value>(user);
    	if (llvm::CallInst *insn = llvm::dyn_cast<llvm::CallInst>(v)) {
        	if (!is_indirect) {
            	if (insn->getCalledOperand() == value) {
                	CallUse use;
                	use.callee = callee;
                	use.inst = insn;
                	set.push_back(use);
                }
              	else {
                  	assert(insn->isDataOperand(&use));
                  	unsigned arg_index = insn->getDataOperandNo(&use);
                  	llvm::Function *f = getCallTarget(insn);
                  	if (!f) {
                      	// llvm::errs() << "call use unknown call target " << *insn << "\n";
                    }
                  	else {
                    	if (arg_index < f->arg_size()) {
                          	from_value(f->getArg(arg_index), callee, checked, set);
                        }
                      	else {
                          	assert(f->isVarArg());
                          	llvm::errs() << "call use function passed to function: " << f->getName() << " " << arg_index << "\n";
                        }
                    }
                }
            }
          	else
            	from_value(insn, callee, checked, set);
        }
      	else if (llvm::SelectInst *insn = llvm::dyn_cast<llvm::SelectInst>(v)) {
          	from_value(insn, callee, checked, set);
        }
      	else if (llvm::PHINode *phi = llvm::dyn_cast<llvm::PHINode>(v)) {
          	from_value(phi, callee, checked, set);
        }
      	else if (llvm::ReturnInst *insn = llvm::dyn_cast<llvm::ReturnInst>(v)) {
          	from_value(insn->getFunction(), callee, checked, set, true);
        }
      	else if (llvm::BitCastOperator *op = llvm::dyn_cast<llvm::BitCastOperator>(v)) {
          	llvm::PointerType *pt_type = llvm::dyn_cast<llvm::PointerType>(op->getDestTy());
          	from_value(op, callee, checked, set);
        }
      	else if (auto store = llvm::dyn_cast<llvm::StoreInst>(v)) {
          	if (store->getValueOperand() == value) {
              	/*
              	load_info store_info = get_load_info(store);
              	llvm::errs() << "call use stored function: " << callee->getName() << "\n";
              	*/
              	/*
              	if (isFunctionPointerType(value)) {
                  	std::unordered_set<load_info> store_infos;
                  	store_infos.insert(store_info);
                  	store_load_set loads = pass->find_loads(store_infos);
                  	for (llvm::LoadInst *load : loads[store_info]) {
                      	from_value(load, callee, checked, set);
                    }
                }
              	else {
                  	llvm::errs() << "call use function casted: " << callee->getName() << " " << *value->getType() << "\n";
                }
              */
            }
          	else {
              	llvm::errs() << "call use store to function: " << *store << "\n";
            }
        }
      	else if (llvm::isa<llvm::ICmpInst>(v)) {
          	// ignore
        }
      	else if (auto inst = llvm::dyn_cast<llvm::Instruction>(v)) {
          	// llvm::errs() << "call use unknown instruction: " << *inst << "\n";
        }
      	else if (auto op = llvm::dyn_cast<llvm::PtrToIntOperator>(v)) {
          	// llvm::errs() << "call use ptrtoint: " << *op << "\n";
        }
      	else if (auto op = llvm::dyn_cast<llvm::Operator>(v)) {
          	// llvm::errs() << "call use unknown operator: " << *op << "\n";
        }
      	else if (llvm::isa<llvm::GlobalVariable>(v) || llvm::isa<llvm::Constant>(v)) {
          	// ignore
        }
      	else {
          	// llvm::errs() << "call use unknown: " << *v << "\n";
        }
    }
}

std::set<llvm::CallInst*> PexCallGraph::findCallSites(llvm::Function *function) {
	auto *node = getNode(function);
	std::set<llvm::CallInst*> result;
	for (auto &item : node->callers) {
		result.insert(item.second->call);
	}
	return result;
}

std::set<llvm::Value *> PexCallGraph::findActualParams(llvm::Argument *formal_param) {
	auto *node = getNode(formal_param->getParent());
	unsigned argno = formal_param->getArgNo();
	std::set<llvm::Value *> result;
	for (auto &item : node->callers) {
		result.insert(item.second->call->getArgOperand(argno));
	}
	return result;
}