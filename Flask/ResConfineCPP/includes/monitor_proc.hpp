#ifndef MONITOR_PROC_HPP
#define MONITOR_PROC_HPP

#include <iostream>
#include "simple_events.hpp"


struct MonitorProcConfig
{
    bool monitor_all;
    std::vector<uint32_t> pids;
    std::vector<uint32_t> cgids;
    std::vector<uint32_t> grids;
    // TODO: we haven't do group accounting currently.
};

int monitor_proc(Event &init_event, MonitorProcConfig config);

#endif