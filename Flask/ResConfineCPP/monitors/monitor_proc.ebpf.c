#include <linux/types.h>
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

struct ProcInfo
{
    __u32 pid;
    __u32 cgid;
};

#ifdef DO_BENCH
// Used for bench
struct
{
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 12); // Adjust RINGBUF_SIZE as needed
} ra_benches_ra SEC(".maps");
#endif

// shared map
struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(key_size, sizeof(__u32));
    __uint(value_size, sizeof(__u32));
    __uint(max_entries, (1 << 12));
} container_table SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 12); // 4096 bytes
} proc_events SEC(".maps");

SEC("tracepoint/sched/sched_process_exec")
int on_process_exec(struct trace_event_raw_sched_process_exec *ctx)
{
#ifdef DO_BENCH
    __u64 start_time = bpf_ktime_get_ns();
#endif
    __u32 monitor_all = 0;
    __u32 value = 1;
    struct ProcInfo info = {};
    info.cgid = bpf_get_current_cgroup_id();

    if (bpf_map_lookup_elem(&container_table, &info.cgid) != NULL)
    // if (bpf_map_lookup_elem(&container_table, &monitor_all) != NULL)
    {
        info.pid = bpf_get_current_pid_tgid() >> 32;
        bpf_ringbuf_output(&proc_events, &info, sizeof(info), 0);
    }
#ifdef DO_BENCH
    __u64 duration = bpf_ktime_get_ns() - start_time;
    bpf_ringbuf_output(&ra_benches_ra, &duration, sizeof(__u64), 0);
#endif
    return 0;
}

SEC("tracepoint/sched/sched_process_exit")
int on_process_exit(struct trace_event_raw_sched_process_exit *ctx)
{
#ifdef DO_BENCH
    __u64 start_time = bpf_ktime_get_ns();
#endif
    __u32 monitor_all = 0;
    struct ProcInfo info = {};
    info.cgid = bpf_get_current_cgroup_id();

    if (bpf_map_lookup_elem(&container_table, &info.cgid) != NULL)
    // if (bpf_map_lookup_elem(&container_table, &monitor_all) != NULL)
    {
        info.pid = (__u32)-(bpf_get_current_pid_tgid() >> 32);
        bpf_ringbuf_output(&proc_events, &info, sizeof(info), 0);
    }

#ifdef DO_BENCH
    __u64 duration = bpf_ktime_get_ns() - start_time;
    bpf_ringbuf_output(&ra_benches_ra, &duration, sizeof(__u64), 0);
#endif
    return 0;
}

char LICENSE[] SEC("license") = "GPL";