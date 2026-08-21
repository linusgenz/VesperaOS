# src/mesa/glapi/CMakeLists.txt
#
# 1:1-Uebersetzung von meson_glapi.build.
#
# glapi_xml_py_deps: reine Depend-Datei-Liste (keine Codegen-Befehle), damit
# CMake bei Aenderung irgendeiner .xml/.py Datei alle betroffenen
# add_custom_command()-Aufrufe in gen/CMakeLists.txt neu anstoesst.

set(GLAPI_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

set(GLAPI_XML_PY_DEPS
        # XML-Dateien zuerst (wie im Original)
        ${GLAPI_ROOT}/glapi/gen/gl_API.dtd
        ${GLAPI_ROOT}/glapi/gen/AMD_depth_clamp_separate.xml
        ${GLAPI_ROOT}/glapi/gen/AMD_draw_buffers_blend.xml
        ${GLAPI_ROOT}/glapi/gen/AMD_gpu_shader_int64.xml
        ${GLAPI_ROOT}/glapi/gen/AMD_performance_monitor.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_base_instance.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_bindless_texture.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_blend_func_extended.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_clear_buffer_object.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_clear_texture.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_clip_control.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_color_buffer_float.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_compressed_texture_pixel_storage.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_compute_shader.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_compute_variable_group_size.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_copy_buffer.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_copy_image.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_debug_output.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_depth_buffer_float.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_depth_clamp.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_direct_state_access.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_draw_buffers_blend.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_draw_buffers.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_draw_elements_base_vertex.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_draw_indirect.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_draw_instanced.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_ES2_compatibility.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_ES3_compatibility.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_framebuffer_no_attachments.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_framebuffer_object.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_get_program_binary.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_get_texture_sub_image.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_gl_spirv.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_gpu_shader5.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_gpu_shader_fp64.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_gpu_shader_int64.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_indirect_parameters.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_instanced_arrays.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_internalformat_query2.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_internalformat_query.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_invalidate_subdata.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_map_buffer_range.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_multi_bind.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_pipeline_statistics_query.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_program_interface_query.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_robustness.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_sampler_objects.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_sample_shading.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_seamless_cube_map.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_separate_shader_objects.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_shader_atomic_counters.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_shader_image_load_store.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_shader_storage_buffer_object.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_shader_subroutine.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_shading_language_include.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_sparse_buffer.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_sparse_texture.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_spirv_extensions.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_sync.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_tessellation_shader.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_texture_barrier.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_texture_buffer_object.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_texture_buffer_range.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_texture_compression_rgtc.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_texture_cube_map_array.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_texture_float.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_texture_gather.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_texture_multisample.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_texture_rgb10_a2ui.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_texture_rg.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_texture_storage_multisample.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_texture_storage.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_texture_view.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_uniform_buffer_object.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_vertex_array_object.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_vertex_attrib_64bit.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_vertex_attrib_binding.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_vertex_type_2_10_10_10_rev.xml
        ${GLAPI_ROOT}/glapi/gen/ARB_viewport_array.xml
        ${GLAPI_ROOT}/glapi/gen/es_EXT.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_direct_state_access.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_draw_buffers2.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_EGL_image_storage.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_external_objects_fd.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_external_objects_win32.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_external_objects.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_framebuffer_object.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_gpu_shader4.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_mesh_shader.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_multisampled_render_to_texture.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_packed_depth_stencil.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_provoking_vertex.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_separate_shader_objects.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_shader_image_load_store.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_shader_pixel_local_storage.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_texture_array.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_texture_integer.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_texture_storage_compression.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_texture_storage.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_transform_feedback.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_vertex_attrib_64bit.xml
        ${GLAPI_ROOT}/glapi/gen/EXT_window_rectangles.xml
        ${GLAPI_ROOT}/glapi/gen/GL3x.xml
        ${GLAPI_ROOT}/glapi/gen/GL4x.xml
        ${GLAPI_ROOT}/glapi/gen/gl_and_es_API.xml
        ${GLAPI_ROOT}/glapi/gen/gl_and_glX_API.xml
        ${GLAPI_ROOT}/glapi/gen/gl_API.xml
        ${GLAPI_ROOT}/glapi/gen/glX_API.xml
        ${GLAPI_ROOT}/glapi/gen/GREMEDY_string_marker.xml
        ${GLAPI_ROOT}/glapi/gen/INTEL_performance_query.xml
        ${GLAPI_ROOT}/glapi/gen/KHR_context_flush_control.xml
        ${GLAPI_ROOT}/glapi/gen/KHR_debug.xml
        ${GLAPI_ROOT}/glapi/gen/KHR_robustness_es.xml
        ${GLAPI_ROOT}/glapi/gen/KHR_robustness.xml
        ${GLAPI_ROOT}/glapi/gen/KHR_shader_subgroup.xml
        ${GLAPI_ROOT}/glapi/gen/KHR_texture_compression_astc.xml
        ${GLAPI_ROOT}/glapi/gen/MESA_tile_raster_order.xml
        ${GLAPI_ROOT}/glapi/gen/NV_alpha_to_coverage_dither_control.xml
        ${GLAPI_ROOT}/glapi/gen/NV_conditional_render.xml
        ${GLAPI_ROOT}/glapi/gen/NV_copy_image.xml
        ${GLAPI_ROOT}/glapi/gen/NV_half_float.xml
        ${GLAPI_ROOT}/glapi/gen/NV_primitive_restart.xml
        ${GLAPI_ROOT}/glapi/gen/NV_texture_barrier.xml
        ${GLAPI_ROOT}/glapi/gen/NV_timeline_semaphore.xml
        ${GLAPI_ROOT}/glapi/gen/NV_viewport_swizzle.xml
        ${GLAPI_ROOT}/glapi/gen/OES_EGL_image.xml
        ${GLAPI_ROOT}/glapi/gen/OES_fixed_point.xml
        ${GLAPI_ROOT}/glapi/gen/OES_single_precision.xml
        ${GLAPI_ROOT}/glapi/gen/OES_texture_compression_astc.xml
        ${GLAPI_ROOT}/glapi/gen/OVR_multiview_multisampled_render_to_texture.xml
        ${GLAPI_ROOT}/glapi/gen/OVR_multiview.xml
        ${GLAPI_ROOT}/glapi/registry/gl.xml

        # Python-Dateien
        ${GLAPI_ROOT}/glapi/gen/apiexec.py
        ${GLAPI_ROOT}/glapi/gen/gl_XML.py
        ${GLAPI_ROOT}/glapi/gen/glX_XML.py
        ${GLAPI_ROOT}/glapi/gen/glX_proto_common.py
        ${GLAPI_ROOT}/glapi/gen/license.py
        ${GLAPI_ROOT}/glapi/gen/marshal_XML.py
        ${GLAPI_ROOT}/glapi/gen/static_data.py
        ${GLAPI_ROOT}/glapi/gen/typeexpr.py
        ${GLAPI_ROOT}/glapi/new/genCommon.py

        # Symbols
        ${GLAPI_ROOT}/../../glx/libgl-symbols.txt
)

add_subdirectory(glapi)

# shared-glapi war nicht Teil des Uploads. Meson bindet es hier ueber
# subdir('shared-glapi') ein (analog zur unveraenderten Mesa-Quellstruktur,
# die dieser CMake-Baum nachfahren soll). Sobald ihr src/mesa/glapi/shared-glapi/
# im Baum habt, hier den add_subdirectory()-Aufruf entkommentieren.
# add_subdirectory(shared-glapi)

# --- Weiterreichen an src/mesa/CMakeLists.txt (Parent-Scope) ---------------
set(MAIN_ENUMS_C ${MAIN_ENUMS_C} PARENT_SCOPE)
set(MAIN_API_EXEC_C ${MAIN_API_EXEC_C} PARENT_SCOPE)
set(MAIN_API_EXEC_DECL_H ${MAIN_API_EXEC_DECL_H} PARENT_SCOPE)
set(MAIN_API_SAVE_INIT_H ${MAIN_API_SAVE_INIT_H} PARENT_SCOPE)
set(MAIN_API_SAVE_H ${MAIN_API_SAVE_H} PARENT_SCOPE)
set(MAIN_API_BEGINEND_INIT_H ${MAIN_API_BEGINEND_INIT_H} PARENT_SCOPE)
set(MAIN_API_HW_SELECT_INIT_H ${MAIN_API_HW_SELECT_INIT_H} PARENT_SCOPE)
set(MAIN_DISPATCH_H ${MAIN_DISPATCH_H} PARENT_SCOPE)
set(MAIN_MARSHAL_GENERATED_H ${MAIN_MARSHAL_GENERATED_H} PARENT_SCOPE)
set(MAIN_UNMARSHAL_TABLE_C ${MAIN_UNMARSHAL_TABLE_C} PARENT_SCOPE)
set(MAIN_API_TRACE_C ${MAIN_API_TRACE_C} PARENT_SCOPE)
set(MAIN_MARSHAL_GENERATED_C ${MAIN_MARSHAL_GENERATED_C} PARENT_SCOPE)
set(GLX_GENERATED ${GLX_GENERATED} PARENT_SCOPE)
set(GLX_INDIRECT_SIZE_H ${GLX_INDIRECT_SIZE_H} PARENT_SCOPE)