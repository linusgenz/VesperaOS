# src/intel/genxml/CMakeLists.txt
#
# Portierung von src/intel/genxml/meson.build. Erzeugt aus den
# XML-Hardwarebeschreibungen (gen*.xml) die generierten Pack-/Bits-/
# Zipped-Header, auf denen intel_dev und intel_compiler aufbauen.
#
# Layer-Position: intel/dev, intel/compiler haengen von diesem Layer ab
# (siehe Kommentar-Reihenfolge in lib/mesa/CMakeLists.txt).

set(GENXML_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(GENXML_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})

file(MAKE_DIRECTORY ${GENXML_GEN_DIR})

# ────────────────────────────────────────────────────────────────
# Quelldaten: XML-Dateien pro Hardware-Generation
# ────────────────────────────────────────────────────────────────

set(GENXML_XML_FILES
        gen40.xml
        gen45.xml
        gen50.xml
        gen60.xml
        gen70.xml
        gen75.xml
        gen80.xml
        gen90.xml
        gen110.xml
        gen120.xml
        gen125.xml
        xe2.xml
        xe3.xml
        xe3p.xml
)

set(GENXML_XML_RT_FILES
        gen125_rt.xml
        xe2_rt.xml
        xe3_rt.xml
        xe3p_rt.xml
)

set(GENXML_ALL_XML_FILES ${GENXML_XML_FILES} ${GENXML_XML_RT_FILES})

# Absolute Pfade fuer DEPENDS / depend_files-Aequivalent. Alle
# Generator-Skripte haengen (transitiv, ueber intel_genxml.py und
# util.py) an jeder XML-Datei plus den Skripten selbst.
set(GENXML_PACK_HEADER_DEPS
        ${GENXML_ROOT}/intel_genxml.py
        ${GENXML_ROOT}/util.py
)
foreach(f ${GENXML_ALL_XML_FILES})
    list(APPEND GENXML_PACK_HEADER_DEPS ${GENXML_ROOT}/${f})
endforeach()

# ────────────────────────────────────────────────────────────────
# genX_xml.h — gezippte XML-Blobs, eingebettet als C-Header
# (../../util/gen_zipped_xml_file.py liegt eine Ebene ueber genxml/,
# also src/intel/util/)
# ────────────────────────────────────────────────────────────────

# gen_zipped_xml_file.py liegt in src/util/ (dem generischen Util-Layer,
# nicht intel/util/) -- von src/intel/genxml/ aus zwei Ebenen hoch nach
# src/, dann in util/ rein. Kein eigenes Build-Target noetig: das Skript
# taucht in util/meson.build nirgends als custom_target auf, es ist reiner
# Consumer-Code, der von genxml aus aufgerufen wird.
set(GENXML_ZIPPED_XML_SCRIPT ${GENXML_ROOT}/../../util/gen_zipped_xml_file.py)

if(NOT EXISTS ${GENXML_ZIPPED_XML_SCRIPT})
    message(FATAL_ERROR
            "genxml: gen_zipped_xml_file.py nicht gefunden unter "
            "${GENXML_ZIPPED_XML_SCRIPT}. Layout weicht vermutlich vom "
            "erwarteten src/intel/genxml + src/util ab -- Pfad in "
            "src/intel/genxml/CMakeLists.txt anpassen."
    )
endif()

# Portabel statt $<LIST:TRANSFORM,...> (das braucht CMake >= 3.27) —
# absolute Pfade der gen_xml_files (ohne die _rt-Variante) von Hand bauen.
set(GENXML_XML_FILES_ABS "")
foreach(f ${GENXML_XML_FILES})
    list(APPEND GENXML_XML_FILES_ABS ${GENXML_ROOT}/${f})
endforeach()

set(GENX_XML_H ${GENXML_GEN_DIR}/genX_xml.h)
add_custom_command(
        OUTPUT ${GENX_XML_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GENXML_ZIPPED_XML_SCRIPT}
        ${GENXML_XML_FILES_ABS} > ${GENX_XML_H}
        DEPENDS ${GENXML_ZIPPED_XML_SCRIPT} ${GENXML_PACK_HEADER_DEPS}
        WORKING_DIRECTORY ${GENXML_ROOT}
        COMMENT "Generating genxml/genX_xml.h"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# genX_bits.h — Meson's genX_bits_included_symbols-Liste 1:1
# ────────────────────────────────────────────────────────────────

set(GENX_BITS_INCLUDED_SYMBOLS
        # instructions
        "COMPUTE_WALKER"
        "GPGPU_WALKER"
        "MEDIA_CURBE_LOAD"
        "MEDIA_INTERFACE_DESCRIPTOR_LOAD"
        "MEDIA_STATE_FLUSH"
        "MEDIA_VFE_STATE"
        "MI_ARB_CHECK"
        "3DMESH_3D"
        "3DPRIMITIVE"
        "3DPRIMITIVE_EXTENDED"
        "3DSTATE_CLEAR_PARAMS"
        "3DSTATE_CLIP"
        "3DSTATE_CLIP_MESH"
        "3DSTATE_CONSTANT_ALL"
        "3DSTATE_CONSTANT_VS"
        "3DSTATE_DS"
        "3DSTATE_GS"
        "3DSTATE_HS"
        "3DSTATE_INDEX_BUFFER"
        "3DSTATE_MESH_CONTROL"
        "3DSTATE_MESH_DISTRIB"
        "3DSTATE_MESH_SHADER"
        "3DSTATE_MESH_SHADER_DATA"
        "3DSTATE_PRIMITIVE_REPLICATION"
        "3DSTATE_PS"
        "3DSTATE_PS_BLEND"
        "3DSTATE_PS_EXTRA"
        "3DSTATE_PUSH_CONSTANT_ALLOC_VS"
        "3DSTATE_RASTER"
        "3DSTATE_SBE"
        "3DSTATE_SBE_MESH"
        "3DSTATE_SBE_SWIZ"
        "3DSTATE_SF"
        "3DSTATE_STREAMOUT"
        "3DSTATE_TASK_CONTROL"
        "3DSTATE_TASK_REDISTRIB"
        "3DSTATE_TASK_SHADER"
        "3DSTATE_TASK_SHADER_DATA"
        "3DSTATE_TE"
        "3DSTATE_URB_ALLOC_MESH"
        "3DSTATE_URB_ALLOC_TASK"
        "3DSTATE_URB_ALLOC_VS"
        "3DSTATE_URB_VS"
        "3DSTATE_VF_INSTANCING"
        "3DSTATE_VF_SGVS"
        "3DSTATE_VF_SGVS_2"
        "3DSTATE_VF_TOPOLOGY"
        "3DSTATE_VFG"
        "3DSTATE_WM"
        "3DSTATE_VS"
        "PIPE_CONTROL"
        "RESOURCE_BARRIER"
        # instruction fields
        "3DSTATE_CPSIZE_CONTROL_BUFFER::Surface Base Address"
        "3DSTATE_CPSIZE_CONTROL_BUFFER::Surface Pitch"
        "3DSTATE_DEPTH_BUFFER::Surface Base Address"
        "3DSTATE_DEPTH_BUFFER::Surface Pitch"
        "3DSTATE_HIER_DEPTH_BUFFER::Surface Base Address"
        "3DSTATE_HIER_DEPTH_BUFFER::Surface Pitch"
        "3DSTATE_STENCIL_BUFFER::Surface Base Address"
        "3DSTATE_STENCIL_BUFFER::Surface Pitch"
        "3DSTATE_SO_BUFFER::Surface Base Address"
        "3DSTATE_SO_BUFFER::Stream Offset"
        "MI_BATCH_BUFFER_START::Batch Buffer Start Address"
        "MI_REPORT_PERF_COUNT::Memory Address"
        "MI_STORE_DATA_IMM::Address"
        "MI_STORE_DATA_IMM::Immediate Data"
        "MI_STORE_REGISTER_MEM::Memory Address"
        "3DSTATE_DS"
        "3DSTATE_HS"
        "3DSTATE_CLEAR_PARAMS"
        "COMPUTE_WALKER::body"
        "EXECUTE_INDIRECT_DISPATCH::body"
        # structures
        "3DSTATE_CONSTANT_ALL_DATA"
        "RENDER_SURFACE_STATE::Surface Base Address"
        "RENDER_SURFACE_STATE::Surface Pitch"
        "RENDER_SURFACE_STATE::Surface QPitch"
        "RENDER_SURFACE_STATE::Auxiliary Surface Base Address"
        "RENDER_SURFACE_STATE::Auxiliary Surface Pitch"
        "RENDER_SURFACE_STATE::Clear Value Address"
        "RENDER_SURFACE_STATE::Red Clear Color"
        "RENDER_SURFACE_STATE::Green Clear Color"
        "RENDER_SURFACE_STATE::Blue Clear Color"
        "RENDER_SURFACE_STATE::Alpha Clear Color"
        "RENDER_SURFACE_STATE::Width"
        "RENDER_SURFACE_STATE::Height"
        "RENDER_SURFACE_STATE::Depth"
        "RENDER_SURFACE_STATE::Surface Format"
        "RENDER_SURFACE_STATE::Surface Type"
        "RENDER_SURFACE_STATE::Render Target View Extent"
        "RENDER_SURFACE_STATE::Tile Mode"
        "RENDER_SURFACE_STATE::Minimum Array Element"
        "CLEAR_COLOR"
        "VERTEX_BUFFER_STATE::Buffer Starting Address"
        "CPS_STATE"
        "INTERFACE_DESCRIPTOR_DATA"
        "RT_DISPATCH_GLOBALS::Hit Group Table"
        "RT_DISPATCH_GLOBALS::Miss Group Table"
        "RT_DISPATCH_GLOBALS::Callable Group Table"
        "RT_DISPATCH_GLOBALS::Hit Group Stride"
        "RT_DISPATCH_GLOBALS::Miss Group Stride"
        "RT_DISPATCH_GLOBALS::Callable Group Stride"
        "RT_DISPATCH_GLOBALS::Launch Width"
        "RT_DISPATCH_GLOBALS::Launch Height"
        "RT_DISPATCH_GLOBALS::Launch Depth"
        "SAMPLER_STATE"
        "VERTEX_ELEMENT_STATE"
)
list(JOIN GENX_BITS_INCLUDED_SYMBOLS "," GENX_BITS_INCLUDED_SYMBOLS_CSV)

set(GENX_BITS_H ${GENXML_GEN_DIR}/genX_bits.h)

# Absolute Pfade der XML_SOURCE-Positional-Args. In Meson kommt das
# implizit ueber input : ['gen_bits_header.py', gen_xml_files,
# gen_xml_rt_files] als @INPUT@ rein (Meson zieht das Skript selbst ab,
# der Rest wird positional durchgereicht) -- in CMake muss das explizit
# aufgelistet werden, sonst fehlt XML_SOURCE komplett.
set(GENXML_ALL_XML_FILES_ABS "")
foreach(f ${GENXML_ALL_XML_FILES})
    list(APPEND GENXML_ALL_XML_FILES_ABS ${GENXML_ROOT}/${f})
endforeach()

add_custom_command(
        OUTPUT ${GENX_BITS_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GENXML_ROOT}/gen_bits_header.py
        ${GENXML_ALL_XML_FILES_ABS}
        -o ${GENX_BITS_H}
        --include-symbols "${GENX_BITS_INCLUDED_SYMBOLS_CSV}"
        DEPENDS ${GENXML_ROOT}/gen_bits_header.py ${GENXML_PACK_HEADER_DEPS}
        WORKING_DIRECTORY ${GENXML_ROOT}
        COMMENT "Generating genxml/genX_bits.h"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# <gen>_pack.h — ein Header pro XML-Datei, Engines render/blitter/compute
# ────────────────────────────────────────────────────────────────

set(GENXML_PACK_HEADERS "")
foreach(f ${GENXML_ALL_XML_FILES})
    get_filename_component(_gen_name ${f} NAME_WE)
    set(_out ${GENXML_GEN_DIR}/${_gen_name}_pack.h)
    add_custom_command(
            OUTPUT ${_out}
            COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GENXML_ROOT}/gen_pack_header.py
            ${GENXML_ROOT}/${f}
            --engines=render,blitter,compute > ${_out}
            DEPENDS ${GENXML_ROOT}/gen_pack_header.py ${GENXML_ROOT}/${f} ${GENXML_PACK_HEADER_DEPS}
            WORKING_DIRECTORY ${GENXML_ROOT}
            COMMENT "Generating genxml/${_gen_name}_pack.h"
            VERBATIM
    )
    list(APPEND GENXML_PACK_HEADERS ${_out})
endforeach()

# ────────────────────────────────────────────────────────────────
# <gen>_video_pack.h — nur fuer gen_xml_files (nicht die _rt-Variante),
# Engine video. Meson haengt diese ebenfalls an gen_xml_pack an; wir
# fuehren sie hier separat, kombinieren aber am Ende gemeinsam.
# ────────────────────────────────────────────────────────────────

foreach(f ${GENXML_XML_FILES})
    get_filename_component(_gen_name ${f} NAME_WE)
    set(_out ${GENXML_GEN_DIR}/${_gen_name}_video_pack.h)
    add_custom_command(
            OUTPUT ${_out}
            COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GENXML_ROOT}/gen_pack_header.py
            ${GENXML_ROOT}/${f}
            --engines=video > ${_out}
            DEPENDS ${GENXML_ROOT}/gen_pack_header.py ${GENXML_ROOT}/${f} ${GENXML_PACK_HEADER_DEPS}
            WORKING_DIRECTORY ${GENXML_ROOT}
            COMMENT "Generating genxml/${_gen_name}_video_pack.h"
            VERBATIM
    )
    list(APPEND GENXML_PACK_HEADERS ${_out})
endforeach()

# ────────────────────────────────────────────────────────────────
# <gen>_cl_pack.h — OpenCL-Repack-Variante, eigene Symbolliste
# ────────────────────────────────────────────────────────────────

set(GENX_CL_INCLUDED_SYMBOLS
        # instructions
        "3DMESH_3D"
        "3DSTATE_CLIP"
        "3DSTATE_CONSTANT_ALL"
        "3DSTATE_CONSTANT_VS"
        "3DSTATE_DS"
        "3DSTATE_GS"
        "3DSTATE_HS"
        "3DSTATE_INDEX_BUFFER"
        "3DSTATE_MESH_CONTROL"
        "3DSTATE_MESH_SHADER_DATA"
        "3DSTATE_PS"
        "3DSTATE_PS_EXTRA"
        "3DSTATE_PS_BLEND"
        "3DSTATE_RASTER"
        "3DSTATE_SF"
        "3DSTATE_STREAMOUT"
        "3DSTATE_TASK_CONTROL"
        "3DSTATE_TASK_SHADER_DATA"
        "3DSTATE_TE"
        "3DSTATE_VERTEX_BUFFERS"
        "3DSTATE_VF_TOPOLOGY"
        "3DSTATE_VFG"
        "3DSTATE_VS"
        "3DSTATE_WM"
        "3DPRIMITIVE"
        "3DPRIMITIVE_EXTENDED"
        "COMPUTE_WALKER"
        "GPGPU_WALKER"
        "MEDIA_CURBE_LOAD"
        "MEDIA_INTERFACE_DESCRIPTOR_LOAD"
        "MEDIA_STATE_FLUSH"
        "MI_ARB_CHECK"
        "MI_BATCH_BUFFER_START"
        "MI_STORE_DATA_IMM"
        "PIPE_CONTROL"
        # structures
        "3DSTATE_CONSTANT_ALL_DATA"
        "3DSTATE_CONSTANT_BODY"
        "BINDLESS_SHADER_RECORD"
        "CALL_STACK_HANDLER"
        "COMPUTE_WALKER_BODY"
        "INTERFACE_DESCRIPTOR_DATA"
        "POSTSYNC_DATA"
        "RT_DISPATCH_GLOBALS"
        "RT_SHADER_TABLE"
        "VERTEX_BUFFER_STATE"
)
list(JOIN GENX_CL_INCLUDED_SYMBOLS "," GENX_CL_INCLUDED_SYMBOLS_CSV)

foreach(f ${GENXML_ALL_XML_FILES})
    get_filename_component(_gen_name ${f} NAME_WE)
    set(_out ${GENXML_GEN_DIR}/${_gen_name}_cl_pack.h)
    add_custom_command(
            OUTPUT ${_out}
            COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GENXML_ROOT}/gen_pack_header.py
            ${GENXML_ROOT}/${f}
            --engines=render,blitter,compute
            --opencl --repack
            --include-symbols "${GENX_CL_INCLUDED_SYMBOLS_CSV}" > ${_out}
            DEPENDS ${GENXML_ROOT}/gen_pack_header.py ${GENXML_ROOT}/${f} ${GENXML_PACK_HEADER_DEPS}
            WORKING_DIRECTORY ${GENXML_ROOT}
            COMMENT "Generating genxml/${_gen_name}_cl_pack.h"
            VERBATIM
    )
    list(APPEND GENXML_PACK_HEADERS ${_out})
endforeach()

# ────────────────────────────────────────────────────────────────
# idep_genxml-Aequivalent.
#
# WICHTIG: Kein INTERFACE-Target hierfuer -- ein INTERFACE-Target hat
# selbst keine kompilierte Objektdatei, und target_sources(... INTERFACE)
# haengt die generierten Header nur "lose" dran. Ninja baut deren
# add_custom_command dann NUR, wenn ein konkretes Consumer-Target sie
# ueber #include tatsaechlich referenziert und CMake/Ninja das per
# Header-Dependency-Scanning erkennt -- passiert das (noch) nicht (z.B.
# weil intel_dev/intel_compiler die Header aktuell nicht includieren,
# oder das Scanning an der Stelle nicht greift), wird genxml nie gebaut,
# obwohl andere Layer wie intel/compiler ganz normal laufen, weil die
# eigene Sourcen tatsaechlich kompiliert werden.
#
# Deshalb: echtes add_custom_target mit expliziter DEPENDS-Liste. Das
# ist ein "immer aktuell zu haltendes" Ziel, das jede Custom-Command-
# Output-Datei zwingend in seine Dependency-Kette zieht -- unabhaengig
# davon, ob irgendwer die Header schon includiert. Consumer haengen sich
# per add_dependencies() explizit dran (siehe unten / bzw. in
# intel/dev, intel/compiler jeweils ergaenzen).
# ────────────────────────────────────────────────────────────────

add_custom_target(mesa_genxml
        DEPENDS
        ${GENXML_PACK_HEADERS}
        ${GENX_BITS_H}
        ${GENX_XML_H}
        COMMENT "Generating all genxml headers"
)

# Include-Pfade fuer Consumer als eigenstaendige Variable, da ein
# add_custom_target keine target_include_directories() transportieren
# kann (das ist ein PHONY-artiges Utility-Target, kein Compile-Target).
# Consumer muessen sich das selbst dazuholen, z.B.:
#
#   add_dependencies(mesa_intel_dev mesa_genxml)
#   target_include_directories(mesa_intel_dev PRIVATE ${GENXML_INCLUDE_DIRS})
#
set(GENXML_INCLUDE_DIRS ${GENXML_GEN_DIR} ${GENXML_ROOT} CACHE INTERNAL "")

# ────────────────────────────────────────────────────────────────
# Sortier-Tests (Meson: test('genxml sort ...', ...))
# ────────────────────────────────────────────────────────────────

if(BUILD_TESTING)
    set(GENXML_TEST_XML_FILES ${GENXML_ALL_XML_FILES} ../decoder/tests/gentest.xml)
    foreach(f ${GENXML_TEST_XML_FILES})
        add_test(
                NAME "genxml_sort_${f}"
                COMMAND ${MESA_PYTHON3_EXECUTABLE}
                ${GENXML_ROOT}/gen_sort_tags.py
                ${GENXML_ROOT}/${f}
                --validate --quiet
                WORKING_DIRECTORY ${GENXML_ROOT}
        )
    endforeach()
endif()