#!/bin/bash

VERSION_MAJOR=0
VERSION_MINOR=7
VERSION_PATCH=1
VERSION_STAGE="dev"
VERSION_BUILD=$(date +%Y%m%d)-g$(git rev-parse --short HEAD)


cat <<EOF > kernel/version.h
#ifndef VESPERAOS_VERSION_HPP
#define VESPERAOS_VERSION_HPP

#define VERSION_MAJOR "${VERSION_MAJOR}"
#define VERSION_MINOR "${VERSION_MINOR}"
#define VERSION_PATCH "${VERSION_PATCH}"
#define VERSION_STAGE "${VERSION_STAGE}"
#define VERSION_BUILD "${VERSION_BUILD}"

#define VERSION_STRING "VesperaOS (${VERSION_STAGE}) x86_64 " \\
                       VERSION_MAJOR "." VERSION_MINOR "." VERSION_PATCH \\
                       "-" VERSION_STAGE "+" VERSION_BUILD

inline const char* get_os_version() {
    return VERSION_STRING;
};

#endif // VESPERAOS_VERSION_HPP
EOF