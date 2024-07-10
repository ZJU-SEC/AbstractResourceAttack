// These codes are for incremental records type.
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

#define NULL ((void *)0)
#define BUFFER_SIZE (10)
#define RINGBUF_SIZE (4)
#define CORE_NUM (16)

// Buffer datas
struct Data
{
    __u32 pid;
    __s32 count;
    __u32 cgid;
};

struct Profile
{
    struct Data data[BUFFER_SIZE];
};

#ifdef DO_BENCH
// Used for bench
struct
{
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 12);
} ra_benches_ra SEC(".maps");
#endif

// shared map
struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(key_size, sizeof(__u32));
    __uint(value_size, sizeof(__u32));
    __uint(max_entries, (1 << 12));
    __uint(pinning, LIBBPF_PIN_BY_NAME);
} container_table SEC(".maps");

// Used to output datas
struct
{
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, (1 << 12) * RINGBUF_SIZE); // Adjust RINGBUF_SIZE as needed
} ra_events_ra SEC(".maps");

// current buffer states
struct
{
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(key_size, sizeof(__u32));
    __uint(value_size, sizeof(__u32));
    __uint(max_entries, 2);
} ra_states_ra SEC(".maps");

// records counts
struct
{
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(key_size, sizeof(__u32));
    __uint(value_size, sizeof(struct Data));
    __uint(max_entries, BUFFER_SIZE);
} ra_buffer_ra SEC(".maps");

// recourd cpu_id|pid
struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(key_size, sizeof(__u32));
    __uint(value_size, sizeof(__u32));
    __uint(max_entries, BUFFER_SIZE * CORE_NUM);
} ra_index_ra SEC(".maps");

static int do_count(__u32 pid, __u32 cgid, __s32 count)
{
    __u32 len_key = 0;
    __u32 states_key = 1;

    __u32 cpu_id = bpf_get_smp_processor_id();
    __u32 cpid = ((__u32)cpu_id << 24) | (__u32)pid;

    // fetch state
    __u32 *state_ref = bpf_map_lookup_elem(&ra_states_ra, &states_key);
    if (state_ref == NULL)
    {
        return -1;
    }

    __u32 cur_state = (*state_ref) + 1;

    // fetch len
    __u32 *len_ref = bpf_map_lookup_elem(&ra_states_ra, &len_key);
    if (len_ref == NULL)
    {
        return -1;
    }

    __u32 cur_len = (*len_ref);

    // Updates
    __u32 *buffer_index_ref = bpf_map_lookup_elem(&ra_index_ra, &cpid);
    if (buffer_index_ref == NULL)
    {
        if (bpf_map_update_elem(&ra_index_ra, &cpid, &cur_len, BPF_NOEXIST) == 0)
        {
            buffer_index_ref = &cur_len;
        }
        else
        {
            return -1;
        }
    }

    struct Data *data_entry = bpf_map_lookup_elem(&ra_buffer_ra, buffer_index_ref);
    if (data_entry == NULL) // This should not happens
    {
        return -1;
    }

    if ((*buffer_index_ref) == cur_len)
    {
        data_entry->pid = pid;
        data_entry->count = count;
        data_entry->cgid = cgid;
        cur_len += 1;
    }
    else
    {
        data_entry->count += count;
    }

    if (cur_state >= BUFFER_SIZE)
    {
        struct Profile *profile = bpf_ringbuf_reserve(&ra_events_ra, sizeof(struct Profile), 0);
        if (profile == NULL)
        {
            return -1;
        }

        // We cannot use cur_len here, though we should...
        for (__u32 i = 0; i < BUFFER_SIZE; i++)
        {
            __u32 ii = i;
            struct Data *data_entry = bpf_map_lookup_elem(&ra_buffer_ra, &ii);
            if (data_entry == NULL) // This should not happens
            {
                break;
            }

            __u32 cpid = ((__u32)cpu_id << 24) | (__u32)data_entry->pid;
            __u32 *buffer_index_ref = bpf_map_lookup_elem(&ra_index_ra, &cpid);
            if (buffer_index_ref == NULL)
            {
                // reach ends
                break;
            }

            // items to transfer
            profile->data[i].pid = data_entry->pid;
            profile->data[i].count = data_entry->count;
            profile->data[i].cgid = data_entry->cgid;

            // Do clean up
            bpf_map_delete_elem(&ra_index_ra, &cpid);
        }

        bpf_ringbuf_submit(profile, 0);

        // Do clean up
        *state_ref = 0;
        *len_ref = 0;
    }
    else
    {
        *state_ref = cur_state;
        *len_ref = cur_len;
    }

    return 0;
}

SEC("kprobe/generic_kprobe_handler")
int allocate_monitor(struct pt_regs *ctx)
{
#ifdef DO_BENCH
    __u64 start_time = bpf_ktime_get_ns();
#endif
    __u32 monitor_all = 0;
    __u32 cgid = bpf_get_current_cgroup_id();
    int res = 0;

    if (bpf_map_lookup_elem(&container_table, &cgid) != NULL)
    // if (bpf_map_lookup_elem(&container_table, &monitor_all) != NULL)
    {
        __u32 pid = bpf_get_current_pid_tgid() >> 32;
        res = do_count(pid, cgid, 1);
    }

#ifdef DO_BENCH
    __u64 duration = bpf_ktime_get_ns() - start_time;
    bpf_ringbuf_output(&ra_benches_ra, &duration, sizeof(__u64), 0);
#endif

    return res;
}

char LICENSE[] SEC("license") = "GPL";