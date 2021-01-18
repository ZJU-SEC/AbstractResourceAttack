/*
 * STEM-CFI 
 * build CFG including indirect calls
 * 2020 <jjjinmeng.zhou@gmail.com>
 */

#include "StructCFI.h"
#include "gatlin_idcs.h"

//#include "cvfa.h"
//my aux headers
//#include "llvm/Transforms/PexCFI/color.h"
//#include "llvm/Transforms/PexCFI/stopwatch.h"
//#include "llvm/Transforms/PexCFI/utility.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include <sstream>
#include <iostream>

#define TOTOAL_NUMBER_OF_STOP_WATCHES 2
#define WID_0 0
#define WID_KINIT 1
#define WID_CC 1
#define WID_PI 1

//STOP_WATCH(TOTOAL_NUMBER_OF_STOP_WATCHES);

using namespace llvm;

char GatlinModule::ID;

static std::unordered_set<Value *> find_bitcasts(Value *value) {
    std::unordered_set<Value *> waiting, visited, result;
    result.insert(value);
    waiting.insert(value);
    while (!waiting.empty()) {
        Value *v = *waiting.begin();
        waiting.erase(v);
        if (visited.find(v) != visited.end())
            continue;
        visited.insert(v);

        for (auto u : v->users()) {
            if (auto bitcast = dyn_cast<BitCastOperator>(u)) {
                result.insert(bitcast);
                waiting.insert(bitcast);
            } else if (auto bitcast = dyn_cast<BitCastInst>(u)) {
                result.insert(bitcast);
                waiting.insert(bitcast);
            } else if (auto cexpr = dyn_cast<ConstantExpr>(u)) {
                if (cexpr->isCast()) {
                    result.insert(cexpr);
                    waiting.insert(cexpr);
                }
            }
        }
    }
    return result;
}


Inst2Func idcs2callee;
InDirectCallSites idcs;

Instruction* x_dbg_ins;
std::list<int> x_dbg_idx;

std::vector<Instruction *> tmp_insts;

static ValueList dbglst;


gatlin_idcs_struct gatlin_idcs;
std::unordered_map<CallInst *, std::unordered_set<Function *> > CFG;//only for indirect calls

std::unordered_map<std::unordered_set<struct_field> *, std::unordered_map<Value *, std::unordered_set<Function *> > > set2fu;
std::unordered_map<std::unordered_set<struct_field> *, std::unordered_set<CallInst *> > set2ci;

std::unordered_map<struct_field, std::unordered_set<struct_field> *> si_disjoint_sets= {};
std::unordered_map<Function *, std::unordered_map<Value *, std::unordered_set<struct_field> > > fu2si;
std::unordered_map<CallInst *, std::unordered_set<struct_field> > ci2si;
std::unordered_map<struct_field, std::unordered_map<Value *, std::unordered_set<Function *> > > si2fu;
SI2Functions si2func;
SI2CallInsts si2call;
// std::unordered_set<struct_field> siset;
std::unordered_set<struct_field> siset_delete;

// int stripPointerType(llvm::Type *type) {
//     llvm::Type *element_type = type;
//     int res = 0;
//     while (isa<llvm::PointerType>(element_type)) {
//         element_type = element_type->getPointerElementType();
//         ++res;
//     }
//     return res;
// }

// int stripArrayType(llvm::Type *type) {
//     llvm::Type *element_type = type;
//     int res = 0;
//     while (isa<>)
// }

// bool naiveTypeComp(llvm::Type *t1, llvm::Type *t2) {
//     if (t1 == t2)
//         return true;
//     llvm::Type *element_t1 = t1, *element_t2 = t2;
//     while (isa<llvm::PointerType>(element_t1)) {
//         element_t2 = llvm::dyn_cast<llvm::PointerType>(element_t2);
//         if (!isa<llvm::PointerType>(element_t2))
//             return false;
//         element_t1 = element_t1->getPointerElementType();
//         element_t2 = element_t2->getPointerElementType();
//     }
// }

class FuncTypeComp {
public:
    static bool isEqual(llvm::Type* const &t1, llvm::Type* const &t2) {
        if (t1 == t2)
            return true;
        llvm::StructType *sty1 = llvm::dyn_cast<llvm::StructType>(t1);
        llvm::StructType *sty2 = llvm::dyn_cast<llvm::StructType>(t2);
        if (sty1 && sty2 && sty1->hasName() && sty2->hasName()) {
            auto name1 = sty1->getName().str();
            auto name2 = sty2->getName().str();
            if (name1.find("struct.anon") == name1.npos && name2.find("struct.anon") == name2.npos) {
                str_truncate_dot_number(name1);
                str_truncate_dot_number(name2);
                return name1 == name2;
            }
            else
                return name1 == name2;
            
        }
        else if (isSameCategory(t1, t2)) {
            if (t1->getNumContainedTypes() == 0 || t2->getNumContainedTypes() == 0) {
                return false;
            }
            if (t1->getNumContainedTypes() == t2->getNumContainedTypes()) {
                for (int i = 0; i < t1->getNumContainedTypes(); ++i) {
                    if (!isEqual(t1->getContainedType(i), t2->getContainedType(i)))
                        return false;
                }
                return true;
            }
        }
        return false;
    }
private:
    static bool isSameCategory(llvm::Type *t1, llvm::Type *t2) {
        if (isa<llvm::StructType>(t1) && isa<llvm::StructType>(t2))
            return true;
        if (isa<llvm::PointerType>(t1) && isa<llvm::PointerType>(t2))
            return true;
        if (isa<llvm::FunctionType>(t1) && isa<llvm::FunctionType>(t2))
            return true;
        if (isa<llvm::ArrayType>(t1) && isa<llvm::ArrayType>(t2))
            return true;
        return false;
    }
};

std::vector<llvm::Function*> addrtaken_funcs;
// std::unordered_set< std::unordered_set<struct_field>* > equivalence_sets;
//static void finish_pass();
/*
namespace{

class Gatlin : public ModulePass
{
public:
    static char ID;
    
    Gatlin():ModulePass(ID){
        initializeGatlinPass(*PassRegistry::getPassRegistry());
    }
    bool runOnModule(Module &M) override {
        errs()<<"--- PROCESS FUNCTIONS ---"<<"\n";
        
        GatlinModule(M).process_cpgf();

        finish_pass();
        
        errs()<<"--- DONE! ---"<<"\n";

        return false;
    }
};
}

char Gatlin::ID = 0;
*/

//INITIALIZE_PASS(Gatlin, "gatlin", "pex cfi", false,
//                false)

//ModulePass* llvm::createGatlinPass(){
//    return new Gatlin();
//}

static void exportCFG()
{
    for (auto &link : CFG)
    {
        Instruction *inst = link.first;
        for (auto f : link.second)
        {
            gatlin_idcs.idcs2callee[inst].push_back(f);
            gatlin_idcs.callee2idcs[f].push_back(inst);
            // llvm::errs() << "XXXXXX " << inst->getFunction()->getName() << "\n";
        }
    }
}

bool findAddressTaken(llvm::Function *function) {
    std::deque<llvm::Value*> pqueue({function});
    while (!pqueue.empty()) {
        auto *curval = pqueue.front();
        pqueue.pop_front();
        for (auto user : curval->users()) {
            if (auto *bco = llvm::dyn_cast<llvm::BitCastOperator>(user)) {
                pqueue.push_back(bco);
            }
            else if (auto *conaggr = llvm::dyn_cast<llvm::ConstantAggregate>(user)) {
                return true;
            }
            else if (auto *ci = llvm::dyn_cast<llvm::CallInst>(user)) {
                if (ci->hasArgument(curval))
                    return true;
            }
            else if (auto *si = llvm::dyn_cast<llvm::SelectInst>(user)) {
                return true;
            }
            else if (auto *phi = llvm::dyn_cast<llvm::PHINode>(user)) {
                return true;
            }
            else if (auto *stri = llvm::dyn_cast<llvm::StoreInst>(user)) {
                return true;
            }
        }
    }
    return false;
}

void collectAddrTaken(llvm::Module *module) {
    for (auto &function : *module) {
        if (findAddressTaken(&function)) {
            addrtaken_funcs.push_back(&function);
        }
    }
    // llvm::StructType *t1, *t2;
    // for (auto ty : module->getIdentifiedStructTypes()) {
    //     if (ty->hasName()) {
    //         if (ty->getName().str() == "struct.softirq_action")
    //             t1 = ty;
    //         else if (ty->getName().str() == "struct.softirq_action.56974")
    //             t2 = ty;
    //     }
    // }
    // llvm::errs() << *t1 << "\n" << *t2 << "\n";
    // llvm::errs() << TypeComp()(t1, t2) << "xxxxx\n";
    // llvm::errs() << TypeComp()(t2, t1) << "yyyyy\n";
}



void finish_pass()
{
    for (Instruction *inst : tmp_insts)
    {
        inst->deleteValue();
    }
    tmp_insts.clear();
}

void dumpCFG() {
    for (auto cfs : CFG) {
        errs() << "\nindirect call: " << *cfs.first << "\n";
        errs() << "target functions: \n";
        for (auto f : cfs.second)
            errs() << "\t" << f->getName();
    }
}

llvm::Type* getIndirectCallType(llvm::Instruction *instruction) {
    if (auto *ci = llvm::dyn_cast<llvm::CallInst>(instruction)) {
        if (ci->isIndirectCall() && !ci->isInlineAsm()) {
            return ci->getCalledOperand()->getType();
        }
    }
    return nullptr;
}

void buildCFG(llvm::Module *module) {
    /*errs() << "begin: build CFG set2fu size: " << set2fu.size() <<"\n";
    for (auto p : set2fu) {
        auto set = p.first;
        CallInstSet calls = set2ci[set];
        FunctionSet fsall = {};

        for (auto vf : p.second) {
            auto fs = vf.second;
            for (Function *f : fs) {
                fsall.insert(f);
            }
        }

        for (CallInst *ci : calls) {
            for (Function *f : fsall) {
                CFG[ci].insert(f);
            }
        }
    }*/

    //complement type-based CFG
    for (auto &F : *module) {
        for (auto &BB : F) {
            for (auto &I : BB) {
                auto *tty = getIndirectCallType(&I);
                if (tty != nullptr) {
                    auto *ci = llvm::dyn_cast<CallInst>(&I);
                    FunctionSet cur_funcset;
                    for (auto &function : addrtaken_funcs) {
                        if (FuncTypeComp::isEqual(function->getType(), tty)) {
                            cur_funcset.insert(function);
                        }
                    }
                    if (CFG[ci].empty()) {
                        CFG[ci].insert(cur_funcset.begin(), cur_funcset.end());
                    }
                    else {
                        // intersect the two sets
                        FunctionSet erase_funcs;
                        for (auto item : CFG[ci]) {
                            if (cur_funcset.count(item) == 0)
                                erase_funcs.insert(item);
                        }
                        for (auto ef : erase_funcs)
                            CFG[ci].erase(ef);
                    }
                }
            }
        }
    }
    errs() << "end: build CFG size: " << CFG.size() <<"\n";
    exportCFG();

    //dumpCFG();
}


///////////////////////////////////////////
//. from utility.cpp

/*SimpleSet* skip_vars;
SimpleSet* skip_funcs;
SimpleSet* crit_syms;
SimpleSet* kernel_api;

void initialize_gatlin_sets(StringRef knob_skip_func_list,
        StringRef knob_skip_var_list,
        StringRef knob_crit_symbol,
        StringRef knob_kernel_api)
{
    llvm::errs()<<"Load supplimental files...\n";
    StringList builtin_skip_functions(std::begin(_builtin_skip_functions),
            std::end(_builtin_skip_functions));
    skip_funcs = new SimpleSet(knob_skip_func_list, builtin_skip_functions);
    if (!skip_funcs->use_builtin())
        llvm::errs()<<"    - Skip function list, total:"<<skip_funcs->size()<<"\n";

    StringList builtin_skip_var(std::begin(_builtin_skip_var),
            std::end(_builtin_skip_var));
    skip_vars = new SimpleSet(knob_skip_var_list, builtin_skip_var);
    if (!skip_vars->use_builtin())
        llvm::errs()<<"    - Skip var list, total:"<<skip_vars->size()<<"\n";

    StringList builtin_crit_symbol;
    crit_syms = new SimpleSet(knob_crit_symbol, builtin_crit_symbol);
    if (!crit_syms->use_builtin())
        llvm::errs()<<"    - Critical symbols, total:"<<crit_syms->size()<<"\n";

    StringList builtin_kapi;
    kernel_api = new SimpleSet(knob_kernel_api, builtin_kapi);
    if (!kernel_api->use_builtin())
        llvm::errs()<<"    - Kernel API list, total:"<<kernel_api->size()<<"\n";
}*/


//only care about case where all indices are constantint
StructType* get_gep_indices_type(GetElementPtrInst* gep, Indices& indices)
{
    if (!gep)
        return nullptr;
    //replace all non-constant with zero
    //because they are literally an array...
    //and we are only interested in the type info
    assert(indices.empty() && "output indices not empty");
    indices.clear();
    StructType *last_struct = nullptr;
    Indices last_indices;
    for (auto i = gep->idx_begin(); i!=gep->idx_end(); ++i)
    {
        int index = 0;
        Type *upper_type = indices.empty() ? gep->getSourceElementType()
            : GetElementPtrInst::getIndexedType(gep->getSourceElementType(), ArrayRef<uint64_t>(std::vector<uint64_t>(indices.begin(), indices.end())));
        if (auto st_type = dyn_cast<StructType>(upper_type))
        {
            last_struct = st_type;
            last_indices = indices;
            ConstantInt* idc = dyn_cast<ConstantInt>(i);
            if (idc)
                index = idc->getSExtValue();
            else
                index = 0;
        }
        else
        {
            last_struct = nullptr;
            index = 0;
        }
        
        indices.push_back(index);
    }

    if (!last_struct)
        return nullptr;

    for (unsigned i = 0; i < last_indices.size(); i++)
        indices.pop_front();
    return last_struct;
    /*
    Type *type = gep->getSourceElementType();
    if (auto ptr = dyn_cast<PointerType>(type))
    {
        return dyn_cast<StructType>(ptr->getElementType()) ? dyn_cast<StructType>(ptr->getElementType()) :
            (StructType *)ptr->getElementType();
    }
    else
    {
        return dyn_cast<StructType>(gep->getSourceElementType());
    }
    */
}
Function* get_callee_function_direct(Instruction* i)
{
    CallInst* ci = dyn_cast<CallInst>(i);
    if (Function* f = ci->getCalledFunction())
        return f;
    Value* cv = ci->getCalledOperand();
    Function* f = dyn_cast<Function>(cv->stripPointerCasts());
    return f;
}


void dump_gdblst(ValueList& list)
{
    errs()<<"Process List:"<<"\n";
    int cnt = 0;
    for (auto* I: list)
    {
        errs()<<"  "<<cnt<<":";
        if (Function*f = dyn_cast<Function>(I))
            errs()<<f->getName();
        else
        {
            I->print(errs());
            if (Instruction* i = dyn_cast<Instruction>(I))
            {
                errs()<<"  ";
                i->getDebugLoc().print(errs());
            }
        }
        errs()<<"\n";
        cnt++;
    }
    errs()<<"-------------"<<"\n";
}

/*
 * part of dynamic KMI - a data flow analysis
 * for value v, we want to know whether it is assigned to a struct field, and
 * we want to know indices and return the struct type
 * NULL is returned if not assigned to struct
 *
 * ! there should be a store instruction in the du-chain
 * TODO: extend this to inter-procedural analysis
 */
//known interesting
inline bool stub_fatst_is_interesting_value(Value* v)
{

    if (isa<BitCastInst>(v)||
        isa<CallInst>(v)||
        isa<ConstantExpr>(v)||
        isa<StoreInst>(v) ||
        isa<Function>(v))
        return true;
    if (SelectInst* si = dyn_cast<SelectInst>(v))
    {
        //result of select shoule be the same as v
        if (si->getType()==v->getType())
            return true;
    }
    if (PHINode *phi = dyn_cast<PHINode>(v))
    {
        if (phi->getType()==v->getType())
            return true;
    }
    //okay if this is a function parameter
    //a value that is not global and not an instruction/phi
    if ((!isa<GlobalValue>(v))
            && (!isa<Instruction>(v)))
    {
        return true;
    }

    return false;
}

//compare two indices
bool indices_equal(const Indices &a, const Indices &b)
{
    if (a.size()!=b.size())
        return false;
    auto ai = a.begin();
    auto bi = b.begin();
    while(ai!=a.end())
    {
        if (*ai!=*bi)
            return false;
        bi++;
        ai++;
    }
    return true;
}

std::unordered_set<struct_field> get_struct_fields_from_addr(Value *addr, std::unordered_set<Value *> &cannot_handle)
{
    std::unordered_set<struct_field> fields;
    ValueList worklist;
    ValueSet visited;
    worklist.push_back(addr);

    //use worklist to track what du-chain
    while (worklist.size())
    {
        Value *po;
        //fetch an item and skip if visited
        po = worklist.front();
        worklist.pop_front();
        if (visited.count(po))
            continue;
        visited.insert(po);

        /*
         * pointer operand is global variable?
         * dont care... we can extend this to support fine grind global-aa, since
         * we already know the target
         */
        if (dyn_cast<GlobalVariable>(po))
            continue;
        if (ConstantExpr* cxpr = dyn_cast<ConstantExpr>(po))
        {
            Instruction* cxpri = cxpr->getAsInstruction();
            tmp_insts.push_back(cxpri);
            worklist.push_back(cxpri);
            continue;
        }
        if (Instruction* i = dyn_cast<Instruction>(po))
        {
            switch(i->getOpcode())
            {
                case(Instruction::PHI):
                {
                    PHINode* phi = dyn_cast<PHINode>(i);
                    for (unsigned int i=0;i<phi->getNumIncomingValues();i++)
                        worklist.push_back(phi->getIncomingValue(i));
                    break;
		      //return NULL;
                }
                case(Instruction::Select):
                {
                    SelectInst* sli = dyn_cast<SelectInst>(i);
                    worklist.push_back(sli->getTrueValue());
                    worklist.push_back(sli->getFalseValue());
                    break;
                } 
                case(BitCastInst::BitCast):
                {
                    BitCastInst *bci = dyn_cast<BitCastInst>(i);
//FIXME:sometimes struct name is purged into i8.. we don't know why,
//but we are not able to resolve those since they are translated
//to gep of byte directly without using any struct type/member/field info
//example: alloc_buffer, drivers/usb/host/ohci-dbg.c
                    worklist.push_back(bci->getOperand(0));
                    break;
                }
                case(Instruction::IntToPtr):
                {
                    IntToPtrInst* i2ptr = dyn_cast<IntToPtrInst>(i);
                    worklist.push_back(i2ptr->getOperand(0));
                    break;
                }
                case(Instruction::GetElementPtr):
                {
                    //only GEP is meaningful
                    GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(i);
                    struct_field field;
                    field.stype = get_gep_indices_type(gep, field.indices);
                    if (field.stype) {
                        if (!field.stype->isLiteral() && field.stype->getName().startswith("union.")) {
                            // worklist.push_back(gep->getOperand(0));
                            cannot_handle.insert(gep);
                        } else {
                            fields.insert(field);
                        }
                    }
                    break;
                }
                case(Instruction::Call):
                {
                    //ignore interprocedural...
                    break;
                }
                case(Instruction::Load):
                {
                    //we are not able to handle load
                    break;
                }
                case(Instruction::Store):
                {
                    //how come we have a store???
                    dump_gdblst(dbglst);
                    //llvm_unreachable("Store to Store?");
                    break;
                }
                case(Instruction::Alloca):
                {
                    //store to a stack variable
                    //maybe interesting to explore who used this.
                    break;
                }
                case(BinaryOperator::Add):
                {
                    //adjust pointer using arithmatic, seems to be weired
                    BinaryOperator *bop = dyn_cast<BinaryOperator>(i);
                    for (unsigned int i=0;i<bop->getNumOperands();i++)
                        worklist.push_back(bop->getOperand(i));
                    break;
                }
                case(Instruction::PtrToInt):
                {
                    PtrToIntInst* p2int = dyn_cast<PtrToIntInst>(i);
                    worklist.push_back(p2int->getOperand(0));
                    break;
                }
                default:
                    // errs()<<"unable to handle instruction:";
                    // i->print(errs());
                    // errs()<<"\n";
                    break;
            }
        }else
        {
            //we got a function parameter
        }
    }
    return fields;
}

/*
 * intra-procedural analysis
 *
 * only handle high level type info right now.
 * maybe we can extend this to global variable as well
 *
 * see if store instruction actually store the value to some field of a struct
 * return non NULL if found, and indices is stored in idcs
 *
 */

StructType* resolve_where_is_it_stored_to(StoreInst* si, Indices& idcs)
{
    // do nothing
    return NULL;
    /*
    struct_field field;
    StructType* ret = NULL;
    //po is the place where we want to store to
    Value* po = si->getPointerOperand();
    bool succeed = get_struct_field_from_addr(po, field);
    if (!succeed)
        return nullptr;
    idcs = field.indices;
    return field.stype;
    */
}

void uses_of_value(
    Value *value,
    std::unordered_set<struct_field> &fields,
    std::unordered_set<CallInst *> &calls,
    std::unordered_set<Value *> &cannot_handle,
    std::unordered_set<Value *> &visited
    );

std::unordered_set<struct_field> *merge_disjoint_sets(std::unordered_set<struct_field> fields);
    
void possible_functions(
    Value *user, Value *v,
    std::unordered_set<struct_field> &fields,
    std::vector<std::pair<Function *, Value *> > &funcuses,
    std::unordered_set<Value *> &cannot_handle,
    std::unordered_set<Value *> &visited
    )
{
    if (visited.find(v) != visited.end())
        return;
    visited.insert(v);

    if (auto load = dyn_cast<LoadInst>(v)) {
        Value *addr = load->getPointerOperand();
        auto load_fields = get_struct_fields_from_addr(addr, cannot_handle);
        fields.insert(load_fields.begin(), load_fields.end());
    } else if (auto func = dyn_cast<Function>(v)) {
        std::unordered_set<struct_field> fields;
        std::unordered_set<CallInst *> calls;
        std::unordered_set<Value *> cannot_handle;
        std::unordered_set<Value *> visited;
        uses_of_value(func, fields, calls, cannot_handle, visited);
        if (cannot_handle.empty()) {
            funcuses.push_back(std::make_pair(func, user));
            merge_disjoint_sets(fields);
        }
    } else if (auto phi = dyn_cast<PHINode>(v)) {
        for (unsigned i = 0; i < phi->getNumIncomingValues(); i++) {
            Value *vi = phi->getIncomingValue(i);
            possible_functions(v, vi, fields, funcuses, cannot_handle, visited);
        }
    } else if (auto sel = dyn_cast<SelectInst>(v)) {
        Value *vtrue = sel->getTrueValue();
        Value *vfalse = sel->getFalseValue();
        possible_functions(v, vtrue, fields, funcuses, cannot_handle, visited);
        possible_functions(v, vfalse, fields, funcuses, cannot_handle, visited);
    } else if (auto bitcast = dyn_cast<BitCastInst>(v)) {
        possible_functions(v, bitcast->getOperand(0), fields, funcuses, cannot_handle, visited);
    } else if (auto bitcast = dyn_cast<BitCastOperator>(v)) {
        possible_functions(v, bitcast->getOperand(0), fields, funcuses, cannot_handle, visited);
    } else if (auto constant = dyn_cast<Constant>(v)) {
        if (constant->isNullValue()) {
            // ignore
        } else {
            cannot_handle.insert(v);
        }
    } else if (auto arg = dyn_cast<Argument>(v)) {
        cannot_handle.insert(v);
    } else {
        cannot_handle.insert(v);
    }
}


std::unordered_set<struct_field> *merge_disjoint_sets(std::unordered_set<struct_field> fields)
{
    /*std::unordered_set<struct_field> *merge_to = nullptr;
    bool hasNewSiset = false;
    for (struct_field field : fields) {
        errs()<<"fields in merge_disjoint_sets: "<<field.stype->getName().str()<<"\n";
        auto siset = si_disjoint_sets[field];
        if (!siset) {
            errs()<<"first if in merge\n";
            siset = new std::unordered_set<struct_field>;
            siset->insert(field);
            si_disjoint_sets[field] = siset;
            for (struct_field field_in : *siset) {
                errs()<<"siset insert: "<<field_in.stype->getName().str()<<"\n";
            }
            delete siset;
        }
        for (struct_field field_in : *siset) {
                errs()<<"siset is: "<<field_in.stype->getName().str()<<"\n";
        }
        if (!merge_to) {
            errs()<<"second if in merge\n";
            merge_to = siset;
        } else if (siset != merge_to) {
            errs()<<"third if in merge\n";
            for(auto field_in = siset->begin();field_in != siset->end();field_in++){
                errs()<<"third if for field_in: "<<field_in->stype->getName().str()<<"\n";
                si_disjoint_sets[*field_in] = merge_to;

            }
            /*for (struct_field field_in : *siset) {
                errs()<<"third if for field_in: "<<field_in.stype->getName().str()<<"\n";
                si_disjoint_sets[field_in] = merge_to;
                errs()<<"after field_in\n";
            }
            errs()<<"third if after for\n";
            merge_to->insert(siset->begin(), siset->end());
            errs()<<"merge_to insert\n";

        }
        errs()<<"test in merge_disjoint_sets\n";
    }
    errs()<<"for end\n";
    if (!merge_to) {
        errs()<<"forth if in merge_disjoint_sets\n";
        merge_to = new std::unordered_set<struct_field>;
    }
    return merge_to;*/

    std::unordered_set<struct_field> *merge_to = nullptr;
    for (struct_field field : fields) {
        auto siset = si_disjoint_sets[field];
        if (!siset) {
            siset = new std::unordered_set<struct_field>;
            siset->insert(field);
            si_disjoint_sets[field] = siset;
        }
        if (!merge_to) {
            merge_to = siset;
        } else if (siset != merge_to) {
            for (struct_field field_in : *siset) {
                si_disjoint_sets[field_in] = merge_to;
            }
            merge_to->insert(siset->begin(), siset->end());
            delete siset;
        }
    }
    if (!merge_to) {
        merge_to = new std::unordered_set<struct_field>;
    }
    return merge_to;

}


void uses_of_value(
    Value *value,
    std::unordered_set<struct_field> &fields,
    std::unordered_set<CallInst *> &calls,
    std::unordered_set<Value *> &cannot_handle,
    std::unordered_set<Value *> &visited
    )
{
    if (visited.find(value) != visited.end())
        return;
    visited.insert(value);

    for (User *v : value->users()) {
        if (auto store = dyn_cast<StoreInst>(v)) {
            Value *addr = store->getPointerOperand();
            auto store_fields = get_struct_fields_from_addr(addr, cannot_handle);
            fields.insert(store_fields.begin(), store_fields.end());
        } else if (auto call = dyn_cast<CallInst>(v)) {
            Value *called = call->getCalledOperand();
            if (called == value) {
                calls.insert(call);
            } else if (auto callee = dyn_cast<Function>(called)) {
                if (callee->isIntrinsic()) {
                    // ignore
                } else if (callee->getName() == "__cfi_slowpath") {
                    // ignore
                } else if (callee->getName() == "__cfi_slowpath_diag") {
                    // ignore
                } else {
                    cannot_handle.insert(v);
                }
            } else {
                cannot_handle.insert(v);
            }
        } else if (auto phi = dyn_cast<PHINode>(v)) {
            uses_of_value(phi, fields, calls, cannot_handle, visited);
        } else if (auto sel = dyn_cast<SelectInst>(v)) {
            uses_of_value(sel, fields, calls, cannot_handle, visited);
        } else if (auto bitcast = dyn_cast<BitCastInst>(v)) {
            uses_of_value(bitcast, fields, calls, cannot_handle, visited);
        } else if (auto bitcast = dyn_cast<BitCastOperator>(v)) {
            uses_of_value(bitcast, fields, calls, cannot_handle, visited);
        } else if (auto bop = dyn_cast<BinaryOperator>(v)) {
            uses_of_value(bop, fields, calls, cannot_handle, visited);
        } else if (auto pti = dyn_cast<PtrToIntInst>(v)) {
            uses_of_value(pti, fields, calls, cannot_handle, visited);
        } else if (auto itp = dyn_cast<IntToPtrInst>(v)) {
            uses_of_value(itp, fields, calls, cannot_handle, visited);
        } else if (auto zext = dyn_cast<ZExtInst>(v)) {
            uses_of_value(zext, fields, calls, cannot_handle, visited);
        } else if (auto sext = dyn_cast<SExtInst>(v)) {
            uses_of_value(sext, fields, calls, cannot_handle, visited);
        } else if (auto icmp = dyn_cast<ICmpInst>(v)) {
            Value *v2 = icmp->getOperand(0) == value ? icmp->getOperand(1) : icmp->getOperand(0);
            if (auto v2c = dyn_cast<Constant>(v2)) {
                if (!v2c->isNullValue()) {
                    cannot_handle.insert(v);
                }
            } else {
                cannot_handle.insert(v);
            }
        } else if (auto constant = dyn_cast<ConstantStruct>(v)) {
            // ignore
        } else {
            cannot_handle.insert(v);
        } 
    }
}


/*
 * store dyn KMI result into DMInterface so that we can use it later
 */
/*
void add_function_to_dmi(Function* f, StructType* t, Indices& idcs, DMInterface& dmi)
{
    IFPairs &ifps = dmi[t];
    FunctionSet fset = NULL;
    for (auto p: ifps)
    {
        if (indices_equal(idcs, p.first))
        {
            fset = p.second;
            break;
        }
    }
    if (fset==NULL)
    {
        fset = new FunctionSet;
        Indices idc;
        for (auto i: idcs)
            idc.push_back(i);
        IFPair ifp(idc,fset);
        ifps.push_back(ifp);
    }
    fset->insert(f);
}
*/


/*
 * is this type a function pointer type or
 * this is a composite type which have function pointer type element
 */
static bool _has_function_pointer_type(Type* type, TypeSet& visited)
{
    if (visited.count(type)!=0)
        return false;
    visited.insert(type);
strip_pointer:
    if (type->isPointerTy())
    {
        type = type->getPointerElementType();
        goto strip_pointer;
    }
    if (type->isFunctionTy())
        return true;

    //ignore array type?
    //if (!type->isAggregateType())
    if (!type->isStructTy())
        return false;
    //for each element in this aggregate type, find out whether the element
    //type is Function pointer type, need to track down more if element is
    //aggregate type
    for (unsigned i=0; i<type->getStructNumElements(); ++i)
    {
        Type* t = type->getStructElementType(i);
        if (t->isPointerTy())
        {
            if (_has_function_pointer_type(t, visited))
                return true;
        }else if (t->isStructTy())
        {
            if (_has_function_pointer_type(t, visited))
                return true;
        }
    }
    //other composite type
    return false;
}

bool has_function_pointer_type(Type* type)
{
    TypeSet visited;
    return _has_function_pointer_type(type, visited);
}


// if v is stored to more than ont struct type, return null
// else return the target struct type and index
bool isStore2DiffType(Value *v, struct_field &ret_si) {
    Indices idcs;
    StructType* ret;
    std::unordered_set<struct_field> sis;
    for (auto *u : v->users()) {
        if (StoreInst* si = dyn_cast<StoreInst>(u)) {
            ret = resolve_where_is_it_stored_to(si, idcs);
            if (ret) {
                Indices indices = idcs;
                sis.insert(struct_field(ret, indices));
                //ts.insert(dyn_cast<Type>(ret));
            }
        }
    }
    if (sis.size() == 1) {
        for (auto s:sis) {
            ret_si = s;
            return false;
        }
    }

    for (auto s:sis) {
        //delete s->second;

        if (!s.stype->isLiteral() && s.stype->hasName()) {
            StringRef sr = s.stype->getName();
            if(sr.startswith("struct.clock_data") || sr.startswith("struct.clock_read_data"))
                errs() << "!!!find struct types: " << sr << "\n";
        }
        siset_delete.insert(s);
    }
    return true;
}


StructType* find_assignment_directly(Value* v, Indices& idcs, ValueSet& visited)
{
    if (visited.count(v))
        return NULL;
    visited.insert(v);

    //dbglst.push_back(v);

    //FIXME: it is possible to assign to global variable!
    //       but currently we are not handling them
    //skip all global variables,
    //the address is statically assigned to global variable
    if (!stub_fatst_is_interesting_value(v))
    {
#if 0
        if (!stub_fatst_is_uninteresting_value(v))
        {
            errs()<<ANSI_COLOR_RED<<"XXX:"
                <<ANSI_COLOR_RESET<<"\n";
            dump_gdblst(dbglst);
        }
#endif
        //dbglst.pop_back();
        return NULL;
    }

    //* ! there should be a store instruction in the du-chain
    if (StoreInst* si = dyn_cast<StoreInst>(v))
    {
        StructType* ret = resolve_where_is_it_stored_to(si, idcs);
        return ret;
        //dbglst.pop_back();
        //add_function_to_dmi(f, st, idcs, dmi);
    }

    for (auto* u: v->users())
    {
        Value* tu = u;
        Type* t = u->getType();
        if (StructType* t_st = dyn_cast<StructType>(t))
            if ((t_st->hasName())
                && t_st->getStructName().startswith("struct.kernel_symbol"))
                    continue;
        /*
        //inter-procedural analysis
        //we are interested if it is used as a function parameter
        if (CallInst* ci = dyn_cast<CallInst>(tu))
        {
            //currently only deal with direct call...
            Function* cif = get_callee_function_direct(ci);
            if ((ci->getCalledValue()==v) || (cif==u))
            {
                //ignore calling myself..
                continue;
            } else if (cif==NULL)
            {
                //indirect call...
#if 0
                errs()<<"fptr used in indirect call";
                ci->print(errs());errs()<<"\n";
                errs()<<"arg v=";
                v->print(errs());errs()<<"\n";
#endif
                continue;
            } else if (!cif->isVarArg())
            {
                //try to figure out which argument is u corresponds to
                int argidx = -1;
                for (unsigned int ai = 0; ai<ci->getNumArgOperands(); ai++)
                {
                    if (ci->getArgOperand(ai)==v)
                    {
                        argidx = ai;
                        break;
                    }
                }
                //argidx should not ==-1
                if (argidx==-1)
                {
                    errs()<<"Calling "<<cif->getName()<<"\n";
                    ci->print(errs());errs()<<"\n";
                    errs()<<"arg v=";
                    v->print(errs());
                    errs()<<"\n";
                }
                //assert(argidx!=-1);
                //errs()<<"Into "<<cif->getName()<<"\n";
                //now are are in the callee function
                //figure out the argument
                auto targ = cif->arg_begin();
                for (int i=0;i<argidx;i++)
                    targ++;
                tu = targ;
            }else
            {
                //means that this is a vararg
                continue;
            }
        }*/
        //FIXME: visited?
        if (StructType* st = find_assignment_directly(tu, idcs, visited))
        {
            //dbglst.pop_back();
            return st;
            //add_function_to_dmi(f, st, idcs, dmi);
        }
    }
    //dbglst.pop_back();
    return NULL;
}

void find_global_variable_initializer(Function *f, Value* value, Indices indices_prefix,
                                      ValueSet& visited, DMInterface& dmi, Value *user_in = nullptr) {
    if (visited.count(value))
        return;
    visited.insert(value);

    for (auto u : value->users()) {
        Value *user = user_in ? user_in : u;
        if (auto c = dyn_cast<Constant>(u)) {
            if (isa<StructType>(c->getType())) {
                auto cs = dyn_cast<ConstantStruct>(c);
                assert(cs);
                Indices indices = indices_prefix;
                for (unsigned i = 0; i < cs->getNumOperands(); i++) {
                    if (cs->getOperand(i) == value) {
                        indices.push_back(i);
                        break;
                    }
                }
                auto st = dyn_cast<StructType>(c->getType());
                struct_field field(st, indices);
                dmi[field].insert(f);
                fu2si[f][user].insert(field);
                
                auto bitcasts = find_bitcasts(cs);
                std::unordered_set<GlobalValue *> globals;
                for (Value *u : cs->users()) {
                    if (auto global = dyn_cast<GlobalValue>(u)) {
                        globals.insert(global);
                    }
                }
                for (GlobalValue *global : globals) {
                    auto global_bitcasts = find_bitcasts(global);
                    bitcasts.insert(global_bitcasts.begin(), global_bitcasts.end());
                }
                for (Value *bitcasted : bitcasts) {
                    Type *t = bitcasted->getType();
                    if (auto ptrt = dyn_cast<PointerType>(t)) {
                        t = ptrt->getElementType();
                    }
                    
                    if (auto st = dyn_cast<StructType>(t)) {
                        if (st->isLiteral())
                            continue;
                        struct_field field(st, indices);
                        dmi[field].insert(f);
                        fu2si[f][user].insert(field);
                    }
                }
            } else if (isa<PointerType>(c->getType())) {
                find_global_variable_initializer(f, c, indices_prefix, visited, dmi, user);
            } else if (isa<ArrayType>(c->getType())) {
                Indices indices = indices_prefix;
                indices.push_back(0);
                find_global_variable_initializer(f, c, indices, visited, dmi, user);
                // errs() << "function " << f->getName() << " stored in array" << c->getName() << "\n";
            } else if (isa<IntegerType>(c->getType())) {
                find_global_variable_initializer(f, c, indices_prefix, visited, dmi, user);
            } else {
                errs() << "???: " << *c->getType() << "\n";
            }
        } else if (auto b = dyn_cast<BitCastOperator>(u)) {
            find_global_variable_initializer(f, b, indices_prefix, visited, dmi, user);
        } else if (isa<Instruction>(u)) {
            // ignore
        } else {
            errs() << *u->getType() << ": " << *u << "\n";
        }
    }   
}

// backward: trace the defuse chain of functions to find where it store to
// return a set of struct type and indices 
void find_assignment_to_struct_type(Function* f, 
                                    ValueSet& visited, DMInterface& dmi) {
    if (visited.count(f))
        return;
    visited.insert(f);
    
    for (auto *u: f->users()) {
        if (CallInst* ci = dyn_cast<CallInst>(u)) {
            Function *cif = get_callee_function_direct(ci);
            if((ci->getCalledOperand()==f) || !cif || (cif==u))
                continue;
            if(!cif->isVarArg()) {
                int argidx = -1;
                for (unsigned int ai = 0; ai<ci->getNumArgOperands(); ai++) {
                    if (ci->getArgOperand(ai) == dyn_cast<Value>(f)) {
                        argidx = ai;
                        break;
                    }
                }

                if (argidx==-1)
                {
                    //errs()<<"Calling "<<cif->getName()<<"\n";
                    //ci->print(errs());errs()<<"\n";
                    //errs()<<"arg v=";
                    //v->print(errs());
                    //errs()<<"\n";
                }
                auto targ = cif->arg_begin();
                for (int i=0;i<argidx;i++)
                    targ++;
                struct_field si;
                if (!isStore2DiffType(dyn_cast<Value>(targ), si)) {
                    // add to dmi and si2func
                    // siset.insert(si);
                    dmi[si].insert(f);
                }
            }
        } else {
            Indices idcs;
            StructType* st = find_assignment_directly(u, idcs, visited);
            if (st) {
                dmi[struct_field(st, idcs)].insert(f);
            }
        }
    }  
}




void str_truncate_dot_number(std::string& str)
{
    if (!isdigit(str.back()))
        return;
    std::size_t found = str.find_last_of('.');
    str = str.substr(0,found);
}


/*
 * return global value if this is loaded from global value, otherwise return NULL
 */
GlobalValue* get_loaded_from_gv(Value* v)
{
    GlobalValue* ret = NULL;
    IntToPtrInst* i2ptr = dyn_cast<IntToPtrInst>(v);
    LoadInst* li;
    Value* addr;
    if (!i2ptr)
        goto end;
    //next I am expectnig a load instruction
    li = dyn_cast<LoadInst>(i2ptr->getOperand(0));
    if (!li)
        goto end;
    addr = li->getPointerOperand()->stripPointerCasts();
    //could be a constant expr of gep?
    if (ConstantExpr* cxpr = dyn_cast<ConstantExpr>(addr))
    {
        GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(cxpr->getAsInstruction());
        tmp_insts.push_back(gep);
        if (Value* tpobj = gep->getPointerOperand())
            ret = dyn_cast<GlobalValue>(tpobj);
    }
end:
    return ret;
}

/*
 * is this a load+bitcast of struct into fptr type?
 * could be multiple load + bitcast 
 */
StructType* identify_ld_bcst_struct(Value* v)
{
#if 0
    LoadInst* li = dyn_cast<LoadInst>(v);
    if (!li)
        return NULL;
    Value* addr = li->getPointerOperand();
    if (BitCastInst* bci = dyn_cast<BitCastInst>(addr))
        addr = bci->getOperand(0);
    else
        return NULL;
    //should be pointer type
    if (PointerType* pt = dyn_cast<PointerType>(addr->getType()))
    {
        Type* et = pt->getElementType();
        if (StructType *st = dyn_cast<StructType>(et))
        {
            //resolved!, they are trying to load the first function pointer
            //from a struct type we already know!
            return st;
        }
    }
    return NULL;
#else
    int num_load = 0;
    Value* nxtv = v;
    while(1)
    {
        if (LoadInst* li = dyn_cast<LoadInst>(nxtv))
        {
            nxtv = li->getPointerOperand();
            num_load++;
            continue;
        }
        if (IntToPtrInst* itoptr = dyn_cast<IntToPtrInst>(nxtv))
        {
            nxtv = itoptr->getOperand(0);
            continue;
        }
        break;
    }
    if (num_load==0)
        return NULL;
    if (BitCastInst* bci = dyn_cast<BitCastInst>(nxtv))
    {
        nxtv = bci->getOperand(0);
    }else
        return NULL;
    //num_load = number of * in nxtv
    Type* ret = nxtv->getType();
    while(num_load)
    {
        //I am expecting a pointer type
        PointerType* pt = dyn_cast<PointerType>(ret);
        if (!pt)
        {
            errs()<<"I am expecting a pointer type! got:";
            ret->print(errs());
            errs()<<"\n";
            return NULL;
        }
        //assert(pt);
        ret = pt->getElementType();
        num_load--;
    }
    auto st = dyn_cast<StructType>(ret);
    
    return st;
#endif
}



/*
 * match a type/indices with known ones
 */
static Value* _get_value_from_composit(Value* cv, Indices& indices)
{
    //cv must be global value
    GlobalVariable* gi = dyn_cast<GlobalVariable>(cv);
    Constant* initializer = dyn_cast<Constant>(cv);
    Value* ret = NULL;
    Value* v;
    int i;
    dbglst.push_back(cv);

    if (!indices.size())
        goto end;

    i = indices.front();
    indices.pop_front();

    if (gi)
        initializer = gi->getInitializer();
    assert(initializer && "must have a initializer!");
    /*
     * no initializer? the member of struct in question does not have a
     * concreat assignment, we can return now.
     */
    if (initializer==NULL)
        goto end;
    if (initializer->isZeroValue())
        goto end;
    v = initializer->getAggregateElement(i);
    assert(v!=cv);
    if (v==NULL)
        goto end;//means that this field is not initialized

    v = v->stripPointerCasts();
    assert(v);
    if (isa<Function>(v))
    {
        ret = v;
        goto end;
    }
    if (indices.size())
        ret = _get_value_from_composit(v, indices);
end:
    dbglst.pop_back();
    return ret;
}

Value* get_value_from_composit(Value* cv, Indices& indices)
{
    Indices i = Indices(indices);
    return _get_value_from_composit(cv, i);
}



InstructionSet get_load_from_gep(Value* v)
{
    InstructionSet lots_of_geps;
    //handle non load instructions first
    //might be gep/phi/select/bitcast
    //collect all load instruction into loads
    InstructionSet loads;
    ValueSet visited;
    ValueList worklist;

    //first, find all interesting load
    worklist.push_back(v);
    while(worklist.size())
    {
        Value* i = worklist.front();
        worklist.pop_front();
        if (visited.count(i))
            continue;
        visited.insert(i);
        assert(i!=NULL);
        if (LoadInst* li = dyn_cast<LoadInst>(i))
        {
            loads.insert(li);
            continue;
        }
        if (BitCastInst * bci = dyn_cast<BitCastInst>(i))
        {
            worklist.push_back(bci->getOperand(0));
            continue;
        }
        if (PHINode* phi = dyn_cast<PHINode>(i))
        {
            for (int k=0; k<(int)phi->getNumIncomingValues(); k++)
                worklist.push_back(phi->getIncomingValue(k));
            continue;
        }
        if (SelectInst* sli = dyn_cast<SelectInst>(i))
        {
            worklist.push_back(sli->getTrueValue());
            worklist.push_back(sli->getFalseValue());
            continue;
        }
        if (IntToPtrInst* itptr = dyn_cast<IntToPtrInst>(i))
        {
            worklist.push_back(itptr->getOperand(0));
            continue;
        }
        if (PtrToIntInst* ptint = dyn_cast<PtrToIntInst>(i))
        {
            worklist.push_back(ptint->getOperand(0));
            continue;
        }
        //binary operand for pointer manupulation
        if (BinaryOperator *bop = dyn_cast<BinaryOperator>(i))
        {
            for (int i=0;i<(int)bop->getNumOperands();i++)
                worklist.push_back(bop->getOperand(i));
            continue;
        }
        if (ZExtInst* izext = dyn_cast<ZExtInst>(i))
        {
            worklist.push_back(izext->getOperand(0));
            continue;
        }
        if (SExtInst* isext = dyn_cast<SExtInst>(i))
        {
            worklist.push_back(isext->getOperand(0));
            continue;
        }
        if (isa<GlobalValue>(i) || isa<ConstantExpr>(i) ||
            isa<GetElementPtrInst>(i) || isa<CallInst>(i)|| isa<CmpInst>(i))
            continue;
        if (!isa<Instruction>(i))
            continue;

	errs() << "is here?";
        i->print(errs());
        errs()<<"\n";
        //llvm_unreachable("no possible");
    }
    //////////////////////////
    //For each load instruction's pointer operand, we want to know whether
    //it is derived from gep or not..
    for (auto* lv: loads)
    {
        LoadInst* li = dyn_cast<LoadInst>(lv);
        Value* addr = li->getPointerOperand();

        //track def-use chain
        worklist.push_back(addr);
        visited.clear();
        while (worklist.size())
        {
            Value* i = worklist.front();
            worklist.pop_front();
            if (visited.count(i))
                continue;
            visited.insert(i);
            if (GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(i))
            {
                lots_of_geps.insert(gep);
                continue;
            }
            if (BitCastInst * bci = dyn_cast<BitCastInst>(i))
            {
                worklist.push_back(bci->getOperand(0));
                continue;
            }
            if (PHINode* phi = dyn_cast<PHINode>(i))
            {
                for (int k=0; k<(int)phi->getNumIncomingValues(); k++)
                    worklist.push_back(phi->getIncomingValue(k));
                continue;
            }
            if (SelectInst* sli = dyn_cast<SelectInst>(i))
            {
                worklist.push_back(sli->getTrueValue());
                worklist.push_back(sli->getFalseValue());
                continue;
            }
            if (IntToPtrInst* itptr = dyn_cast<IntToPtrInst>(i))
            {
                worklist.push_back(itptr->getOperand(0));
                continue;
            }
            if (PtrToIntInst* ptint = dyn_cast<PtrToIntInst>(i))
            {
                worklist.push_back(ptint->getOperand(0));
                continue;
            }
            //binary operand for pointer manupulation
            if (BinaryOperator *bop = dyn_cast<BinaryOperator>(i))
            {
                for (int i=0;i<(int)bop->getNumOperands();i++)
                    worklist.push_back(bop->getOperand(i));
                continue;
            }
            if (ZExtInst* izext = dyn_cast<ZExtInst>(i))
            {
                worklist.push_back(izext->getOperand(0));
                continue;
            }
            if (SExtInst* isext = dyn_cast<SExtInst>(i))
            {
                worklist.push_back(isext->getOperand(0));
                continue;
            }
            //gep in constantexpr?
            if (ConstantExpr* cxpr = dyn_cast<ConstantExpr>(i))
            {
                Instruction *inst = cxpr->getAsInstruction();
                tmp_insts.push_back(inst);
                worklist.push_back(inst);
                
                continue;
            }

            if (isa<GlobalValue>(i) || isa<LoadInst>(i) ||
                isa<AllocaInst>(i) || isa<CallInst>(i))
                continue;
            if (!isa<Instruction>(i))
                continue;
            //what else?
            i->print(errs());
            errs()<<"\n";
        }
    }
    return lots_of_geps;
}


/*
 * trace point function as callee?
 * similar to load+gep, we can not know callee statically, because it is not defined
 * trace point is a special case where the indirect callee is defined at runtime,
 * we simply mark it as resolved since we can find where the callee fptr is loaded
 * from
 */
bool is_tracepoint_func(Value* v)
{
    if (StructType* st = identify_ld_bcst_struct(v))
    {
#if 0
        errs()<<"Found:";
        if (st->isLiteral())
            errs()<<"Literal\n";
        else
            errs()<<st->getStructName()<<"\n";
#endif
        //no name ...
        if (!st->hasName())
            return false;
        StringRef name = st->getStructName();
        if (name=="struct.tracepoint_func")
        {
            //errs()<<" ^ a tpfunc:";
            //addr->print(errs());
            LoadInst* li = dyn_cast<LoadInst>(v);
            Value* addr = li->getPointerOperand()->stripPointerCasts();

            //addr should be a phi
            PHINode * phi = dyn_cast<PHINode>(addr);
            assert(phi);
            //one of the incomming value should be a load
            for (unsigned int i=0;i<phi->getNumIncomingValues();i++)
            {
                Value* iv = phi->getIncomingValue(i);
                //should be a load from a global defined object
                if (GlobalValue* gv = get_loaded_from_gv(iv))
                {
                    //gv->print(errs());
                    //errs()<<(gv->getName());
                    break;
                }
            }
            //errs()<<"\n";
            return true;
        }
        return false;
    }
    //something else?
    return false;
}

/*
 * FIXME: we are currently not able to handle container_of, which is expanded
 * into gep with negative index and high level type information is stripped
 * maybe we can define a function to repalce container_of... so that high level
 * type information won't be stripped during compilation
 */
bool is_container_of(Value* cv)
{
    InstructionSet geps = get_load_from_gep(cv);
    for (auto _gep: geps)
    {
        GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(_gep);
        //container_of has gep with negative index
        //and must have negative or non-zero index in the first element
        auto i = gep->idx_begin();
        ConstantInt* idc = dyn_cast<ConstantInt>(i);
        if (idc && (idc->getSExtValue()!=0))
        {
#if 0
            Type* pty = gep->getSourceElementType();
            if(StructType* sty = dyn_cast<StructType>(pty))
            {
                if (!sty->isLiteral())
                    errs()<<sty->getStructName()<<" ";
            }
#endif
            return true;
        }
    }
    return false;
}





void getStypeInd_dkmi(StructType* stype, const Indices &ind, 
    CallInst* ci, DMInterface& dmi) {
    //errs() << "dkmi\n";
    
    if (!stype->hasName())
        return;

    //errs() << "dkmi indices size: " << indices.size()<< "\n";
    //for(auto i:indices)
    //    errs()<< i <<"\n";
    Indices indices(ind);
    struct_field si(stype, indices);
    // siset.insert(si);
    si2call[si].insert(ci);

    FunctionSet &fs = si2func[si];

    std::string stname = stype->getStructName().str();
    str_truncate_dot_number(stname);

    si2func[si].insert(dmi[si].begin(), dmi[si].end());
    /*
    for (auto& ifpsp: dmi)
    {
        StructType* cst = ifpsp.first;
        if (cst->isLiteral())
            continue;
        std::string cstn = cst->getStructName().str();
        str_truncate_dot_number(cstn);
        if (cstn == stname)
        {
            //errs() << "dkmi: " <<stname <<"\n";
            ifpairs = ifpsp.second;
            for (auto p: ifpairs)
                if (indices_equal(ind, p.first)) {
                    for(auto f: *p.second)
                        fs->insert(f);
                }
        }
    }
    */
}


void GatlinModule::dump_kmi()
{
    //if (!knob_gatlin_kmi)
    //    return;
    errs()<<"=Kernel Module Interfaces="<<"\n";
    for (auto msi: mi2m)
    {
        StructType * stype = dyn_cast<StructType>(msi.first);
        if (stype->hasName())
            errs()<<stype->getName()<<"\n";
        else
            errs()<<"AnnonymouseType"<<"\n";
        for (auto m: (*msi.second))
        {
            if (m->hasName())
                errs()<<"    "<<m->getName()<<"\n";
            else
                errs()<<"    "<<"Annoymous"<<"\n";
        }
    }
    errs()<<"=o=\n";
}



////////////////////////////////////////////////////////////////////////////////
/*
 * resolve indirect callee
 * method 1 suffers from accuracy issue
 * method 2 is too slow
 * method 3 use the fact that most indirect call use function pointer loaded
 *          from struct(mi2m, kernel interface)
 */
FunctionSet GatlinModule::resolve_indirect_callee_ldcst_kmi(CallInst* ci, int&err,
        int& kmi_cnt, int& dkmi_cnt)
{
    FunctionSet fs;
    //non-gep case. loading from bitcasted struct address
    if (StructType* ldbcstty = identify_ld_bcst_struct(ci->getCalledOperand()))
    {
#if 0
        errs()<<"Found ld+bitcast sty to ptrty:";
        if (ldbcstty->isLiteral())
            errs()<<"Literal, ";
        else
            errs()<<ldbcstty->getName()<<", ";
#endif
        //dump_kmi_info(ci);
        Indices indices;
        indices.push_back(0);
        err = 2;//got type
        /*
        //match - kmi
        ModuleSet ms;
        find_in_mi2m(ldbcstty, ms);
        if (ms.size())
        {
            err = 1;//found module object
            for (auto m: ms)
                if (Value* v = get_value_from_composit(m, indices))
                {
                    Function *f = dyn_cast<Function>(v);
                    assert(f);
                    getStypeInd_kmi(ldbcstty, indices, f, ci);
                    fs.insert(f);
                }
        }
        if (fs.size()!=0)
        {
            kmi_cnt++;
            goto end;
        }
        */
        //match - dkmi
        /*
        if (dmi_type_exists(ldbcstty, dmi))
            err = 1;
        */

        indices.clear();
        indices.push_back(0);
        FunctionSet &_fs = dmi[struct_field(ldbcstty, indices)];
        if (!_fs.empty())
        {
            for (auto f : _fs)
                fs.insert(f);
            // getStypeInd_dkmi(ldbcstty, indices, ci, dmi);
            dkmi_cnt++;
            goto end;
        }
#if 0
        errs()<<"Try rkmi\n";
#endif
    }
end:
    if (fs.size())
        err = 0;
    return fs;
}

#if 0
//method 3, improved accuracy
FunctionSet GatlinModule::resolve_indirect_callee_using_kmi(CallInst* ci, int& err)
{
    FunctionSet fs;
    Value* cv = ci->getCalledValue();

    err = 6;
    //GEP case.
    //need to find till gep is exhausted and mi2m doesn't have a match
    InstructionSet geps = get_load_from_gep(cv);
    for(auto _gep: geps)
    {
        GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(_gep);
        Indices indices;
        x_dbg_ins = gep;
        Type* cvt = get_gep_indices_type(gep, indices);
        if (!cvt || !cvt->isAggregateType())
            continue;

        x_dbg_idx = indices;
        assert(indices.size()!=0);
        Indices idcs = indices;
        while(1)
        {
            if (err>2)
                err = 2;//found the type, going to match module
            /*
            ModuleSet ms;
            find_in_mi2m(cvt, ms);
            if (ms.size())
            {
                if (err>1)
                    err = 1;//found matching module
                for (auto m: ms)
                {
                    Value* v = get_value_from_composit(m, indices);
                    if (v==NULL)
                    {
                        /*
                         * NOTE: some of the method may not be implemented
                         *       it is ok to ignore them
                         * for example: .release method in
                         *      struct tcp_congestion_ops
                         * /
#if 0
                        errs()<<m->getName();
                        errs()<<" - can not get value from composit [ ";
                        for (auto i: indices)
                            errs()<<","<<i;
                        errs()<<"], this method may not implemented yet.\n";
#endif
                        continue;
                    }
                    Function *f = dyn_cast<Function>(v);
                    assert(f);
                    getStypeInd_kmi(dyn_cast<StructType>(cvt), idcs, f, ci);
                    fs.insert(f);
                }
                break;
            }
            */
            //not found in mi2m
            if (indices.size()<=1)
            {
                //no match! we are also done here, mark it as resolved anyway
                //this object may be dynamically allocated,
                //try dkmi if possible
#if 0
                errs()<<" MIDC err, try DKMI\n";
                cvt = get_load_from_type(cv);
                errs()<<"!!!  : ";
                cvt->print(errs());
                errs()<<"\n";
                
                errs()<<"idcs:";
                for (auto i: x_dbg_idx)
                    errs()<<","<<i;
                errs()<<"\n";
                //gep->print(errs());
                errs()<<"\n";
#endif
                break;
            }
            //no match, we can try inner element
            //deal with array of struct here
            Type* ncvt;
            if (ArrayType *aty = dyn_cast<ArrayType>(cvt))
            {
                ncvt = aty->getElementType();
                //need to remove another one index
                indices.pop_front();
            }else
            {
                int idc = indices.front();
                indices.pop_front();
                if (!cvt->isStructTy())
                {
                    cvt->print(errs());
                }
                ncvt = cvt->getStructElementType(idc);
                //FIXME! is this correct?
                if (PointerType* pty = dyn_cast<PointerType>(ncvt))
                {
                    ncvt = pty->getElementType();
                }

                //cvt should be aggregated type!
                if (!ncvt->isAggregateType())
                {
                    /* bad cast!!!
                     * struct sk_buff { cb[48] }
                     * XFRM_TRANS_SKB_CB(__skb) ((struct xfrm_trans_cb *)&((__skb)->cb[0]))
                     */
                    //errs()<<"Can not resolve\n";
                    //x_dbg_ins->getDebugLoc().print(errs());
                    //errs()<<"\n";
                    errs()<<"Bad cast from type:";
                    ncvt->print(errs());
                    errs()<<" we can not resolve this\n";
                    //dump_kmi_info(ci);
                    //llvm_unreachable("NOT POSSIBLE!");
                    err = 5;
                    break;
                }
            }
            cvt = ncvt;
        }
    }
    if (fs.size()==0)
    {
        if (!isa<Instruction>(cv))
            err = 3;
        else if (load_from_global_fptr(cv))
            err = 4;
    }else
        err = 0;
    return fs;
}
#endif

/*
 * this is also kmi, but dynamic one
 */
FunctionSet GatlinModule::resolve_indirect_callee_using_dkmi(CallInst* ci, int& err)
{
    FunctionSet fs;
    Value* cv = ci->getCalledOperand();
    InstructionSet geps = get_load_from_gep(cv);

    err = 6;
    for (auto * _gep: geps)
    {
        GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(_gep);
        Indices indices;
        //need to find till gep is exhausted and mi2m doesn't have a match
        x_dbg_ins = gep;
        StructType* cvt = get_gep_indices_type(gep, indices);
        /*
        if (!cvt || !cvt->isAggregateType())
            continue;

        x_dbg_idx = indices;
        assert(indices.size()!=0);
        //dig till we are at struct type
        while (1)
        {
            if (isa<StructType>(cvt))
                break;
            //must be an array
            if (ArrayType *aty = dyn_cast<ArrayType>(cvt))
            {
                cvt = aty->getElementType();
                //need to remove another one index
                indices.pop_front();
            }else
            {
                //no struct inside it and all of them are array?
#if 0
                errs()<<"All array?:";
                cvt->print(errs());
                errs()<<"\n";
#endif
                break;
            }
        }
        */
        if (!cvt)
            continue;
        if (err>2)
            err = 2;
        /* TODO
        if (dmi_type_exists(dyn_cast<StructType>(cvt), dmi) && (err>1))
            err = 1;
        */
        //OK. now we match through struct type and indices
        FunctionSet &_fs = dmi[struct_field(cvt, indices)];
        if (!_fs.empty())
        {
            /* TODO
            //TODO:iteratively explore basic element type if current one is not found
            if (_fs->size()==0)
            {
                //dump_kmi_info(ci);
                errs()<<"uk-idcs:";
                if(!dyn_cast<StructType>(cvt)->isLiteral())
                    errs()<<cvt->getStructName();
                errs()<<" [";
                for (auto i: x_dbg_idx)
                    errs()<<","<<i;
                errs()<<"]\n";
            }
            */
            // getStypeInd_dkmi(cvt, indices, ci, dmi);
            //merge _fs into fs
            for (auto f:_fs)
                fs.insert(f);
        }
    }
    if (fs.size())
        err = 0;
    return fs;
}

bool GatlinModule::load_from_global_fptr(Value* cv)
{
    ValueList worklist;
    ValueSet visited;
    worklist.push_back(cv);
    int cnt = 0;
    while(worklist.size() && (cnt++<5))
    {
        Value* v = worklist.front();
        worklist.pop_front();
        if (visited.count(v))
            continue;
        visited.insert(v);

        if (isa<GlobalVariable>(v))
            return true;

        if (isa<Function>(v) || isa<GetElementPtrInst>(v) || isa<CallInst>(v))
            continue;

        if (LoadInst* li = dyn_cast<LoadInst>(v))
        {
            worklist.push_back(li->getPointerOperand());
            continue;
        }
        if (SelectInst* sli = dyn_cast<SelectInst>(v))
        {
            worklist.push_back(sli->getTrueValue());
            worklist.push_back(sli->getFalseValue());
            continue;
        }
        if (PHINode* phi = dyn_cast<PHINode>(v))
        {
            for (unsigned int i=0;i<phi->getNumIncomingValues();i++)
                worklist.push_back(phi->getIncomingValue(i));
            continue;
        }
        //instruction
        if (Instruction* i = dyn_cast<Instruction>(v))
            for (unsigned int j = 0;j<i->getNumOperands();j++)
                worklist.push_back(i->getOperand(j));
        //constant value
        if (ConstantExpr* cxpr = dyn_cast<ConstantExpr>(v))
        {
            Instruction *inst = cxpr->getAsInstruction();
            tmp_insts.push_back(inst);
            worklist.push_back(inst);
        }
    }
    return false;
}

void GatlinModule::dump_kmi_info(CallInst* ci)
{
    Value* cv = ci->getCalledOperand();
    ValueList worklist;
    ValueSet visited;
    worklist.push_back(cv);
    int cnt = 0;
    ci->print(errs());
    errs()<<"\n";
    while(worklist.size() && (cnt++<5))
    {
        Value* v = worklist.front();
        worklist.pop_front();
        if (visited.count(v))
            continue;
        visited.insert(v);
        if (isa<Function>(v))
            errs()<<v->getName();
        else
            v->print(errs());
        errs()<<"\n";
        if (LoadInst* li = dyn_cast<LoadInst>(v))
        {
            worklist.push_back(li->getPointerOperand());
            continue;
        }
        if (SelectInst* sli = dyn_cast<SelectInst>(v))
        {
            worklist.push_back(sli->getTrueValue());
            worklist.push_back(sli->getFalseValue());
            continue;
        }
        if (PHINode* phi = dyn_cast<PHINode>(v))
        {
            for (unsigned int i=0;i<phi->getNumIncomingValues();i++)
                worklist.push_back(phi->getIncomingValue(i));
            continue;
        }
        if (isa<CallInst>(v))
            continue;
        if (Instruction* i = dyn_cast<Instruction>(v))
            for (unsigned int j = 0;j<i->getNumOperands();j++)
                worklist.push_back(i->getOperand(j));
    }
}

/*
 * create mapping for
 *  indirect call site -> callee
 *  callee -> indirect call site
 */
void GatlinModule::populate_indcall_list_through_kmi()
{
    //indirect call is load+gep and can be found in mi2m?
    int count = 0;
    int ldbcst_cnt = 0;
    int targets = 0;
    int fpar_cnt = 0;
    int gptr_cnt = 0;
    int cast_cnt = 0;
    int container_of_cnt = 0;;
    int undefined_1 = 0;
    int undefined_2 = 0;
    int unknown = 0;
    int kmi_cnt = 0;
    int dkmi_cnt = 0;

    int tracepoint_num=0;
    bool found_b = false, udf_b = false;

    FunctionSet fs_final;
#if 0
    errs()<<ANSI_COLOR(BG_WHITE,FG_GREEN)
        <<"indirect callsite, match"
        <<ANSI_COLOR_RESET<<"\n";
#endif
    for (auto* idc: idcs)
    {
        fs_final.clear();
#if 0
        errs()<<ANSI_COLOR_YELLOW<<" * ";
        idc->getDebugLoc().print(errs());
        errs()<<ANSI_COLOR_RESET<<"";
#endif
        //is this a trace point?
        //special condition, ignore tracepoint, we are not interested in them.
        if (is_tracepoint_func(idc->getCalledOperand()))
        {
            count++;
            targets++;
            kmi_cnt++;
            tracepoint_num++;
            //errs()<<"tracepoint: "<<*idc<<"\n";
#if 0
            errs()<<" [tracepoint]\n";
#endif
            continue;
        }
        if (is_container_of(idc->getCalledOperand()))
        {
            container_of_cnt++;
#if 0
            errs()<<" [container_of]\n";
#endif
            continue;
        }

        //try kmi
        //err - 0 no error
        //    - 1 undefined fptr in module, mark as resolved
        //    - 2 undefined module, mark as resolved(ok to fail)
        //    - 3 fptr comes from function parameter
        //    - 4 fptr comes from global fptr
        //    - 5 bad cast
        //    - 6 max error code- this is the bound 
        int err = 6;
        //we resolved type and there's a matching object, but no fptr defined
        bool found_module = false;
        //we resolved type but there's no matching object
        bool udf_module = false;
        FunctionSet fs;
        /*
        fs = resolve_indirect_callee_using_kmi(idc, err);
        if (err<2)
            found_module = true;
        else if (err==2)
            udf_module = true;

        if (fs.size()!=0)
        {
#if 0
            errs()<<" [KMI]\n";
#endif
            kmi_cnt++;
            for (auto f:fs) {
                fs_final.insert(f);
            }
            //goto resolved;
        }
        //using a fptr not implemented yet
        switch(err)
        {
            case(6):
            case(0):
            //{
            //    goto unresolvable;
            //}
            case(1):
            case(2)://try dkmi
            {
                //try dkmi
                break;
            }
            case(3):
            {
                //function parameter, unable to be solved by kmi and dkmi, try SVF
                fpar_cnt++;
                goto unresolvable;
            }
            case(4):
            {
                gptr_cnt++;
                goto unresolvable;
            }
            case(5):
            {
                cast_cnt++;
                goto unresolvable;
            }
            default:{}
        }
        */
        //try dkmi
        fs = resolve_indirect_callee_using_dkmi(idc, err);
        if (err<2)
            found_module = true;
        else if (err==2)
            udf_module = true;

        if (fs.size()!=0)
        {
#if 0
            errs()<<" [DKMI]\n";
#endif
            dkmi_cnt++;
            for (auto f:fs) {
                fs_final.insert(f);
            }
            //goto resolved;
        }


        /*
        fs = resolve_indirect_callee_ldcst_kmi(idc, err, kmi_cnt, dkmi_cnt);
        if (err<2)
            found_module = true;
        else if (err==2)
            udf_module = true;

        if (fs.size()!=0)
        {
#if 0
            errs()<<" [LDCST-KMI]\n";
#endif
            if (!fs_final.size()) {
                errs() << *idc->getFunction() << "\n";
                errs() << *idc << "\n";
                llvm_unreachable("...");
            }
            
            for (auto f:fs) {
                fs_final.insert(f);
            }
            //goto resolved;
            ldbcst_cnt++;
        }
        */

        if (fs_final.size() != 0)
            goto resolved;
        
        if (found_module)
        {
#if 0
                errs()<<" [UNDEFINED1-found-m]\n";
#endif
		count++;
                targets++;
                undefined_1++;
		if(found_b)
			continue;
		errs()<<" [UNDEFINED1-found-m]\n";
                dump_kmi_info(idc);
		found_b = true;
                continue;
        }
        if (udf_module)
        {
#if 0
            errs()<<" [UNDEFINED2-udf-m]\n";
#endif
	    count++;
            targets++;
            undefined_2++;
	    if(udf_b)
		    continue;
	    errs()<<" [UNDEFINED2-udf-m]\n";
            dump_kmi_info(idc);
	    udf_b = true;
            continue;
        }
unresolvable:
        //can not resolve
        fuidcs.insert(idc->getFunction());
        switch(err)
        {
            case (3):
            {
                //function parameter
#if 0
                errs()<<" [UPARA]\n";
#endif
                break;
            }
            case (4):
            {
                //global fptr
#if 0
                errs()<<" [GFPTR]\n";
#endif
                break;
            }
            case (5):
            {
#if 0
                errs()<<" [BAD CAST]\n";
#endif
                break;
            }
            default:
            {
#if 0
                errs()<<" [UNKNOWN]\n";
#endif
                unknown++;
                //dump the struct
                //dump_kmi_info(idc);
            }
        }
        continue;
resolved:
        count++;
        targets += fs.size();
        FunctionSet &funcs = idcs2callee[idc];
        for (auto f:fs)
        {
#if 0
            errs()<<"     - "<<f->getName()<<"\n";
#endif
            funcs.insert(f);
            InstructionSet* csis = f2csi_type1[f];
            if (csis==NULL)
            {
                csis = new InstructionSet;
                f2csi_type1[f] = csis;
            }
            csis->insert(idc);
        }
    }
    errs()<<"------ KMI STATISTICS ------\n";
    
    errs()<<"# tracepoint number: " << tracepoint_num<<"\n";

    errs()<<"# of indirect call sites: "<< idcs.size()<<"\n";
    errs()<<"# resolved by KMI:"<< count<<"\n";
    errs()<<"#     - KMI:"<< kmi_cnt<<"\n";
    errs()<<"#     - DKMI:"<< dkmi_cnt<<"\n";
    errs()<<"# resolved by ldbcst:"<< ldbcst_cnt <<"\n";
    errs()<<"# (total target) of callee:"<<targets<<"\n";
    errs()<<"# undefined-found-m : "<<undefined_1<<"\n";
    errs()<<"# undefined-udf-m : "<<undefined_2<<"\n";
    errs()<<"# fpara(KMI can not handle, try SVF?): "
                <<fpar_cnt
                <<"\n";
    errs()<<"# global fptr(try SVF?): "
                <<gptr_cnt
                <<"\n";
    errs()<<"# cast fptr(try SVF?): "
                <<cast_cnt
                <<"\n";
    errs()<<"# call use container_of(), high level type info stripped: "
                <<container_of_cnt
                <<"\n";
    errs()<<"# unknown pattern:"
                <<unknown
                <<"\n";
    //exit(0);*/
}


/*
 * method 2: cvf: Complex Value Flow Analysis
 * figure out candidate for indirect callee using value flow analysis
 */
/*void Gatlin::populate_indcall_list_using_cvf(Module& module)
{
    //create svf instance
    CVFA cvfa;*/

    /*
     * NOTE: shrink our analyse scope so that we can run faster
     * remove all functions which don't have function pointer use and
     * function pointer propagation, because we only interested in getting 
     * indirect callee here, this will help us make cvf run faster
     */
/*    FunctionSet keep;
    FunctionSet remove;
    //add skip functions to remove
    //add kernel_api to remove
    for (auto f: *skip_funcs)
        remove.insert(module.getFunction(f));
    for (auto f: *kernel_api)
        remove.insert(module.getFunction(f));
    for (auto f: trace_event_funcs)
        remove.insert(f);
    for (auto f: bpf_funcs)
        remove.insert(f);
    for (auto f: irq_funcs)
        remove.insert(f);

    FunctionList new_add;
    //for (auto f: all_functions)
    //    if (is_using_function_ptr(f) || is_address_taken(f))
    //        keep.insert(f);
    for (auto f: fuidcs)
        keep.insert(f);

    for (auto f: syscall_list)
        keep.insert(f);

    ModuleDuplicator md(module, keep, remove);
    Module& sm = md.getResult();

    //CVF: Initialize, this will take some time
    cvfa.initialize(sm);

    //do analysis(idcs=sink)
    //find out all possible value of indirect callee
    errs()<<ANSI_COLOR(BG_WHITE, FG_BLUE)
        <<"SVF indirect call track:"
        <<ANSI_COLOR_RESET<<"\n";
    for (auto f: all_functions)
    {
        ConstInstructionSet css;
        Function* df = dyn_cast<Function>(md.map_to_duplicated(f));
        cvfa.get_callee_function_indirect(df, css);
        if (css.size()==0)
            continue;
        errs()<<ANSI_COLOR(BG_CYAN, FG_WHITE)
            <<"FUNC:"<<f->getName()
            <<", found "<<css.size()
            <<ANSI_COLOR_RESET<<"\n";
        for (auto* _ci: css)
        {
            //indirect call sites->function
            const CallInst* ci = dyn_cast<CallInst>(md.map_to_origin(_ci));
            assert(ci!=NULL);
            FunctionSet* funcs = idcs2callee[ci];
            if (funcs==NULL)
            {
                funcs = new FunctionSet;
                idcs2callee[ci] = funcs;
            }
            funcs->insert(f);
            //func->indirect callsites
            InstructionSet* csis = f2csi_type1[f];
            if (csis==NULL)
            {
                csis = new InstructionSet;
                f2csi_type1[f] = csis;
            }
            CallInst *non_const_ci = const_cast<CallInst*>
                            (static_cast<const CallInst*>(ci));

            csis->insert(non_const_ci);

#if 1
            errs()<<"CallSite: ";
            ci->getDebugLoc().print(errs());
            errs()<<"\n";
#endif
        }
    }
}*/

/*
 * need to populate idcs2callee before calling this function
 * should not call into this function using direct call
 */
/*FunctionSet Gatlin::resolve_indirect_callee(CallInst* ci)
{
    FunctionSet fs;
    if (ci->isInlineAsm())
        return fs;
    if (get_callee_function_direct(ci))
        llvm_unreachable("resolved into direct call!");

    auto _fs = idcs2callee.find(ci);
    if (_fs != idcs2callee.end())
    {
        for (auto* f: *(_fs->second))
            fs.insert(f);
    }

#if 0
    //FUZZY MATCHING
    //method 1: signature based matching
    //only allow precise match when collecting protected functions
        Value* cv = ci->getCalledValue();
        Type *ft = cv->getType()->getPointerElementType();
        if (!is_complex_type(ft))
            return fs;
        if (t2fs.find(ft)==t2fs.end())
            return fs;
        FunctionSet *fl = t2fs[ft];
        for (auto* f: *fl)
            fs.insert(f);
#endif
    return fs;
}*/
////////////////////////////////////////////////////////////////////////////////

/*
 * collect all gating function callsite
 * ----
 * f2chks: Function to Gating Function CallSite
 */
/*void Gatlin::collect_chkps(Module& module)
{
    for (auto func: all_functions)
    {
        if (gating->is_gating_function(func))
            continue;
        
        InstructionSet *chks = f2chks[func];
        if (!chks)
        {
            chks = new InstructionSet();
            f2chks[func] = chks;
        }

        for(Function::iterator fi = func->begin(), fe = func->end(); fi != fe; ++fi)
        {
            BasicBlock* bb = dyn_cast<BasicBlock>(fi);
            for (BasicBlock::iterator ii = bb->begin(), ie = bb->end(); ii!=ie; ++ii)
            {
                if (CallInst* ci = dyn_cast<CallInst>(ii))
                    if (Function* _f = get_callee_function_direct(ci))
                    {
                        if (gating->is_gating_function(_f))
                            chks->insert(ci);
                    }
            }
        }
    }
#if 0
    //dump all checks
    for(auto& pair: f2chks)
    {
        ValueSet visited;
        Function* f = pair.first;
        InstructionSet* chkins = pair.second;
        if (chkins->size()==0)
            continue;
        gating->dump_interesting(chkins);
    }
#endif
}*/

/*
 * track user of functions which have checks, and see whether it is tied
 * to any interesting type(struct)
 */
/*Value* find_struct_use(Value* f, ValueSet& visited)
{
    if (visited.count(f))
        return NULL;
    visited.insert(f);
    for (auto* u: f->users())
    {
        if (u->getType()->isStructTy())
            return u;
        if (Value*_u = find_struct_use(u, visited))
            return _u;
    }
    return NULL;
}


void Gatlin::identify_interesting_struct(Module& module)
{
    //first... functions which have checks in them
    for(auto& pair: f2chks)
    {
        ValueSet visited;
        Function* f = pair.first;
        InstructionSet* chkins = pair.second;
        if (chkins->size()==0)
            continue;
        if (Value* u = find_struct_use(f, visited))
        {
            StructType* type = dyn_cast<StructType>(u->getType());
            if (!type->hasName())
                continue;
            //should always skip this
            if (type->getStructName().startswith("struct.kernel_symbol"))
                continue;
            bool already_exists = is_interesting_type(type);
           */ /*errs()<<"Function: "<<f->getName()
                <<" used by ";
            if (!already_exists)
                errs()<<ANSI_COLOR_GREEN<<" new discover:";
            if (type->getStructName().size()==0)
                errs()<<ANSI_COLOR_RED<<"Annonymouse Type";
            else
                errs()<<type->getStructName();
            errs()<<ANSI_COLOR_RESET<<"\n";*/
            //discovered_interesting_type.insert(type);
        //}
    //}
#if 0
    //second... all functions
    for (auto f: all_functions)
    {
        ValueSet visited;
        if (Value* u = find_struct_use(f, visited))
        {
            StructType* type = dyn_cast<StructType>(u->getType());
            if (type->isLiteral())
                continue;
            if (!type->hasName())
                continue;
            if (type->getStructName().startswith("struct.kernel_symbol"))
                continue;
            bool already_exists = is_interesting_type(type);
            errs()<<"Function: "<<f->getName()
                <<" used by ";
            if (!already_exists)
                errs()<<ANSI_COLOR_GREEN<<" new discover:";
            if (type->getStructName().size()==0)
                errs()<<ANSI_COLOR_RED<<"Annonymouse Type";
            else
                errs()<<type->getStructName();
            errs()<<ANSI_COLOR_RESET<<"\n";
            discovered_interesting_type.insert(type);

        }
    }
#endif
    //sort functions
    /*for (auto f: all_functions)
    {
        StringRef fname = f->getName();
        if (fname.startswith("trace_event") ||
                fname.startswith("perf_trace") ||
                fname.startswith("trace_raw"))
        {
            trace_event_funcs.insert(f);
            continue;
        }
        if (fname.startswith("bpf") || 
                fname.startswith("__bpf") ||
                fname.startswith("___bpf"))
        {
            bpf_funcs.insert(f);
            continue;
        }
        if (fname.startswith("irq"))
        {
            irq_funcs.insert(f);
            continue;
        }

        ValueSet visited;
        Value* u = find_struct_use(f, visited);
        if (u)
        {
            bool skip = false;
            for (Value* v: visited)
                if (isa<Instruction>(v))
                {
                    assert("this is impossible\n");
                    skip = true;
                    break;
                }
            if (!skip)
                kmi_funcs.insert(f);
        }
    }

}
*/
/*
 * this is used to identify any assignment of fptr to struct field, and we 
 * collect this in complementary of identify_kmi
 */
void GatlinModule::identify_dynamic_kmi()
{
    int cnt_resolved = 0;
    for (auto *f: all_functions)
    {
        //Value* v = dyn_cast<Value>(f);
        Indices inds;
        ValueSet visited;
        //StructType *t = find_assignment_to_struct_type(v, inds, visited);
        //if (!t)
        //    continue;
        //Great! we got one! merge to know list or creat new

        //cnt_resolved++;
        //add_function_to_dmi(f, t, inds, dmi);
        // find_assignment_to_struct_type(f, visited, dmi);
        ValueSet visited2;
        find_global_variable_initializer(f, (Value *)f, Indices(), visited2, dmi);
    }
    errs()<<"#dyn kmi resolved:"<<cnt_resolved<<"\n";
}

/*
void GatlinModule::dump_dkmi()
{
    //if (!knob_gatlin_dkmi)
    //    return;
    errs()<<"=dynamic KMI="<<"\n";
    for (auto tp: dmi)
    {
        //type to metadata mapping
        StructType* t = tp.first;
        errs()<<"Type:";
        if (t->isLiteral())
            errs()<<"Literal\n";
        else
            errs()<<t->getStructName()<<"\n";
        //here comes the pairs
        IFPairs ifps = tp.second;
        for (auto ifp: ifps)
        {
            //indicies
            Indices idcs = ifp.first;
            FunctionSet* fset = ifp.second;
            errs()<<"  @ [";
            for (auto i: idcs)
            {
                errs()<<i<<",";
            }
            errs()<<"]\n";
            //function names
            for (Function* f: *fset)
            {
                errs()<<"        - ";
                errs()<<f->getName();
                errs()<<"\n";
            }
        }
    }
    errs()<<"\n";
}
*/

/*
 * identify logical kernel module
 * kernel module usually connect its functions to a struct that can be called 
 * by upper layer
 * collect all global struct variable who have function pointer field
 */
void GatlinModule::identify_kmi()
{
    //Module::GlobalListType &globals = module.getGlobalList();
    //not an interesting type, no function ptr inside this struct
    TypeSet nomo;
    for(GlobalVariable &gvi: M->globals())
    {
        GlobalVariable* gi = &gvi;
        if (gi->isDeclaration())
            continue;
        assert(isa<Value>(gi));

        StringRef gvn = gi->getName();
        if (gvn.startswith("__kstrtab") || 
                gvn.startswith("__tpstrtab") || 
                gvn.startswith(".str") ||
                gvn.startswith("llvm.") ||
                gvn.startswith("__setup_str"))
            continue;

        Type* mod_interface = gi->getType();

        if (mod_interface->isPointerTy())
            mod_interface = mod_interface->getPointerElementType();
        if (!mod_interface->isAggregateType())
            continue;
        if (mod_interface->isArrayTy())
        {
            mod_interface
                = dyn_cast<ArrayType>(mod_interface)->getElementType();
        }
        if (!mod_interface->isStructTy())
        {
            if (mod_interface->isFirstClassType())
                continue;
            //report any non-first class type
            errs()<<"IDKMI: aggregate type not struct?\n";
            mod_interface->print(errs());
            errs()<<"\n";
            errs()<<gi->getName()<<"\n";
            continue;
        }
        if (nomo.find(mod_interface)!=nomo.end())
            continue;
        //function pointer inside struct?
        if (!has_function_pointer_type(mod_interface))
        {
            nomo.insert(mod_interface);
            continue;
        }
        //add
        ModuleSet *ms;
        if (mi2m.find(mod_interface) != mi2m.end())
        {
            ms = mi2m[mod_interface];
        }else
        {
            ms = new ModuleSet;
            mi2m[mod_interface] = ms;
        }
        assert(ms);
        ms->insert(gi);
        //if (array_type)
        //    errs()<<"Added ArrayType:"<<gvn<<"\n";
    }
    TypeList to_remove;
    ModuleInterface2Modules to_add;
    //resolve Annoymous type into known type
    for (auto msi: mi2m)
    {
        StructType * stype = dyn_cast<StructType>(msi.first);
        if (stype->hasName())
            continue;
        StructType *rstype = NULL;
        assert(msi.second);
        for (auto m: (*msi.second))
        {
            //constant bitcast into struct
            for (auto *_u: m->users())
            {
                ConstantExpr* u = dyn_cast<ConstantExpr>(_u);
                BitCastInst* bciu = dyn_cast<BitCastInst>(_u);
                PointerType* type = NULL;
                if((u) && (u->isCast()))
                {
                    type = dyn_cast<PointerType>(u->getType());
                    goto got_bitcast;
                }
                if (bciu)
                {
                    type = dyn_cast<PointerType>(bciu->getType());
                    goto got_bitcast;
                }
                //what else???
                continue;
got_bitcast:
                //struct object casted into non pointer type?
                if (type==NULL)
                    continue;
                StructType* _stype = dyn_cast<StructType>(type->getElementType());
                if ((!_stype) || (!_stype->hasName()))
                    continue;
                rstype = _stype;
                goto out;
            }
        }
out:
        if (!rstype)
            continue;
        //resolved, merge with existing type
        if (mi2m.find(rstype)!=mi2m.end())
        {
            ModuleSet* ms = mi2m[rstype];
            for (auto m: (*msi.second))
                ms->insert(m);
        }else if (to_add.find(rstype)!=to_add.end())
        {
            ModuleSet* ms = to_add[rstype];
            for (auto m: (*msi.second))
                    ms->insert(m);
        }else
        {
            //does not exists? reuse current one!
            to_add[rstype] = msi.second;
            /*
             * this should not cause crash as we already parsed current element
             * and this should be set to NULL in order to not be deleted later
             */
            mi2m[stype] = NULL;
        }
        to_remove.push_back(stype);
    }
    for (auto r: to_remove)
    {
        delete mi2m[r];
        mi2m.erase(r);
    }
    for (auto r: to_add)
        mi2m[r.first] = r.second;
}

//////////////////add from here
/*
void dump_equivalence_sets() {
    for (auto es:equivalence_sets) {
        errs() << "\n new set\n";
        for (auto e:*es)
            errs() << e.stype->getStructName() <<"  ";
    }
    errs() <<"\n\n";
}
*/

/*
bool is_same_StypeInd(StypeInd* s, StypeInd* ss) {
    StructType *st_s = s->first;
    StructType *st_ss = ss->first;
    if (st_s->isLiteral() || st_ss->isLiteral()) {
        if (st_s == st_ss)
            return true;
        else 
            return false;
    }

    std::string stname_s = st_s->getStructName().str();
    std::string stname_ss = st_ss->getStructName().str();
    str_truncate_dot_number(stname_s);
    str_truncate_dot_number(stname_ss);
    if(stname_ss == stname_s) {
        //errs() << "same type: " << stname_s <<"\n";
        return true;
    }
    else 
        return false;

}
*/

bool find_es(std::unordered_set<struct_field> *es, struct_field s) {
    for (auto es_element:*es) {
        if (es_element == s)
            return true;
    }
    return false;
}


/*
void merge_diff_structs(std::unordered_set<struct_field> s0,std::unordered_set<struct_field> s1) {
    if (s0.size() && s1.size()) {
        for (auto s:s0) {
            for (auto ss:s1) {
                // not the same struct type and index
                if (s != ss){
                    std::unordered_set<struct_field> *es0 = NULL, *es1 = NULL;
                    bool is_founds0 = false, is_foundss1 = false;
                    for (auto es: equivalence_sets) {
                        if (!is_founds0)
                            is_founds0 = find_es(es, s);
                        if (!is_foundss1)
                            is_foundss1 = find_es(es, s);

                        if (is_founds0) {
                            es0 = es;
                        }
                        if (is_foundss1){
                            es1 = es;
                        }
                    }
                    if (!es0 && !es1) {
                        std::unordered_set<struct_field> *new_si_set = new std::unordered_set<struct_field>;
                        new_si_set->insert(s);
                        new_si_set->insert(ss);
                        equivalence_sets.insert(new_si_set);
                    } else if (es0 && es1) {
                        if(es0 != es1) {
                            for (auto es1_element : *es1) 
                                es0->insert(es1_element);
                            equivalence_sets.erase(es1);
                        }
                    } else if (es1) {
                        es1->insert(s);
                    } else {
                        es1->insert(ss);
                    }
                }
            }
        }
    }
}
*/

// get load from gep, only contain the last struct type and index
void GatlinModule::use_of_structs(Value *v, std::unordered_set<struct_field>& si) {
    InstructionSet geps = get_load_from_gep(v);

    for (auto _gep: geps) {
        GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(_gep);
        Indices indices;
        //x_dbg_ins = gep;
        StructType* cvt = get_gep_indices_type(gep, indices);
        if (!cvt)
            continue;

        /*
        Indices idcs;
        for(auto i:indices)
            idcs.push_back(i);

        if(indices.size() >= 1)
            idcs.pop_front();    
        bool isAgg = true;

        while (idcs.size() > 1) {
            if (ArrayType *aty = dyn_cast<ArrayType>(cvt)) {
                cvt = aty->getElementType();
                idcs.pop_front();
            } else if (StructType *st = dyn_cast<StructType>(cvt)){
                cvt = st->getTypeAtIndex(idcs.front());
                idcs.pop_front();
            }

            if (!cvt->isAggregateType()) {
                isAgg = false;
                break;
            }
        }
        */
        StructType *final_sty = cvt;
        if(final_sty)
            si.insert(struct_field(final_sty, indices));
    }
}


//place at the end of module
void GatlinModule::populate_between_diff_structs(StoreInst* i) {
    //errs() << "store instruction\n";
    Value *v = i->getValueOperand();
    PointerType *pt = dyn_cast<PointerType>(v->getType());

    if(pt == NULL)
        return;

    //errs() << "store value type: " << *v->getType() <<"\n";
    if(!isa<FunctionType>(pt->getElementType()))
        return;

    std::unordered_set<struct_field> s;
    //for the source
    use_of_structs(v, s);
    if(s.size() == 0)
        return;

    StructType *final_sty = NULL;
    bool isAgg = true;
    //for the destination
    Indices indices;
    StructType *sty = resolve_where_is_it_stored_to(i, indices);
    if(sty == NULL)
        goto final_delete;

    /*
    cvt = dyn_cast<Type>(sty);
    if (indices.size() >= 1)
        indices.pop_front();
    
    

    while (indices.size() > 1) {
        if (ArrayType *aty = dyn_cast<ArrayType>(cvt)) {
            cvt = aty->getElementType();
            indices.pop_front();
        } else if (StructType *st = dyn_cast<StructType>(cvt)) {
            cvt = st->getTypeAtIndex(indices.front());
            indices.pop_front();
        }

        if (!cvt->isAggregateType()) {
            isAgg = false;
                //delete indices;
            break;
        }
    }
    */
    
    final_sty = sty;
    if (final_sty && indices.size() == 1) {
        // errs() << "\nsource: ";
        // for ( auto sele:s) {
        //     errs() << sele->first->getStructName() << "\n";
        //     for(auto indx: *sele->second)
        //         errs() << indx <<" ";
        // }
        // errs() << "\ntarget: " << final_sty->getStructName() <<"\n";
        // for(auto indx : indices) 
        //     errs() << indx <<" ";
        
        // Done:
        //    need to determine if they are the same struct type and index
        //    merge the jump table
        std::unordered_set<struct_field> s1;
        Indices idcs = indices;
        s1.insert(struct_field(final_sty, idcs));

        // merge_diff_structs(s, s1);
        return;
    }
    // if the struct store to other vars,e.g. fp = A.ptr;, delete the struct
    //if (!final_sty) {
    //    for (auto dsi:s)
    //        siset_delete.insert(dsi);
    //}

final_delete:
    if (!final_sty) {
        while (!s.empty()) {
            auto it = s.begin();
            auto dsi = *it;
            s.erase(it);
        }
    }
}

/*
void GatlinModule::handle_structs_in_cmp(CmpInst* cmp) {
//    errs() << "cmp instruction\n";
    Value *v[2];
    v[0] = cmp->getOperand(0);
    v[1] = cmp->getOperand(1);

    if (dyn_cast<ConstantPointerNull>(v[1]) || dyn_cast<ConstantPointerNull>(v[0]))
        return;
    for (int i = 0; i<2; i++) {
        PointerType *pt = dyn_cast<PointerType>(v[i]->getType());
        if(pt == NULL)
            return;
        if(!isa<FunctionType>(pt->getElementType()))
            return;
    }




    std::unordered_set<struct_field> s[2];
    use_of_structs(v[0], s[0]);
    use_of_structs(v[1], s[1]);

    for (auto i = 0; i<2 ;i++) {
        if (Function *f = dyn_cast<Function>(v[i])) {
            int j = (i+1) % 2;
            //std::set<StypeInd *> sv;;
            //use_of_structs(v[j], sv);
            if (s[j].size()) {

                for (auto it = s[j].begin(); it != s[j].end(); ) {
                    auto sv_elements = *it;
                    if (siset.find(sv_elements) == siset.end()) {
                        siset.insert(sv_elements);
                        //TODO
                        // may need to add into dmi, current add to si2func
                        it++;
                    } else {
                        it = s[j].erase(it);
                    }

                    FunctionSet &fs = si2func[sv_elements];
                    fs.insert(f);
                    //errs() << "cmp struct: " << si_in_siset->first->getStructName() << "\n";
                    //errs() << "cmp function: " << f->getName() <<"\n";
                }
            }
            return;
        }
    }

    //llvm::Instruction::OtherOps op= cmp->getOpcode();
    //errs() << "OtherOps: " << op <<"\n";
    //errs() <<"fist arg" << *v[0]<<"\n";
    //errs() <<"second arg" << *v[1]<<"\n";


    if(s[0].size() && s[1].size()) {
        // if they come from the same struct and index, ignore; else merge the jump table;
        merge_diff_structs(s[0], s[1]);
    } else {
        if (s[0].size()) {
            for (auto ss:s[0])
                siset_delete.insert(ss);
        } else if (s[1].size()) {
            for(auto ss:s[1])
                siset_delete.insert(ss);
        }
    }
    

}
*/
void handle_leakage(Module *m)
{
    for (llvm::Function &fi : m->getFunctionList()) {
        llvm::Function *f = &fi;
        for (llvm::BasicBlock &bi : f->getBasicBlockList()) {
            llvm::BasicBlock *b = &bi;
            for (llvm::Instruction &ii : b->getInstList()) {
                llvm::Instruction *i = &ii;
                if (auto store = dyn_cast<StoreInst>(i)) {
                    Value *addr = store->getPointerOperand();
                    Value *value = store->getValueOperand();                
                    std::unordered_set<Value *> cannot_handle;
                    auto store_fields = get_struct_fields_from_addr(addr, cannot_handle);
                    //errs()<<"test3231*****\n";
                    if (store_fields.empty())
                        continue;

                    std::unordered_set<struct_field> fields;
                    std::vector<std::pair<Function *, Value *> > funcuses;
                    std::unordered_set<Value *> visited;
                    possible_functions(store, value, fields, funcuses, cannot_handle, visited);
                    //errs()<<"test3238*****\n";
                    if (cannot_handle.size() > 0) {
                        // for (auto v : cannot_handle) {
                        //     errs() << "store cannot handle: " << *v << "\n";
                        // }
                        siset_delete.insert(store_fields.begin(), store_fields.end());
                        // } else {
                    }
                    {
                        for (auto fu : funcuses) {
                            for (auto field : store_fields) {
                                si2func[field].insert(fu.first);
                                fu2si[fu.first][fu.second].insert(field);
                            }
                        }
                        fields.insert(store_fields.begin(), store_fields.end());
                        merge_disjoint_sets(fields);
                        //errs()<<"test3256*****\n";
                    }
                } else if (auto load = dyn_cast<LoadInst>(i)) {
                    //errs()<<"test3259*****\n";
                    Value *addr = load->getPointerOperand();
                    std::unordered_set<Value *> cannot_handle;
                    auto load_fields = get_struct_fields_from_addr(addr, cannot_handle);
                    if (load_fields.empty())
                        continue;
                    std::unordered_set<struct_field> fields;
                    std::unordered_set<CallInst *> calls;
                    std::unordered_set<Value *> visited;
                    uses_of_value(load, fields, calls, cannot_handle, visited);
                    if (cannot_handle.size() > 0) {
                        // for (auto v : cannot_handle) {
                        //     errs() << "load cannot handle: " << *v << "\n";
                        // }
                        siset_delete.insert(load_fields.begin(), load_fields.end());
                    }
                    //errs()<<"test3275*****\n";
                    for (CallInst *call : calls) {
                        std::unordered_set<struct_field> fields;
                        std::vector<std::pair<Function *, Value *> > funcuses;
                        std::unordered_set<Value *> cannot_handle;
                        std::unordered_set<Value *> visited;
                        possible_functions(call, dyn_cast<Value>(call), fields, funcuses, cannot_handle, visited);
                        if (cannot_handle.empty() && funcuses.empty()) {
                            merge_disjoint_sets(fields);
                        } else {
                            siset_delete.insert(load_fields.begin(), load_fields.end());
                            siset_delete.insert(fields.begin(), fields.end());
                        }
                        for (auto field : load_fields) {
                            si2call[field].insert(call);
                            ci2si[call].insert(field);
                        }
                        for (auto field : fields) {
                            si2call[field].insert(call);
                            ci2si[call].insert(field);
                        }
                    }
                    //errs()<<"test3296*****\n";
                    fields.insert(load_fields.begin(), load_fields.end());
                    /*for(auto &it : fields){
                        errs()<<"fields :"<<it.stype->getName().str()<<"\n";
                    }*/
                    //errs()<<"test3296 and 3297*****\n";
                    merge_disjoint_sets(fields);
                    //errs()<<"test3297********\n";
                    //errs()<<"test3297 Function name: "<<fi.getName().str()<<"\n";
                }
            }
        }
        //errs()<<"Function name: "<<fi.getName().str()<<"\n";
    }


    auto deleted_set = merge_disjoint_sets(siset_delete);

    std::unordered_set<struct_field> cisis, allsis;
    for (auto p : ci2si) {
        auto sis = p.second;
        cisis.insert(sis.begin(), sis.end());
        allsis.insert(sis.begin(), sis.end());
    }
    for (auto p : fu2si) {
        auto u2si = p.second;
        for (auto p2 : u2si) {
            auto sis = p2.second;
            allsis.insert(sis.begin(), sis.end());
        }
    }
    
    for (struct_field field : allsis) {
        auto set = si_disjoint_sets[field];
        if (!set) {
            si_disjoint_sets[field] = new std::unordered_set<struct_field>;
            si_disjoint_sets[field]->insert(field);
        }
    }
    
    for (auto p : fu2si) {
        Function *f = p.first;
        auto u2si = p.second;
        for (auto p2 : u2si) {
            Value *user = p2.first;
            auto fields = p2.second;
            for (auto field :fields) {
                si2fu[field][user].insert(f);
                si2func[field].insert(f);
            }
        }
    }
    
    for (auto p : fu2si) {
        Function *f = p.first;
        auto u2si = p.second;
        for (auto p2 : u2si) {
            Value *user = p2.first;
            auto fields = p2.second;
            std::unordered_set<struct_field> to_delete;
            for (auto field : fields) {
                if (deleted_set->find(field) != deleted_set->end()) {
                    to_delete.insert(field);
                }
            }
            for (auto field : to_delete) {
                fields.erase(field);
            }
            merge_disjoint_sets(fields);
        }
    }

    
    for (auto p : si2call) {
        for (auto c : p.second) {
            if (ci2si[c].find(p.first) == ci2si[c].end()) {
                // errs() << "ci : " << *c << " not found\n";
            }
        }
    }

    for (auto p : ci2si) {
        bool leaked = false;
        auto ci = p.first;
        auto &fields = p.second;
        for (auto field : fields) {
            if (deleted_set->find(field) != deleted_set->end()) {
                leaked = true;
                break;
            }
        }
        if (!leaked) {
            merge_disjoint_sets(fields);
        }
    }

    
    for (auto p : fu2si) {
        Function *f = p.first;
        auto u2si = p.second;
        for (auto p2 : u2si) {
            Value *user = p2.first;
            auto fields = p2.second;
            for (struct_field field : fields) {
                if (si_disjoint_sets[field] == deleted_set)
                    continue;
                auto set = si_disjoint_sets[field];
                set2fu[set][user].insert(f);
            }
        }
    }

    for (auto p : ci2si) {
        CallInst *ci = p.first;
        auto fields = p.second;
        for (struct_field field : fields) {
            set2ci[si_disjoint_sets[field]].insert(ci);
        }
    }

    
    int set_count = 0;
    int single_set_count = 0;
    std::unordered_set<void *> visited_set;
    for (auto p : si_disjoint_sets) {
        auto field = p.first;
        if (p.second) {
            if (visited_set.find(p.second) == visited_set.end()) {
                set_count++;
                visited_set.insert(p.second);
            }
            if (p.second->size() == 1)
                single_set_count++;
        } else {
            single_set_count++;
            set_count++;
        }
    }

    for (struct_field field : cisis) {
        auto u2f = si2fu[field];
        if (deleted_set->find(field) != deleted_set->end())
            continue;
        if (u2f.size() == 0) {
            errs() << "no functions: " << (field.stype->isLiteral() ? "!literal!" : field.stype->getName()) << "[" << *field.indices.begin() << "]\n";
        }
    }
  
    int nocicount = 0;
    int total_targets = 0;
    int calls = 0;
    int nocalls = 0;
    for (auto p : ci2si) {
        bool leaked = false;
        auto c = p.first;
        auto &s = p.second;
        for (auto fi : s) {
            if (deleted_set->find(fi) != deleted_set->end()) {
                leaked = true;
                nocicount++;
                break;
            }
        }
        if (!leaked) {
            std::unordered_set<void *> visited;
            int targets = 0;
            std::unordered_set<struct_field> *set;
            if (s.empty()) {
                llvm_unreachable("call inst with no corresponding si");
                continue;
            }
            for (auto fi : s) {
                if ((set = si_disjoint_sets[fi])) {
                    break;
                }
            }
            for (auto fi : *set) {
                auto u2f = si2fu[fi];
                std::unordered_set<Function *> fs;
                for (auto p : u2f) {
                    auto ufs = p.second;
                    fs.insert(ufs.begin(), ufs.end());
                }
                targets += fs.size();
            }
            if (targets)
                calls++;
            else {
                nocalls++;
            }
            total_targets += targets;
        }
    }

    set2ci.erase(deleted_set);
    set2fu.erase(deleted_set);

    errs() << "setdump begin\n";
    for (auto p : set2ci) {
        auto set = p.first;
        auto calls = p.second;
        auto u2f = set2fu[set];
        auto si_count = set->size();
        auto call_count = calls.size();
        std::unordered_set<Function *> fs;
        for (auto p2 : u2f) {
            for (Function *f : p2.second) {
                fs.insert(f);
            }
        }
        auto f_count = fs.size();
        if (f_count > 100 || call_count > 100 || si_count > 100) {
            errs() << "abnormal ";
            for (auto si : *set) {
                errs() << "{" << (si.stype->isLiteral() ? "!literal!" : si.stype->getName()) << " " << *si.indices.begin() << "}";
            }
            errs() << "\n";
        }
        errs() << si_count << " " << call_count << " " << f_count << "\n";
    }

    errs() << "setdump end\n";
    
    errs() << "indirect call sites: " << idcs.size()<<"\n";
    errs() << "leak calls: " << nocicount << "\n";
    errs() << "avg targets: " << total_targets / (double) calls << "\n";
    errs() << "calls: " << calls << "\n";
    errs() << "deleted struct fields: " << deleted_set->size() << "\n";
    errs() << "struct fields: " << cisis.size() << "\n";
    errs() << "no target calls: " << nocalls << "\n";
    errs() << "set count: " << set_count << "\n";
    errs() << "single set count: " << single_set_count << "\n";
}

void GatlinModule::postprocess()
{
    handle_leakage(M);
}

/*
 * populate cache
 * --------------
 * all_functions
 * t2fs(Type to FunctionSet)
 * syscall_list
 * f2csi_type0 (Function to BitCast CallSite)
 * idcs(indirect call site)
 */

void GatlinModule::preprocess()
{
	errs()<<"in preprocess\n";
    for (Module::iterator fi = M->begin(), f_end = M->end();
            fi != f_end; ++fi)
    {
        //errs()<<"in module fun\n";
	    
	Function *func = dyn_cast<Function>(fi);
        if (func->isDeclaration())
        {
            //ExternalFuncCounter++;
            continue;
        }
        if (func->isIntrinsic())
            continue;

        //FuncCounter++;
        all_functions.insert(func);
        Type* type = func->getFunctionType();
        FunctionSet &fl = t2fs[type];
        fl.insert(func);
        
        if (is_syscall_prefix(func->getName()))
            syscall_list.insert(func);

        for(Function::iterator fi = func->begin(), fe = func->end();
                fi != fe; ++fi)
        {
            BasicBlock* bb = dyn_cast<BasicBlock>(fi);
            for (BasicBlock::iterator ii = bb->begin(), ie = bb->end(); ii!=ie; ++ii)
            {
                if (StoreInst *st = dyn_cast<StoreInst>(ii)) {
                    // populate_between_diff_structs(st);
                    continue;
                } else if (CmpInst *cmp = dyn_cast<CmpInst>(ii)) {
                    // handle_structs_in_cmp(cmp);
                    continue;
                }

                CallInst* ci = dyn_cast<CallInst>(ii);
                if (!ci || ci->getCalledFunction() || ci->isInlineAsm())
                    continue;
                
                Value* cv = ci->getCalledOperand();
                Function *bcf = dyn_cast<Function>(cv->stripPointerCasts());
                if (bcf)
                {
                    //this is actually a direct call with function type cast
                    InstructionSet* csis = f2csi_type0[bcf];
                    if (csis==NULL)
                    {
                        csis = new InstructionSet;
                        f2csi_type0[bcf] = csis;
                    }
                    csis->insert(ci);
                    continue;
                }
                idcs.insert(ci);
		//errs()<<"idcs in pex: " << *ci <<"\n";
            }
        }
    }
}


/*
 * process capability protected globals and functions
 */
void GatlinModule::process_cpgf()
{
    //my_debug(module);
    /*
     * pre-process
     * generate resource/functions from syscall entry function
     */
    //initialize_gatlin_sets(knob_skip_func_list, knob_skip_var_list,
                            //knob_crit_symbol, knob_kernel_api);

    errs()<<"Pre-processing...\n";
    //STOP_WATCH_MON(WID_0, preprocess());
    preprocess();
    errs()<<"Found "<<idcs.size()<<" idcs\n";
    errs()<<"found functions"<<all_functions.size() <<"\n";


    errs()<<"Identify Kernel Modules Interface\n";
    //STOP_WATCH_MON(WID_0, identify_kmi());
    // identify_kmi();
    //dump_kmi();
    errs()<<"dynamic KMI\n";
    //STOP_WATCH_MON(WID_0, identify_dynamic_kmi());
    identify_dynamic_kmi();
    //dump_dkmi();

    errs()<<"Populate indirect callsite using kernel module interface\n";
 
    postprocess();
    //dump_equivalence_sets();
    // delete_siset();
    finish_pass();
}

bool GatlinModule::runOnModule(Module &module)
{
    M = &module;
    //errs()<<ANSI_COLOR_CYAN
    //    <<"--- PROCESS FUNCTIONS ---"
    //    <<ANSI_COLOR_RESET<<"\n";
    collectAddrTaken(&module);
    //process_cpgf();
    buildCFG(&module);
    //errs()<<ANSI_COLOR_CYAN
    //    <<"--- DONE! ---"
    //    <<ANSI_COLOR_RESET<<"\n";

    return false;
}
/*
bool Gatlin::GatlinPass(Module &module)
{
    errs()<<ANSI_COLOR_CYAN
        <<"--- PROCESS FUNCTIONS ---"
        <<ANSI_COLOR_RESET<<"\n";
    process_cpgf(module);
    errs()<<ANSI_COLOR_CYAN
        <<"--- DONE! ---"
        <<ANSI_COLOR_RESET<<"\n";

#if CUSTOM_STATISTICS
    dump_statistics();
#endif
    //just quit
    //exit(0);
    //never reach here
    return false;
}*/

static RegisterPass<GatlinModule>
XXX("gatlin", "Gatlin Pass (with getAnalysisUsage implemented)");


//PreservedAnalyses GatlinPass::run(Module &M, ModuleAnalysisManager &AM) {
//	GatlinModule(M).process_cpgf();
//	return PreservedAnalyses::none();
//}