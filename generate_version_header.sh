#!/bin/bash

VERSION_MAJOR=0
VERSION_MINOR=19
VERSION_PATCH=8
VERSION_STAGE="dev"
VERSION_NAME="crazy dog"
VERSION_BUILD=$(date +%Y%m%d)-g$(git rev-parse --short HEAD)


cat <<EOF > "$(dirname "$0")/kernel/kversion.h"
#ifndef VESPERAOS_KVERSION_H
#define VESPERAOS_KVERSION_H

#define VERSION_MAJOR "${VERSION_MAJOR}"
#define VERSION_MINOR "${VERSION_MINOR}"
#define VERSION_PATCH "${VERSION_PATCH}"
#define VERSION_STAGE "${VERSION_STAGE}"
#define VERSION_BUILD "${VERSION_BUILD}"
#define VERSION_NAME "${VERSION_NAME}"

#define VERSION_STRING "Vespera '" VERSION_NAME "' (${VERSION_STAGE}) x86_64 " \\
                       VERSION_MAJOR "." VERSION_MINOR "." VERSION_PATCH \\
                       "-" VERSION_STAGE "+" VERSION_BUILD

inline const char* get_kernel_version() {
    return VERSION_STRING;
};

#endif // VESPERAOS_KVERSION_H
EOF