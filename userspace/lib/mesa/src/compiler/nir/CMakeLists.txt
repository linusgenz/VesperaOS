# ════════════════════════════════════════════════════════════════
# lib/mesa/src/compiler/nir/CMakeLists.txt
#
# Modeled on upstream src/compiler/nir/meson.build. with_tests block
# (gtest, ~30 test files + 8-way split algebraic pattern tests) is
# INTENTIONALLY OMITTED — no gtest infrastructure in this build.
# with_nir_headers_only is treated as always-false — we build the
# full files_libnir list (~250 files), the branch meson takes for
# any real gfx/compute driver build (iris included), not the
# headers-only stub used only for narrow d3d12-without-graphics
# configurations that don't apply here.
# ════════════════════════════════════════════════════════════════

set(NIR_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(NIR_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})

set(NIR_OPCODES_PY ${NIR_ROOT}/nir_opcodes.py)
set(NIR_INTRINSICS_PY ${NIR_ROOT}/nir_intrinsics.py)
set(NIR_ALGEBRAIC_PY ${NIR_ROOT}/nir_algebraic.py)

# nir_depends / nir_algebraic_depends in meson.build (lines 4, 6) are
# depend_files — extra trigger-a-rebuild-if-changed inputs on top of
# each custom_target's primary `input`. CMake's DEPENDS list serves
# the same purpose; each add_custom_command below lists both its
# primary generator script AND these shared files.

# ────────────────────────────────────────────────────────────────
# 1. nir_builder_opcodes.h — meson.build lines 8–15
# ────────────────────────────────────────────────────────────────
set(NIR_BUILDER_OPCODES_H ${NIR_GEN_DIR}/nir_builder_opcodes.h)
add_custom_command(
        OUTPUT ${NIR_BUILDER_OPCODES_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${NIR_ROOT}/nir_builder_opcodes_h.py > ${NIR_BUILDER_OPCODES_H}
        DEPENDS ${NIR_ROOT}/nir_builder_opcodes_h.py ${NIR_OPCODES_PY} ${NIR_INTRINSICS_PY}
        COMMENT "Generating nir/nir_builder_opcodes.h"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# 2. nir_constant_expressions.c — meson.build lines 17–24
# ────────────────────────────────────────────────────────────────
set(NIR_CONSTANT_EXPRESSIONS_C ${NIR_GEN_DIR}/nir_constant_expressions.c)
add_custom_command(
        OUTPUT ${NIR_CONSTANT_EXPRESSIONS_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${NIR_ROOT}/nir_constant_expressions.py > ${NIR_CONSTANT_EXPRESSIONS_C}
        DEPENDS ${NIR_ROOT}/nir_constant_expressions.py ${NIR_OPCODES_PY} ${NIR_INTRINSICS_PY}
        COMMENT "Generating nir/nir_constant_expressions.c"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# 3. nir_opcodes.h — meson.build lines 26–33
# ────────────────────────────────────────────────────────────────
set(NIR_OPCODES_H ${NIR_GEN_DIR}/nir_opcodes.h)
add_custom_command(
        OUTPUT ${NIR_OPCODES_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${NIR_ROOT}/nir_opcodes_h.py > ${NIR_OPCODES_H}
        DEPENDS ${NIR_ROOT}/nir_opcodes_h.py ${NIR_OPCODES_PY} ${NIR_INTRINSICS_PY}
        COMMENT "Generating nir/nir_opcodes.h"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# 4. nir_opcodes.c — meson.build lines 35–42
# ────────────────────────────────────────────────────────────────
set(NIR_OPCODES_C ${NIR_GEN_DIR}/nir_opcodes.c)
add_custom_command(
        OUTPUT ${NIR_OPCODES_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${NIR_ROOT}/nir_opcodes_c.py > ${NIR_OPCODES_C}
        DEPENDS ${NIR_ROOT}/nir_opcodes_c.py ${NIR_OPCODES_PY} ${NIR_INTRINSICS_PY}
        COMMENT "Generating nir/nir_opcodes.c"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# 5. nir_opt_algebraic.c — meson.build lines 44–52.
# with_tests output (nir_opt_algebraic_pattern_test.cpp) omitted —
# script only gets --out, never --out-tests, matching with_tests=false.
# ────────────────────────────────────────────────────────────────
set(NIR_OPT_ALGEBRAIC_C ${NIR_GEN_DIR}/nir_opt_algebraic.c)
add_custom_command(
        OUTPUT ${NIR_OPT_ALGEBRAIC_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${NIR_ROOT}/nir_opt_algebraic.py --out ${NIR_OPT_ALGEBRAIC_C}
        DEPENDS ${NIR_ROOT}/nir_opt_algebraic.py ${NIR_OPCODES_PY} ${NIR_ALGEBRAIC_PY}
        COMMENT "Generating nir/nir_opt_algebraic.c"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# 6. nir_intrinsics.h — meson.build lines 54–61
# ────────────────────────────────────────────────────────────────
set(NIR_INTRINSICS_H ${NIR_GEN_DIR}/nir_intrinsics.h)
add_custom_command(
        OUTPUT ${NIR_INTRINSICS_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${NIR_ROOT}/nir_intrinsics_h.py --out ${NIR_INTRINSICS_H}
        DEPENDS ${NIR_ROOT}/nir_intrinsics_h.py ${NIR_INTRINSICS_PY}
        COMMENT "Generating nir/nir_intrinsics.h"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# 7. nir_intrinsics_indices.h — meson.build lines 63–70
# ────────────────────────────────────────────────────────────────
set(NIR_INTRINSICS_INDICES_H ${NIR_GEN_DIR}/nir_intrinsics_indices.h)
add_custom_command(
        OUTPUT ${NIR_INTRINSICS_INDICES_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${NIR_ROOT}/nir_intrinsics_indices_h.py --out ${NIR_INTRINSICS_INDICES_H}
        DEPENDS ${NIR_ROOT}/nir_intrinsics_indices_h.py ${NIR_INTRINSICS_PY}
        COMMENT "Generating nir/nir_intrinsics_indices.h"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# 8. nir_intrinsics.c — meson.build lines 72–79
# (upstream target name is 'nir_intrinsic.c' [sic, no trailing s] —
# preserved as a comment only; our OUTPUT filename matches meson's
# actual `output: 'nir_intrinsics.c'`, which is what code includes.)
# ────────────────────────────────────────────────────────────────
set(NIR_INTRINSICS_C ${NIR_GEN_DIR}/nir_intrinsics.c)
add_custom_command(
        OUTPUT ${NIR_INTRINSICS_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${NIR_ROOT}/nir_intrinsics_c.py --out ${NIR_INTRINSICS_C}
        DEPENDS ${NIR_ROOT}/nir_intrinsics_c.py ${NIR_INTRINSICS_PY}
        COMMENT "Generating nir/nir_intrinsics.c"
        VERBATIM
)

set(NIR_GENERATED_SOURCES
        ${NIR_OPT_ALGEBRAIC_C}
        ${NIR_OPCODES_C}
        ${NIR_OPCODES_H}
        ${NIR_CONSTANT_EXPRESSIONS_C}
        ${NIR_BUILDER_OPCODES_H}
        ${NIR_INTRINSICS_C}
        ${NIR_INTRINSICS_H}
        ${NIR_INTRINSICS_INDICES_H}
)

# ────────────────────────────────────────────────────────────────
# files_libnir — meson.build lines 101–358 (with_nir_headers_only
# branch, i.e. the full list). Header files listed alongside .c
# files in meson's `files()` call are omitted here — CMake doesn't
# need headers in the source list to track them (unlike meson,
# which uses that list for install/distribution purposes we don't
# replicate). Only translation units (.c) are included below;
# compare against meson.build directly if a file appears to be
# "missing" — it's very likely a .h that doesn't need to be here.
# ────────────────────────────────────────────────────────────────
set(NIR_CORE_SOURCES
        nir.c
        nir_builder.c
        nir_builtin_builder.c
        nir_convert_address_format.c
        nir_clip_cull_distance_io_utils.c
        nir_clone.c
        nir_control_flow.c
        nir_deref.c
        nir_divergence_analysis.c
        nir_dominance.c
        nir_dominance_lca.c
        nir_downgrade_pls_vars.c
        nir_fixup_is_exported.c
        nir_format_convert.c
        nir_from_ssa.c
        nir_functions.c
        nir_gather_info.c
        nir_gather_output_deps.c
        nir_gather_tcs_info.c
        nir_gather_types.c
        nir_gather_xfb_info.c
        nir_opt_group_loads.c
        nir_gs_count_vertices.c
        nir_inline_sysval.c
        nir_inline_uniforms.c
        nir_instr_set.c
        nir_io_add_xfb_info.c
        nir_legacy.c
        nir_linking_helpers.c
        nir_liveness.c
        nir_loop_analyze.c
        nir_lower_abort.c
        nir_lower_alu.c
        nir_lower_alu_width.c
        nir_lower_alpha.c
        nir_lower_amul.c
        nir_lower_array_deref_of_vec.c
        nir_lower_atomics_to_ssbo.c
        nir_lower_bitmap.c
        nir_lower_blend.c
        nir_lower_bool_to_float.c
        nir_lower_bool_to_int32.c
        nir_lower_calls_to_builtins.c
        nir_lower_cl_images.c
        nir_lower_clamp_color_outputs.c
        nir_lower_clip.c
        nir_lower_clip_disable.c
        nir_lower_clip_halfz.c
        nir_lower_const_arrays_to_uniforms.c
        nir_lower_continue_constructs.c
        nir_lower_convert_alu_types.c
        nir_lower_cooperative_matrix.c
        nir_lower_variable_initializers.c
        nir_lower_discard_if.c
        nir_lower_double_ops.c
        nir_lower_explicit_io.c
        nir_lower_fb_read.c
        nir_lower_flatshade.c
        nir_lower_floats.c
        nir_lower_flrp.c
        nir_lower_fp16_conv.c
        nir_lower_fragcoord_wtrans.c
        nir_lower_frag_coord_to_pixel_coord.c
        nir_lower_fragcolor.c
        nir_lower_frexp.c
        nir_lower_global_vars_to_local.c
        nir_lower_goto_ifs.c
        nir_lower_gs_intrinsics.c
        nir_lower_halt_to_return.c
        nir_lower_helper_writes.c
        nir_lower_load_const_to_scalar.c
        nir_lower_locals_to_regs.c
        nir_lower_idiv.c
        nir_lower_image.c
        nir_lower_image_atomics_to_global.c
        nir_lower_indirect_derefs_to_if_else_trees.c
        nir_lower_input_attachments.c
        nir_lower_int64.c
        nir_lower_interpolation.c
        nir_lower_int_to_float.c
        nir_lower_io.c
        nir_lower_io_array_vars_to_elements.c
        nir_lower_io_indirect_loads.c
        nir_lower_io_vars_to_temporaries.c
        nir_lower_io_to_scalar.c
        nir_lower_io_vars_to_scalar.c
        nir_lower_is_helper_invocation.c
        nir_lower_multiview.c
        nir_lower_mediump.c
        nir_lower_mem_access_bit_sizes.c
        nir_lower_memcpy.c
        nir_lower_memory_model.c
        nir_lower_non_uniform_access.c
        nir_lower_packing.c
        nir_lower_passthrough_edgeflags.c
        nir_lower_patch_vertices.c
        nir_lower_phis_to_scalar.c
        nir_lower_pntc_ytransform.c
        nir_lower_point_size.c
        nir_lower_point_smooth.c
        nir_lower_poly_line_smooth.c
        nir_lower_printf.c
        nir_lower_reg_intrinsics_to_ssa.c
        nir_lower_readonly_images_to_tex.c
        nir_lower_returns.c
        nir_lower_robust_access.c
        nir_lower_samplers.c
        nir_lower_sample_shading.c
        nir_lower_scratch.c
        nir_lower_scratch_to_var.c
        nir_lower_shader_calls.c
        nir_lower_single_sampled.c
        nir_lower_ssbo.c
        nir_lower_subgroups.c
        nir_lower_system_values.c
        nir_lower_task_shader.c
        nir_lower_terminate_to_demote.c
        nir_lower_tess_coord_z.c
        nir_lower_tex_shadow.c
        nir_lower_tex.c
        nir_lower_texcoord_replace.c
        nir_lower_texcoord_replace_late.c
        nir_lower_two_sided_color.c
        nir_lower_undef_to_zero.c
        nir_lower_vars_to_ssa.c
        nir_lower_var_copies.c
        nir_lower_vec_to_regs.c
        nir_lower_vec3_to_vec4.c
        nir_lower_view_index_to_device_index.c
        nir_lower_viewport_transform.c
        nir_lower_wpos_center.c
        nir_lower_wpos_ytransform.c
        nir_lower_wrmasks.c
        nir_lower_xfb_to_stores.c
        nir_lower_bit_size.c
        nir_lower_ubo_vec4.c
        nir_lower_uniforms_to_ubo.c
        nir_lower_workgroup_size.c
        nir_lower_sysvals_to_varyings.c
        nir_metadata.c
        nir_mod_analysis.c
        nir_move_output_stores_to_end.c
        nir_move_vec_src_uses_to_dest.c
        nir_normalize_cubemap_coords.c
        nir_normalize_sin_cos.c
        nir_opt_access.c
        nir_opt_barriers.c
        nir_opt_barycentric.c
        nir_opt_call.c
        nir_opt_clip_cull_const.c
        nir_opt_combine_stores.c
        nir_opt_comparison_pre.c
        nir_opt_constant_folding.c
        nir_opt_copy_prop_vars.c
        nir_opt_copy_propagate.c
        nir_opt_cse.c
        nir_opt_dce.c
        nir_opt_dead_cf.c
        nir_opt_dead_write_vars.c
        nir_opt_find_array_copies.c
        nir_opt_fp_math_ctrl.c
        nir_opt_frag_coord_to_pixel_coord.c
        nir_opt_fragdepth.c
        nir_opt_gcm.c
        nir_opt_generate_bfi.c
        nir_opt_idiv_const.c
        nir_opt_if.c
        nir_opt_intrinsics.c
        nir_opt_large_constants.c
        nir_opt_licm.c
        nir_opt_load_skip_helpers.c
        nir_opt_load_store_vectorize.c
        nir_opt_loop.c
        nir_opt_loop_unroll.c
        nir_opt_memcpy.c
        nir_opt_move.c
        nir_opt_move_discards_to_top.c
        nir_opt_move_to_top.c
        nir_opt_mqsad.c
        nir_opt_non_uniform_access.c
        nir_opt_offsets.c
        nir_opt_peephole_select.c
        nir_opt_phi_precision.c
        nir_opt_phi_to_bool.c
        nir_opt_preamble.c
        nir_opt_ray_queries.c
        nir_opt_reassociate.c
        nir_opt_reassociate_bfi.c
        nir_opt_rematerialize_compares.c
        nir_opt_remove_phis.c
        nir_opt_shared_vars_to_subgroup.c
        nir_opt_shrink_stores.c
        nir_opt_shrink_vectors.c
        nir_opt_sink.c
        nir_opt_undef.c
        nir_opt_uniform_atomics.c
        nir_opt_uniform_subgroup.c
        nir_opt_uub.c
        nir_opt_varyings.c
        nir_opt_vectorize.c
        nir_opt_vectorize_io.c
        nir_opt_vectorize_io_vars.c
        nir_passthrough_gs.c
        nir_passthrough_tcs.c
        nir_phi_builder.c
        nir_print.c
        nir_propagate_invariant.c
        nir_range_analysis.c
        nir_recompute_io_bases.c
        nir_remove_dead_variables.c
        nir_remove_outputs.c
        nir_remove_tex_shadow.c
        nir_repair_ssa.c
        nir_scale_fdiv.c
        nir_schedule.c
        nir_search.c
        nir_separate_merged_clip_cull_io.c
        nir_serialize.c
        nir_shader_bisect.c
        nir_split_64bit_vec3_and_vec4.c
        nir_split_conversions.c
        nir_split_per_member_structs.c
        nir_split_var_copies.c
        nir_split_vars.c
        nir_sweep.c
        nir_to_lcssa.c
        nir_trivialize_registers.c
        nir_unlower_io_to_vars.c
        nir_use_dominance.c
        nir_validate.c
        nir_worklist.c
        nir_lower_atomics.c
)
list(TRANSFORM NIR_CORE_SOURCES PREPEND "${NIR_ROOT}/")

set(NIR_ALL_SOURCES
        ${NIR_CORE_SOURCES}
        ${NIR_GENERATED_SOURCES}
)

# ────────────────────────────────────────────────────────────────
# _libnir — meson.build lines 388–396. Static, hidden visibility,
# depends on idep_compiler (mesa_compiler, from the parent
# CMakeLists.txt one level up) + idep_mesautil (mesa_util).
# no_misleading_indentation (meson.build 366–369) is GCC-only and
# does not apply to our Clang-based cross toolchain — omitted.
# ────────────────────────────────────────────────────────────────
add_library(mesa_nir STATIC ${NIR_ALL_SOURCES})

target_include_directories(mesa_nir
        PUBLIC
        ${NIR_ROOT}                          # inc_nir equivalent
        ${NIR_GEN_DIR}                        # for generated headers included unqualified
        ${MESA_ROOT}/include                  # inc_include equivalent
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_nir PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${MESA_COMMON_C_FLAGS}>
        $<$<COMPILE_LANGUAGE:CXX>:${MESA_COMMON_CXX_FLAGS}>
)

# gnu_symbol_visibility: 'hidden' in meson.build — matches Mesa's
# general practice of not exporting internal symbols from static
# libs that only ever get linked into a larger .so.
target_compile_options(mesa_nir PRIVATE -fvisibility=hidden)

target_link_libraries(mesa_nir PUBLIC mesa_compiler mesa_util)

add_dependencies(mesa_nir sysroot_headers mesa_compiler mesa_util)