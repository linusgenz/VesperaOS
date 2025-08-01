#ifndef VESPERAOS_VERSION_HPP
#define VESPERAOS_VERSION_HPP

#define VERSION_MAJOR "0"
#define VERSION_MINOR "3"
#define VERSION_PATCH "1"
#define VERSION_STAGE "dev"
#define VERSION_BUILD "20250801-g3308242"

#define VERSION_STRING "VesperaOS (dev) x86_64 " \
                       VERSION_MAJOR "." VERSION_MINOR "." VERSION_PATCH \
                       "-" VERSION_STAGE "+" VERSION_BUILD

inline const char* get_os_version() {
    return VERSION_STRING;
};

#endif // VESPERAOS_VERSION_HPP
