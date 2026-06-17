# cmake/KernelTargetDefaults.cmake
#
# Shared utilities used by every kernel-side module (arch, drivers,
# filesystem, kernel).  Include once from the root CMakeLists.txt
# BEFORE any add_subdirectory() call so the functions are in scope
# everywhere.
# ─────────────────────────────────────────────────────────────────

# ── add_debug_option ─────────────────────────────────────────────
# Declares a CMake option and immediately propagates it as a
# compile definition (NAME=1 / NAME=0) to every target in scope.
#
# Usage:
#   add_debug_option(ENABLE_FOO "Enable foo subsystem" ON)
function(add_debug_option name description default_value)
    option(${name} "${description}" ${default_value})

    if(${name})
        add_compile_definitions(${name}=1)
        set(status "ENABLED")
    else()
        add_compile_definitions(${name}=0)
        set(status "DISABLED")
    endif()

    message(STATUS "  ${name}: ${status}")
endfunction()


# ── collect_cpp ──────────────────────────────────────────────────
# Recursively collects all .cpp / .cc files under every directory
# listed in ARGN and stores the result in OUT_VAR.
# Files generated inside CMAKE_BINARY_DIR are excluded automatically.
#
# Usage:
#   collect_cpp(MY_SOURCES ${CMAKE_SOURCE_DIR}/kernel ${CMAKE_SOURCE_DIR}/lib)
function(collect_cpp OUT_VAR)
    set(_files "")
    foreach(dir ${ARGN})
        file(GLOB_RECURSE _dir_files "${dir}/*.cpp" "${dir}/*.cc")
        list(APPEND _files ${_dir_files})
    endforeach()
    list(FILTER _files EXCLUDE REGEX "${CMAKE_BINARY_DIR}/.*")
    set(${OUT_VAR} ${_files} PARENT_SCOPE)
endfunction()


# ── collect_asm ──────────────────────────────────────────────────
# Same as collect_cpp but for NASM .asm files.
function(collect_asm OUT_VAR)
    set(_files "")
    foreach(dir ${ARGN})
        file(GLOB_RECURSE _dir_files "${dir}/*.asm")
        list(APPEND _files ${_dir_files})
    endforeach()
    list(FILTER _files EXCLUDE REGEX "${CMAKE_BINARY_DIR}/.*")
    set(${OUT_VAR} ${_files} PARENT_SCOPE)
endfunction()


# ── apply_kernel_compile_options ─────────────────────────────────
# Applies the standard freestanding / no-stdlib compile flags to
# TARGET.  Call this after add_library() / add_executable().
#
# Usage:
#   apply_kernel_compile_options(my_lib)
function(apply_kernel_compile_options target)
    target_compile_options(${target} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-ffreestanding>
            $<$<COMPILE_LANGUAGE:CXX>:-fno-stack-protector>
            $<$<COMPILE_LANGUAGE:CXX>:-fno-pic>
            $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
            $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
            $<$<COMPILE_LANGUAGE:CXX>:-mno-red-zone>
            $<$<COMPILE_LANGUAGE:CXX>:-mcmodel=kernel>
            $<$<COMPILE_LANGUAGE:CXX>:-O2>
            $<$<COMPILE_LANGUAGE:CXX>:-Wall>
            $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
    )
endfunction()

# ── kernel_module_includes ───────────────────────────────────────
# Sets the canonical include-directory rules for a kernel module:
#
#   PRIVATE  → only the module's own source tree
#   PUBLIC   → the shared public include/ root
#
# This is the key function that enforces the public/private
# boundary: callers of the resulting OBJECT library only see
# include/, never the module's internal source dir.
#
# Usage:
#   kernel_module_includes(arch_lib ${CMAKE_SOURCE_DIR}/arch)
function(kernel_module_includes target module_src_dir)
    target_include_directories(${target}
            PRIVATE
            ${module_src_dir}               # internal headers – never exported
            ${CMAKE_BINARY_DIR}             # generated headers (e.g. ap_trampoline_blob.h)
            PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/../include     # cross-module public API only
            ${CMAKE_CURRENT_SOURCE_DIR}/../limine      # limine protocol headers
    )
endfunction()