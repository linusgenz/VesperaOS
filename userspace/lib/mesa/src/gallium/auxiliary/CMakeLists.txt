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
        ${GALLIUM_AUX_ROOT}/util/u_simple_shaders.c
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

        ${GALLIUM_AUX_ROOT}/nir/nir_draw_helpers.c
        ${GALLIUM_AUX_ROOT}/nir/tgsi_to_nir.c
        ${GALLIUM_AUX_ROOT}/nir/nir_to_tgsi.c
        #  ${GALLIUM_AUX_ROOT}/nir/nir_to_tgsi_info.c

        ${GALLIUM_AUX_ROOT}/hud/font.c
        ${GALLIUM_AUX_ROOT}/hud/hud_context.c
        ${GALLIUM_AUX_ROOT}/hud/hud_cpu.c
        ${GALLIUM_AUX_ROOT}/hud/hud_driver_query.c
        ${GALLIUM_AUX_ROOT}/hud/hud_fps.c
        # ${GALLIUM_AUX_ROOT}/hud/hud_cpufreq.c
        #${GALLIUM_AUX_ROOT}/hud/hud_diskstat.c

        ${GALLIUM_AUX_ROOT}/pipe-loader/pipe_loader.c
        ${GALLIUM_AUX_ROOT}/pipe-loader/pipe_loader_drm.c
        ${GALLIUM_AUX_ROOT}/pipe-loader/pipe_loader_sw.c

        ${GALLIUM_AUX_ROOT}/sw_screen_create_vk.c
)

set(GALLIUM_AUX_CSO_SOURCES
        ${GALLIUM_AUX_ROOT}/cso_cache/cso_cache.c
        ${GALLIUM_AUX_ROOT}/cso_cache/cso_cache.h
        ${GALLIUM_AUX_ROOT}/cso_cache/cso_context.c
        ${GALLIUM_AUX_ROOT}/cso_cache/cso_context.h
        ${GALLIUM_AUX_ROOT}/cso_cache/cso_hash.c
        ${GALLIUM_AUX_ROOT}/cso_cache/cso_hash.h
)

set(GALLIUM_AUX_TRANSLATE_SOURCES
        ${GALLIUM_AUX_ROOT}/translate/translate.c
        ${GALLIUM_AUX_ROOT}/translate/translate.h
        ${GALLIUM_AUX_ROOT}/translate/translate_cache.c
        ${GALLIUM_AUX_ROOT}/translate/translate_cache.h
        ${GALLIUM_AUX_ROOT}/translate/translate_generic.c
        ${GALLIUM_AUX_ROOT}/translate/translate_sse.c
)

set(GALLIUM_AUX_RTASM_SOURCES
        ${GALLIUM_AUX_ROOT}/rtasm/rtasm_execmem.c
        ${GALLIUM_AUX_ROOT}/rtasm/rtasm_execmem.h
        ${GALLIUM_AUX_ROOT}/rtasm/rtasm_x86sse.c
        ${GALLIUM_AUX_ROOT}/rtasm/rtasm_x86sse.h
)

set(GALLIUM_AUX_INDICES_SOURCES
        ${GALLIUM_AUX_ROOT}/indices/u_indices.h
        ${GALLIUM_AUX_ROOT}/indices/u_indices_priv.h
        ${GALLIUM_AUX_ROOT}/indices/u_primconvert.c
        ${GALLIUM_AUX_ROOT}/indices/u_primconvert.h
)

set(GALLIUM_AUX_TGSI_SOURCES
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_aa_point.c
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_aa_point.h
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_build.c
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_build.h
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_dump.c
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_dump.h
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_dynamic_indexing.c
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_exec.c
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_exec.h
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_from_mesa.c
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_from_mesa.h
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_info.c
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_info.h
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_info_opcodes.h
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_iterate.c
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_iterate.h
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_opcode_tmp.h
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_parse.c
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_parse.h
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_point_sprite.c
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_point_sprite.h
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_sanity.c
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_sanity.h
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_scan.c
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_scan.h
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_strings.c
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_strings.h
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_text.c
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_text.h
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_transform.c
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_transform.h
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_two_side.c
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_two_side.h
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_ureg.c
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_ureg.h
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_util.c
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_util.h
        ${GALLIUM_AUX_ROOT}/tgsi/tgsi_vpos.c
)

set(GALLIUM_AUX_DRIVER_TRACE_SOURCES
        ${GALLIUM_AUX_ROOT}/driver_trace/tr_context.c
        ${GALLIUM_AUX_ROOT}/driver_trace/tr_context.h
        ${GALLIUM_AUX_ROOT}/driver_trace/tr_dump.c
        ${GALLIUM_AUX_ROOT}/driver_trace/tr_dump_defines.h
        ${GALLIUM_AUX_ROOT}/driver_trace/tr_dump.h
        ${GALLIUM_AUX_ROOT}/driver_trace/tr_dump_state.c
        ${GALLIUM_AUX_ROOT}/driver_trace/tr_dump_state.h
        ${GALLIUM_AUX_ROOT}/driver_trace/tr_public.h
        ${GALLIUM_AUX_ROOT}/driver_trace/tr_screen.c
        ${GALLIUM_AUX_ROOT}/driver_trace/tr_screen.h
        ${GALLIUM_AUX_ROOT}/driver_trace/tr_texture.c
        ${GALLIUM_AUX_ROOT}/driver_trace/tr_texture.h
)

set(GALLIUM_AUX_DRAW_SOURCES
        ${GALLIUM_AUX_ROOT}/draw/draw_cliptest_tmp.h
        ${GALLIUM_AUX_ROOT}/draw/draw_context.c
        ${GALLIUM_AUX_ROOT}/draw/draw_context.h
        ${GALLIUM_AUX_ROOT}/draw/draw_decompose_tmp.h
        ${GALLIUM_AUX_ROOT}/draw/draw_fs.c
        ${GALLIUM_AUX_ROOT}/draw/draw_fs.h
        ${GALLIUM_AUX_ROOT}/draw/draw_gs.c
        ${GALLIUM_AUX_ROOT}/draw/draw_gs.h
        ${GALLIUM_AUX_ROOT}/draw/draw_gs_tmp.h
        ${GALLIUM_AUX_ROOT}/draw/draw_mesh.c
        ${GALLIUM_AUX_ROOT}/draw/draw_mesh.h
        ${GALLIUM_AUX_ROOT}/draw/draw_mesh_prim.c
        ${GALLIUM_AUX_ROOT}/draw/draw_mesh_prim.h
        ${GALLIUM_AUX_ROOT}/draw/draw_mesh_prim_tmp.h
        ${GALLIUM_AUX_ROOT}/draw/draw_pipe_aaline.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pipe_aapoint.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pipe.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pipe.h
        ${GALLIUM_AUX_ROOT}/draw/draw_pipe_clip.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pipe_cull.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pipe_flatshade.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pipe_offset.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pipe_pstipple.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pipe_stipple.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pipe_twoside.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pipe_unfilled.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pipe_user_cull.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pipe_util.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pipe_validate.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pipe_vbuf.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pipe_wide_line.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pipe_wide_point.c
        ${GALLIUM_AUX_ROOT}/draw/draw_prim_assembler.c
        ${GALLIUM_AUX_ROOT}/draw/draw_prim_assembler.h
        ${GALLIUM_AUX_ROOT}/draw/draw_prim_assembler_tmp.h
        ${GALLIUM_AUX_ROOT}/draw/draw_private.h
        ${GALLIUM_AUX_ROOT}/draw/draw_pt.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pt_decompose.h
        ${GALLIUM_AUX_ROOT}/draw/draw_pt_emit.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pt_fetch.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pt_fetch_shade_emit.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pt_fetch_shade_pipeline.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pt_mesh_pipeline.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pt.h
        ${GALLIUM_AUX_ROOT}/draw/draw_pt_post_vs.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pt_so_emit.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pt_util.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pt_vsplit.c
        ${GALLIUM_AUX_ROOT}/draw/draw_pt_vsplit_tmp.h
        ${GALLIUM_AUX_ROOT}/draw/draw_so_emit_tmp.h
        ${GALLIUM_AUX_ROOT}/draw/draw_split_tmp.h
        ${GALLIUM_AUX_ROOT}/draw/draw_tess.c
        ${GALLIUM_AUX_ROOT}/draw/draw_tess.h
        ${GALLIUM_AUX_ROOT}/draw/draw_vbuf.h
        ${GALLIUM_AUX_ROOT}/draw/draw_vertex.c
        ${GALLIUM_AUX_ROOT}/draw/draw_vertex.h
        ${GALLIUM_AUX_ROOT}/draw/draw_vertex_header.h
        ${GALLIUM_AUX_ROOT}/draw/draw_vs.c
        ${GALLIUM_AUX_ROOT}/draw/draw_vs_exec.c
        ${GALLIUM_AUX_ROOT}/draw/draw_vs.h
        ${GALLIUM_AUX_ROOT}/draw/draw_vs_variant.c
)

set(DRAW_NIR_LOWER_OPCODES_C ${GALLIUM_AUX_GEN_DIR}/draw_nir_lower_opcodes.c)
add_custom_command(
        OUTPUT ${DRAW_NIR_LOWER_OPCODES_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE}
        ${GALLIUM_AUX_ROOT}/draw/draw_nir_lower_opcodes.py
        -p ${MESA_ROOT}/src/compiler/nir
        > ${DRAW_NIR_LOWER_OPCODES_C}
        DEPENDS ${GALLIUM_AUX_ROOT}/draw/draw_nir_lower_opcodes.py
        COMMENT "Generating gallium/auxiliary/draw_nir_lower_opcodes.c (Python codegen)"
        VERBATIM
)

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
        ${DRAW_NIR_LOWER_OPCODES_C}
)

# ────────────────────────────────────────────────────────────────
# libgallium — meson.build Zeilen 510-524.
# ────────────────────────────────────────────────────────────────
add_library(mesa_gallium_aux STATIC
        ${GALLIUM_AUX_SOURCES}
        ${GALLIUM_AUX_CSO_SOURCES}
        ${GALLIUM_AUX_TRANSLATE_SOURCES}
        ${GALLIUM_AUX_RTASM_SOURCES}
        ${GALLIUM_AUX_INDICES_SOURCES}
        ${GALLIUM_AUX_TGSI_SOURCES}
        ${GALLIUM_AUX_DRAW_SOURCES}
        ${DRAW_NIR_LOWER_OPCODES_C}
        ${GALLIUM_AUX_DRIVER_TRACE_SOURCES}
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
        ${MESA_ROOT}/src/gallium/winsys
        ${MESA_ROOT}/src
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src/loader
        ${GALLIUM_AUX_GEN_DIR}
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_gallium_aux PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${MESA_COMMON_C_FLAGS}>
        $<$<COMPILE_LANGUAGE:CXX>:${MESA_COMMON_CXX_FLAGS}>
)

target_compile_options(mesa_gallium_aux PRIVATE -fvisibility=hidden)

target_compile_definitions(mesa_gallium_aux PRIVATE HAVE_DRI)

target_link_libraries(mesa_gallium_aux PUBLIC
        mesa_util
        mesa_nir
        swdri
        wsw
        ws_null
        vesplib
)

add_dependencies(mesa_gallium_aux
        sysroot_headers
        mesa_gallium_aux_gen
        mesa_util
        mesa_nir
        swdri
        wsw
        ws_null
)
