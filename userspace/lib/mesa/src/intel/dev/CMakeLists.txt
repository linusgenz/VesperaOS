# ════════════════════════════════════════════════════════════════
# lib/mesa/src/intel/dev/CMakeLists.txt
#
# Modeled on upstream src/intel/dev/meson.build, BUT: i915/ and xe/
# device-info sources are DELIBERATELY EXCLUDED from this build,
# unlike an earlier version of this file that compiled both in as
# dead code. VesperaOS has exactly one real KMD type (Lucifer,
# lib/mesa/src/intel/dev/lucifer/) — intel_kmd.c's
# intel_get_kmd_type() unconditionally returns INTEL_KMD_TYPE_LUCIFER,
# so the i915/xe dispatch branches in intel_device_info.c are
# unreachable regardless of whether those two files are compiled in.
# Removing them outright avoids dragging in drm-uapi/i915_drm.h and
# drm-uapi/xe_drm.h (and their BSD/Linux-specific #ifdef assumptions
# — see the sys/ioccom.h compatibility shim this project needed
# along the way) for code that can never run.
#
# NOT ported: virtio/ sources (with_intel_virtio gate — no virtio-GPU
# passthrough backend exists or is planned here), with_tests block
# (intel_device_info_test.c, drm-shim-based override test — no gtest
# infra, consistent with every other layer so far).
#
# libdrm status: NOT NEEDED on this layer at all anymore.
# intel_kmd.c's intel_get_kmd_type() returns INTEL_KMD_TYPE_LUCIFER
# directly, no drmGetVersion() call, no util/libdrm.h include. The
# "detect no GPU present, fall back to llvmpipe software rendering"
# case is intentionally NOT handled here — that's a higher-level EGL
# init decision (whether to load iris at all), planned but deferred
# until iris/Mesa itself works end-to-end. See [[vespera-mesa-port]].
# The LIBDRM SWITCH block below is kept only in case some OTHER file
# under src/intel/ still #includes "util/libdrm.h" for unrelated
# reasons — check before removing outright.
# ════════════════════════════════════════════════════════════════

set(INTEL_DEV_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(INTEL_DEV_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})

# ────────────────────────────────────────────────────────────────
# LIBDRM SWITCH
#
# Currently: Mesa's own no-op shim (src/util/libdrm.h) is active —
# HAVE_LIBDRM is NOT defined below, so every drm*() call in
# intel_kmd.c / i915/intel_device_info.c / xe/intel_device_info.c
# resolves to the stub versions (return NULL / -ENOENT / -EINVAL).
# This compiles and links successfully; intel_get_kmd_type() will
# resolve to INTEL_KMD_TYPE_INVALID at runtime until real DRM/RCS
# communication exists.
#
# TO SWITCH TO A REAL libdrm IMPLEMENTATION LATER: delete this
# comment block's "CURRENT (shim)" section below and uncomment the
# "FUTURE (real libdrm)" section right after it. That's the only
# change needed in this file — no other target/include logic here
# depends on which one is active.
# ────────────────────────────────────────────────────────────────

# --- CURRENT (shim): nothing to set, HAVE_LIBDRM stays undefined ---
set(INTEL_DEV_LIBDRM_INCLUDE "")
set(INTEL_DEV_LIBDRM_LINK "")
set(INTEL_DEV_LIBDRM_DEFINITIONS "")

# --- FUTURE (real libdrm): uncomment once your own libdrm exists ---
# set(INTEL_DEV_LIBDRM_INCLUDE ${VESPERA_LIBDRM_INCLUDE_DIR})
# set(INTEL_DEV_LIBDRM_LINK vespera_libdrm)
# set(INTEL_DEV_LIBDRM_DEFINITIONS HAVE_LIBDRM)
# ────────────────────────────────────────────────────────────────

# ────────────────────────────────────────────────────────────────
# intel_wa.h / intel_wa.c — meson.build lines 40-43. Workaround
# tables generated from mesa_defs.json via gen_wa_helpers.py.
# "ensures intel_wa.h exists before implementation files are
# compiled" (meson.build line 57 comment) — mirrored here by every
# file in INTEL_DEV_CORE_SOURCES depending on the library target,
# which depends on this custom_command's output being listed among
# its sources (same mechanism as util/compiler's generated headers).
# ────────────────────────────────────────────────────────────────
set(INTEL_WA_H ${INTEL_DEV_GEN_DIR}/intel_wa.h)
set(INTEL_WA_C ${INTEL_DEV_GEN_DIR}/intel_wa.c)
add_custom_command(
        OUTPUT ${INTEL_WA_H} ${INTEL_WA_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${INTEL_DEV_ROOT}/gen_wa_helpers.py
        ${INTEL_DEV_ROOT}/mesa_defs.json ${INTEL_WA_H} ${INTEL_WA_C}
        DEPENDS ${INTEL_DEV_ROOT}/gen_wa_helpers.py ${INTEL_DEV_ROOT}/mesa_defs.json
        COMMENT "Generating intel/dev/intel_wa.[ch]"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# intel_device_info_gen.h — meson.build lines 45-49.
# ────────────────────────────────────────────────────────────────
set(INTEL_DEVICE_INFO_GEN_H ${INTEL_DEV_GEN_DIR}/intel_device_info_gen.h)
add_custom_command(
        OUTPUT ${INTEL_DEVICE_INFO_GEN_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${INTEL_DEV_ROOT}/intel_device_info_gen_h.py ${INTEL_DEVICE_INFO_GEN_H}
        DEPENDS ${INTEL_DEV_ROOT}/intel_device_info_gen_h.py ${INTEL_DEV_ROOT}/intel_device_info.py
        COMMENT "Generating intel/dev/intel_device_info_gen.h"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# intel_device_info_serialize_gen.c — meson.build lines 51-55.
# NOTE: matching upstream exactly, this generated file is NOT added
# to libintel_dev's sources below (meson.build's own
# static_library() call for libintel_dev does not list
# intel_dev_serialize_src either — only intel_dev_wa_src is
# included). Generated here for parity/future use, but intentionally
# unused by the library target, same as upstream.
# ────────────────────────────────────────────────────────────────
set(INTEL_DEVICE_INFO_SERIALIZE_GEN_C ${INTEL_DEV_GEN_DIR}/intel_device_info_serialize_gen.c)
add_custom_command(
        OUTPUT ${INTEL_DEVICE_INFO_SERIALIZE_GEN_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${INTEL_DEV_ROOT}/intel_device_info_serialize_gen_c.py ${INTEL_DEVICE_INFO_SERIALIZE_GEN_C}
        DEPENDS ${INTEL_DEV_ROOT}/intel_device_info_serialize_gen_c.py ${INTEL_DEV_ROOT}/intel_device_info.py
        COMMENT "Generating intel/dev/intel_device_info_serialize_gen.c (unused by libintel_dev, matches upstream)"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# files_libintel_dev — meson.build lines 6-19, reduced to what
# VesperaOS actually needs: only the Lucifer device-info backend.
# i915/intel_device_info.c and xe/intel_device_info.c are NOT
# included — intel_kmd.c's intel_get_kmd_type() always returns
# INTEL_KMD_TYPE_LUCIFER, so intel_device_info.c's
# switch(devinfo->kmd_type) can only ever reach the LUCIFER case;
# the i915/xe cases exist in that switch only because it's
# unmodified upstream code, but nothing ever compiles or links
# those two source files anymore.
# ────────────────────────────────────────────────────────────────
set(INTEL_DEV_CORE_SOURCES
        i915/intel_device_info.c
        xe/intel_device_info.c

        lucifer/intel_device_info.c
        intel_debug.c
        intel_device_info.c
        intel_hwconfig.c
        intel_kmd.c
)
list(TRANSFORM INTEL_DEV_CORE_SOURCES PREPEND "${INTEL_DEV_ROOT}/")

add_library(mesa_intel_dev STATIC
        ${INTEL_DEV_CORE_SOURCES}
        ${INTEL_WA_H}
        ${INTEL_WA_C}
        ${INTEL_DEVICE_INFO_GEN_H}
)

target_include_directories(mesa_intel_dev
        PUBLIC
        ${INTEL_DEV_ROOT}                     # inc_intel_dev — lets siblings #include "dev/intel_device_info.h"
        ${INTEL_DEV_ROOT}/..                   # inc_intel — for "dev/...", "common/...", etc. once those layers exist
        ${MESA_ROOT}/src                        # inc_src — "util/..." from anywhere
        ${MESA_ROOT}/include                    # inc_include
        ${INTEL_DEV_GEN_DIR}                    # generated intel_wa.h / intel_device_info_gen.h, included unqualified (from within intel/dev/*.c)
        ${INTEL_DEV_GEN_DIR}/../..                # BUILD_DIR/src — lets #include "intel/dev/intel_wa.h" (qualified form, used by intel_device_info.h) resolve to the generated header above
        ${INTEL_DEV_LIBDRM_INCLUDE}
        ${INTEL_DEV_GEN_DIR}/../..
        ${INTEL_DEV_GEN_DIR}/..
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_intel_dev PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${MESA_COMMON_C_FLAGS}>
        $<$<COMPILE_LANGUAGE:CXX>:${MESA_COMMON_CXX_FLAGS}>
)

target_compile_definitions(mesa_intel_dev PRIVATE
        ${INTEL_DEV_LIBDRM_DEFINITIONS}
)

# gnu_symbol_visibility: 'hidden' — meson.build line 69, same
# rationale as mesa_nir (internal static lib, not an exported ABI
# surface on its own).
target_compile_options(mesa_intel_dev PRIVATE -fvisibility=hidden)

target_link_libraries(mesa_intel_dev PUBLIC mesa_util ${INTEL_DEV_LIBDRM_LINK})

add_dependencies(mesa_intel_dev sysroot_headers mesa_util)