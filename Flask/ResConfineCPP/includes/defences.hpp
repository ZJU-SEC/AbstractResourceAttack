#ifndef DEFENCES_HPP
#define DEFENCES_HPP

#include <unordered_map>
#include <string>
#include <vector>
#include <tuple>

#define TOTAL_DEFENCE_NUM 40

struct DefenceData
{
    std::string in_point;
    std::string out_point;
    std::string count_type;
    std::string in_count;
    std::string out_count;
};

// support_defence
const std::unordered_map<std::string, DefenceData> support_defence = {
    {"pid", {"alloc_pid", "free_pid", "increment", "1", "-1"}},
    {"file", {"alloc_empty_file", "__fput", "increment", "1", "-1"}},
    {"pty", {"devpts_new_index", "devpts_kill_index", "increment", "1", "-1"}},
    {"mdr", {"account_page_dirtied", "", "increment", "1", "-1"}},
    {"inode", {"alloc_inode", "__destroy_inode", "increment", "1", "-1"}},
    {"entropy", {"extract_entropy.constprop.0", "", "increment", "1", "-1"}},
    {"connect", {"__nf_conntrack_alloc", "__nf_conntrack_free", "increment", "1", "-1"}},
    {"nr_threads", {"copy_process", "", "increment", "1", "-1"}},
    {"work_pool", {"create_worker", "", "increment", "1", "-1"}},
    {"ucount", {"alloc_ucounts", "", "increment", "1", "-1"}},
    {"super", {"get_super", "", "increment", "1", "-1"}},
    {"proc_inode", {"proc_alloc_inum", "", "increment", "1", "-1"}},
    {"alarm", {"alarm_init", "", "increment", "1", "-1"}},
    {"irq_desc", {"alloc_desc", "", "increment", "1", "-1"}},
    {"pipe_info", {"alloc_pipe_info", "", "increment", "1", "-1"}},
    {"disk_event", {"disk_alloc_events", "", "increment", "1", "-1"}},
    {"shmem", {"shmem_get_inode", "", "increment", "1", "-1"}},
    {"swap_pages", {"get_swap_pages", "", "increment", "1", "-1"}},
    {"shg", {"newseg", "", "increment", "1", "-1"}},
    {"mempool", {"mempool_create", "", "increment", "1", "-1"}},
    {"aio_nr", {"ioctx_alloc", "", "increment", "1", "-1"}},
    {"mbcache", {"mb_cache_create", "", "increment", "1", "-1"}},
    {"iocq", {"ioc_create_icq", "", "increment", "1", "-1"}},
    {"irq", {"request_threaded_irq", "", "increment", "1", "-1"}},
    {"bdev", {"bdev_alloc", "", "increment", "1", "-1"}},
    {"kioctx", {"ioctx_alloc", "", "increment", "1", "-1"}},
    {"dcache", {"__d_alloc", "", "increment", "1", "-1"}},
    {"percpu", {"__alloc_percpu", "", "increment", "1", "-1"}},
    {"icmp", {"__icmp_send", "", "increment", "1", "-1"}},
    {"msg", {"newque", "", "increment", "1", "-1"}},
    {"sema_set", {"newary", "", "increment", "1", "-1"}},
    {"bioset", {"bioset_init", "", "increment", "1", "-1"}},
    {"mbcache_entry", {"mb_cache_entry_create", "", "increment", "1", "-1"}},
    {"jbd2_buffer", {"jbd2_journal_get_descriptor_buffer", "", "increment", "1", "-1"}},
    {"ipc_ids", {"ipc_init_ids", "", "increment", "1", "-1"}},
    {"dma_pool", {"dma_pool_create", "", "increment", "1", "-1"}},
    {"sock", {"sk_alloc", "", "increment", "1", "-1"}},
    {"icmp6", {"icmp6_send", "", "increment", "1", "-1"}},
    {"tty_buffer", {"tty_buffer_alloc", "", "increment", "1", "-1"}},
    {"inet_bind_bucket", {"inet_bind_bucket_create", "", "increment", "1", "-1"}},
};

// enabled_defence
const std::vector<std::string> enabled_defence = {
    "pid",
    "pty",
    "kioctx",
    "entropy",
    "connect",
    "nr_threads",
    "work_pool",
    "inode",
    "super",
    "proc_inode",
    
    "alarm",
    "irq_desc",
    "pipe_info",
    "disk_event",
    "file",
    "shmem",
    "swap_pages",
    "shg",
    "mempool",
    "aio_nr",

    "mbcache",
    "iocq",
    "irq",
    "bdev",
    "mdr",
    "dcache",
    "percpu",
    "icmp",
    "msg",
    "sema_set",
    
    "bioset",
    "mbcache_entry",
    "jbd2_buffer",
    "ipc_ids",
    "dma_pool",
    "ucount",
    "sock",
    "inet_bind_bucket",
    "icmp6",
    "tty_buffer",
};

#endif