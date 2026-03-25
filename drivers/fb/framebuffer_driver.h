#ifndef BASIC_RENDERER_H
#define BASIC_RENDERER_H
#include <klib/string.h>
#include <vespera/devices/device_info.h>
#include <vespera/devices/device_manager.h>
#include <vespera/terminal.h>

#include "vespera/cpu/simd.h"

class FramebufferDriver final : public IRenderDriver, public IDeviceInfo {
   public:
    void simd_gop_init(const SimdFeatures& f);
    FramebufferDriver(Framebuffer* fb, PsfFont* font);
    void init_simd() noexcept;

    // IRenderDriver Interface
    bool fill_rect(u32 px, u32 py, u32 w, u32 h, u32 colour) override;
    bool scroll_pixels(int dy) override;
    bool blit_buffer(const void* pixels, u32 buffer_width, u32 buffer_height, u32 dst_x, u32 dst_y) override;
    [[nodiscard]] u32 screen_width_px() const override;
    [[nodiscard]] u32 screen_height_px() const override;
    [[nodiscard]] u32 bytes_per_scanline() const override;

    [[nodiscard]] KernelDevice* get_kd() const {
        return kd_;
    }

    void put_char(char c, u32 x, u32 y, u32 fg_color, u32 bg_color) const;
    void clear();

    bool using_avx;
    bool using_sse;

    bool get_vendor(char* out, usize len) override {
        strcpy(out, "UEFI");
        return true;
    }

    bool get_model(char* out, usize len) override {
        strcpy(out, "Software Rendering");
        return true;
    }

    bool get_firmware(char* out, usize len) override {
        strcpy(out, "UEFI GOP");
        return true;
    }

   private:
    Framebuffer* fb_;
    PsfFont* font_;

    void* (*fn_memcpy_)(void*, const void*, usize) = nullptr;
    void (*fn_fill_rect_)(void*, u32, u32, u32, u32, u32, u32) = nullptr;
    void* (*fn_memmove_)(void*, const void*, usize) = nullptr;

    KernelDevice* kd_;
};

#endif  // BASIC_RENDERER_H
