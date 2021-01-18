/*
 * common types
 * 2018 Tong Zhang<t.zhang2@partner.samsung.com>
 */
#ifndef _COMMON_TYPES_
#define _COMMON_TYPES_

#include <list>
#include <stack>
#include <queue>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

#include <llvm/IR/Function.h>

using namespace llvm;

enum _REACHABLE
{
    RCHKED,//fully checked
    RPRCHK,//partically checked
    RNOCHK,//no check at all
    RKINIT,//hit kernel init functions, ignored
    RUNRESOLVEABLE,//unable to resolve due to indirect call
    RNA,//not available
};

typedef std::list<std::string> StringList;
typedef std::list<Value*> ValueList;
typedef std::list<Instruction*> InstructionList;
typedef std::list<CallInst*> CallInstList;
typedef std::list<BasicBlock*> BasicBlockList;
typedef std::list<Function*> FunctionList;
typedef std::list<Type*> TypeList;
typedef std::list<int> Indices;

typedef std::unordered_set<std::string> StringSet;
typedef std::unordered_set<Value*> ValueSet;
typedef std::unordered_set<Type*> TypeSet;
typedef std::unordered_set<Instruction*> InstructionSet;
typedef std::unordered_set<CallInst*> CallInstSet;
typedef std::unordered_set<const Instruction*> ConstInstructionSet;
typedef std::unordered_set<BasicBlock*> BasicBlockSet;
typedef std::unordered_set<Function*> FunctionSet;
typedef std::unordered_set<CallInst*> InDirectCallSites;
typedef ValueSet ModuleSet;

typedef std::unordered_map<Function*,_REACHABLE> FunctionToCheckResult;
typedef std::unordered_map<Function*, InstructionSet*> Function2ChkInst;
typedef std::unordered_map<Function*, InstructionSet*> Function2CSInst;
typedef std::unordered_map<Function*, int> FunctionData;
typedef std::unordered_map<Type*, std::unordered_set<Function*>> TypeToFunctions;
typedef std::unordered_map<Type*, std::unordered_set<int>> Type2Fields;
typedef std::unordered_map<Type*, InstructionSet*> Type2ChkInst;
typedef std::unordered_map<Type*, ModuleSet*> ModuleInterface2Modules;
typedef std::unordered_map<Value*, InstructionSet*> Value2ChkInst;
typedef std::unordered_map<Instruction*, FunctionSet> Inst2Func;
typedef std::unordered_map<const Instruction*, FunctionSet> ConstInst2Func;
typedef std::unordered_map<std::string, int> Str2Int;

//dynamic KMI
//pair between indices and function set(fptr stored into this position)
//typedef std::pair<Indices*, FunctionSet*> IFPair;
// all those pairs
//typedef std::list<IFPair*> IFPairs;
// map struct type to pairs
//typedef std::unordered_map<StructType*, IFPairs*> SIFMap;


inline std::string to_function_name(std::string llvm_name)
{
  size_t dot;
  dot = llvm_name.find_first_of('.');
  return llvm_name.substr(0, dot);
}

inline std::string to_struct_name(std::string llvm_name)
{
  size_t dot_first, dot_last;
  dot_first = llvm_name.find_first_of('.');
  dot_last = llvm_name.find_last_of('.');
  if (dot_first == dot_last)
    return llvm_name;
  else
    return llvm_name.substr(0, dot_last);
}


struct struct_field
{
    StructType *stype;
    Indices indices;
    
    struct_field() {}
    struct_field(StructType *stype, Indices indices) : stype(stype), indices(indices) {}
    bool operator==(const struct_field &field) const {
        assert(field.stype && stype);
        bool is_same_struct = false;
        if ((field.stype->isLiteral() || stype->isLiteral())
            || (!field.stype->hasName() || !stype->hasName())
            || (field.stype->getName().startswith("struct.anon.") || stype->getName().startswith("struct.anon."))
            || (field.stype->getName().startswith("union.anon.") || stype->getName().startswith("union.anon."))) {
            is_same_struct = field.stype->isLayoutIdentical(stype);
        } else {
            is_same_struct = (to_struct_name(std::string(field.stype->getName())) == to_struct_name(std::string(stype->getName())));
        }
        return is_same_struct
            && (field.indices.size() == indices.size()
                && std::equal(field.indices.begin(), field.indices.end(), indices.begin()));
    }
    bool operator!=(const struct_field &field) const {
        return !(*this == field);
    }
};

namespace std
{
    template<> struct hash<struct_field>
    {
        std::size_t operator()(struct_field const& field) const noexcept {
            std::string name;
            if (field.stype->isLiteral()) {
                name = "";
            } else {
                name = to_struct_name(std::string(field.stype->getName()));
            }
            return std::hash<std::string>()(name);
        }
  };
}

/*
typedef std::pair<Indices,FunctionSet> IFPair;
//all those pairs
typedef std::list<IFPair> IFPairs;
//map struct type to pairs
*/
typedef std::unordered_map<struct_field, FunctionSet> DMInterface;

//interface for lowertypetests
// typedef std::pair<StructType*, Indices> StypeInd;
// each StypeInd corresponds to a target function set and 
// a set of indirect calls

typedef std::unordered_map<struct_field, FunctionSet> SI2Functions;
typedef std::unordered_map<struct_field, CallInstSet> SI2CallInsts;

//typedef std::unordered_set<SI2Functions*> SIset;

//used for initializations
typedef std::pair<Indices, Function*> IF;
typedef std::pair<Type*, IF*> TF;
typedef std::set<TF*> TFs;

typedef std::pair<Value*, Value*> funcUsePair;
typedef std::set<funcUsePair> funcUsePairSet;

#endif//_COMMON_TYPES_
