#ifndef MYBENCH_HPP
#define MYBENCH_HPP

#include <chrono>
#include <shared_mutex>
#include <unordered_set>

#include "defences.hpp"

class MyBench
{
public:
    void bench_init(uint64_t time)
    {
        init_time += time;
    }

    void bench_trace_proc_ebpf(uint64_t time)
    {
        trace_proc_ebpf_time += time;
        trace_proc_ebpf_count += 1;
    }

    void bench_trace_proc_user(uint64_t time)
    {
        trace_proc_user_time += time;
        trace_proc_user_count += 1;
    }

    void bench_trace_ra_ebpf(uint64_t ra_id, uint64_t time)
    {
        total_ra_ebpf_time += time;
        total_ra_ebpf_count += 1;
        trace_ra_ebpf_time[ra_id] += time;
        trace_ra_ebpf_count[ra_id] += 1;
    }

    void bench_trace_ra_user(uint64_t ra_id, uint64_t time)
    {
        total_ra_user_time += time;
        total_ra_user_count += 1;
        trace_ra_user_time[ra_id] += time;
        trace_ra_user_count[ra_id] += 1;
    }

    void print_benches()
    {
        printf("init time: %lu ms\n", init_time / 1000 / 1000);
        printf("trace_proc_ebpf_time: %lu us, count: %lu, avg: %lu ns\n", trace_proc_ebpf_time / 1000, trace_proc_ebpf_count, trace_proc_ebpf_count == 0 ? 0 : (trace_proc_ebpf_time / trace_proc_ebpf_count));
        printf("trace_proc_user_time: %lu us, count: %lu, avg: %lu ns\n", trace_proc_user_time / 1000, trace_proc_user_count, trace_proc_user_count == 0 ? 0 : (trace_proc_user_time / trace_proc_user_count));

        printf("trace_ra_ebpf_time: %lu us, count: %lu, avg: %lu ns\n", total_ra_ebpf_time / 1000, total_ra_ebpf_count, total_ra_ebpf_count == 0 ? 0 : (total_ra_ebpf_time / total_ra_ebpf_count));
        printf("trace_ra_user_time: %lu us, count: %lu, avg: %lu ns\n", total_ra_user_time / 1000, total_ra_user_count, total_ra_user_count == 0 ? 0 : (total_ra_user_time / total_ra_user_count));
        

        for (int i = 0; i < TOTAL_DEFENCE_NUM; ++i)
        {
            printf("\n");
            printf("trace_ra_ebpf_time[%s]: %lu us, count: %lu, avg: %lu ns\n", enabled_defence[i].c_str(), trace_ra_ebpf_time[i] / 1000, trace_ra_ebpf_count[i], trace_ra_ebpf_count[i] == 0 ? 0 : (trace_ra_ebpf_time[i] / trace_ra_ebpf_count[i]));
            printf("trace_ra_user_time[%s]: %lu us, count: %lu, avg: %lu ns\n", enabled_defence[i].c_str(), trace_ra_user_time[i] / 1000, trace_ra_user_count[i], trace_ra_user_count[i] == 0 ? 0 : (trace_ra_user_time[i] / trace_ra_user_count[i]));
        }

        // printf("all traced procs: %lu\n", procs.size());
        // std::vector<uint32_t> sortedProcs(procs.begin(), procs.end());
        // std::sort(sortedProcs.begin(), sortedProcs.end());

        // for(auto proc : sortedProcs) {
        //     printf("pid: %d\n", proc);
        // }
    }

private:
    uint64_t init_time = 0;

    uint64_t trace_proc_ebpf_time = 0;
    uint64_t trace_proc_ebpf_count = 0;

    uint64_t trace_proc_user_time = 0;
    uint64_t trace_proc_user_count = 0;

    uint64_t trace_ra_ebpf_time[TOTAL_DEFENCE_NUM] = {0};
    uint64_t trace_ra_ebpf_count[TOTAL_DEFENCE_NUM] = {0};

    uint64_t trace_ra_user_time[TOTAL_DEFENCE_NUM] = {0};
    uint64_t trace_ra_user_count[TOTAL_DEFENCE_NUM] = {0};

    uint64_t total_ra_ebpf_time = 0;
    uint64_t total_ra_ebpf_count = 0;

    uint64_t total_ra_user_time = 0;
    uint64_t total_ra_user_count = 0;

    // std::unordered_set<uint32_t> procs;
};

extern std::shared_ptr<MyBench> global_bench;

#endif