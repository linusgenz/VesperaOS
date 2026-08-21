# src/mesa/program/CMakeLists.txt
#
# 1:1-Uebersetzung von meson_program.build. Erzeugt den Flex-Lexer und den
# Bison-Parser fuer die ARB-Programmsprache. Beide custom_targets werden
# ueber PARENT_SCOPE-Variablen an src/mesa/CMakeLists.txt zurueckgereicht,
# genau wie mesa_lex / program_parse_tab im Meson-Baum an files_libmesa
# angehaengt werden.

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

set(PROGRAM_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(PROGRAM_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})

# --- mesa_lex : program_lexer.l -> lex.yy.c -------------------------------
set(MESA_LEX_C ${PROGRAM_GEN_DIR}/lex.yy.c)
add_custom_command(
        OUTPUT ${MESA_LEX_C}
        COMMAND ${FLEX_EXECUTABLE} -o ${MESA_LEX_C} ${PROGRAM_ROOT}/program_lexer.l
        DEPENDS ${PROGRAM_ROOT}/program_lexer.l
        COMMENT "Generating mesa/program/lex.yy.c (flex)"
        VERBATIM
)

# --- program_parse_tab : program_parse.y -> .tab.c / .tab.h ---------------
set(PROGRAM_PARSE_TAB_C ${PROGRAM_GEN_DIR}/program_parse.tab.c)
set(PROGRAM_PARSE_TAB_H ${PROGRAM_GEN_DIR}/program_parse.tab.h)
add_custom_command(
        OUTPUT ${PROGRAM_PARSE_TAB_C} ${PROGRAM_PARSE_TAB_H}
        COMMAND ${BISON_EXECUTABLE} -o ${PROGRAM_PARSE_TAB_C} --defines=${PROGRAM_PARSE_TAB_H}
        ${PROGRAM_ROOT}/program_parse.y
        DEPENDS ${PROGRAM_ROOT}/program_parse.y
        COMMENT "Generating mesa/program/program_parse.tab.[ch] (bison)"
        VERBATIM
)

# An src/mesa/CMakeLists.txt zurueckreichen (entspricht mesa_lex / program_parse_tab
# als Meson-Objekte, die in files_libmesa einfliessen).
set(MESA_LEX_C ${MESA_LEX_C} PARENT_SCOPE)
set(PROGRAM_PARSE_TAB_C ${PROGRAM_PARSE_TAB_C} PARENT_SCOPE)
set(PROGRAM_PARSE_TAB_H ${PROGRAM_PARSE_TAB_H} PARENT_SCOPE)
set(PROGRAM_GEN_DIR ${PROGRAM_GEN_DIR} PARENT_SCOPE)