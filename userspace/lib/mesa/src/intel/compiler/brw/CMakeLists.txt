# lib/mesa/src/intel/compiler/brw/CMakeLists.txt

set(BRW_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(BRW_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR})

# --- brw_nir_lower_fsign.c: generiert aus .py + NIR-Algebraic-Regeln ---
set(BRW_NIR_LOWER_FSIGN_C ${BRW_GENERATED_DIR}/brw_nir_lower_fsign.c)
add_custom_command(
        OUTPUT ${BRW_NIR_LOWER_FSIGN_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${BRW_DIR}/brw_nir_lower_fsign.py
        -p ${MESA_ROOT}/src/compiler/nir
        > ${BRW_NIR_LOWER_FSIGN_C}
        DEPENDS ${BRW_DIR}/brw_nir_lower_fsign.py
        COMMENT "Generating brw_nir_lower_fsign.c"
        VERBATIM
)

# --- brw_nir_workarounds.c: gleiches Prinzip ---
set(BRW_NIR_WORKAROUNDS_C ${BRW_GENERATED_DIR}/brw_nir_workarounds.c)
add_custom_command(
        OUTPUT ${BRW_NIR_WORKAROUNDS_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${BRW_DIR}/brw_nir_workarounds.py
        -p ${MESA_ROOT}/src/compiler/nir
        > ${BRW_NIR_WORKAROUNDS_C}
        DEPENDS ${BRW_DIR}/brw_nir_workarounds.py
        COMMENT "Generating brw_nir_workarounds.c"
        VERBATIM
)

set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/../brw_device_sha1_gen.c PROPERTIES GENERATED TRUE)

set(MESA_INTEL_COMPILER_BRW_SOURCES
        brw_analysis.cpp
        brw_analysis_def.cpp
        brw_analysis_liveness.cpp
        brw_analysis_performance.cpp
        brw_builder.cpp
        brw_cfg.cpp
        brw_compile_bs.cpp
        brw_compile_cs.cpp
        brw_compile_fs.cpp
        brw_compile_gs.cpp
        brw_compile_mesh.cpp
        brw_compile_tcs.cpp
        brw_compile_tes.cpp
        brw_compile_vs.cpp
        brw_compiler.c
        brw_eu.c
        brw_from_nir.cpp
        brw_inst.cpp
        brw_load_reg.cpp
        brw_lower.cpp
        brw_lower_dpas.cpp
        brw_lower_fill_spill.cpp
        brw_lower_integer_multiplication.cpp
        brw_lower_logical_sends.cpp
        brw_lower_pack.cpp
        brw_lower_regioning.cpp
        brw_lower_scoreboard.cpp
        brw_lower_simd_width.cpp
        brw_lower_subgroup_ops.cpp
        brw_nir.c
        brw_nir_fence_shared_stores.c
        brw_nir_lower_cooperative_matrix.c
        brw_nir_lower_cs_intrinsics.c
        brw_nir_lower_alpha_to_coverage.c
        brw_nir_lower_divergent_barriers.c
        brw_nir_lower_fs_barycentrics.c
        brw_nir_lower_fs_load_output.c
        brw_nir_lower_fully_covered.c
        brw_nir_lower_immediate_offsets.c
        brw_nir_lower_intersection_shader.c
        brw_nir_lower_ray_queries.c
        brw_nir_lower_rt_intrinsics.c
        brw_nir_lower_rt_intrinsics_pre_trace.c
        brw_nir_lower_sample_index_in_coord.c
        brw_nir_lower_shader_calls.c
        brw_nir_lower_storage_image.c
        brw_nir_lower_texel_address.c
        brw_nir_lower_texture.c
        brw_nir_opt_divergent_atomics.c
        brw_nir_wa_18019110168.c
        brw_nir_opt_fsat.c
        brw_nir_opt_systolic_vectorize.c
        brw_nir_rt.c
        brw_opt.cpp
        brw_opt_address_reg_load.cpp
        brw_opt_algebraic.cpp
        brw_opt_bank_conflicts.cpp
        brw_opt_cmod_propagation.cpp
        brw_opt_cmp_flag_destination.cpp
        brw_opt_combine_constants.cpp
        brw_opt_copy_propagation.cpp
        brw_opt_cse.cpp
        brw_opt_dead_code_eliminate.cpp
        brw_opt_fill_spill.cpp
        brw_opt_predicate_logic.cpp
        brw_opt_register_coalesce.cpp
        brw_opt_saturate_propagation.cpp
        brw_opt_txf_combiner.cpp
        brw_opt_virtual_grfs.cpp
        brw_packed_float.c
        brw_print.cpp
        brw_reg.cpp
        brw_reg_allocate.cpp
        brw_reg_type.c
        brw_sampler.c
        brw_schedule_instructions.cpp
        brw_shader.cpp
        brw_simd_selection.cpp
        brw_thread_payload.cpp
        brw_to_binary.cpp
        brw_validate.cpp
        brw_vue_map.c
        brw_workaround.cpp

        ${BRW_NIR_LOWER_FSIGN_C}
        ${BRW_NIR_WORKAROUNDS_C}
        ${CMAKE_CURRENT_BINARY_DIR}/../brw_device_sha1_gen.c
)

add_library(mesa_intel_compiler_brw STATIC ${MESA_INTEL_COMPILER_BRW_SOURCES})

target_include_directories(mesa_intel_compiler_brw
        PUBLIC
        ${BRW_DIR}
        ${BRW_DIR}/..                 # fuer ../brw_list.h, ../intel_prim.h
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_link_libraries(mesa_intel_compiler_brw
        PUBLIC
        mesa_util
        mesa_intel_dev
        mesa_intel_compiler_nir
        mesa_intel_compiler_gen
        mesa_intel_mda
        mesa_nir
        mesa_intel_isl
        mesa_intel_common
)

target_compile_options(mesa_intel_compiler_brw PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${MESA_COMMON_C_FLAGS}>
        $<$<COMPILE_LANGUAGE:CXX>:${MESA_COMMON_CXX_FLAGS}>
)

add_dependencies(mesa_intel_compiler_brw sysroot_headers mesa_intel_compiler_gen mesa_intel_brw_device_sha1 mesa_genxml)