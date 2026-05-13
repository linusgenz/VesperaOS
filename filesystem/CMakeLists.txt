# filesystem/CMakeLists.txt
#
# Builds the filesystem subsystem OBJECT library (filesystem_lib).
#
# Include rules
# ─────────────
#   PRIVATE  → filesystem/   (internal fs headers – never exported)
#   PUBLIC   → include/      (public API cross-modules depend on)
# ─────────────────────────────────────────────────────────────────────

collect_cpp(FS_CPP_SOURCES ${CMAKE_SOURCE_DIR}/filesystem)

add_library(filesystem_lib OBJECT
        ${FS_CPP_SOURCES}
)

apply_kernel_compile_options(filesystem_lib)
kernel_module_includes(filesystem_lib ${CMAKE_SOURCE_DIR}/filesystem)