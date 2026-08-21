# lib/mesa/src/gallium/auxiliary/CMakeLists.txt
#
# Portierung von src/gallium/auxiliary/meson.build.
#
# ════════════════════════════════════════════════════════════════
# BEWUSST AUSGELASSEN — with_gfx_compute-Block (draw/, gallivm/):
#
# Steuert in Upstream einen Software-Fallback-Vertex-Pipeline
# (draw/) und einen LLVM-JIT-Softwarerenderer (gallivm/). Fuer iris
# als reinen Hardware-Treiber auf Gen9.5 (Kaby Lake, breite native
# Tessellation/Geometry-Shader/Transform-Feedback-Unterstuetzung)
# ist das Risiko fehlender Fallback-Pfade fuer den ersten lauffaehigen
# Treiber vertretbar. gallivm/ wuerde zusaetzlich eine CROSS-kompilierte
# LLVM-Installation fuer x86_64-vespera-elf erfordern (nicht nur
# Host-LLVM wie beim mesa_clc-Host-Tool-Build) -- eigenes, groesseres
# Vorhaben, das wir bewusst aufschieben.
#
# Falls spaeter benoetigt: draw/, gallivm/, tessellator/, die drei
# nir/*_tgsi*-Dateien (tgsi_to_nir, nir_to_tgsi, nir_draw_helpers)
# und die beiden custom_targets draw_nir_lower_opcodes.c sowie
# lp_bld_nir_no_integer_algebraic.c aus meson.build Zeilen 111-396
# nachruesten. tgsi_to_nir.c/nir_to_tgsi.c haengen NICHT an
# with_gfx_compute (siehe unten, Kernliste) und sind daher bereits
# Teil dieses Layers -- nur die *_info-Variante (nir_to_tgsi_info.c)
# und nir_draw_helpers.c stecken im draw/gallivm-Block.
#
# BEWUSST AUSGELASSEN — vl/ (Video-Layer, VDPAU/VAAPI-Codec-Support):
# files_libgalliumvl, files_libgalliumvlwinsys, _libgalliumvl_stub,
# libgalliumvl, libgalliumvlwinsys (meson.build Zeilen 398-578).
# Kein Video-Decode/VAAPI-Ziel fuer VesperaOS vorgesehen. Falls sich
# das aendert: eigene CMakeLists.txt fuer diesen Teil, da vl/ zusaetzlich
# an X11/DRM/Kopper-Fenstersystem-Deps haengt, die bei uns noch nicht
# existieren (with_platform_x11, with_gallium_zink, etc.).
#
# hud/ IST enthalten (Performance-HUD-Overlay) -- haengt aber ebenfalls
# an with_gfx_compute in Upstream (Zeile 201-212 liegt im with_gfx_compute-
# Block!). Da wir with_gfx_compute auslassen, ist hud/ bei uns AUCH
# NICHT dabei, auch wenn es funktional nichts mit draw/gallivm zu tun
# hat -- Upstream gruppiert es trotzdem in denselben meson.build-Zweig.
# Falls ihr das HUD unabhaengig wollt, muesste es als eigener kleiner
# Nachtrag separat verdrahtet werden (hud/hud_context.c + Konsorten
# haengen nicht zwingend an draw/gallivm, nur die meson.build-Struktur
# legt sie zusammen).
# ════════════════════════════════════════════════════════════════

set(GALLIUM_AUX_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(GALLIUM_AUX_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})

# ────────────────────────────────────────────────────────────────
# Kern-Quelldateien — meson.build Zeilen 4-109. Diese sind IMMER
# dabei, unabhaengig von with_gfx_compute.
# ────────────────────────────────────────────────────────────────
set(GALLIUM_AUX_SOURCES
        ${GALLIUM_AUX_ROOT}/driver_ddebug/dd_draw.c
        ${GALLIUM_AUX_ROOT}/driver_ddebug/dd_util.h
        ${GALLIUM_AUX_ROOT}/pipebuffer/pb_buffer_fenced.c
        ${GALLIUM_AUX_ROOT}/pipebuffer/pb_buffer_fenced.h
        ${GALLIUM_AUX_ROOT}/pipebuffer/pb_buffer.h
        ${GALLIUM_AUX_ROOT}/pipebuffer/pb_bufmgr_cache.c
        ${GALLIUM_AUX_ROOT}/pipebuffer/pb_bufmgr_debug.c
        ${GALLIUM_AUX_ROOT}/pipebuffer/pb_bufmgr.h
        ${GALLIUM_AUX_ROOT}/pipebuffer/pb_bufmgr_mm.c
        ${GALLIUM_AUX_ROOT}/pipebuffer/pb_bufmgr_slab.c
        ${GALLIUM_AUX_ROOT}/pipebuffer/pb_cache.c
        ${GALLIUM_AUX_ROOT}/pipebuffer/pb_cache.h
        ${GALLIUM_AUX_ROOT}/pipebuffer/pb_validate.c
        ${GALLIUM_AUX_ROOT}/pipebuffer/pb_validate.h
        ${GALLIUM_AUX_ROOT}/util/u_async_debug.h
        ${GALLIUM_AUX_ROOT}/util/u_async_debug.c
        ${GALLIUM_AUX_ROOT}/util/u_bitcast.h
        ${GALLIUM_AUX_ROOT}/util/u_bitmask.c
        ${GALLIUM_AUX_ROOT}/util/u_bitmask.h
        ${GALLIUM_AUX_ROOT}/util/u_blend.h
        ${GALLIUM_AUX_ROOT}/util/u_blitter.c
        ${GALLIUM_AUX_ROOT}/util/u_blitter.h
        ${GALLIUM_AUX_ROOT}/util/u_debug_cb.h
        ${GALLIUM_AUX_ROOT}/util/u_debug_describe.c
        ${GALLIUM_AUX_ROOT}/util/u_debug_describe.h
        ${GALLIUM_AUX_ROOT}/util/u_debug_flush.c
        ${GALLIUM_AUX_ROOT}/util/u_debug_flush.h
        ${GALLIUM_AUX_ROOT}/util/u_debug_image.c
        ${GALLIUM_AUX_ROOT}/util/u_debug_image.h
        ${GALLIUM_AUX_ROOT}/util/u_debug_refcnt.c
        ${GALLIUM_AUX_ROOT}/util/u_debug_refcnt.h
        ${GALLIUM_AUX_ROOT}/util/u_draw.c
        ${GALLIUM_AUX_ROOT}/util/u_draw.h
        ${GALLIUM_AUX_ROOT}/util/u_draw_quad.c
        ${GALLIUM_AUX_ROOT}/util/u_draw_quad.h
        ${GALLIUM_AUX_ROOT}/util/u_driconf.c
        ${GALLIUM_AUX_ROOT}/util/u_driconf.h
        ${GALLIUM_AUX_ROOT}/util/u_dual_blend.h
        ${GALLIUM_AUX_ROOT}/util/u_dump_defines.c
        ${GALLIUM_AUX_ROOT}/util/u_dump.h
        ${GALLIUM_AUX_ROOT}/util/u_dump_state.c
        ${GALLIUM_AUX_ROOT}/util/u_framebuffer.c
        ${GALLIUM_AUX_ROOT}/util/u_framebuffer.h
        ${GALLIUM_AUX_ROOT}/util/u_gen_mipmap.c
        ${GALLIUM_AUX_ROOT}/util/u_gen_mipmap.h
        ${GALLIUM_AUX_ROOT}/util/u_handle_table.c
        ${GALLIUM_AUX_ROOT}/util/u_handle_table.h
        ${GALLIUM_AUX_ROOT}/util/u_helpers.c
        ${GALLIUM_AUX_ROOT}/util/u_helpers.h
        ${GALLIUM_AUX_ROOT}/util/u_index_modify.c
        ${GALLIUM_AUX_ROOT}/util/u_index_modify.h
        ${GALLIUM_AUX_ROOT}/util/u_inlines.h
        ${GALLIUM_AUX_ROOT}/util/u_live_shader_cache.c
        ${GALLIUM_AUX_ROOT}/util/u_live_shader_cache.h
        ${GALLIUM_AUX_ROOT}/util/u_log.c
        ${GALLIUM_AUX_ROOT}/util/u_log.h
        ${GALLIUM_AUX_ROOT}/util/u_prim.h
        ${GALLIUM_AUX_ROOT}/util/u_prim.c
        ${GALLIUM_AUX_ROOT}/util/u_prim_restart.c
        ${GALLIUM_AUX_ROOT}/util/u_prim_restart.h
        ${GALLIUM_AUX_ROOT}/util/u_pstipple.c
        ${GALLIUM_AUX_ROOT}/util/u_pstipple.h
        ${GALLIUM_AUX_ROOT}/util/u_pwr8.h
        ${GALLIUM_AUX_ROOT}/util/u_range.h
        ${GALLIUM_AUX_ROOT}/util/u_rect.h
        ${GALLIUM_AUX_ROOT}/util/u_resource.c
        ${GALLIUM_AUX_ROOT}/util/u_resource.h
        ${GALLIUM_AUX_ROOT}/util/u_sample_positions.c
        ${GALLIUM_AUX_ROOT}/util/u_sample_positions.h
        ${GALLIUM_AUX_ROOT}/util/u_sampler.c
        ${GALLIUM_AUX_ROOT}/util/u_sampler.h
        ${GALLIUM_AUX_ROOT}/util/u_screen.c
        ${GALLIUM_AUX_ROOT}/util/u_screen.h
        ${GALLIUM_AUX_ROOT}/util/u_simple_shaders.h
        ${GALLIUM_AUX_ROOT}/util/u_split_draw.c
        ${GALLIUM_AUX_ROOT}/util/u_split_draw.h
        ${GALLIUM_AUX_ROOT}/util/u_split_prim.h
        ${GALLIUM_AUX_ROOT}/util/u_sse.h
        ${GALLIUM_AUX_ROOT}/util/u_suballoc.c
        ${GALLIUM_AUX_ROOT}/util/u_suballoc.h
        ${GALLIUM_AUX_ROOT}/util/u_surface.c
        ${GALLIUM_AUX_ROOT}/util/u_surface.h
        ${GALLIUM_AUX_ROOT}/util/u_tests.h
        ${GALLIUM_AUX_ROOT}/util/u_texture.c
        ${GALLIUM_AUX_ROOT}/util/u_texture.h
        ${GALLIUM_AUX_ROOT}/util/u_tile.c
        ${GALLIUM_AUX_ROOT}/util/u_tile.h
        ${GALLIUM_AUX_ROOT}/util/u_transfer.c
        ${GALLIUM_AUX_ROOT}/util/u_transfer.h
        ${GALLIUM_AUX_ROOT}/util/u_transfer_helper.c
        ${GALLIUM_AUX_ROOT}/util/u_transfer_helper.h
        ${GALLIUM_AUX_ROOT}/util/u_threaded_context.c
        ${GALLIUM_AUX_ROOT}/util/u_threaded_context.h
        ${GALLIUM_AUX_ROOT}/util/u_threaded_context_calls.h
        ${GALLIUM_AUX_ROOT}/util/u_trace_gallium.c
        ${GALLIUM_AUX_ROOT}/util/u_trace_gallium.h
        ${GALLIUM_AUX_ROOT}/util/u_upload_mgr.c
        ${GALLIUM_AUX_ROOT}/util/u_upload_mgr.h
        ${GALLIUM_AUX_ROOT}/util/u_vbuf.c
        ${GALLIUM_AUX_ROOT}/util/u_vbuf.h
        ${GALLIUM_AUX_ROOT}/util/u_vertex_state_cache.c
        ${GALLIUM_AUX_ROOT}/util/u_vertex_state_cache.h
        ${GALLIUM_AUX_ROOT}/util/u_video.h
        ${GALLIUM_AUX_ROOT}/util/u_viewport.h
        # nir/tgsi_to_nir.c + nir_to_tgsi.c haengen NICHT an
        # with_gfx_compute (stehen in meson.build ausserhalb des
        # if with_gfx_compute-Blocks-Aequivalents -- Kernliste).
        # iris nutzt intern NIR direkt, aber tgsi_to_nir/nir_to_tgsi
        # werden von gallium-generischem Code (u_threaded_context,
        # Treiber-Interop) potenziell referenziert; mit aufgenommen,
        # da sie nicht am with_gfx_compute-Gate haengen.
        ${GALLIUM_AUX_ROOT}/nir/tgsi_to_nir.c
        ${GALLIUM_AUX_ROOT}/nir/tgsi_to_nir.h
        ${GALLIUM_AUX_ROOT}/nir/nir_to_tgsi.c
        ${GALLIUM_AUX_ROOT}/nir/nir_to_tgsi.h
)

# ────────────────────────────────────────────────────────────────
# tgsi/ — meson.build Zeilen 221-258 liegen INNERHALB des
# with_gfx_compute-Blocks. tgsi/ selbst (TGSI IR Parsing/Dump/Scan)
# wird aber von etlichen generischen Gallium-Codepfaden potenziell
# referenziert (z.B. tgsi_from_mesa.c fuer aeltere API-Kompat-Layer).
#
# TODO(vespera): Pruefen, ob iris zur Laufzeit tatsaechlich TGSI-
# Symbole braucht, oder ob das ausschliesslich vom draw/gallivm-
# Software-Pfad genutzt wird. Da wir with_gfx_compute komplett
# auslassen, ist tgsi/ hier vorerst NICHT eingebunden -- falls beim
# Linken von mesa_gallium_aux oder spaeter iris "undefined reference"
# auf tgsi_*-Symbole auftaucht, ist das der Ort, an dem tgsi/ nach-
# getragen werden muss.
# ────────────────────────────────────────────────────────────────

# ────────────────────────────────────────────────────────────────
# u_indices_gen.c / u_unfilled_gen.c — meson.build Zeilen 495-507.
# Python-Codegen, IMMER aktiv (nicht an with_gfx_compute gebunden),
# analog zum genxml/tracepoints-Muster.
# ────────────────────────────────────────────────────────────────
set(U_INDICES_GEN_C ${GALLIUM_AUX_GEN_DIR}/u_indices_gen.c)
add_custom_command(
        OUTPUT ${U_INDICES_GEN_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE}
        ${GALLIUM_AUX_ROOT}/indices/u_indices_gen.py
        ${U_INDICES_GEN_C}
        DEPENDS ${GALLIUM_AUX_ROOT}/indices/u_indices_gen.py
        COMMENT "Generating gallium/auxiliary/u_indices_gen.c (Python codegen)"
        VERBATIM
)

set(U_UNFILLED_GEN_C ${GALLIUM_AUX_GEN_DIR}/u_unfilled_gen.c)
add_custom_command(
        OUTPUT ${U_UNFILLED_GEN_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE}
        ${GALLIUM_AUX_ROOT}/indices/u_unfilled_gen.py
        ${U_UNFILLED_GEN_C}
        DEPENDS ${GALLIUM_AUX_ROOT}/indices/u_unfilled_gen.py
        COMMENT "Generating gallium/auxiliary/u_unfilled_gen.c (Python codegen)"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# tr_util.[ch] — meson.build Zeilen 456-463. enums2names.py generiert
# aus mehreren Header-Inputs eine Enum<->String-Tabelle. Braucht
# p_defines.h, p_video_enums.h aus gallium/include/pipe/, sowie
# util/blend.h aus dem Haupt-util-Layer.
# ────────────────────────────────────────────────────────────────
set(TR_UTIL_C ${GALLIUM_AUX_GEN_DIR}/tr_util.c)
set(TR_UTIL_H ${GALLIUM_AUX_GEN_DIR}/tr_util.h)
add_custom_command(
        OUTPUT ${TR_UTIL_C} ${TR_UTIL_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE}
        ${GALLIUM_AUX_ROOT}/driver_trace/enums2names.py
        ${MESA_ROOT}/src/gallium/include/pipe/p_defines.h
        ${MESA_ROOT}/src/gallium/include/pipe/p_video_enums.h
        ${MESA_ROOT}/src/util/blend.h
        -C ${TR_UTIL_C}
        -H ${TR_UTIL_H}
        DEPENDS
        ${GALLIUM_AUX_ROOT}/driver_trace/enums2names.py
        ${MESA_ROOT}/src/gallium/include/pipe/p_defines.h
        ${MESA_ROOT}/src/gallium/include/pipe/p_video_enums.h
        ${MESA_ROOT}/src/util/blend.h
        COMMENT "Generating gallium/auxiliary/tr_util.[c|h] (enums2names.py)"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# u_tracepoints.[ch] — meson.build Zeilen 465-493. Gleiches
# Python-Codegen-Muster wie intel_tracepoints (siehe intel/ds/
# CMakeLists.txt) — ohne --perfetto-hdr, da Gallium's u_tracepoints
# nur den u_trace-Header + Quelle erzeugt, keine gesonderte
# Perfetto-Variante wie im Intel-Layer.
# ────────────────────────────────────────────────────────────────
set(U_TRACEPOINTS_C ${GALLIUM_AUX_GEN_DIR}/u_tracepoints.c)
set(U_TRACEPOINTS_H ${GALLIUM_AUX_GEN_DIR}/u_tracepoints.h)
add_custom_command(
        OUTPUT ${U_TRACEPOINTS_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE}
        ${GALLIUM_AUX_ROOT}/util/u_tracepoints.py
        -p ${MESA_ROOT}/src/util/perf/
        -C ${U_TRACEPOINTS_C}
        DEPENDS ${GALLIUM_AUX_ROOT}/util/u_tracepoints.py
        COMMENT "Generating gallium/auxiliary/u_tracepoints.c (Python codegen)"
        VERBATIM
)
add_custom_command(
        OUTPUT ${U_TRACEPOINTS_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE}
        ${GALLIUM_AUX_ROOT}/util/u_tracepoints.py
        -p ${MESA_ROOT}/src/util/perf/
        -H ${U_TRACEPOINTS_H}
        DEPENDS ${GALLIUM_AUX_ROOT}/util/u_tracepoints.py
        COMMENT "Generating gallium/auxiliary/u_tracepoints.h (Python codegen)"
        VERBATIM
)

add_custom_target(mesa_gallium_aux_gen
        DEPENDS
        ${U_INDICES_GEN_C} ${U_UNFILLED_GEN_C}
        ${TR_UTIL_C} ${TR_UTIL_H}
        ${U_TRACEPOINTS_C} ${U_TRACEPOINTS_H}
)

# ────────────────────────────────────────────────────────────────
# libgallium — meson.build Zeilen 510-524.
# ────────────────────────────────────────────────────────────────
add_library(mesa_gallium_aux STATIC
        ${GALLIUM_AUX_SOURCES}
        ${U_INDICES_GEN_C}
        ${U_UNFILLED_GEN_C}
        ${TR_UTIL_C}
        ${TR_UTIL_H}
        ${U_TRACEPOINTS_C}
)

target_include_directories(mesa_gallium_aux
        PUBLIC
        ${GALLIUM_AUX_ROOT}
        ${GALLIUM_AUX_ROOT}/util
        ${MESA_ROOT}/src/gallium/include
        ${MESA_ROOT}/src/gallium/auxiliary
        ${MESA_ROOT}/src
        ${MESA_ROOT}/include
        ${GALLIUM_AUX_GEN_DIR}
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_gallium_aux PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${MESA_COMMON_C_FLAGS}>
        $<$<COMPILE_LANGUAGE:CXX>:${MESA_COMMON_CXX_FLAGS}>
)

target_compile_options(mesa_gallium_aux PRIVATE -fvisibility=hidden)

# dependencies -- meson.build: dep_libdrm, dep_llvm, dep_dl, dep_m,
# dep_thread, dep_lmsensors, dep_ws2_32, idep_nir, idep_nir_headers,
# idep_mesautil.
#
# Bei uns:
#   - dep_llvm      -> NICHT gelinkt, da wir gallivm/ auslassen (kein
#                       Cross-LLVM vorhanden/noetig fuer diesen Layer)
#   - dep_ws2_32    -> Windows-Sockets, entfaellt komplett
#   - dep_lmsensors -> Sensor-Support fuer hud/, entfaellt (hud/ ist
#                       ohnehin nicht Teil dieses Layers bei uns)
#   - dep_libdrm    -> TODO(vespera): noch nicht als Cross-Target
#                       verfuegbar (gleiche Situation wie beim
#                       ausgelassenen pps_intel in intel/ds). Sobald
#                       libdrm im Sysroot verfuegbar ist, hier
#                       ergaenzen -- pb_bufmgr* und Teile von
#                       driver_trace/tr_screen.c erwarten das
#                       eigentlich, koennten also bis dahin nur
#                       eingeschraenkt funktionieren.
#   - dep_dl        -> falls eure libc dlopen/dlsym separat auslagert,
#                       hier ergaenzen; sonst i.d.R. Teil der libc
#   - dep_thread    -> euer pthread-Layer (Targetname pruefen, siehe
#                       auch TODO in util/c11/CMakeLists.txt)
target_link_libraries(mesa_gallium_aux PUBLIC
        mesa_util
        mesa_nir
        vesplib
)

add_dependencies(mesa_gallium_aux
        sysroot_headers
        mesa_gallium_aux_gen
        mesa_util
        mesa_nir
)
