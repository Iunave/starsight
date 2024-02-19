#include "filesystem.hpp"
#include "assertion.hpp"
#include "utility_functions.hpp"
#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef STARSIGHT_DIRECTORY
#error "STARSIGHT_DIRECTORY must be defined to the top level directory"
#endif

std::filesystem::path ProjectAbsolutePath(const std::string& RelativePath)
{
    std::string Result = STARSIGHT_DIRECTORY;

    ASSERT(!Result.empty() && !RelativePath.empty());

    if(Result.ends_with('/'))
    {
        if(RelativePath.starts_with('/'))
        {
            Result.append(RelativePath.begin() + 1, RelativePath.end());
        }
        else
        {
            Result.append(RelativePath.begin(), RelativePath.end());
        }
    }
    else
    {
        if(RelativePath.starts_with('/'))
        {
            Result.append(RelativePath.begin(), RelativePath.end());
        }
        else
        {
            Result.append("/").append(RelativePath.begin(), RelativePath.end());
        }
    }

    return Result;
}

std::vector<uint8_t> ReadFileBinary(std::fpath filepath, bool create)
{
    int fd = create ? open(filepath.c_str(), O_RDONLY | O_CREAT, S_IWUSR | S_IRUSR) : open(filepath.c_str(), O_RDONLY);
    VERIFY(fd != -1, filepath, strerror(errno));

    struct stat64 statbuf;
    VERIFY(fstat64(fd, &statbuf) != -1, filepath, strerror(errno));

    std::vector<uint8_t> file_data(statbuf.st_size);

    size_t nread = read(fd, file_data.data(), file_data.size());
    VERIFY(nread == statbuf.st_size, filepath);

    VERIFY(close(fd) != -1, filepath, strerror(errno));

    return file_data;
}

void WriteFileBinary(std::fpath filepath, std::vector<uint8_t>&& data, bool create)
{
    global::TaskExecutor.silent_async([filepath = std::move(filepath), data = std::move(data), create]()
    {
        int fd = create ? open(filepath.c_str(), O_WRONLY | O_TRUNC | O_CREAT, S_IWUSR | S_IRUSR) : open(filepath.c_str(), O_WRONLY | O_TRUNC);
        VERIFY(fd != -1, filepath, strerror(errno));

        size_t nwrite = write(fd, data.data(), data.size());
        VERIFY(nwrite == data.size());

        VERIFY(close(fd) != -1, filepath, strerror(errno));
    });
}

std::future<std::vector<uint8_t>> ReadFileBinaryAsync(std::fpath filepath, bool create)
{
    return global::TaskExecutor.async([filepath = std::move(filepath), create]() mutable
    {
        return ReadFileBinary(std::move(filepath), create);
    });
}
