#include "framebuffer_driver.h"

#include <vespera/cpu/simd.h>
#include <vespera/cpu/simd_mem.h>
#include <vespera/terminal.h>

#include <vespera/filesystem/devfs.h>
#include "vespera/devices/device_manager.h"

static void* scalar_memcpy(void* dst, const void* src, usize len) {
    auto* d = static_cast<u8*>(dst);
    const auto* s = static_cast<const u8*>(src);
    while (len--) *d++ = *s++;
    return dst;
}

static void scalar_fill_rect(void* base, const u32 stride, const u32 px, const u32 py, const u32 w, const u32 h, const u32 colour) {
    auto* fb = static_cast<u32*>(base);
    for (u32 y = 0; y < h; y++) {
        u32* row = fb + (py + y) * stride + px;
        for (u32 x = 0; x < w; x++) row[x] = colour;
    }
}

static void* scalar_memmove(void* dst, const void* src, usize len) {
    auto* d = static_cast<u8*>(dst);
    const auto* s = static_cast<const u8*>(src);
    if (d < s) {
        while (len--) *d++ = *s++;
    } else {
        d += len;
        s += len;
        while (len--) *--d = *--s;
    }
    return dst;
}

FramebufferDriver::FramebufferDriver(Framebuffer* fb, PsfFont* font)
    : fb_(fb)
    , font_(font) {
    char name[10];
    DeviceManager::alloc_unique_device_name("uefi_gop", name, sizeof(name));
    kd_ = DeviceManager::register_device(
        DeviceDescriptor{}
            .set_name(name)
            .set_type(DeviceType::Gpu)
            .set_class(DeviceClass::Graphics)
            .set_bus(BusType::VIRTUAL)
            .set_controller(ControllerType::UefiGop)
            .with_gpu(this)
    );
    DevFs::register_device(kd_);
    init_simd();
}

void FramebufferDriver::init_simd() noexcept {
    const auto& f = simd_features();
    if (f.avx2) {
        using_avx = true;
        fn_memcpy_ = fb_memcpy_avx2;
        fn_fill_rect_ = fb_fill_rect_sse2; // there is an issue with fb_fill_rect_avx2 and interrupt handlers, so we use sse2 for now
        fn_memmove_ = fb_memmove_avx2;
    } else if (f.sse2) {
        using_sse = true;
        fn_memcpy_ = fb_memcpy_sse2;
        fn_fill_rect_ = fb_fill_rect_sse2;
        fn_memmove_ = scalar_memmove;
    } else {
        fn_memcpy_ = scalar_memcpy;
        fn_fill_rect_ = scalar_fill_rect;
        fn_memmove_ = scalar_memmove;
    }
}

bool FramebufferDriver::fill_rect(const u32 px, const u32 py, const u32 w, const u32 h, const u32 colour) {
    if (px + w > fb_->width || py + h > fb_->height) return false;
    fn_fill_rect_(fb_->base_address, fb_->pixels_per_scanline, px, py, w, h, colour);
    return true;
}

void FramebufferDriver::clear() {
    fill_rect(0, 0, fb_->width, fb_->height, 0x00000000);
}

void FramebufferDriver::put_char(char c, const u32 x, const u32 y, const u32 fg_color, const u32 bg_color) const {
    if (c >= static_cast<psf2_header_t*>(font_->header)->length) c = '?';
    if (!c) return;

    const auto pix_ptr = static_cast<u32*>(fb_->base_address);
    const char* glyph = static_cast<char*>(font_->glyph_buffer) + (c * font_->charsize);

    for (u32 row = 0; row < font_->height; row++) {
        for (u32 bx = 0; bx < (font_->width + 7) / 8; bx++) {
            const u8 byte = glyph[row * ((font_->width + 7) / 8) + bx];
            for (u32 bit = 0; bit < 8; bit++) {
                const u32 xpix = bx * 8 + bit;
                if (xpix >= font_->width) break;

               const  u32 color_to_draw = (byte & (0x80 >> bit)) ? fg_color : bg_color;
                *(pix_ptr + (x + xpix) + (y + row) * fb_->pixels_per_scanline) = color_to_draw;
            }
        }
    }
}

bool FramebufferDriver::blit_buffer(const void* pixels, const u32 buffer_width, const u32 buffer_height, const u32 dst_x, const u32 dst_y) {
    if (!pixels) return false;
    if (dst_x >= fb_->width || dst_y >= fb_->height) return false;

    const u32 max_w = (dst_x + buffer_width > fb_->width) ? fb_->width - dst_x : buffer_width;
    const u32 max_h = (dst_y + buffer_height > fb_->height) ? fb_->height - dst_y : buffer_height;

    const auto* src = static_cast<const u32*>(pixels);
    auto* dst = static_cast<u32*>(fb_->base_address);

    for (u32 y = 0; y < max_h; y++) {
        fn_memcpy_(dst + (dst_y + y) * fb_->pixels_per_scanline + dst_x, src + y * buffer_width, max_w * sizeof(u32));
    }
    return true;
}

bool FramebufferDriver::scroll_pixels(const int dy) {
    if (dy <= 0 || static_cast<u32>(dy) >= fb_->height) return false;

    const u32 bps = fb_->pixels_per_scanline * 4;
    fn_memmove_(fb_->base_address, static_cast<u8*>(fb_->base_address) + bps * dy, bps * (fb_->height - dy));
    fill_rect(0, fb_->height - dy, fb_->width, dy, 0x00000000);
    return true;
}

u32 FramebufferDriver::screen_width_px() const {
    return fb_->width;
}
u32 FramebufferDriver::screen_height_px() const {
    return fb_->height;
}
u32 FramebufferDriver::bytes_per_scanline() const {
    return fb_->pixels_per_scanline * 4;
}
