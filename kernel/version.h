#ifndef LUMINOS_VERSION_HPP
#define LUMINOS_VERSION_HPP

#define VERSION_MAJOR "0"
#define VERSION_MINOR "2"
#define VERSION_PATCH "0"
#define VERSION_STAGE "dev"
#define VERSION_BUILD "20250720-g7f4024d"

#define VERSION_STRING "LuminOS (dev) x86_64 " \
                       VERSION_MAJOR "." VERSION_MINOR "." VERSION_PATCH \
                       "-" VERSION_STAGE "+" VERSION_BUILD

inline const char* get_os_version() {
    return VERSION_STRING;
};

#endif // LUMINOS_VERSION_HPP
