# src/mesa/CMakeLists.txt
#
# Portierung von src/mesa/meson.build.

set(MESA_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(MESA_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})

# --- Unterverzeichnisse einbinden -------------------------------------------
add_subdirectory(glapi)
add_subdirectory(program)

# --- Python Code-Generatoren -----------------------------------------------

# 1. format_fallback.c
set(FORMAT_FALLBACK_C ${MESA_GEN_DIR}/format_fallback.c)
add_custom_command(
        OUTPUT ${FORMAT_FALLBACK_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${MESA_SRC_DIR}/main/format_fallback.py ${MESA_SRC_DIR}/main/formats.csv ${FORMAT_FALLBACK_C}
        DEPENDS ${MESA_SRC_DIR}/main/format_fallback.py ${MESA_SRC_DIR}/main/formats.csv ${MESA_SRC_DIR}/main/format_parser.py
        COMMENT "Generating main/format_fallback.c"
        VERBATIM
)

# 2. get_hash.h
set(GET_HASH_H ${MESA_GEN_DIR}/get_hash.h)
add_custom_command(
        OUTPUT ${GET_HASH_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${MESA_SRC_DIR}/main/get_hash_generator.py -f ${MESA_ROOT}/src/mesa/glapi/glapi/gen/gl_and_es_API.xml > ${GET_HASH_H}
        DEPENDS ${MESA_SRC_DIR}/main/get_hash_generator.py ${MESA_ROOT}/src/mesa/glapi/glapi/gen/gl_and_es_API.xml ${MESA_SRC_DIR}/main/get_hash_params.py ${GLAPI_XML_PY_DEPS}
        COMMENT "Generating main/get_hash.h"
        VERBATIM
)

# 3. format_info.h
set(FORMAT_INFO_H ${MESA_GEN_DIR}/format_info.h)
add_custom_command(
        OUTPUT ${FORMAT_INFO_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${MESA_SRC_DIR}/main/format_info.py ${MESA_SRC_DIR}/main/formats.csv > ${FORMAT_INFO_H}
        DEPENDS ${MESA_SRC_DIR}/main/format_info.py ${MESA_SRC_DIR}/main/formats.csv ${MESA_SRC_DIR}/main/format_parser.py
        COMMENT "Generating main/format_info.h"
        VERBATIM
)

# Optional: Shader Replacement
if(SHADER_REPLACEMENT_DIR)
    set(SHADER_REPLACEMENT_H ${MESA_GEN_DIR}/shader_replacement.h)
    add_custom_command(
            OUTPUT ${SHADER_REPLACEMENT_H}
            COMMAND ${MESA_PYTHON3_EXECUTABLE} ${SHADER_REPLACEMENT_DIR}/process_shaders.py ${SHADER_REPLACEMENT_DIR} ${SHADER_REPLACEMENT_H}
            DEPENDS ${SHADER_REPLACEMENT_DIR}/process_shaders.py
            COMMENT "Generating main/shader_replacement.h"
            VERBATIM
    )
endif()

# --- Quelldateien für libmesa ------------------------------------------------
set(FILES_LIBMESA
        ${MESA_SRC_DIR}/main/accum.c
        ${MESA_SRC_DIR}/main/accum.h
        ${MESA_SRC_DIR}/main/api_arrayelt.c
        ${MESA_SRC_DIR}/main/api_arrayelt.h
        ${MESA_SRC_DIR}/main/api_trace_helpers.c
        ${MESA_SRC_DIR}/main/api_trace_helpers.h
        ${MESA_SRC_DIR}/main/arbprogram.c
        ${MESA_SRC_DIR}/main/arrayobj.c
        ${MESA_SRC_DIR}/main/arrayobj.h
        ${MESA_SRC_DIR}/main/atifragshader.c
        ${MESA_SRC_DIR}/main/atifragshader.h
        ${MESA_SRC_DIR}/main/attrib.c
        ${MESA_SRC_DIR}/main/attrib.h
        ${MESA_SRC_DIR}/main/barrier.c
        ${MESA_SRC_DIR}/main/bbox.c
        ${MESA_SRC_DIR}/main/bbox.h
        ${MESA_SRC_DIR}/main/blend.c
        ${MESA_SRC_DIR}/main/blend.h
        ${MESA_SRC_DIR}/main/blit.c
        ${MESA_SRC_DIR}/main/blit.h
        ${MESA_SRC_DIR}/main/bufferobj.c
        ${MESA_SRC_DIR}/main/bufferobj.h
        ${MESA_SRC_DIR}/main/buffers.c
        ${MESA_SRC_DIR}/main/buffers.h
        ${MESA_SRC_DIR}/main/clear.c
        ${MESA_SRC_DIR}/main/clip.c
        ${MESA_SRC_DIR}/main/clip.h
        ${MESA_SRC_DIR}/main/colormac.h
        ${MESA_SRC_DIR}/main/compute.c
        ${MESA_SRC_DIR}/main/condrender.c
        ${MESA_SRC_DIR}/main/condrender.h
        ${MESA_SRC_DIR}/main/config.h
        ${MESA_SRC_DIR}/main/conservativeraster.c
        ${MESA_SRC_DIR}/main/conservativeraster.h
        ${MESA_SRC_DIR}/main/context.c
        ${MESA_SRC_DIR}/main/context.h
        ${MESA_SRC_DIR}/main/copyimage.c
        ${MESA_SRC_DIR}/main/dd.h
        ${MESA_SRC_DIR}/main/debug.c
        ${MESA_SRC_DIR}/main/debug.h
        ${MESA_SRC_DIR}/main/debug_output.c
        ${MESA_SRC_DIR}/main/debug_output.h
        ${MESA_SRC_DIR}/main/depth.c
        ${MESA_SRC_DIR}/main/depth.h
        ${MESA_SRC_DIR}/main/dlist.c
        ${MESA_SRC_DIR}/main/dlist.h
        ${MESA_SRC_DIR}/main/draw.c
        ${MESA_SRC_DIR}/main/draw.h
        ${MESA_SRC_DIR}/main/draw_validate.c
        ${MESA_SRC_DIR}/main/draw_validate.h
        ${MESA_SRC_DIR}/main/drawpix.c
        ${MESA_SRC_DIR}/main/drawtex.c
        ${MESA_SRC_DIR}/main/enable.c
        ${MESA_SRC_DIR}/main/enable.h
        ${MESA_SRC_DIR}/main/enums.h
        ${MESA_SRC_DIR}/main/errors.c
        ${MESA_SRC_DIR}/main/errors.h
        ${MESA_SRC_DIR}/main/es1_conversion.c
        ${MESA_SRC_DIR}/main/eval.c
        ${MESA_SRC_DIR}/main/eval.h
        ${MESA_SRC_DIR}/main/extensions.c
        ${MESA_SRC_DIR}/main/extensions.h
        ${MESA_SRC_DIR}/main/extensions_table.c
        ${MESA_SRC_DIR}/main/extensions_table.h
        ${MESA_SRC_DIR}/main/externalobjects.c
        ${MESA_SRC_DIR}/main/externalobjects.h
        ${MESA_SRC_DIR}/main/fbobject.c
        ${MESA_SRC_DIR}/main/fbobject.h
        ${MESA_SRC_DIR}/main/feedback.c
        ${MESA_SRC_DIR}/main/feedback.h
        ${MESA_SRC_DIR}/main/ff_fragment_shader.c
        ${MESA_SRC_DIR}/main/ffvertex_prog.c
        ${MESA_SRC_DIR}/main/ffvertex_prog.h
        ${MESA_SRC_DIR}/main/fog.c
        ${MESA_SRC_DIR}/main/fog.h
        ${MESA_SRC_DIR}/main/format_pack.h
        ${MESA_SRC_DIR}/main/format_unpack.h
        ${MESA_SRC_DIR}/main/format_utils.c
        ${MESA_SRC_DIR}/main/format_utils.h
        ${MESA_SRC_DIR}/main/formatquery.c
        ${MESA_SRC_DIR}/main/formatquery.h
        ${MESA_SRC_DIR}/main/formats.c
        ${MESA_SRC_DIR}/main/formats.h
        ${MESA_SRC_DIR}/main/framebuffer.c
        ${MESA_SRC_DIR}/main/framebuffer.h
        ${MESA_SRC_DIR}/main/genmipmap.c
        ${MESA_SRC_DIR}/main/genmipmap.h
        ${MESA_SRC_DIR}/main/get.c
        ${MESA_SRC_DIR}/main/get.h
        ${MESA_SRC_DIR}/main/getstring.c
        ${MESA_SRC_DIR}/main/glformats.c
        ${MESA_SRC_DIR}/main/glformats.h
        ${MESA_SRC_DIR}/main/glspirv.c
        ${MESA_SRC_DIR}/main/glspirv.h
        ${MESA_SRC_DIR}/main/glthread.c
        ${MESA_SRC_DIR}/main/glthread.h
        ${MESA_SRC_DIR}/main/glthread_bufferobj.c
        ${MESA_SRC_DIR}/main/glthread_draw.c
        ${MESA_SRC_DIR}/main/glthread_draw_unroll.c
        ${MESA_SRC_DIR}/main/glthread_get.c
        ${MESA_SRC_DIR}/main/glthread_list.c
        ${MESA_SRC_DIR}/main/glthread_marshal.h
        ${MESA_SRC_DIR}/main/glthread_pixels.c
        ${MESA_SRC_DIR}/main/glthread_shaderobj.c
        ${MESA_SRC_DIR}/main/glthread_varray.c
        ${MESA_SRC_DIR}/main/hash.c
        ${MESA_SRC_DIR}/main/hash.h
        ${MESA_SRC_DIR}/main/hint.c
        ${MESA_SRC_DIR}/main/hint.h
        ${MESA_SRC_DIR}/main/image.c
        ${MESA_SRC_DIR}/main/image.h
        ${MESA_SRC_DIR}/main/light.c
        ${MESA_SRC_DIR}/main/light.h
        ${MESA_SRC_DIR}/main/lines.c
        ${MESA_SRC_DIR}/main/lines.h
        ${MESA_SRC_DIR}/main/macros.h
        ${MESA_SRC_DIR}/main/matrix.c
        ${MESA_SRC_DIR}/main/matrix.h
        ${MESA_SRC_DIR}/main/menums.h
        ${MESA_SRC_DIR}/main/mesa_private.h
        ${MESA_SRC_DIR}/main/mesh_shader.c
        ${MESA_SRC_DIR}/main/mipmap.c
        ${MESA_SRC_DIR}/main/mipmap.h
        ${MESA_SRC_DIR}/main/mtypes.h
        ${MESA_SRC_DIR}/main/multisample.c
        ${MESA_SRC_DIR}/main/multisample.h
        ${MESA_SRC_DIR}/main/objectlabel.c
        ${MESA_SRC_DIR}/main/pack.c
        ${MESA_SRC_DIR}/main/pack.h
        ${MESA_SRC_DIR}/main/pbo.c
        ${MESA_SRC_DIR}/main/pbo.h
        ${MESA_SRC_DIR}/main/performance_monitor.c
        ${MESA_SRC_DIR}/main/performance_monitor.h
        ${MESA_SRC_DIR}/main/performance_query.c
        ${MESA_SRC_DIR}/main/performance_query.h
        ${MESA_SRC_DIR}/main/pipelineobj.c
        ${MESA_SRC_DIR}/main/pipelineobj.h
        ${MESA_SRC_DIR}/main/pixel.c
        ${MESA_SRC_DIR}/main/pixel.h
        ${MESA_SRC_DIR}/main/pixelstore.c
        ${MESA_SRC_DIR}/main/pixelstore.h
        ${MESA_SRC_DIR}/main/pixeltransfer.c
        ${MESA_SRC_DIR}/main/pixeltransfer.h
        ${MESA_SRC_DIR}/main/points.c
        ${MESA_SRC_DIR}/main/points.h
        ${MESA_SRC_DIR}/main/polygon.c
        ${MESA_SRC_DIR}/main/polygon.h
        ${MESA_SRC_DIR}/main/program_binary.c
        ${MESA_SRC_DIR}/main/program_binary.h
        ${MESA_SRC_DIR}/main/program_resource.c
        ${MESA_SRC_DIR}/main/querymatrix.c
        ${MESA_SRC_DIR}/main/queryobj.c
        ${MESA_SRC_DIR}/main/queryobj.h
        ${MESA_SRC_DIR}/main/rastpos.c
        ${MESA_SRC_DIR}/main/rastpos.h
        ${MESA_SRC_DIR}/main/readpix.c
        ${MESA_SRC_DIR}/main/readpix.h
        ${MESA_SRC_DIR}/main/renderbuffer.c
        ${MESA_SRC_DIR}/main/renderbuffer.h
        ${MESA_SRC_DIR}/main/robustness.c
        ${MESA_SRC_DIR}/main/samplerobj.c
        ${MESA_SRC_DIR}/main/samplerobj.h
        ${MESA_SRC_DIR}/main/scissor.c
        ${MESA_SRC_DIR}/main/scissor.h
        ${MESA_SRC_DIR}/main/shader_query.cpp
        ${MESA_SRC_DIR}/main/shaderapi.c
        ${MESA_SRC_DIR}/main/shaderapi.h
        ${MESA_SRC_DIR}/main/shaderimage.c
        ${MESA_SRC_DIR}/main/shaderimage.h
        ${MESA_SRC_DIR}/main/shaderobj.c
        ${MESA_SRC_DIR}/main/shaderobj.h
        ${MESA_SRC_DIR}/main/shared.c
        ${MESA_SRC_DIR}/main/shared.h
        ${MESA_SRC_DIR}/main/spirv_capabilities.c
        ${MESA_SRC_DIR}/main/spirv_capabilities.h
        ${MESA_SRC_DIR}/main/spirv_extensions.c
        ${MESA_SRC_DIR}/main/spirv_extensions.h
        ${MESA_SRC_DIR}/main/state.c
        ${MESA_SRC_DIR}/main/state.h
        ${MESA_SRC_DIR}/main/stencil.c
        ${MESA_SRC_DIR}/main/stencil.h
        ${MESA_SRC_DIR}/main/syncobj.c
        ${MESA_SRC_DIR}/main/syncobj.h
        ${MESA_SRC_DIR}/main/texcompress.c
        ${MESA_SRC_DIR}/main/texcompress.h
        ${MESA_SRC_DIR}/main/texcompress_bptc.c
        ${MESA_SRC_DIR}/main/texcompress_bptc.h
        ${MESA_SRC_DIR}/main/texcompress_cpal.c
        ${MESA_SRC_DIR}/main/texcompress_cpal.h
        ${MESA_SRC_DIR}/main/texcompress_etc.c
        ${MESA_SRC_DIR}/main/texcompress_etc.h
        ${MESA_SRC_DIR}/main/texcompress_fxt1.c
        ${MESA_SRC_DIR}/main/texcompress_fxt1.h
        ${MESA_SRC_DIR}/main/texcompress_rgtc.c
        ${MESA_SRC_DIR}/main/texcompress_rgtc.h
        ${MESA_SRC_DIR}/main/texcompress_s3tc.c
        ${MESA_SRC_DIR}/main/texcompress_s3tc.h
        ${MESA_SRC_DIR}/main/texenv.c
        ${MESA_SRC_DIR}/main/texenvprogram.h
        ${MESA_SRC_DIR}/main/texgen.c
        ${MESA_SRC_DIR}/main/texgetimage.c
        ${MESA_SRC_DIR}/main/texgetimage.h
        ${MESA_SRC_DIR}/main/teximage.c
        ${MESA_SRC_DIR}/main/teximage.h
        ${MESA_SRC_DIR}/main/texobj.c
        ${MESA_SRC_DIR}/main/texobj.h
        ${MESA_SRC_DIR}/main/texparam.c
        ${MESA_SRC_DIR}/main/texparam.h
        ${MESA_SRC_DIR}/main/texstate.c
        ${MESA_SRC_DIR}/main/texstate.h
        ${MESA_SRC_DIR}/main/texstorage.c
        ${MESA_SRC_DIR}/main/texstorage.h
        ${MESA_SRC_DIR}/main/texstore.c
        ${MESA_SRC_DIR}/main/texstore.h
        ${MESA_SRC_DIR}/main/texturebindless.c
        ${MESA_SRC_DIR}/main/texturebindless.h
        ${MESA_SRC_DIR}/main/textureview.c
        ${MESA_SRC_DIR}/main/textureview.h
        ${MESA_SRC_DIR}/main/transformfeedback.c
        ${MESA_SRC_DIR}/main/transformfeedback.h
        ${MESA_SRC_DIR}/main/uniform_query.cpp
        ${MESA_SRC_DIR}/main/uniforms.c
        ${MESA_SRC_DIR}/main/uniforms.h
        ${MESA_SRC_DIR}/main/varray.c
        ${MESA_SRC_DIR}/main/varray.h
        ${MESA_SRC_DIR}/main/version.c
        ${MESA_SRC_DIR}/main/version.h
        ${MESA_SRC_DIR}/main/viewport.c
        ${MESA_SRC_DIR}/main/viewport.h
        ${MESA_SRC_DIR}/math/m_eval.c
        ${MESA_SRC_DIR}/math/m_eval.h
        ${MESA_SRC_DIR}/math/m_matrix.c
        ${MESA_SRC_DIR}/math/m_matrix.h
        ${MESA_SRC_DIR}/program/arbprogparse.c
        ${MESA_SRC_DIR}/program/arbprogparse.h
        ${MESA_SRC_DIR}/program/prog_cache.c
        ${MESA_SRC_DIR}/program/prog_cache.h
        ${MESA_SRC_DIR}/program/prog_instruction.c
        ${MESA_SRC_DIR}/program/prog_instruction.h
        ${MESA_SRC_DIR}/program/prog_parameter.c
        ${MESA_SRC_DIR}/program/prog_parameter.h
        ${MESA_SRC_DIR}/program/prog_parameter_layout.c
        ${MESA_SRC_DIR}/program/prog_parameter_layout.h
        ${MESA_SRC_DIR}/program/prog_print.c
        ${MESA_SRC_DIR}/program/prog_print.h
        ${MESA_SRC_DIR}/program/prog_statevars.c
        ${MESA_SRC_DIR}/program/prog_statevars.h
        ${MESA_SRC_DIR}/program/prog_to_nir.c
        ${MESA_SRC_DIR}/program/prog_to_nir.h
        ${MESA_SRC_DIR}/program/program.c
        ${MESA_SRC_DIR}/program/program.h
        ${MESA_SRC_DIR}/program/program_parse_extra.c
        ${MESA_SRC_DIR}/program/program_parser.h
        ${MESA_SRC_DIR}/program/symbol_table.c
        ${MESA_SRC_DIR}/program/symbol_table.h
        ${MESA_SRC_DIR}/state_tracker/st_atifs_to_nir.c
        ${MESA_SRC_DIR}/state_tracker/st_atifs_to_nir.h
        ${MESA_SRC_DIR}/state_tracker/st_atom_array.cpp
        ${MESA_SRC_DIR}/state_tracker/st_atom_atomicbuf.c
        ${MESA_SRC_DIR}/state_tracker/st_atom_blend.c
        ${MESA_SRC_DIR}/state_tracker/st_atom_clip.c
        ${MESA_SRC_DIR}/state_tracker/st_atom_constbuf.c
        ${MESA_SRC_DIR}/state_tracker/st_atom_constbuf.h
        ${MESA_SRC_DIR}/state_tracker/st_atom_depth.c
        ${MESA_SRC_DIR}/state_tracker/st_atom_framebuffer.c
        ${MESA_SRC_DIR}/state_tracker/st_atom_image.c
        ${MESA_SRC_DIR}/state_tracker/st_atom_list.h
        ${MESA_SRC_DIR}/state_tracker/st_atom_msaa.c
        ${MESA_SRC_DIR}/state_tracker/st_atom_pixeltransfer.c
        ${MESA_SRC_DIR}/state_tracker/st_atom_rasterizer.c
        ${MESA_SRC_DIR}/state_tracker/st_atom_sampler.c
        ${MESA_SRC_DIR}/state_tracker/st_atom_scissor.c
        ${MESA_SRC_DIR}/state_tracker/st_atom_shader.c
        ${MESA_SRC_DIR}/state_tracker/st_atom_stipple.c
        ${MESA_SRC_DIR}/state_tracker/st_atom_storagebuf.c
        ${MESA_SRC_DIR}/state_tracker/st_atom_tess.c
        ${MESA_SRC_DIR}/state_tracker/st_atom_texture.c
        ${MESA_SRC_DIR}/state_tracker/st_atom_viewport.c
        ${MESA_SRC_DIR}/state_tracker/st_bc1_tables.h
        ${MESA_SRC_DIR}/state_tracker/st_cb_bitmap.c
        ${MESA_SRC_DIR}/state_tracker/st_cb_bitmap.h
        ${MESA_SRC_DIR}/state_tracker/st_cb_clear.c
        ${MESA_SRC_DIR}/state_tracker/st_cb_clear.h
        ${MESA_SRC_DIR}/state_tracker/st_cb_copyimage.c
        ${MESA_SRC_DIR}/state_tracker/st_cb_copyimage.h
        ${MESA_SRC_DIR}/state_tracker/st_cb_drawpixels.c
        ${MESA_SRC_DIR}/state_tracker/st_cb_drawpixels.h
        ${MESA_SRC_DIR}/state_tracker/st_cb_drawtex.c
        ${MESA_SRC_DIR}/state_tracker/st_cb_drawtex.h
        ${MESA_SRC_DIR}/state_tracker/st_cb_eglimage.c
        ${MESA_SRC_DIR}/state_tracker/st_cb_eglimage.h
        ${MESA_SRC_DIR}/state_tracker/st_cb_feedback.c
        ${MESA_SRC_DIR}/state_tracker/st_cb_feedback.h
        ${MESA_SRC_DIR}/state_tracker/st_cb_flush.c
        ${MESA_SRC_DIR}/state_tracker/st_cb_flush.h
        ${MESA_SRC_DIR}/state_tracker/st_cb_rasterpos.c
        ${MESA_SRC_DIR}/state_tracker/st_cb_rasterpos.h
        ${MESA_SRC_DIR}/state_tracker/st_cb_readpixels.c
        ${MESA_SRC_DIR}/state_tracker/st_cb_readpixels.h
        ${MESA_SRC_DIR}/state_tracker/st_cb_texture.c
        ${MESA_SRC_DIR}/state_tracker/st_cb_texture.h
        ${MESA_SRC_DIR}/state_tracker/st_context.c
        ${MESA_SRC_DIR}/state_tracker/st_context.h
        ${MESA_SRC_DIR}/state_tracker/st_copytex.c
        ${MESA_SRC_DIR}/state_tracker/st_copytex.h
        ${MESA_SRC_DIR}/state_tracker/st_debug.c
        ${MESA_SRC_DIR}/state_tracker/st_debug.h
        ${MESA_SRC_DIR}/state_tracker/st_draw.c
        ${MESA_SRC_DIR}/state_tracker/st_draw.h
        ${MESA_SRC_DIR}/state_tracker/st_draw_feedback.c
        ${MESA_SRC_DIR}/state_tracker/st_draw_hw_select.c
        ${MESA_SRC_DIR}/state_tracker/st_extensions.c
        ${MESA_SRC_DIR}/state_tracker/st_extensions.h
        ${MESA_SRC_DIR}/state_tracker/st_format.c
        ${MESA_SRC_DIR}/state_tracker/st_format.h
        ${MESA_SRC_DIR}/state_tracker/st_gen_mipmap.c
        ${MESA_SRC_DIR}/state_tracker/st_gen_mipmap.h
        ${MESA_SRC_DIR}/state_tracker/st_glsl_to_nir.cpp
        ${MESA_SRC_DIR}/state_tracker/st_glsl_to_nir.h
        ${MESA_SRC_DIR}/state_tracker/st_interop.c
        ${MESA_SRC_DIR}/state_tracker/st_interop.h
        ${MESA_SRC_DIR}/state_tracker/st_manager.c
        ${MESA_SRC_DIR}/state_tracker/st_manager.h
        ${MESA_SRC_DIR}/state_tracker/st_nir.h
        ${MESA_SRC_DIR}/state_tracker/st_nir_builtins.c
        ${MESA_SRC_DIR}/state_tracker/st_nir_lower_alpha_test.c
        ${MESA_SRC_DIR}/state_tracker/st_nir_lower_builtin.c
        ${MESA_SRC_DIR}/state_tracker/st_nir_lower_drawpixels.c
        ${MESA_SRC_DIR}/state_tracker/st_nir_lower_fog.c
        ${MESA_SRC_DIR}/state_tracker/st_nir_lower_point_size_mov.c
        ${MESA_SRC_DIR}/state_tracker/st_nir_lower_position_invariant.c
        ${MESA_SRC_DIR}/state_tracker/st_nir_lower_tex_src_plane.c
        ${MESA_SRC_DIR}/state_tracker/st_pbo.c
        ${MESA_SRC_DIR}/state_tracker/st_pbo_compute.c
        ${MESA_SRC_DIR}/state_tracker/st_pbo.h
        ${MESA_SRC_DIR}/state_tracker/st_program.c
        ${MESA_SRC_DIR}/state_tracker/st_program.h
        ${MESA_SRC_DIR}/state_tracker/st_sampler_view.c
        ${MESA_SRC_DIR}/state_tracker/st_sampler_view.h
        ${MESA_SRC_DIR}/state_tracker/st_scissor.c
        ${MESA_SRC_DIR}/state_tracker/st_scissor.h
        ${MESA_SRC_DIR}/state_tracker/st_shader_cache.c
        ${MESA_SRC_DIR}/state_tracker/st_shader_cache.h
        ${MESA_SRC_DIR}/state_tracker/st_texcompress_compute.c
        ${MESA_SRC_DIR}/state_tracker/st_texcompress_compute.h
        ${MESA_SRC_DIR}/state_tracker/st_texture.c
        ${MESA_SRC_DIR}/state_tracker/st_texture.h
        ${MESA_SRC_DIR}/state_tracker/st_util.h
        ${MESA_SRC_DIR}/vbo/vbo.h
        ${MESA_SRC_DIR}/vbo/vbo_attrib.h
        ${MESA_SRC_DIR}/vbo/vbo_attrib_tmp.h
        ${MESA_SRC_DIR}/vbo/vbo_context.c
        ${MESA_SRC_DIR}/vbo/vbo_exec.c
        ${MESA_SRC_DIR}/vbo/vbo_exec.h
        ${MESA_SRC_DIR}/vbo/vbo_exec_api.c
        ${MESA_SRC_DIR}/vbo/vbo_exec_draw.c
        ${MESA_SRC_DIR}/vbo/vbo_exec_eval.c
        ${MESA_SRC_DIR}/vbo/vbo_minmax_index.c
        ${MESA_SRC_DIR}/vbo/vbo_noop.c
        ${MESA_SRC_DIR}/vbo/vbo_save.c
        ${MESA_SRC_DIR}/vbo/vbo_save.h
        ${MESA_SRC_DIR}/vbo/vbo_save_api.c
        ${MESA_SRC_DIR}/vbo/vbo_save_draw.c
        ${MESA_SRC_DIR}/vbo/vbo_save_loopback.c
        ${MESA_SRC_DIR}/vbo/vbo_util.h
)

# --- SSE4.1 Optimierungs-Bibliothek (Optional) ------------------------------
if(WITH_SSE41)
    add_library(mesa_sse41 STATIC ${MESA_SRC_DIR}/main/sse_minmax.c)

    set_target_properties(mesa_sse41 PROPERTIES
            C_VISIBILITY_PRESET hidden
    )

    if(SSE41_ARGS)
        target_compile_options(mesa_sse41 PRIVATE ${SSE41_ARGS})
    endif()

    target_include_directories(mesa_sse41 PRIVATE
            ${MESA_ROOT}/include
            ${MESA_ROOT}/src
            ${MESA_SRC_DIR}/main
    )

    set(LIBMESA_SSE41 mesa_sse41)
else()
    set(LIBMESA_SSE41 "")
endif()

# --- Haupt-Bibliothek: libmesa ----------------------------------------------
add_library(mesa STATIC
        ${FILES_LIBMESA}
        ${FORMAT_FALLBACK_C}
        ${GET_HASH_H}
        ${FORMAT_INFO_H}
        $<$<BOOL:${SHADER_REPLACEMENT_DIR}>:${SHADER_REPLACEMENT_H}>

        # Aus Subdirectories / Parent Scope geerbte/generierte Quellcodes
        ${MESA_LEX}
        ${PROGRAM_PARSE_TAB}
        ${MAIN_API_EXEC_C}
        ${MAIN_API_TRACE_C}
        ${MAIN_API_EXEC_DECL_H}
        ${MAIN_API_SAVE_H}
        ${MAIN_API_SAVE_INIT_H}
        ${MAIN_API_BEGINEND_INIT_H}
        ${MAIN_API_HW_SELECT_INIT_H}
        ${MAIN_ENUMS_C}
        ${MAIN_MARSHAL_GENERATED_H}
        ${MAIN_DISPATCH_H}
        ${IR_EXPRESSION_OPERATION_H}
        ${SHA1_H}
        ${MAIN_UNMARSHAL_TABLE_C}
        ${MAIN_MARSHAL_GENERATED_C}
)

set_target_properties(mesa PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
)


target_include_directories(mesa PRIVATE
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_SRC_DIR}/main
        ${MESA_GEN_DIR}
)

target_link_libraries(mesa PRIVATE
        ${LIBMESA_SSE41}
        glsl
        nir
        mesa_util_c11
)