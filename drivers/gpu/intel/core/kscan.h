// intel_kscan.h
// Debug-Helfer: Kernel-seitiger Scan des Render-Targets (Gegenstück zu scan_buffer()
// im Compositor), um zu prüfen, ob die Seqno "fertig" meldet, bevor die Pixel im RAM sind.
//
// Annahme (wie im Compositor): der Test-Client schreibt pro Frame einen Stempel
// (frame & 0xFF) in den G-Kanal ((px >> 8) & 0xFF) aller Pixel.
#pragma once

#include "vespera/time.h"
#include "vespera/log.h"
#include "vespera/types.h"

namespace gpu::intel::core::kscan {

    struct BadPixel {
        u32 x;
        u32 y;
        u32 val; // voller 32-Bit-Pixelwert
    };

    constexpr u32 MAX_BAD_PIXELS = 20;

    struct Result {
        u32 stamp;          // häufigster G-Wert (gleiche Auswahlregel wie im Compositor)
        u32 share_pct;      // Anteil der Pixel mit diesem häufigsten Stempel
        u32 exp_share_pct;  // Anteil der Pixel mit dem erwarteten Stempel
        u32 bad_lines;      // 64-B-Lines, in denen ein Pixel nicht dem Stempel entspricht
        u32 bad_pages;      // 4-KiB-Seiten mit mindestens einer schlechten Line
        u32 bad_px_count;   // Anzahl gespeicherter abweichender Pixel (max. MAX_BAD_PIXELS)
        BadPixel bad_px[MAX_BAD_PIXELS];
    };

    inline void flush_range(const u8* base, const usize len) {
        for (usize o = 0; o < len; o += 64)
            asm volatile("clflush (%0)" :: "r"(base + o) : "memory");
        asm volatile("mfence" ::: "memory");
    }

    // base = Kernel-Direktmapping des BOs, also virt_ptr(phys_to_virt(obj->phys_addr)).
    // expected_stamp: erwarteter G-Kanal-Wert (0..0xFF), oder 0xFFFFFFFFu falls unbekannt.
    inline Result scan(const u8* base, const u32 stride, const u32 w, const u32 h,
                       const u32 expected_stamp = 0xFFFFFFFFu) {
        const usize total = static_cast<usize>(stride) * h;
        flush_range(base, total); // Daten frisch aus dem RAM, nicht aus dem CPU-Cache

        Result res{};
        u32 hist[256] = {};
        for (u32 y = 0; y < h; y++) {
            const auto* r = reinterpret_cast<const volatile u32*>(base + static_cast<usize>(y) * stride);
            for (u32 x = 0; x < w; x++) hist[(r[x] >> 8) & 0xFF]++;
        }

        u32 stamp = 0;
        for (u32 i = 1; i < 256; i++) if (hist[i] > hist[stamp]) stamp = i;
        res.stamp = stamp;

        const u32 total_px = w * h;
        res.share_pct = total_px ? (hist[stamp] * 100u) / total_px : 0u;

        const u32 ref_stamp = (expected_stamp <= 0xFFu) ? expected_stamp : stamp;
        res.exp_share_pct = total_px ? (hist[ref_stamp] * 100u) / total_px : 0u;

        // Erste bis zu 20 Pixel sammeln, die vom erwarteten Stempel abweichen
        for (u32 y = 0; y < h && res.bad_px_count < MAX_BAD_PIXELS; y++) {
            const auto* r = reinterpret_cast<const volatile u32*>(base + static_cast<usize>(y) * stride);
            for (u32 x = 0; x < w && res.bad_px_count < MAX_BAD_PIXELS; x++) {
                const u32 px = r[x];
                if (((px >> 8) & 0xFFu) != ref_stamp) {
                    res.bad_px[res.bad_px_count++] = BadPixel{x, y, px};
                }
            }
        }

        u32 bad_lines = 0, bad_pages = 0;
        for (usize pg = 0; pg * 4096 < total; pg++) {
            bool page_bad = false;
            for (usize o = pg * 4096; o < (pg + 1) * 4096 && o < total; o += 64) {
                const auto* p = reinterpret_cast<const volatile u32*>(base + o);
                bool line_bad = false;
                for (int i = 0; i < 16; i++)
                    if (((p[i] >> 8) & 0xFF) != stamp) { line_bad = true; break; }
                if (line_bad) { bad_lines++; page_bad = true; }
            }
            if (page_bad) bad_pages++;
        }
        res.bad_lines = bad_lines;
        res.bad_pages = bad_pages;
        return res;
    }

    // Letzter "eingeschwungener" Stempel pro RT (für die Erkennung "alter Frame").
    struct PrevSlot { const u8* base; u32 stamp; };
    inline PrevSlot g_prev[8] = {};

    inline PrevSlot& prev_for(const u8* base) {
        for (auto& s : g_prev) if (s.base == base) return s;
        for (auto& s : g_prev) if (!s.base) { s.base = base; s.stamp = 0xFFFFFFFFu; return s; }
        g_prev[0] = PrevSlot{base, 0xFFFFFFFFu};
        return g_prev[0];
    }

    struct RegionStat {
        u32 samples;   // Anzahl gelesener Probe-Pixel
        u32 new_g;     // G == expected
        u32 prev_g;    // G == expected-1 (alter Frame)
        u32 other_g;   // alles andere
        u32 first_px;  // erster Pixelwert der Region (Rohwert für R/B-Check)
    };

    struct Probe {
        u32 expected;  // aus den Rand-Samples abgeleitet, 0xFFFFFFFF = unklar
        RegionStat interior, top, bottom, left, right;
    };

    inline u32 px_at(const u8* base, const u32 stride, const u32 x, const u32 y) {
        return *reinterpret_cast<const volatile u32*>(base + static_cast<usize>(y) * stride + static_cast<usize>(x) * 4);
    }

    inline void probe_add(RegionStat& r, const u32 px, const u32 expected) {
        const u32 g = (px >> 8) & 0xFFu;
        if (r.samples == 0) r.first_px = px;
        r.samples++;
        if (g == expected)                    r.new_g++;
        else if (g == ((expected - 1) & 0xFFu)) r.prev_g++;
        else                                  r.other_g++;
    }

    // Annahmen (Memory-Layout wie im Log beobachtet): Zeile 0 = "oben", Rand-Breite b = 3.
    inline Probe probe(const u8* base, const u32 stride, const u32 w, const u32 h,
                       const u32 expected_override = 0xFFFFFFFFu) {
        flush_range(base, static_cast<usize>(stride) * h);

        constexpr u32 b = 3;
        Probe p{};

        // 1) Erwarteten Stempel aus Rand-Samples bestimmen (Modus über G-Werte).
        u32 hist[256] = {};
        auto vote = [&](const u32 x, const u32 y) { hist[(px_at(base, stride, x, y) >> 8) & 0xFFu]++; };
        for (u32 i = 1; i <= 5; i++) {
            const u32 xs = (w * i) / 6, ys = (h * i) / 6;
            vote(xs, 1);          vote(xs, h - 2);
            vote(1, ys);          vote(w - 2, ys);
        }
        u32 best = 0;
        for (u32 i = 1; i < 256; i++) if (hist[i] > hist[best]) best = i;

        if (expected_override <= 0xFFu)  p.expected = expected_override;
        else if (hist[best] >= 3)        p.expected = best;
        else                             p.expected = 0xFFFFFFFFu;

        const u32 e = (p.expected <= 0xFFu) ? p.expected : 0u;

        // 2) Regionen abtasten (je 5 Punkte pro Rand, Raster 16x10 im Inneren).
        for (u32 i = 1; i <= 5; i++) {
            const u32 xs = (w * i) / 6, ys = (h * i) / 6;
            probe_add(p.top,    px_at(base, stride, xs, 1),         e);
            probe_add(p.bottom, px_at(base, stride, xs, h - 2),     e);
            probe_add(p.left,   px_at(base, stride, 1, ys),         e);
            probe_add(p.right,  px_at(base, stride, w - 2, ys),     e);
        }
        for (u32 gy = 0; gy < 10; gy++)
            for (u32 gx = 0; gx < 16; gx++)
                probe_add(p.interior,
                          px_at(base, stride, b + 2 + gx * ((w - 2 * b - 4) / 16), b + 2 + gy * ((h - 2 * b - 4) / 10)), e);
        return p;
    }

    inline char region_flag(const RegionStat& r) {
        if (r.samples == 0)          return '?';
        if (r.new_g == r.samples)    return 'N';  // alles neu
        if (r.prev_g == r.samples)   return 'O';  // alles alter Frame
        if (r.new_g == 0)            return 'X';  // weder neu noch alt
        return 'P';                               // gemischt
    }

    inline void probe_log(const char* tag, const u32 seq, const char* when, const Probe& p) {
        Log::log_dbc("kscan[%s] seq=%u PROBE@%s exp=%02x | int=%c(%u/%u) top=%c(%u/%u) bot=%c(%u/%u) left=%c(%u/%u) right=%c(%u/%u) | px int=%08x top=%08x bot=%08x left=%08x right=%08x\n",
                     tag, seq, when, p.expected & 0xFFu,
                     region_flag(p.interior), p.interior.new_g, p.interior.samples,
                     region_flag(p.top),      p.top.new_g,      p.top.samples,
                     region_flag(p.bottom),   p.bottom.new_g,   p.bottom.samples,
                     region_flag(p.left),     p.left.new_g,     p.left.samples,
                     region_flag(p.right),    p.right.new_g,    p.right.samples,
                     p.interior.first_px, p.top.first_px, p.bottom.first_px, p.left.first_px, p.right.first_px);
    }

    constexpr u8 EXP_R[5] = {0xFF, 0x00, 0x00, 0x00, 0x00};
    constexpr u8 EXP_B[5] = {0x00, 0x80, 0x40, 0xC0, 0xFF};

    inline char cls_char(const u32 px, const u32 i, const u32 exp) {
        const u32 g = (px >> 8) & 0xFF, r = (px >> 16) & 0xFF, b = px & 0xFF;
        if (g == exp)                  return (r == EXP_R[i] && b == EXP_B[i]) ? '.' : 'c'; // c = G neu, Farbe falsch
        if (g == ((exp - 1) & 0xFF))   return 'o'; // alter Frame
        return '?';
    }

    inline u32 g_cls_n = 0, g_cls_ok = 0;

    inline void classify_log(const char* tag, const u32 seq, const Probe& p) {
        if (p.expected > 0xFF) return;
        const u32 px[5] = {p.interior.first_px, p.top.first_px, p.bottom.first_px,
                           p.left.first_px, p.right.first_px};
        if (!(px[0] | px[1] | px[2] | px[3] | px[4])) return; // Vor-Stempel-Phase (alles 0)
        char s[6]; bool ok = true;
        for (u32 i = 0; i < 5; i++) { s[i] = cls_char(px[i], i, p.expected); ok &= (s[i] == '.'); }
        s[5] = 0;
        g_cls_n++; if (ok) g_cls_ok++;
        Log::log_dbc("kscan[%s] seq=%u CLS=%s exp=%02x px=%08x,%08x,%08x,%08x,%08x\n",
                     tag, seq, s, p.expected, px[0], px[1], px[2], px[3], px[4]);
        if ((g_cls_n % 50) == 0)
            Log::log_dbc("kscan-summary: n=%u ok=%u (%u%%)\n", g_cls_n, g_cls_ok, (g_cls_ok * 100u) / g_cls_n);
    }

    // Wartet (Spin, mit clflush auf die Seqno-Zeile), bis die Seqno erreicht ist, und scannt
    // das RT dann sofort sowie nach +0.2 / +1 / +5 / +20 ms.
    // Optional: expected_stamp (z. B. client_frame & 0xFF); Default ist target & 0xFF.
    inline void watch(const u8* rt, const u32 stride, const u32 w, const u32 h,
                      const volatile u32* seqno_ptr, const u32 target, const char* tag,
                      const u32 expected_stamp = 0xFFFFFFFFu) {
        constexpr int N = 5;
        constexpr u64 delays_us[N] = {0, 200, 1000, 5000, 20000};
        constexpr u64 TIMEOUT_US = 200000;

        const u32 expected = (expected_stamp <= 0xFFu) ? expected_stamp : (target & 0xFFu);

        const u64 t0 = kernel::time::get_uptime_us();
        u64 t_seen = 0;
        while (kernel::time::get_uptime_us() - t0 < TIMEOUT_US) {
            asm volatile("clflush (%0)" :: "r"(seqno_ptr) : "memory");
            asm volatile("mfence" ::: "memory");
            if (static_cast<i32>(*seqno_ptr - target) >= 0) { t_seen = kernel::time::get_uptime_us(); break; }
            asm volatile("pause");
        }
        if (!t_seen) {
            Log::log_dbc("kscan[%s] seq=%u exp=%02x TIMEOUT (gpu nicht fertig nach %llu us)\n",
                         tag, target, expected, (unsigned long long)TIMEOUT_US);
            return;
        }

        const Probe p_now = probe(rt, stride, w, h, expected);

        Result r[N];
        u64 at[N];
        for (int i = 0; i < N; i++) {
            while (kernel::time::get_uptime_us() - t_seen < delays_us[i]) asm volatile("pause");
            at[i] = kernel::time::get_uptime_us() - t_seen;
            r[i] = scan(rt, stride, w, h, expected);
        }

        const Probe pf = probe(rt, stride, w, h);
        classify_log(tag, target, pf);

        const Probe p_late = probe(rt, stride, w, h, expected);
        if (region_flag(p_now.interior) != 'N' || region_flag(p_late.interior) != 'N'
            || region_flag(p_now.top) == 'O' || region_flag(p_now.left) == 'O') {
            probe_log(tag, target, "now",   p_now);
            probe_log(tag, target, "+20ms", p_late);
            }

        PrevSlot& prev = prev_for(rt);
        const char* verdict = "ok";
        if (r[N - 1].stamp == prev.stamp)                         verdict = "NEVER-FRESH (alter Frame bleibt auch nach 20ms)";
        else if (r[0].stamp == prev.stamp)                        verdict = "EARLY-STALE (jetzt alt, spaeter neu)";
        else if (r[0].bad_lines > 0 && r[N - 1].bad_lines == 0)   verdict = "EARLY-PARTIAL (jetzt gemischt, spaeter sauber)";
        else if (r[0].bad_px_count > 0 || r[N - 1].stamp != expected) verdict = "STAMP-MISMATCH";

        Log::log_dbc("kscan[%s] seq=%u exp=%02x prev=%02x | now(+%lluus) st=%02x(%u%%) bad=%u | +%lluus st=%02x(%u%%) bad=%u | +%lluus st=%02x(%u%%) bad=%u | +%lluus st=%02x(%u%%) bad=%u | +%lluus st=%02x(%u%%) bad=%u | %s\n",
                     tag, target, expected, prev.stamp & 0xFF,
                     (unsigned long long)at[0], r[0].stamp, r[0].share_pct, r[0].bad_lines,
                     (unsigned long long)at[1], r[1].stamp, r[1].share_pct, r[1].bad_lines,
                     (unsigned long long)at[2], r[2].stamp, r[2].share_pct, r[2].bad_lines,
                     (unsigned long long)at[3], r[3].stamp, r[3].share_pct, r[3].bad_lines,
                     (unsigned long long)at[4], r[4].stamp, r[4].share_pct, r[4].bad_lines,
                     verdict);

        // Detail-Log für Bad-Events: share_pct, erwarteter Stempel und erste ~20 abweichende Pixel
        int bad_idx = -1;
        for (int i = 0; i < N; i++) {
            if (r[i].bad_px_count > 0 || r[i].bad_lines > 0) {
                bad_idx = i;
                break;
            }
        }

        if (bad_idx >= 0) {
            const Result& br = r[bad_idx];
            Log::log_dbc("kscan[%s] BAD-DETAIL (+%lluus): exp=%02x (seq_lo=%02x) dom_st=%02x share_pct=%u%% exp_share_pct=%u%% bad_px=%u:",
                         tag, (unsigned long long)at[bad_idx],
                         expected, target & 0xFFu,
                         br.stamp, br.share_pct, br.exp_share_pct, br.bad_px_count);
            for (u32 k = 0; k < br.bad_px_count; k++) {
                Log::log_dbc(" (%u,%u,0x%08x)", br.bad_px[k].x, br.bad_px[k].y, br.bad_px[k].val);
            }
            Log::log_dbc("\n");
        }

        prev.stamp = r[N - 1].stamp;
    }

} // namespace gpu::intel::core::kscan