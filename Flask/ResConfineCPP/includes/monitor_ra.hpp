#ifndef MONITOR_RA_HPP
#define MONITOR_RA_HPP

#include <iostream>
#include "simple_events.hpp"

#define BUFFER_SIZE (10)

struct Data
{
    uint32_t pid;
    int32_t count;
    uint32_t cgid;
};

struct Profile
{
    struct Data data[BUFFER_SIZE];
};

struct MonitorRAConfig
{
    int32_t ra_id;
    std::string ra_name;
    std::string in_point;
    std::string out_point;
    std::string in_count;
    std::string out_count;
};

int monitor_ra(Event &event, MonitorRAConfig config);

#endif