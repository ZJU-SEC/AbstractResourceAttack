#ifndef _FINDLOCKANDATOMIC_H_
#define _FINDLOCKANDATOMIC_H_




#include "Utils.h"
#include <utility>
#include <set>
#include <vector>

class FindLockAndAtomic{
    public:
        FindLockAndAtomic(llvm::Module &);//公有方法，用来暴露私有对象
        std::set<std::string> GetSpinLock();
        std::set<std::string> GetSpinUnlock();
        std::set<std::string> GetAtomicFunc();
        std::set<std::string> GetPerCPU();
        std::set<std::string> GetWriteLock();
        std::set<std::string> GetWriteUnlock();
        std::set<std::string> GetMutexLock();
        std::set<std::string> GetMutexUnlock();
        std::set<std::string> GetAllocFunction();
        std::set<std::string> GetRlimitFunction();
        void FindLock();
        
    private:
        llvm::Module* _module_;
        std::set<std::string> spin_lock;
        std::set<std::string> spin_unlock;
        std::set<std::string> atomic_function;
        std::set<std::string> percpu_function;
        std::set<std::string> write_lock;
        std::set<std::string> write_unlock;
        std::set<std::string> mutex_lock;
        std::set<std::string> mutex_unlock;
        std::set<std::string> AllocFunctionNames;
        std::set<std::string> rlimit_function;

        std::set<std::string> basic_raw_spin_lock;
        std::set<std::string> basic_spin_lock;
        std::set<std::string> basic_mutex_lock;
        std::set<std::string> basic_read_lock;
        std::set<std::string> basic_write_lock;
        void GenerateRawSpinLock();
        void GenerateBasicSpinLock();
        void GenerateBasicMutexLock();
        void GenerateBasicRWLock();  
        void SetSpin(std::set<std::string> *,std::set<std::string> *);
        void SetRawSpin(std::set<std::string> *,std::set<std::string> *);
        void SetMutex(std::set<std::string> *,std::set<std::string> *);
        void SetWriteLock(std::set<std::string> *,std::set<std::string> *);
        void BasicAndUpperLockSet(std::set<std::string>,std::set<std::string>,std::set<std::string>,std::set<std::string> *,std::set<std::string> *);
        void AllSpin();
        void AllMutex();
        void AllWrite();
        void AllPerCPU();
        void AllAtomic();
        void AllAllocFunction();
        std::set<std::string> GenerateTaskRlimit();
        void GenerateRlimit();
};

#endif