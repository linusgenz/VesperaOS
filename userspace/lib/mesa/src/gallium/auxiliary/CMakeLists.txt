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
        # nir/tgsi_to_nir.c + nir_to_tgsi.c haengen NICHT an
        # with_gfx_compute (stehen in meson.build ausserhalb des
        # if with_gfx_compute-Blocks-Aequivalents -- Kernliste).
        # iris nutzt intern NIR direkt, aber tgsi_to_nir/nir_to_tgsi
        # werden von gallium-generischem Code (u_threaded_context,
        # Treiber-Interop) potenziell referenziert; mit aufgenommen,
        # da sie nicht am with_gfx_compute-Gate haengen.
        ${GALLIUM_AUX_ROOT}/nir/nir_draw_helpers.c
        ${GALLIUM_AUX_ROOT}/nir/tgsi_to_nir.c
        ${GALLIUM_AUX_ROOT}/nir/nir_to_tgsi.c
        #  ${GALLIUM_AUX_ROOT}/nir/nir_to_tgsi_info.c
)

# ────────────────────────────────────────────────────────────────
# cso_cache/ — meson.build Zeilen 113-118, liegt strukturell INNERHALB
# des with_gfx_compute-Blocks (siehe Kopfkommentar), hat aber KEINE
# funktionale Abhaengigkeit zu draw/gallivm: cso_cache ist reines
# High-Level State-Object-Caching/Deduplizierung fuer pipe_context
# (Blend/Rasterizer/DSA/Sampler-State-Objekte), das der State-Tracker
# (state_tracker/st_atom_*.c, st_cb_*.c) direkt nutzt, unabhaengig
# vom Software-Vertex-Pipeline-Pfad. Bewusst separat aus dem
# with_gfx_compute-Block herausgezogen, da es fuer den iris-Pfad
# gebraucht wird, ohne draw/gallivm nachzuziehen.
# ────────────────────────────────────────────────────────────────
set(GALLIUM_AUX_CSO_SOURCES
        ${GALLIUM_AUX_ROOT}/cso_cache/cso_cache.c
        ${GALLIUM_AUX_ROOT}/cso_cache/cso_cache.h
        ${GALLIUM_AUX_ROOT}/cso_cache/cso_context.c
        ${GALLIUM_AUX_ROOT}/cso_cache/cso_context.h
        ${GALLIUM_AUX_ROOT}/cso_cache/cso_hash.c
        ${GALLIUM_AUX_ROOT}/cso_cache/cso_hash.h
)

# ────────────────────────────────────────────────────────────────
# translate/ — meson.build Zeilen 259-264, liegt ebenfalls strukturell
# INNERHALB des with_gfx_compute-Blocks, wird aber von util/u_vbuf.c
# gebraucht (bereits unconditional Teil von GALLIUM_AUX_SOURCES oben,
# meson.build Zeile 103-104) — u_vbuf ist der generische Vertex-Buffer-
# Emulationslayer, den Treiber fuer von der Hardware nicht direkt
# unterstuetzte Vertex-Formate nutzen. translate_generic.c ist der
# portable C-Pfad; translate_sse.c ist eine optionale SSE-Beschleunigung
# (x86-spezifisch, fuer uns unproblematisch, da VesperaOS x86_64-only
# ist). Bewusst separat aus dem with_gfx_compute-Block herausgezogen,
# analog zu cso_cache/ oben.
# ────────────────────────────────────────────────────────────────
set(GALLIUM_AUX_TRANSLATE_SOURCES
        ${GALLIUM_AUX_ROOT}/translate/translate.c
        ${GALLIUM_AUX_ROOT}/translate/translate.h
        ${GALLIUM_AUX_ROOT}/translate/translate_cache.c
        ${GALLIUM_AUX_ROOT}/translate/translate_cache.h
        ${GALLIUM_AUX_ROOT}/translate/translate_generic.c
        ${GALLIUM_AUX_ROOT}/translate/translate_sse.c
)

# ────────────────────────────────────────────────────────────────
# rtasm/ — meson.build Zeilen 217-220, liegt ebenfalls strukturell
# INNERHALB des with_gfx_compute-Blocks. Runtime-Assembler: jittet zur
# Laufzeit x86-SSE-Maschinencode fuer translate_cache.c (spezialisierte
# Vertex-Attribut-Konvertierungsroutinen statt generischem interpretiertem
# Code). translate/ haengt direkt daran, siehe oben.
#
# ACHTUNG: rtasm_execmem.c braucht ausfuehrbaren, beschreibbaren Speicher
# (klassisch via mmap(PROT_READ|PROT_WRITE|PROT_EXEC, ...) oder aequivalent).
# TODO(vespera): Pruefen, ob euer Speicherverwaltungs-Syscall-Interface das
# in dieser Form hergibt -- falls nicht, ist das der erste Stolperstein,
# entweder beim Kompilieren (fehlende mmap-Flags in eurer libc) oder zur
# Laufzeit (SIGSEGV/SIGILL beim Ausfuehren des gejitteten Codes).
# ────────────────────────────────────────────────────────────────
set(GALLIUM_AUX_RTASM_SOURCES
        ${GALLIUM_AUX_ROOT}/rtasm/rtasm_execmem.c
        ${GALLIUM_AUX_ROOT}/rtasm/rtasm_execmem.h
        ${GALLIUM_AUX_ROOT}/rtasm/rtasm_x86sse.c
        ${GALLIUM_AUX_ROOT}/rtasm/rtasm_x86sse.h
)

# ────────────────────────────────────────────────────────────────
# indices/ — meson.build Zeilen 213-216, liegt strukturell INNERHALB
# des with_gfx_compute-Blocks. util_primconvert_* wird von u_vbuf.c
# direkt aufgerufen (u_vbuf_draw_vbo -> util_primconvert_draw_vbo),
# also unconditional gebraucht, analog zu cso_cache/translate/rtasm.
#
# u_indices_gen.c/u_unfilled_gen.c (die Codegen-Ausgaben) sind bei uns
# bereits unconditional Teil des Kern-Layers weiter oben in dieser
# Datei -- indices/u_primconvert.c ist die noch fehlende statische
# Quelle aus demselben Verzeichnis.
# ────────────────────────────────────────────────────────────────
set(GALLIUM_AUX_INDICES_SOURCES
        ${GALLIUM_AUX_ROOT}/indices/u_indices.h
        ${GALLIUM_AUX_ROOT}/indices/u_indices_priv.h
        ${GALLIUM_AUX_ROOT}/indices/u_primconvert.c
        ${GALLIUM_AUX_ROOT}/indices/u_primconvert.h
)

# ────────────────────────────────────────────────────────────────
# tgsi/ — meson.build Zeilen 221-258, liegt strukturell INNERHALB
# des with_gfx_compute-Blocks. TGSI-IR (Parsing/Dump/Scan/Transform)
# wird vom state_tracker direkt fuer Kompat-Funktionalitaet gebraucht
# (tgsi_get_generic_gl_varying_index in st_program.h) -- bestaetigt
# als tatsaechlich unconditional benoetigt, analog zu cso_cache/
# translate/rtasm/indices/draw oben. Der eigene TODO-Kommentar von
# weiter oben in dieser Datei ("falls undefined reference auf tgsi_*
# Symbole auftaucht") hat sich bestaetigt.
#
# 1:1 aus meson.build uebernommen, keine Datei ausgelassen.
# ────────────────────────────────────────────────────────────────
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

# ────────────────────────────────────────────────────────────────
# driver_trace/ — meson.build Zeilen 180-200 (Kernblock, dd_* separat
# behandelt) sind bereits als dd_context.c/dd_screen.c/dd_pipe.h/
# dd_public.h TEIL des with_gfx_compute-Blocks; die driver_trace/tr_*
# Dateien selbst waren bislang NUR ueber enums2names.py (fuer den
# tr_util.c/h Codegen) referenziert, nie als eigene Sourcen-Liste
# gebaut -- trace_dumping_* (aus tr_dump.c) wird von cso_context.c
# direkt aufgerufen, ist also unconditional noetig, analog zu allen
# vorherigen Blocken. 1:1 aus meson.build uebernommen.
# ────────────────────────────────────────────────────────────────
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

# ────────────────────────────────────────────────────────────────
# draw/ — meson.build Zeilen 119-179 (+ Custom-Target Zeilen 275-296).
# Liegt strukturell INNERHALB des with_gfx_compute-Blocks (Software-
# Vertex-Pipeline), wird aber vom state_tracker fuer Kern-Funktionalitaet
# gebraucht, die nicht optional ist: glRasterPos (st_cb_rasterpos.c),
# GL_FEEDBACK/GL_SELECT Rendermodes (st_cb_feedback.c, st_draw_feedback.c),
# und den generischen Vertex-Shader-Pfad in st_draw.c/st_program.c.
# Bewusst 1:1 aus meson.build uebernommen, analog zu cso_cache/translate/
# rtasm/indices oben -- keine Dateien aus diesem Block ausgelassen.
#
# NICHT enthalten: die drei gallivm-abhaengigen draw_llvm*.c-Dateien
# (meson.build Zeilen 370-373, liegen im *verschachtelten* llvm-gated
# Unterblock, nicht im hier portierten draw/-Kernblock) sowie
# tessellator/ (Zeilen 374-377, ebenfalls im llvm-Unterblock). Falls
# spaeter Tessellation-Shader ueber den draw-Software-Pfad noetig
# werden, muss das zusammen mit gallivm/ nachgezogen werden.
# ────────────────────────────────────────────────────────────────
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

# draw_nir_lower_opcodes.c — meson.build Zeilen 275-283. Python-Codegen,
# analog zum bereits vorhandenen u_indices_gen.c/u_unfilled_gen.c-Muster.
# -p erwartet das nir-Quellverzeichnis fuer die Algebraic-Replacement-
# Regeln, die das Skript einliest.
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
