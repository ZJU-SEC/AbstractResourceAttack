// These codes are for incremental records type.

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/types.h>

#define NULL ((void *)0)
#define BUFFER_SIZE (RA_BUFFER_SIZE_RA)
#define RINGBUF_SIZE (RA_RINGBUF_SIZE_RA)
#define CORE_NUM (RA_CORE_NUM_RA)

// Buffer datas
struct Data
{
    __u32 pid;
    __s32 count;
    __u32 cgid;
};

// Used to output datas
BPF_RINGBUF_OUTPUT(ra_events_ra, RINGBUF_SIZE);
// current buffer states
BPF_PERCPU_ARRAY(ra_states_ra, __u32, 2);
// records counts
BPF_PERCPU_ARRAY(ra_buffer_ra, struct Data, BUFFER_SIZE);
// recourd cpu_id|pid - index, max 64 cores here
BPF_HASH(ra_index_ra, __u32, __u32, BUFFER_SIZE *CORE_NUM);
// recourd container to be monitord
BPF_HASH(ra_cgid_ra, __u32, __u32);
// count tigger numbers.

static int do_count(__u32 pid, __u32 cgid, __s32 count)
{
    __u32 len_key = 0;
    __u32 states_key = 1;

    __u32 initial_value = 0;
    __u32 cpu_id = bpf_get_smp_processor_id();

    __u32 cpid = ((__u32)cpu_id << 24) | (__u32)pid;

    // fetch state
    __u32 *state_ref = ra_states_ra.lookup_or_try_init(&states_key, &initial_value);
    if (state_ref == NULL)
    {
        bpf_trace_printk("[Warn CurrentAR %d] state_ref is null!", cpu_id);
        return -1;
    }

    __u32 cur_state = (*state_ref) + 1;

    // fetch len
    __u32 *len_ref = ra_states_ra.lookup_or_try_init(&len_key, &initial_value);
    if (len_ref == NULL)
    {
        bpf_trace_printk("[Warn CurrentAR %d] len_ref is null!", cpu_id);
        return -1;
    }

    __u32 cur_len = (*len_ref);

    // Updates
    // bpf_trace_printk("[Debug CurrentAR %d] updates, pid: %d", cpu_id, pid);
    __u32 *buffer_index_ref = ra_index_ra.lookup_or_try_init(&cpid, &cur_len);
    if (buffer_index_ref == NULL) // This should not happens
    {
        bpf_trace_printk("[Warn CurrentAR %d] buffer_index_ref is null!", cpu_id);
        return -1;
    }

    struct Data *data_entry = ra_buffer_ra.lookup(buffer_index_ref);
    if (data_entry == NULL) // This should not happens
    {
        bpf_trace_printk("[Warn CurrentAR %d] data_entry is null!", cpu_id);
        return -1;
    }

    if ((*buffer_index_ref) == cur_len)
    {
        data_entry->pid = pid;
        data_entry->count = count;
        data_entry->cgid = cgid;
        cur_len += 1;
        // bpf_trace_printk("[Debug CurrentAR %d] new item, count: %d", cpu_id, data_entry->count);
    }
    else
    {
        data_entry->count += count;
        // bpf_trace_printk("[Debug CurrentAR %d] same item, count: %d", cpu_id, data_entry->count);
    }

    // bpf_trace_printk("[Debug CurrentAR %d] state: %d, len: %d", cpu_id, cur_state, cur_len);

    if (cur_state >= BUFFER_SIZE)
    {
        // struct Profile *profile = ra_events_ra.ringbuf_reserve(sizeof(struct Profile));
        // if (profile == NULL)
        // {
        //     bpf_trace_printk("[Warn CurrentAR %d] profile is null!", cpu_id);
        //     return -1;
        // }

        // bpf_trace_printk("[Debug CurrentAR %d] preparing profiles", cpu_id);
        // We cannot use cur_len here, though we should...
        for (__u32 i = 0; i < BUFFER_SIZE; i++)
        {
            __u32 ii = i;
            struct Data *data_entry = ra_buffer_ra.lookup(&ii);
            if (data_entry == NULL) // This should not happens
            {
                bpf_trace_printk("[Warn CurrentAR %d] data_entry is null!", cpu_id);
                break;
            }
            __u32 cpid = ((__u32)cpu_id << 24) | (__u32)data_entry->pid;

            __u32 *buffer_index_ref = ra_index_ra.lookup(&cpid);
            if (buffer_index_ref == NULL)
            {
                // reach ends
                break;
            }

            struct Data *profile = ra_events_ra.ringbuf_reserve(sizeof(struct Data));
            if (profile == NULL)
            {
                bpf_trace_printk("[Warn CurrentAR %d] profile is null!", cpu_id);
                return -1;
            }

            // items to transfer
            profile->pid = data_entry->pid;
            profile->count = data_entry->count;
            profile->cgid = data_entry->cgid;

            // bpf_trace_printk("[Debug CurrentAR %d] pid: %d, count: %d", cpu_id, data_entry->pid, data_entry->count);

            ra_events_ra.ringbuf_submit(profile, 0);
            // Do clean up
            ra_index_ra.delete(&cpid);
        }

        // profile->size = cur_len;
        // ra_events_ra.ringbuf_submit(profile, 0);

        // Do clean up
        *state_ref = 0;
        *len_ref = 0;
        // bpf_trace_printk("[Debug CurrentAR %d] outputs done with size %d, circle again", cpu_id, cur_len);
    }
    else
    {
        *state_ref = cur_state;
        *len_ref = cur_len;
    }

    return 0;
}

int allocate_monitor(struct pt_regs *ctx)
{
    __u32 pid = bpf_get_current_pid_tgid() >> 32;
    __u32 cgid = bpf_get_current_cgroup_id();
    __s32 count = inc_count;

    if (pid == 0)
    { // We won't count process 0
        return 0;
    }

    __u32 *cgroup_ref = ra_cgid_ra.lookup(&cgid);
    if (cgroup_ref == NULL)
    { // Not what we need
        return 0;
    }

    return do_count(pid, cgid, count);
}

int dealloc_monitor(struct pt_regs *ctx)
{
    __u32 pid = bpf_get_current_pid_tgid() >> 32;
    __u32 cgid = bpf_get_current_cgroup_id();
    __s32 count = dec_count;

    if (pid == 0)
    { // We won't count process 0
        return 0;
    }

    __u32 *cgroup_ref = ra_cgid_ra.lookup(&cgid);
    if (cgroup_ref == NULL)
    { // Not what we need
        return 0;
    }

    return do_count(pid, cgid, count);
}