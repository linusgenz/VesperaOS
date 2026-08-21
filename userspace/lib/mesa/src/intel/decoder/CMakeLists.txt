# lib/mesa/src/intel/decoder/CMakeLists.txt
#
# Portierung von src/intel/decoder/meson.build.
#
# WICHTIG -- Design-Entscheidung: STUB-Variante statt echtem Decoder.
#
# Upstream verzweigt selbst per `if not dep_expat.found()` zwischen einer
# echten Implementierung (intel_decoder.c, intel_batch_decoder.c, XML-
# basiertes Parsing von Batch-Buffern fuer Debug-Tools wie aubinator) und
# einem no-op Stub (intel_batch_decoder_stub.c). Wir nehmen bewusst den
# Stub-Pfad:
#   - ihr habt noch kein expat (gleicher Grund wie beim Auslassen von
#     xmlconfig weiter oben in der Kette)
#   - with_intel_tools (Haupt-Konsument von intel_decoder, z.B. aubinator)
#     ist bei euch ohnehin nicht eingehaengt
#   - Upstream selbst markiert das gesamte Target als build_by_default:
#     false -- auch Mesa behandelt es als rein optionales Debug-Feature,
#     nicht als Teil des Kern-Renderpfads
#
# Funktional bedeutet das: Batch-Buffer-Dekodierung (INTEL_DEBUG=bat) ist
# nicht verfuegbar, aber das hat KEINE Auswirkung auf iris' eigentlichen
# Render-Pfad -- iris erzeugt Batches, es dekodiert sie zur Laufzeit nicht
# zurueck. Falls ihr spaeter echtes Batch-Debugging wollt: expat als
# Dependency nachziehen und hier auf intel_decoder.c + intel_batch_decoder.c
# + intel_batch_decoder_gen.c umschalten (Pfad ist unten als Kommentar
# vorbereitet).
#
# with_intel_elk (intel_decoder_elk) komplett raus -- ihr baut nur brw,
# kein elk (siehe intel/compiler/CMakeLists.txt).
# Tests (genxml_test) ebenfalls raus wie ueberall sonst.

set(DECODER_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(MESA_INTEL_DECODER_SOURCES
        intel_batch_decoder_stub.c
)

# --- Echter Decoder-Pfad, falls ihr spaeter expat nachzieht: ---
# set(MESA_INTEL_DECODER_SOURCES
#     intel_decoder.c
#     intel_batch_decoder.c
# )
# und zusaetzlich ein mesa_intel_decoder_gen Target mit
# intel_batch_decoder_gen.c, das gegen mesa_intel_compiler_gen linkt
# (idep_intel_compiler_gen in upstream) -- analog zum Aufbau unten.

add_library(mesa_intel_decoder STATIC ${MESA_INTEL_DECODER_SOURCES})

target_include_directories(mesa_intel_decoder
        PUBLIC
        ${DECODER_DIR}
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/intel
        ${GENXML_INCLUDE_DIRS}
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_link_libraries(mesa_intel_decoder
        PUBLIC
        mesa_util
        mesa_intel_dev
)

target_compile_options(mesa_intel_decoder PRIVATE ${MESA_COMMON_C_FLAGS})

add_dependencies(mesa_intel_decoder sysroot_headers mesa_genxml)

# idep_intel_decoder_gen-Aequivalent: im Stub-Fall identisch zu
# mesa_intel_decoder selbst, da libintel_decoder_gen_files im Stub-Zweig
# leer ist (libintel_decoder_gen_files = files() in upstream, Zeile 16).
# Wir bilden trotzdem ein eigenes Alias-Target ab, damit Consumer-CMakeLists
# (z.B. iris spaeter) denselben Targetnamen wie in upstream referenzieren
# koennen, ohne wissen zu muessen, dass es aktuell nur ein Alias ist.
add_library(mesa_intel_decoder_gen ALIAS mesa_intel_decoder)