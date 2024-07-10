#include <iostream>
#include <bpf/libbpf.h>

#include "resources.hpp"
#include "monitor_ra.hpp"

static int handle_event(void *ctx, void *data, size_t data_sz);
thread_local int ra_id = -1;

#ifdef DO_BENCH
#include "mybench.hpp"

static int handle_bench(void *ctx, void *data, size_t data_sz)
{
    auto event = static_cast<uint64_t *>(data);
    global_bench->bench_trace_ra_ebpf(ra_id, *event);

    return 0;
}
#endif

int monitor_ra(Event &init_event, MonitorRAConfig config)
{
    // Create monitor for resource %s, start initing..." % config.ra_name
    std::cout << "Create monitor for resource " << config.ra_name << ", start initing..." << std::endl;

    ra_id = config.ra_id;

    struct bpf_object *obj = bpf_object__open("../monitors/monitor_ra.ebpf.o");
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

    struct bpf_program *prog = bpf_object__find_program_by_name(obj, "allocate_monitor");
    if (!prog)
    {
        std::cerr << "Failed to find BPF program" << std::endl;
        bpf_object__close(obj);
        return 1;
    }

    struct ring_buffer *rb = ring_buffer__new(bpf_object__find_map_fd_by_name(obj, "ra_events_ra"), handle_event, NULL, NULL);
    if (!rb)
    {
        std::cerr << "Failed to open ring buffer" << std::endl;
        bpf_object__close(obj);
        return 1;
    }

    struct bpf_link *link = bpf_program__attach_kprobe(prog, false, config.in_point.c_str());
    if (libbpf_get_error(link))
    {
        std::cerr << "Failed to attach kprobe" << std::endl;
        ring_buffer__free(rb);
        bpf_object__close(obj);
        return 1;
    }

    init_event.set();

#ifdef DO_BENCH
    std::thread t([&]
                  {
        ra_id = config.ra_id;
        struct ring_buffer *rbx = ring_buffer__new(bpf_object__find_map_fd_by_name(obj, "ra_benches_ra"), handle_bench, NULL, NULL);
        while (!global_stop_work) {
            ring_buffer__poll(rbx, 100);
        }
        ring_buffer__free(rbx); });
#endif

    while (!global_stop_work)
    {
        ring_buffer__poll(rb, 100);
    }

    ring_buffer__free(rb);
    bpf_link__destroy(link);
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

    auto event = static_cast<Profile *>(data);
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        if (event->data[i].pid == 0)
        {
            break;
        }
        global_resource_tree->monitors_accounting(ra_id, event->data[i].pid, event->data[i].cgid, event->data[i].count);
    }
    // std::cout << "[Debug] Consume datas: " << event->pid << ", " << event->cgid << ", " << event->count << std::endl;

#ifdef DO_BENCH
    auto end = std::chrono::high_resolution_clock::now();
    uint64_t duration = (end - start).count();

    global_bench->bench_trace_ra_user(ra_id, duration);
#endif
    return 0;
}