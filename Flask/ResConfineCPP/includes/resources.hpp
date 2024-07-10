#ifndef RESOURCE_TREE_HPP
#define RESOURCE_TREE_HPP

#include <iostream>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <memory>
#include <mutex>

#include "defences.hpp"

struct ProcessTableItem
{
    mutable std::shared_mutex each_mutex;
    std::array<int32_t, 2> data;
};

class ResourceTree
{
public:
    // called by monitor_proc.
    void monitor_proc_new(int32_t pid, int32_t cgid, int32_t grid)
    {
        process_usages.insert_or_assign(pid, std::array<int32_t, TOTAL_DEFENCE_NUM>{0});
        // process_usages.insert({pid, std::array<int32_t, TOTAL_DEFENCE_NUM>{}});
        // container_process[cgid].insert(pid);
        // if (monitor_proc_ok(cgid, grid))
        // {
        //     // std::cout << "[Debug] Process " << pid << " created in cgroup " << cgid << std::endl;

        //     process_usages.insert({pid, std::array<int32_t, TOTAL_DEFENCE_NUM>{}});
        //     {
        //         std::unique_lock<std::shared_mutex> lock(process_table_mutex);
        //         process_table.emplace(pid, std::array<int32_t, 2>{cgid, grid});
        //     }
        //     process_container.insert({pid, cgid});
        //     container_process[cgid].insert(pid);
        // }
    }

    // called by monitor_proc.
    void monitor_proc_end(int32_t pid, int32_t cgid, int32_t grid)
    {
        // TODO: How to clear all datas.
        // process_usages.erase(pid);
        // container_process[cgid].erase(pid);
        // if (monitor_proc_ok(cgid, grid))
        // {
        //     // std::cout << "[Debug] Process " << pid << " exited in cgroup " << cgid << std::endl;

        //     {
        //         std::unique_lock<std::shared_mutex> lock(process_table_mutex);
        //         process_table.erase(pid);
        //     }
        //     // auto it = process_usages.find(pid);
        //     // if (it != process_usages.end())
        //     // {
        //     //     std::fill(it->second.begin(), it->second.end(), 0);
        //     // }
        //     process_usages.erase(pid);

        //     process_container.erase(pid);
        //     container_process[cgid].erase(pid);
        // }
    }

    // called by multiple threads with different ra_id.
    void monitors_accounting(int32_t ra_id, int32_t pid, int32_t cgid, int32_t count)
    {
        // std::cout << "[Debug] monitor insert " << ra_id << pid << cgid << count <<std::endl;
        process_usages[pid][ra_id] += count;
        container_usages[cgid][ra_id] += count;
    }

    void print_status()
    {
        // std::cout << "Process Table:\n";
        // print1(process_table);

        // std::cout << "Process <-> Container:\n";
        // print2(process_container);

        // std::cout << "Container <-> Process:\n";
        // print3(container_process);

        std::cout << "Process Usages:\n";
        print4(process_usages);

        std::cout << "Container Usage:\n";
        print4(container_usages);
    }

private:
    // // We only care procs that we need to care
    // inline bool monitor_proc_ok(int32_t cgid, int32_t grid)
    // {
    //     if (account_all_flag)
    //     {
    //         return true;
    //     }
    //     else
    //     {
    //         return (container_process.find(cgid) != container_process.end());
    //     }
    // }

    void print1(const std::unordered_map<int32_t, std::array<int32_t, 2>> &process_table)
    {
        for (const auto &entry : process_table)
        {
            std::cout << "PID: " << entry.first << ", Values: [" << entry.second[0] << ", " << entry.second[1] << "]\n";
        }
    }

    void print2(const std::unordered_map<int32_t, int32_t> &process_container)
    {
        for (const auto &entry : process_container)
        {
            std::cout << "ID: " << entry.first << ", ID: " << entry.second << "\n";
        }
    }

    void print3(const std::unordered_map<int32_t, std::unordered_set<int32_t>> &container_process)
    {
        for (const auto &entry : container_process)
        {
            std::cout << "ID: " << entry.first << " => {";
            bool isFirst = true;
            for (const int32_t process_id : entry.second)
            {
                if (!isFirst)
                {
                    std::cout << ", ";
                }
                std::cout << process_id;
                isFirst = false;
            }
            std::cout << "}\n";
        }
    }

    void print4(const std::unordered_map<int32_t, std::array<int32_t, TOTAL_DEFENCE_NUM>> &process_usages)
    {
        for (const auto &entry : process_usages)
        {
            std::cout << "ID: " << entry.first << ", Usages: [";
            for (int i = 0; i < TOTAL_DEFENCE_NUM; ++i)
            {
                std::cout << entry.second[i];
                if (i < TOTAL_DEFENCE_NUM - 1)
                    std::cout << ", ";
            }
            std::cout << "]\n";
        }
    }

private:
    // process table, shared(read) across all threads.
    // mutable std::shared_mutex process_table_mutex;
    // std::unordered_map<int32_t, std::array<int32_t, 2>> process_table;

    // relation table, only monitor_proc use it.
    // std::unordered_map<int32_t, int32_t> process_container;
    // std::unordered_map<int32_t, std::unordered_set<int32_t>> container_process;

    // std::unordered_map<int32_t, int32_t> container_group;
    // std::unordered_map<int32_t, std::unordered_set<int32_t>> group_container;

    // data table, shared(read/write) across all threads.
    // TODO: data table are not thread safely used, use each-process lock?
    std::unordered_map<int32_t, std::array<int32_t, TOTAL_DEFENCE_NUM>> process_usages;
    std::unordered_map<int32_t, std::array<int32_t, TOTAL_DEFENCE_NUM>> container_usages;
    std::unordered_map<int32_t, std::array<int32_t, TOTAL_DEFENCE_NUM>> group_usages;

    // // some configs
    // bool account_all_flag;
};

extern std::shared_ptr<ResourceTree> global_resource_tree;
extern std::atomic_bool global_stop_work;

#endif