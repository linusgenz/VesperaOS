# drivers/CMakeLists.txt
#
# Builds the driver subsystem OBJECT library (drivers_lib).
#
# Include rules
# ─────────────
#   PRIVATE  → drivers/      (internal driver headers – never exported)
#   PUBLIC   → include/      (public API cross-modules depend on)

collect_cpp(DRIVERS_CPP_SOURCES ${CMAKE_SOURCE_DIR}/drivers)

add_library(drivers_lib OBJECT
        ${DRIVERS_CPP_SOURCES}
)

apply_kernel_compile_options(drivers_lib)
kernel_module_includes(drivers_lib ${CMAKE_SOURCE_DIR}/drivers)