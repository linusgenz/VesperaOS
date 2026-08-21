# ════════════════════════════════════════════════════════════════
# lib/mesa/cmake/FindMesaPython.cmake
#
# Single source of truth for which python3 interpreter Mesa's code
# generation steps use. Every layer (util, compiler, nir, ...) was
# independently discovering Python — util/CMakeLists.txt via
# execute_process(COMMAND which python3 ...), compiler/CMakeLists.txt
# via find_package(Python3 REQUIRED COMPONENTS Interpreter) — and
# those two mechanisms can resolve to DIFFERENT interpreters (venv
# vs. system Python, different PATH at cmake-configure-time vs. an
# interactive shell), which is exactly what happened here:
# find_package(Python3) found a system interpreter without mako
# installed, while `which python3` in the user's shell resolves to
# one that has it.
#
# `which python3` (shell PATH resolution) is the correct approach
# for this project specifically, because dependencies like mako are
# installed into whatever interpreter the user's shell/environment
# already uses for Mesa's generator scripts — not necessarily the
# "first Python3 CMake's own search logic happens to find". Every
# layer includes this file once and uses ${MESA_PYTHON3_EXECUTABLE}
# instead of re-deriving it.
# ════════════════════════════════════════════════════════════════

if(DEFINED MESA_PYTHON3_EXECUTABLE)
    # Already resolved (e.g. this file got include()'d more than
    # once across layers) — don't re-run the shell lookup.
    return()
endif()

execute_process(
        COMMAND which python3
        OUTPUT_VARIABLE MESA_PYTHON3_EXECUTABLE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE MESA_PYTHON3_LOOKUP_RESULT
)

if(NOT MESA_PYTHON3_LOOKUP_RESULT EQUAL 0 OR MESA_PYTHON3_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR
            "FindMesaPython.cmake: 'which python3' did not resolve to an "
            "interpreter (result=${MESA_PYTHON3_LOOKUP_RESULT}). Mesa's code "
            "Check that python3 is on PATH in the environment."
            "cmake itself runs in — not just your interactive shell, since "
            "those can differ."
    )
endif()

message(STATUS "vespera-mesa – using python3: ${MESA_PYTHON3_EXECUTABLE}")

# Verify mako is actually importable with THIS interpreter, right
# now, at configure time — rather than letting every generator
# script fail individually, mid-build, with the same traceback.
execute_process(
        COMMAND ${MESA_PYTHON3_EXECUTABLE} -c "import mako"
        RESULT_VARIABLE MESA_PYTHON3_MAKO_CHECK
        OUTPUT_QUIET
        ERROR_QUIET
)
if(NOT MESA_PYTHON3_MAKO_CHECK EQUAL 0)
    message(FATAL_ERROR
            "FindMesaPython.cmake: ${MESA_PYTHON3_EXECUTABLE} cannot import "
            "'mako' (needed by builtin_types_h.py, ir_expression_operation.py, "
            "and other Mesa code generators). Install it for this interpreter, "
            "e.g.: ${MESA_PYTHON3_EXECUTABLE} -m pip install mako"
    )
endif()
