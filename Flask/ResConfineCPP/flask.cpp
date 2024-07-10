#include <iostream>
#include <filesystem>
#include <csignal>
#include <thread>

#include "cxxopts.hpp"
#include "resources.hpp"
#include "monitor_proc.hpp"
#include "monitor_ra.hpp"
#include "defences.hpp"
#include "simple_events.hpp"
#include "extract_cgid.hpp"

#ifdef DO_BENCH
#include "mybench.hpp"

std::shared_ptr<MyBench> global_bench = std::make_shared<MyBench>();
#endif

std::shared_ptr<ResourceTree> global_resource_tree = std::make_shared<ResourceTree>();
std::atomic_bool global_stop_work = std::atomic_bool(false);

std::vector<std::thread> threads;

void do_exit()
{
    global_stop_work = true;
    std::cout << std::endl;
    std::cout << "waiting for submonitors to ends..." << std::endl;
    for (auto &thread : threads)
    {
        thread.join();
    }
    std::cout << "all submonitors exit..." << std::endl;

#ifdef DO_BENCH
    std::cout << "print benches..." << std::endl;
    global_bench->print_benches();
#endif

    // global_resource_tree->print_status();
    std::cout << "flask exit..." << std::endl;
}

void sigint_handler(int signum)
{
    std::exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
#ifdef DO_BENCH
    auto init_start = std::chrono::high_resolution_clock::now();
#endif

    // functions for expected or abnormal exit
    std::atexit(do_exit);
    std::signal(SIGINT, sigint_handler);

    try
    {
        cxxopts::Options options("ResourceConfinement", "Abstract resource confinement system");

        options.add_options()(
            "i,container_id", "Container to be restrained", cxxopts::value<std::vector<std::string>>())(
            "g,group_id", "Group to be restrained", cxxopts::value<std::vector<std::string>>())(
            "d,defence_num", "Defence number", cxxopts::value<uint>()->default_value("0"));

        auto result = options.parse(argc, argv);

        std::vector<std::string> container_id;
        std::vector<std::string> group_id;

        if (result.count("container_id"))
        {
            container_id = result["container_id"].as<std::vector<std::string>>();
        }

        if (result.count("group_id"))
        {
            group_id = result["group_id"].as<std::vector<std::string>>();
        }

        uint defence_num = result["defence_num"].as<uint>();

        MonitorProcConfig config;
        config.monitor_all = true;
        if (!container_id.empty())
        {
            config.monitor_all = false;
            for (auto cid : container_id)
            {
                auto id = extract_cgid(cid);
                if (!id.has_value())
                {
                    std::cerr << "Error, fail to extract id for " << cid << std::endl;
                    return EXIT_FAILURE;
                }

                config.cgids.emplace_back(id.value());
            }
        }

        // print configurations
        {
            std::cout << "config to monitor resources: ";
            for (int i = 0; i < defence_num; i++)
            {
                std::cout << enabled_defence[i] << ", ";
            }
            std::cout << defence_num << " in total." << std::endl;

            if (container_id.empty())
            {
                std::cout << "on all processes." << std::endl;
            }
            else
            {
                std::cout << "on container: ";
                std::ranges::copy(container_id, std::ostream_iterator<std::string>(std::cout, ", "));
                std::cout << "." << std::endl;
            }
        }
        
        // start trace procs first
        Event trace_init_event;
        threads.emplace_back(monitor_proc, std::ref(trace_init_event), config);
        trace_init_event.wait();

        // start monitor procs
        std::vector<Event> init_events(defence_num);
        for (int ra_id = 0; ra_id < defence_num; ra_id++)
        {
            // std::cout << "[Debug] creating monitor on " << enabled_defence[ra_id] << std::endl;

            std::string ra_name = enabled_defence[ra_id];
            auto defence = support_defence.at(ra_name);

            MonitorRAConfig config = {
                ra_id,
                ra_name,
                defence.in_point,
                defence.out_point,
                defence.in_count,
                defence.out_count};
            threads.emplace_back(monitor_ra, std::ref(init_events[ra_id]), config);
            init_events[ra_id].wait();
        }

        for (auto &event : init_events)
        {
            event.wait();
        }

        std::cout << "All submonitors init done, start monitoring..." << std::endl;

#ifdef DO_BENCH
        auto init_end = std::chrono::high_resolution_clock::now();
        uint64_t init_duration = (init_end - init_start).count();

        global_bench->bench_init(init_duration);
#endif

        int clocks = 0;
        while (true)
        {
            clocks += 10;
            std::this_thread::sleep_for(std::chrono::seconds(10));
            std::cout << "running " << clocks << "seconds" << std::endl;
        }
    }
    catch (const cxxopts::exceptions::exception &e)
    {
        std::cerr << "Error parsing options: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
