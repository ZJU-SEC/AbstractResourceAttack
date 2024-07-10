#include <sys/vfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <memory>
#include <filesystem>
#include <vector>

#include "extract_cgid.hpp"

/* 67e9c74b8a873408c27ac9a8e4c1d1c8d72c93ff (4.5) */
#ifndef CGROUP2_SUPER_MAGIC
#define CGROUP2_SUPER_MAGIC 0x63677270
#endif

std::optional<uint64_t> do_extract(const char *pathname)
{
    struct statfs fs;
    int mount_id;
    uint64_t ret;

    if (statfs(pathname, &fs) != 0)
    {
        std::cerr << "statfs on " << pathname << " failed: " << strerror(errno) << std::endl;
        return std::nullopt;
    }

    if (fs.f_type != CGROUP2_SUPER_MAGIC)
    {
        std::cerr << "File " << pathname << " is not on a cgroup2 mount." << std::endl;
        return std::nullopt;
    }

    auto h = std::make_unique<cgid_file_handle>();
    if (!h)
    {
        std::cerr << "Cannot allocate memory." << std::endl;
        return std::nullopt;
    }

    h->handle_bytes = 8;
    if (name_to_handle_at(AT_FDCWD, pathname, (struct file_handle *)h.get(), &mount_id, 0) != 0)
    {
        std::cerr << "name_to_handle_at failed: " << strerror(errno) << std::endl;
        return std::nullopt;
    }

    if (h->handle_bytes != 8)
    {
        std::cerr << "Unexpected handle size: " << h->handle_bytes << "." << std::endl;
        return std::nullopt;
    }

    ret = h->cgid;
    return ret;
}

std::string dockercg_path = "/sys/fs/cgroup/system.slice/"; // For cgroup v2 and systemd types

std::optional<uint64_t> extract_cgid(std::string &container_id)
{
    std::string prefix = "docker-" + container_id;
    std::vector<std::filesystem::path> matched_paths;

    for (const auto &entry : std::filesystem::directory_iterator(dockercg_path))
    {
        if (entry.is_directory() && entry.path().filename().string().starts_with(prefix))
        {
            matched_paths.push_back(entry.path());
        }
    }

    if (matched_paths.size() != 1)
    {
        std::cerr << "fetch cg_path fails or multiple paths found" << std::endl;
        return std::nullopt;
    }

    return do_extract(matched_paths[0].c_str());
}