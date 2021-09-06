#pragma once
#include <unordered_map>

struct gatlin_idcs_struct
{
  std::unordered_map<llvm::Instruction *, std::vector<llvm::Function *> > idcs2callee;
  std::unordered_map<llvm::Function *, std::vector<llvm::Instruction *> > callee2idcs;
};

extern gatlin_idcs_struct gatlin_idcs;
