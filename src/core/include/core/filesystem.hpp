#ifndef STARSIGHT_FILESYSTEM_HPP
#define STARSIGHT_FILESYSTEM_HPP

#include <filesystem>
#include <future>
#include <vector>

namespace std
{
    using fpath = filesystem::path;
}

std::fpath ProjectAbsolutePath(const std::string& RelativePath);

std::vector<uint8_t> ReadFileBinary(std::fpath filepath, bool create = false);
std::future<std::vector<uint8_t>> ReadFileBinaryAsync(std::fpath, bool create = false);
void WriteFileBinary(std::fpath filepath, std::vector<uint8_t>&& data, bool create = false);

#endif //STARSIGHT_FILESYSTEM_HPP
