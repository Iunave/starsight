#ifndef STARSIGHT_FILESYSTEM_HPP
#define STARSIGHT_FILESYSTEM_HPP

#ifndef STARSIGHT_DIRECTORY
#error "STARSIGHT_DIRECTORY must be defined to the top level directory"
#endif

#include <string>

inline std::string ProjectRelativePath(const std::string& Path)
{
    return std::string{STARSIGHT_DIRECTORY} + "/" + Path;
}

#endif //STARSIGHT_FILESYSTEM_HPP
