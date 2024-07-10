#ifndef EXTRACT_CGID_HPP
#define EXTRACT_CGID_HPP

// From https://github.com/iovisor/bcc/blob/master/examples/cgroupid/cgroupid.c as utils
#include <cstdlib>
#include <optional>

// struct file_handle handle;
struct cgid_file_handle
{
    unsigned int handle_bytes;
    int handle_type;
    uint64_t cgid;
};

std::optional<uint64_t> extract_cgid(std::string &container_id);

#endif