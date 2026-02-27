#ifndef VESPERAOS_KVERSION_H
#define VESPERAOS_KVERSION_H

#define VERSION_MAJOR "0"
#define VERSION_MINOR "20"
#define VERSION_PATCH "5"
#define VERSION_STAGE "dev"
#define VERSION_BUILD "20260226-gcbac045"
#define VERSION_NAME "holy basil"

#define VERSION_STRING "Vespera '" VERSION_NAME "' (dev) x86_64 " \
                       VERSION_MAJOR "." VERSION_MINOR "." VERSION_PATCH \
                       "-" VERSION_STAGE "+" VERSION_BUILD

inline const char* get_kernel_version() {
    return VERSION_STRING;
};

#endif // VESPERAOS_KVERSION_H
