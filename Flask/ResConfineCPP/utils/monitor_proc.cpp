#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <filesystem>
#include <cstring>
#include <bpf/libbpf.h>

#include "monitor_proc.hpp"
#include "resources.hpp"

static int handle_event(void *ctx, void *data, size_t data_sz);
const char *pin_path = "/sys/fs/bpf/container_table";

#ifdef DO_BENCH
#include "mybench.hpp"

static int handle_bench(void *ctx, void *data, size_t data_sz)
{
    auto event = static_cast<uint64_t *>(data);
    global_bench->bench_trace_proc_ebpf(*event);

    return 0;
}
#endif

struct ProcInfo
{
    int32_t pid;
    uint32_t cgid;
};

// monitor process creation (exec) and exits, return 1 if an error occurred
int monitor_proc(Event &init_event, MonitorProcConfig config)
{
    struct bpf_object *obj = bpf_object__open("../monitors/monitor_proc.ebpf.o");
    if (libbpf_get_error(obj))
    {
        std::cerr << "Failed to open BPF object" << std::endl;
        return 1;
    }

    if (bpf_object__load(obj))
    {
        std::cerr << "Failed to load BPF object" << std::endl;
        return 1;
    }

    struct bpf_program *prog_exec = bpf_object__find_program_by_name(obj, "on_process_exec");
    struct bpf_program *prog_exit = bpf_object__find_program_by_name(obj, "on_process_exit");
    if (!prog_exec || !prog_exec)
    {
        std::cerr << "Failed to find BPF program" << std::endl;
        bpf_object__close(obj);
        return 1;
    }

    struct ring_buffer *rb = ring_buffer__new(bpf_object__find_map_fd_by_name(obj, "proc_events"), handle_event, NULL, NULL);
    if (!rb)
    {
        std::cerr << "Failed to open ring buffer" << std::endl;
        bpf_object__close(obj);
        return 1;
    }

    // Do configs
    struct bpf_map *container_table = bpf_object__find_map_by_name(obj, "container_table");
    __u32 init_val = 1;
    if (config.monitor_all)
    {
        __u32 monitor_all = 0;
        bpf_map__update_elem(container_table, &monitor_all, sizeof(__u32), &init_val, sizeof(__u32), BPF_ANY);
    }
    else
    {
        for (auto cid : config.cgids)
        {
            bpf_map__update_elem(container_table, &cid, sizeof(__u32), &init_val, sizeof(__u32), BPF_ANY);
        }
    }

    struct bpf_map *shared_map = bpf_object__find_map_by_name(obj, "container_table");
    bpf_map__unpin(shared_map, pin_path);
    if (bpf_map__pin(shared_map, pin_path) != 0)
    {
        std::cerr << "Failed to pin process table: " << std::strerror(errno) << std::endl;
        bpf_object__close(obj);
        return 1;
    }

    struct bpf_link *link_exec = bpf_program__attach(prog_exec);
    if (libbpf_get_error(link_exec))
    {
        std::cerr << "Failed to attach kprobe" << std::endl;
        ring_buffer__free(rb);
        bpf_object__close(obj);
        return 1;
    }

    struct bpf_link *link_exit = bpf_program__attach(prog_exit);
    if (libbpf_get_error(link_exit))
    {
        std::cerr << "Failed to attach kprobe" << std::endl;
        ring_buffer__free(rb);
        bpf_object__close(obj);
        bpf_link__destroy(link_exec);
        return 1;
    }

    init_event.set();

#ifdef DO_BENCH
    std::thread t([&]
                  {
        struct ring_buffer *rbx = ring_buffer__new(bpf_object__find_map_fd_by_name(obj, "ra_benches_ra"), handle_bench, NULL, NULL);
        while (!global_stop_work) {
            ring_buffer__poll(rbx, 50);
        }
        ring_buffer__free(rbx); });
#endif

    while (!global_stop_work)
    {
        ring_buffer__poll(rb, 100);
    }

    ring_buffer__free(rb);
    bpf_link__destroy(link_exec);
    bpf_link__destroy(link_exit);
    bpf_object__close(obj);

#ifdef DO_BENCH
    t.join();
#endif
    return 0;
}

int handle_event(void *ctx, void *data, size_t data_sz)
{
#ifdef DO_BENCH
    auto start = std::chrono::high_resolution_clock::now();
#endif

    auto event = static_cast<ProcInfo *>(data);
    if (event->pid > 0)
    { // create
        global_resource_tree->monitor_proc_new(event->pid, event->cgid, 0);
    }
    else
    { // exit
        global_resource_tree->monitor_proc_end(-event->pid, event->cgid, 0);
    }

#ifdef DO_BENCH
    auto end = std::chrono::high_resolution_clock::now();
    uint64_t duration = (end - start).count();
    // global_bench->trace_proc(abs(event->pid));
    global_bench->bench_trace_proc_user(duration);
#endif

    return 0;
}

// // for test
// std::shared_ptr<ResourceTree> global_resource_tree = std::make_shared<ResourceTree>();
// std::atomic_bool global_stop_work = std::atomic_bool(false);

// #include <thread>
// int main()
// {
//     global_resource_tree->account_all(true);
//     Event e;
//     std::thread t(trace_proc, std::ref(e));
//     t.join();
//     return 0;
// }
