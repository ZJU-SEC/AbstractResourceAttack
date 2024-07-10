#!/usr/bin/python3 -u
import threading
import multiprocessing
import time
import argparse
import os
import atexit
import signal
from monitor import monitor, MoniterConfig, MonitorOP
from utils.extract_cgid import extract_cgid

container_status: dict[int, list[int]] = dict()
container_cgid: dict[str, int] = dict()

subprocesses: list[multiprocessing.Process] = []

# Temp usage, (in_point, out_point, count_type, in_count, out_count, ceiling)
support_defence = {
    "pid": ("alloc_pid", "free_pid", "increment", "1", "-1"),
    "file": ("alloc_empty_file", "__fput", "increment", "1", "-1"),
    "pty": ("devpts_new_index", "devpts_kill_index", "increment", "1", "-1"),
    "mdr": ("account_page_dirtied", "", "increment", "1", "-1"),
    "inode": ("alloc_inode", "__destroy_inode", "increment", "1", "-1"),
    "entropy": ("extract_entropy.constprop.0", "", "increment", "1", "-1"),
    # "entropy": (
    #     "extract_entropy.constprop.0",
    #     "",
    #     "absolute",
    #     "(unsigned long) PT_REGS_PARM2(ctx)",
    #     "(unsigned long) PT_REGS_PARM3(ctx)",
    # ),
    "connect": (
        "nf_conntrack_alloc",
        "nf_conntrack_free",
        "increment",
        "1",
        "-1",
    ),
    # More unchecked ARs
    "nr_threads": ("copy_process", "", "increment", "1", "-1"),
    "work_pool": ("create_worker", "", "increment", "1", "-1"),
    "ucount": ("get_ucounts", "", "increment", "1", "-1"),
    
    "super": ("get_super", "", "increment", "1", "-1"),
    "proc_inode": ("proc_alloc_inum", "", "increment", "1", "-1"),
    "alarm": ("alarm_init", "", "increment", "1", "-1"),
    "irq_desc": ("alloc_desc", "", "increment", "1", "-1"),
    "pipe": ("alloc_pipe_info", "", "increment", "1", "-1"),
    "disk_event": ("disk_alloc_events", "", "increment", "1", "-1"),
    "shmem": ("shmem_get_inode", "", "increment", "1", "-1"),
    "swap_pages": ("get_swap_pages", "", "increment", "1", "-1"),
    "shg": ("newseg", "", "increment", "1", "-1"),
    "mempool": ("mempool_create", "", "increment", "1", "-1"),
    
    "aio_nr": ("ioctx_alloc", "", "increment", "1", "-1"),
    "mbcache": ("mb_cache_create", "", "increment", "1", "-1"),
    "iocq": ("ioc_create_icq", "", "increment", "1", "-1"),
    "irq": ("request_threaded_irq", "", "increment", "1", "-1"),
    "bdev": ("bdev_alloc", "", "increment", "1", "-1"),
    "kioctx": ("ioctx_alloc", "", "increment", "1", "-1"),
    "dcache": ("__d_alloc", "", "increment", "1", "-1"),
    "percpu": ("__alloc_percpu", "", "increment", "1", "-1"),
    "icmp": ("__icmp_send", "", "increment", "1", "-1"),
    "msg": ("newque", "", "increment", "1", "-1"),
    
    "sema_set": ("newary", "", "increment", "1", "-1"),
    "bioset": ("bioset_init", "", "increment", "1", "-1"),
    "mbcache_entry": ("mb_cache_entry_create", "", "increment", "1", "-1"),
    "jbd2_buffer": ("jbd2_journal_get_descriptor_buffer", "", "increment", "1", "-1"),
    "ipc_ids": ("ipc_init_ids", "", "increment", "1", "-1"),
    "dma_pool": ("dma_pool_create", "", "increment", "1", "-1"),
    "sock": ("sk_alloc", "", "increment", "1", "-1"),
    "inet_bind_bucket": ("inet_bind_bucket_create", "", "increment", "1", "-1"),
    "icmp6": ("icmp6_send", "", "increment", "1", "-1"),
    "tty_buff": ("tty_buffer_alloc", "", "increment", "1", "-1"),
    
    "buffer": ("create_empty_buffers", "", "increment", "1", "-1"),
    "skb": ("__alloc_skb", "", "increment", "1", "-1"),
}

# enabled_defence = ["percpu"]
enabled_defence = [
    "pid",          # 3400
    "pty",
    "kioctx",
    "entropy",
    "connect",      # 2400
    "nr_threads",
    "work_pool",
    "inode",        # 65800
    "super",
    "proc_inode",
    # 10
    "alarm",
    "irq_desc",
    "pipe",         # 4500
    "disk_event",
    "file",         # 334500
    "shmem",
    "swap_pages",
    "shg",
    "mempool",
    "aio_nr",
    # 20
    "mbcache",
    "iocq",
    "irq",
    "bdev",
    "mdr",          # 326300
    "dcache",       # 66300
    "percpu",
    "icmp",
    "msg",
    "sema_set",
    # 30
    "bioset",
    "mbcache_entry",
    "jbd2_buffer",
    "ipc_ids",
    "ucount",       # 9300
    "dma_pool",
    "sock",         # 2400
    "buffer",       # 319600
    "icmp6",
    "tty_buff",
]

# enabled_num = 40

test_bool = False

def main():
    atexit.register(do_exit)
    signal.signal(signal.SIGINT, sigint_handler)
    
    parser = argparse.ArgumentParser(description="Abstract resource confinement system")

    parser.add_argument(
        "--container_id",
        "-i",
        type=str,
        help="container to be restrained, if set '0', then for develop test usage",
        required=True,
    )
    parser.add_argument(
        "--defence_num",
        "-d",
        type=int,
        required=True,
    )
    # parser.add_argument('--ceiling', '-c', type=str, help='container resource ceiling', required=True)
    args = parser.parse_args()
    container_id: str = args.container_id[:12]
    defence_num: int = args.defence_num

    print("Monitor containers: %s" % container_id)

    global enabled_defence
    enabled_defence = enabled_defence[0:defence_num]
    # enabled_defence = random.sample(list(support_defence.keys()), enabled_num)

    # Process container_id to cid
    if container_id != "0":
        dockercg_path = (
            "/sys/fs/cgroup/system.slice/"  # For cgroup v2 and systemd types
        )
        all_items = os.listdir(dockercg_path)
        prefix = "docker-" + container_id
        cg_path = [item for item in all_items if item.startswith(prefix)]
        if len(cg_path) != 1:
            print("Error, fetch cg_path fails")
            exit(-1)
        cgid = extract_cgid(dockercg_path + cg_path[0])
        if cgid > 0:
            print("Fetch cgid: %d" % cgid)
        else:
            print("Error, fetch cgid fails")
            exit(-1)
    else:
        print("Flask on test mode...")
        cgid = 0
        global test_bool
        test_bool = True

    container_cgid[container_id] = cgid
    container_status[cgid] = [0 for _ in range(defence_num)]

    init_events = [multiprocessing.Event() for _ in range(defence_num)]

    # Ready to start monitors
    for ra_id in range(defence_num):
        threading.Thread(
            target=start_monitor,
            args=(init_events[ra_id], [cgid], ra_id),
            daemon=True,
        ).start()

    for e in init_events:
        e.wait()

    print("All submonitors init done, start monitoring")
    print(enabled_defence)

    clocks = 0
    while True:
        clocks += 1
        time.sleep(10)
        if test_bool:
            print(container_status)
        else:
            print("running %d" % clocks)


def start_monitor(init_event, cids: set[int], ra_id: int):
    ra_name = enabled_defence[ra_id]
    in_point = support_defence[ra_name][0]
    out_point = support_defence[ra_name][1]
    count_type = support_defence[ra_name][2]
    in_count = support_defence[ra_name][3]
    out_count = support_defence[ra_name][4]

    queue = multiprocessing.Queue()
    config = MoniterConfig(
        cids, 0, ra_id, ra_name, in_point, out_point, count_type, in_count, out_count
    )

    p = multiprocessing.Process(target=monitor, args=(init_event, queue, config))
    subprocesses.append(p)
    p.start()

    if test_bool:
        while True:
            op: MonitorOP = queue.get()
            # print("[Debug] get op: ", op)
            # Normal data updates
            if op.msg == None:
                container_status[op.cgid][ra_id] = op.status
            else:
                # TODO
                pass


def do_exit():
    print("flask ended, cleaning...")
    for sp in subprocesses:
        print("ending %d" % sp.pid)
        sp.terminate()
    print("flask exit")
    
def sigint_handler(sig, frame):
    exit() # this will trigger do_exit()

if __name__ == "__main__":
    main()
