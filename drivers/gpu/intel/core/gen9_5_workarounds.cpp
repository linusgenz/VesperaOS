// gen9_5_workarounds.cpp
// Gen9.5 (KBL) Workarounds, abgeleitet aus i915 intel_workarounds.c. Offsets und Bits stammen aus
// deiner intel_gt_regs.h. Stepping-abhängige Einträge sind als Kommentar markiert.
//
// Drei Klassen, wie bei i915:
//   GT      : einfache MMIO-RMWs, bei GT-Init und nach jedem Reset   (gt_workarounds_init)
//   Engine  : RCS-MMIO-RMWs, bei jedem Engine-Resume                 (rcs_engine_wa_init)
//   Context : "masked" Chicken-Bits, im Context gespeichert -> per LRI einmal pro Context
//             (bei i915: intel_engine_emit_ctx_wa im ersten Request, danach im Default-Context-Image)
//
// Masked-Register: Schreibwert = ((clr | set) << 16) | set  (obere 16 Bit = Write-Mask).
// "Mcr" = Multicast-Register (GEN8_MCR_SELECTOR 0xFDC beachten, nach dem Schreiben zurücklesen).

#include <vespera/types.h>

#include "vespera/log.h"

namespace gpu::intel::core::wa {

    constexpr u32 kGen8McrSelectorReg = 0xFDC;

    enum class Kind : u8 { Plain, PlainMcr, Masked, MaskedMcr };

    struct Wa {
        u32 reg;
        u32 clr;   // Bits, die 0 werden
        u32 set;   // Bits, die 1 werden
        Kind kind;
        const char* name;
    };

    // ---------------------------------------------------------------- GT (Plain)
    constexpr Wa kGt[] = {
        // WaDisableKillLogic:skl,kbl (NICHT cfl/cml) + WaDisableHDCInvalidation:skl,bxt,kbl,cfl
        {0x4090, 0, (1u << 8) | (1u << 25), Kind::Plain, "GAM_ECOCHK DIS_TLB|DISABLE_HDC_INVALIDATION"},
        // WaDisableGafsUnitClkGating:skl,kbl
        {0x940C, 0, (1u << 14),             Kind::Plain, "GEN7_UCGCTL4 EU_GAUNIT_CLOCK_GATE_DISABLE"},
        // WaInPlaceDecompressionHang:kbl (nur relevant, wenn Kompression/CCS benutzt wird)
        {0x4AB0, 0, (1u << 18),             Kind::Plain, "GEN9_GAMT_ECO_REG_RW_IA IN_PLACE_DECOMPRESS"},
        // WaDisableDynamicCreditSharing:kbl  (nur Stepping < C0)
        // {0x4AB8, 0, (1u << 28), Kind::Plain, "GAMT_CHKN_BIT_REG DISABLE_DYNAMIC_CREDIT_SHARING"},
        // WaCompressedResource*NewHashMode (nur mit CCS; muss zur Display-Engine passen)
        // {0x4DDC, 0, (1u << 31) | (1u << 27), Kind::Plain, "MMCD_MISC_CTRL PCLA|HOTSPOT_EN"},
    };

    // ---------------------------------------------------------------- RCS-Engine
    constexpr Wa kRcsEngine[] = {
        // Voraussetzung dafür, dass UMD-Schreibzugriffe auf CS_CHICKEN1 greifen (du whitelistest es)
        {0x20E0, 0, (1u << 14), Kind::Masked,  "GEN7_FF_SLICE_CS_CHICKEN1 PERCTX_PREEMPT_CTRL"},
        // WaEnableGapsTsvCreditFix:skl,kbl,cfl
        {0xB004, 0, (1u << 7),  Kind::Plain,   "GEN8_GARBCNTL GAPS_TSV_CREDIT_DISABLE"},
        // WaContextSwitchWithConcurrentTLBInvalidate (betrifft TLB-Invalidates bei Context-Wechsel)
        {0x20D4, 0, (1u << 2),  Kind::Masked,  "GEN9_CSFE_CHICKEN1_RCS GPGPU_SYNC_SWITCH_DISABLE"},
        // WaEnableLbsSlaRetryTimerDecrement + "eviction perf fix" aus (L3-Eviction!)
        {0xB11C, (1u << 8), (1u << 2), Kind::PlainMcr, "SCRATCH1 LBS_SLA_RETRY on, EVICTION_PERF_FIX off"},
        // WaOCLCoherentLineFlush + L3-Atomics aus ("prevent unrecoverable hangs")
        {0xB118, (1u << 22), (1u << 21), Kind::PlainMcr, "GEN8_L3SQCREG4 LQSC_FLUSH_COHERENT_LINES on, NONIA atomics off"},
        {0xB008, (1u << 0), 0,  Kind::Plain,   "GEN9_SCRATCH_LNCF1 NONIA_COHERENT_ATOMICS off"},
    };

    // ---------------------------------------------------------------- Context (alles masked, per LRI)
    constexpr Wa kRcsContext[] = {
        // WaForceContextSaveRestoreNonCoherent + WaForceEnableNonCoherent
        {0x7300, 0, (1u << 5) | (1u << 15) | (1u << 4), Kind::Masked, "HDC_CHICKEN0 non-coherent bits"},
        // Wa4x4STCOptimizationDisable + WaDisablePartialResolveInVc
        {0x7004, 0, (1u << 6) | (1u << 1),              Kind::Masked, "CACHE_MODE_1"},
        // KBL Stepping >= C0 zusätzlich (1u << 8) GEN8_SBE_DISABLE_REPLAY_BUF_OPTIMIZATION
        // (HAS_LLC + Kompression zusätzlich (1u << 13) PBE_COMPRESSED_HASH_SELECTION)
        {0x7014, 0, 0,                                  Kind::Masked, "COMMON_SLICE_CHICKEN2 (nur stepping-/ccs-abhängig)"},
        // WaClearFlowControlGpgpuContextSave + WaDisablePartialInstShootdown
        {0xE4F0, 0, (1u << 15) | (1u << 8),             Kind::MaskedMcr, "GEN8_ROW_CHICKEN"},
        // WaEnableYV12BugFix + WaEnableSamplerGPGPUPreemptionSupport
        {0xE194, 0, (1u << 4) | (1u << 2),              Kind::MaskedMcr, "GEN9_HALF_SLICE_CHICKEN7"},
        // WaCcsTlbPrefetchDisable
        {0xE188, (1u << 3), 0,                          Kind::MaskedMcr, "GEN9_HALF_SLICE_CHICKEN5 CCS_TLB_PREFETCH off"},
        // WaDisableSamplerPowerBypassForSOPingPong
        {0xE184, 0, (1u << 1),                          Kind::MaskedMcr, "GEN8_HALF_SLICE_CHICKEN3"},
        // WaDisableSTUnitPowerOptimization
        {0xE180, 0, (1u << 13),                         Kind::MaskedMcr, "HALF_SLICE_CHICKEN2 ST_PO_DISABLE"},
        // WaDisable3DMidCmdPreemption + WaDisableGPGPUMidCmdPreemption (Command-Level)
        {0x2580, 0x3, 0x4,                              Kind::Masked, "GEN8_CS_CHICKEN1 preemption levels"},
        // WaDisableSbeCacheDispatchPortSharing:kbl
        {0xE100, 0, (1u << 4),                          Kind::MaskedMcr, "GEN8_HALF_SLICE_CHICKEN1"},
    };

    // Plain-RMW mit Readback. Für Engine/GT-Listen (Forcewake vorher halten).
    bool apply_plain(volatile u8* mmio, const Wa& w) {
        auto* p = reinterpret_cast<volatile u32*>(mmio + w.reg);
        const u32 want_mask = w.clr | w.set;
        const u32 v = (*p & ~w.clr) | w.set;
        *p = v;
        const u32 got = *p;
        const bool ok = ((got ^ v) & want_mask) == 0;
        if (!ok) Log::error("intel-wa: %s reg=0x%x readback 0x%08x want 0x%08x", w.name, w.reg, got, v);
        return ok;
    }

    // Masked-MMIO-Write mit Readback (obere 16 Bit = Write-Mask).
    bool apply_masked(volatile u8* mmio, const Wa& w) {
        auto* p = reinterpret_cast<volatile u32*>(mmio + w.reg);
        const u32 want_mask = w.clr | w.set;
        *p = (want_mask << 16) | w.set;
        const u32 got = *p;
        const bool ok = ((got ^ w.set) & want_mask) == 0;
        if (!ok) Log::error("intel-wa: %s reg=0x%x readback 0x%08x want_set 0x%08x (mask 0x%04x)",
                            w.name, w.reg, got, w.set, want_mask);
        return ok;
    }

    bool apply_one_mmio(volatile u8* mmio, const Wa& w, u32 mcr_selector = 0) {
        if ((w.clr | w.set) == 0) {
            return true;  // Platzhalter ohne gesetzte Bits überspringen
        }

        switch (w.kind) {
            case Kind::Plain:
                return apply_plain(mmio, w);

            case Kind::Masked:
                return apply_masked(mmio, w);

            case Kind::PlainMcr:
            case Kind::MaskedMcr: {
                auto* mcr = reinterpret_cast<volatile u32*>(mmio + kGen8McrSelectorReg);
                const u32 saved_mcr = *mcr;
                // Slice/Subslice-Bits in GEN8_MCR_SELECTOR für das Readback setzen
                *mcr = (saved_mcr & ~0x07E00000u) | (mcr_selector & 0x07E00000u);

                const bool ok = (w.kind == Kind::PlainMcr)
                                    ? apply_plain(mmio, w)
                                    : apply_masked(mmio, w);

                *mcr = saved_mcr;
                return ok;
            }
        }
        return false;
    }

    template <usize N>
    inline bool apply_mmio_list(volatile u8* mmio, const Wa (&list)[N], u32 mcr_selector = 0) {
        bool all_ok = true;
        for (const auto& w : list) {
            if (!apply_one_mmio(mmio, w, mcr_selector)) {
                all_ok = false;
            }
        }
        return all_ok;
    }

    bool apply_gt_workarounds(volatile u8* mmio, u32 mcr_selector) {
        return apply_mmio_list(mmio, kGt, mcr_selector);
    }

    bool apply_rcs_engine_workarounds(volatile u8* mmio, u32 mcr_selector) {
        return apply_mmio_list(mmio, kRcsEngine, mcr_selector);
    }

    bool enable_all_mmio_workarounds(volatile u8* mmio, u32 mcr_selector) {
        const bool gt_ok = apply_gt_workarounds(mmio, mcr_selector);
        const bool rcs_ok = apply_rcs_engine_workarounds(mmio, mcr_selector);
        return gt_ok && rcs_ok;
    }

    // Context-Liste als LRI für den ersten Batch jedes Contexts (vor dem User-Batch, mit Flush/Stall davor).
    u32* emit_ctx_lri(u32* cs) {
        u32 active_count = 0;
        for (const auto& w : kRcsContext) {
            if ((w.clr | w.set) != 0) ++active_count;
        }
        if (active_count == 0) return cs;

        *cs++ = (0x22u << 23) | (2 * active_count - 1);        // MI_LOAD_REGISTER_IMM
        for (const auto& w : kRcsContext) {
            const u32 want_mask = w.clr | w.set;
            if (want_mask == 0) continue;                      // leere Platzhalter (z. B. 0x7014) auslassen
            *cs++ = w.reg;
            *cs++ = (want_mask << 16) | w.set;                 // masked write
        }
        return cs;
    }

}  // namespace gpu::intel::core::wa
