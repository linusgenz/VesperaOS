set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SYSROOT ${CMAKE_SOURCE_DIR}/sysroot)

set(COMMON_C_FLAGS
        "-ffreestanding -nostdlib -nostdinc -fno-stack-protector -fPIE -g -O0 -Wall \
     --sysroot=${SYSROOT} \
     -isystem ${SYSROOT}/usr/include"
)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_C_FLAGS}")

set(USER_LD_FLAGS
        "-nostdlib -ffreestanding -fno-stack-protector -pie \
     --sysroot=${SYSROOT} \
     -L${SYSROOT}/usr/lib"
)

add_subdirectory(lib)

function(vespera_bin name)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})

    if (ARG_SOURCES)
        set(sources ${ARG_SOURCES})
    else ()
        set(sources ${CMAKE_SOURCE_DIR}/userspace/bin/${name}.c)
    endif ()

    add_executable(${name} ${sources})

    add_dependencies(${name} sysroot_install)

    target_link_libraries(${name} PRIVATE
            ${SYSROOT}/usr/lib/libvesp.a
    )

    set_target_properties(${name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${SYSROOT}/bin
            LINK_FLAGS "${USER_LD_FLAGS}"
    )
endfunction()

vespera_bin(nox SOURCES ${CMAKE_SOURCE_DIR}/userspace/bin/nox/nox.c ${CMAKE_SOURCE_DIR}/userspace/bin/nox/statusbar.c)
vespera_bin(lsusb)
vespera_bin(logd)
vespera_bin(uptime)
vespera_bin(memstat)
vespera_bin(diskinfo)
vespera_bin(gpuinfo)
vespera_bin(stat)
vespera_bin(vedit)
vespera_bin(pipetest)
vespera_bin(pipespawntest)
vespera_bin(pipewriter)
vespera_bin(pipereader)
vespera_bin(mount)
vespera_bin(umount)
vespera_bin(mv)
vespera_bin(sleep)
vespera_bin(ls)
vespera_bin(cat)
vespera_bin(rm)
vespera_bin(cp)
vespera_bin(mkdir)
vespera_bin(rmdir)
vespera_bin(touch)
vespera_bin(shutdown)
vespera_bin(reboot)
vespera_bin(pwd)
vespera_bin(kill)
vespera_bin(find)
vespera_bin(vesfetch)
vespera_bin(env      SOURCES ${CMAKE_SOURCE_DIR}/userspace/bin/env.c)
vespera_bin(printenv SOURCES ${CMAKE_SOURCE_DIR}/userspace/bin/env.c)
vespera_bin(cal)
vespera_bin(testbin)

add_custom_target(elfs DEPENDS
        nox lsusb logd uptime memstat diskinfo gpuinfo
        stat vedit pipetest pipespawntest pipereader
        pipewriter mount umount mv sleep ls cat rm cp
        mkdir rmdir touch shutdown reboot pwd
        kill env printenv find vesfetch cal testbin
)