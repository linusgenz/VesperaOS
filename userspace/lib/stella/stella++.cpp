// stella++.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 08.06.26.
//
// This file is part of VesperaOS.
//
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

// stella++.cpp
// Implementation of the stella++ C++ wrapper.
// Compile and link alongside your app: c++ -std=c++17 app.cpp stella++.cpp -lstella

#include "stella++.h"

namespace stella {

// ─── Internal callback helpers ────────────────────────────────────────────────
// Not exposed in the header — purely an implementation detail.

namespace {

    using VoidFn = std::function<void()>;

    // Heap-allocates fn so its address stays stable for LVGL's lifetime.
    // Widgets typically live until the window is destroyed, so this is a
    // bounded allocation, not a leak.
    void* alloc_fn(VoidFn fn) {
        return new VoidFn(std::move(fn));
    }

    void click_trampoline(stella_widget_t, void* ud) {
        (*static_cast<VoidFn*>(ud))();
    }

    void close_trampoline(stella_window_t*, void* ud) {
        (*static_cast<VoidFn*>(ud))();
    }

    void timer_trampoline(stella_timer_t*, void* ud) {
        (*static_cast<VoidFn*>(ud))();
    }

} // anonymous namespace

// ─── Widget ───────────────────────────────────────────────────────────────────

Widget::Widget(stella_widget_t h) noexcept : _w(h) {}

Widget::operator stella_widget_t() const noexcept { return _w; }
stella_widget_t Widget::handle()   const noexcept { return _w; }

// Size & position

Widget& Widget::size(int32_t w, int32_t h)       { stella_widget_set_size(_w, w, h);  return *this; }
Widget& Widget::width(int32_t w)                  { stella_widget_set_width(_w, w);    return *this; }
Widget& Widget::height(int32_t h)                 { stella_widget_set_height(_w, h);   return *this; }
Widget& Widget::pos(int32_t x, int32_t y)         { stella_widget_set_pos(_w, x, y);   return *this; }
Widget& Widget::center()                          { stella_widget_center(_w);           return *this; }

Widget& Widget::align(Align a, int32_t dx, int32_t dy) {
    stella_widget_align(_w, static_cast<stella_align_t>(a), dx, dy);
    return *this;
}

// Background

Widget& Widget::bg(Color c, Opa opa) {
    stella_widget_set_bg(_w, c, static_cast<stella_opa_t>(opa));
    return *this;
}

Widget& Widget::bgTransp() {
    stella_widget_set_bg_transp(_w);
    return *this;
}

Widget& Widget::gradient(Color top, Color bot,
                          uint8_t main_stop, uint8_t grad_stop, Opa opa) {
    stella_widget_set_vertical_gradient(
        _w, top, bot, main_stop, grad_stop, static_cast<stella_opa_t>(opa));
    return *this;
}

Widget& Widget::hoverBg(Color c, Opa opa) {
    stella_widget_set_hover_bg(_w, c, static_cast<stella_opa_t>(opa));
    return *this;
}

// Border

Widget& Widget::border(Color c, int32_t w, Opa opa) {
    stella_widget_set_border(_w, c, w, static_cast<stella_opa_t>(opa));
    return *this;
}

Widget& Widget::borderBottom(Color c, int32_t w) {
    stella_widget_set_border_bottom(_w, c, w);
    return *this;
}

Widget& Widget::borderTop(Color c, int32_t w) {
    stella_widget_set_border_top(_w, c, w);
    return *this;
}

Widget& Widget::noBorder() {
    stella_widget_no_border(_w);
    return *this;
}

// Shape

Widget& Widget::radius(int32_t r) { stella_widget_set_radius(_w, r); return *this; }

// Padding

Widget& Widget::pad(int32_t p)     { stella_widget_set_pad_all(_w, p);  return *this; }
Widget& Widget::padHor(int32_t p)  { stella_widget_set_pad_hor(_w, p);  return *this; }
Widget& Widget::padVer(int32_t p)  { stella_widget_set_pad_ver(_w, p);  return *this; }
Widget& Widget::padTop(int32_t p)  { stella_widget_set_pad_top(_w, p);  return *this; }
Widget& Widget::padLeft(int32_t p) { stella_widget_set_pad_left(_w, p); return *this; }
Widget& Widget::padRow(int32_t p)  { stella_widget_set_pad_row(_w, p);  return *this; }
Widget& Widget::padCol(int32_t p)  { stella_widget_set_pad_col(_w, p);  return *this; }

// Flex layout

Widget& Widget::flexRow(Flex main, Flex cross, Flex track) {
    stella_widget_flex_row(_w,
        static_cast<stella_flex_align_t>(main),
        static_cast<stella_flex_align_t>(cross),
        static_cast<stella_flex_align_t>(track));
    return *this;
}

Widget& Widget::flexCol(Flex main, Flex cross, Flex track) {
    stella_widget_flex_col(_w,
        static_cast<stella_flex_align_t>(main),
        static_cast<stella_flex_align_t>(cross),
        static_cast<stella_flex_align_t>(track));
    return *this;
}

// Events

Widget& Widget::onClick(std::function<void()> cb) {
    stella_widget_on_click(_w, click_trampoline, alloc_fn(std::move(cb)));
    return *this;
}

// Misc

Widget& Widget::noScroll() { stella_widget_no_scroll(_w); return *this; }
void    Widget::destroy()  { stella_widget_delete(_w); }

// ─── Label ────────────────────────────────────────────────────────────────────

Label::Label(stella_widget_t parent, const char* text)
    : Widget(stella_label_create(parent, text)) {}

Label& Label::text(const char* t)       { stella_label_update(_w, t);                                  return *this; }
Label& Label::font(const stella_font_t* f) { stella_text_set_font(_w, f);                              return *this; }
Label& Label::color(Color c)            { stella_text_set_color(_w, c);                                return *this; }
Label& Label::textAlign(Text a)         { stella_text_set_align(_w, static_cast<stella_text_align_t>(a)); return *this; }
Label& Label::textPadTop(int32_t p)     { stella_text_set_pad_top(_w, p);                              return *this; }
Label& Label::longDot()                 { stella_label_set_long_dot(_w);                               return *this; }

// ─── Button ───────────────────────────────────────────────────────────────────

Button::Button(stella_widget_t parent, const char* text, int32_t w, int32_t h)
    : Widget(stella_button_create(parent, text, w, h)) {}

// ─── Container ────────────────────────────────────────────────────────────────

Container::Container(stella_widget_t parent)
    : Widget(stella_container_create(parent)) {}

void Container::clean() { stella_widget_clean(_w); }

// ─── Bar ──────────────────────────────────────────────────────────────────────

Bar::Bar(stella_widget_t parent, int32_t w, int32_t h)
    : Widget(stella_bar_create(parent, w, h)) {}

Bar& Bar::range(int32_t min, int32_t max) { stella_bar_set_range(_w, min, max);      return *this; }
Bar& Bar::value(int32_t v)                { stella_bar_set_value(_w, v);             return *this; }
Bar& Bar::trackColor(Color c)             { stella_bar_set_track_color(_w, c);       return *this; }
Bar& Bar::trackRadius(int32_t r)          { stella_bar_set_track_radius(_w, r);      return *this; }
Bar& Bar::indicatorColor(Color c)         { stella_bar_set_indicator_color(_w, c);   return *this; }
Bar& Bar::indicatorRadius(int32_t r)      { stella_bar_set_indicator_radius(_w, r);  return *this; }

// ─── Image ────────────────────────────────────────────────────────────────────

Image::Image(stella_widget_t parent, const void* src, int32_t w, int32_t h)
    : Widget(stella_image_create(parent, src, w, h)) {}

Image::Image(stella_widget_t parent, const char* path, int32_t w, int32_t h)
    : Widget(stella_image_create_from_path(parent, path, w, h)) {}

// ─── Timer ────────────────────────────────────────────────────────────────────

Timer::Timer(std::function<void()> cb, uint32_t period_ms) {
    _fn = new std::function<void()>(std::move(cb));
    _t  = stella_timer_create(timer_trampoline, period_ms, _fn);
}

Timer::~Timer() {
    stella_timer_delete(_t);
    delete _fn;
}

void Timer::fireNow() { stella_timer_fire_now(_t); }

// ─── Window ───────────────────────────────────────────────────────────────────

Window::Window(const char* title, uint32_t w, uint32_t h, uint32_t flags) {
    stella_config_t cfg;
    cfg.title  = title;
    cfg.width  = w;
    cfg.height = h;
    cfg.flags  = flags;
    _win = stella_window_create(&cfg);
}

Window::~Window() {
    delete _close_fn;
    stella_window_destroy(_win);
}

stella_widget_t Window::screen() const { return stella_window_get_screen(_win); }

Window& Window::bg(Color c, Opa opa) {
    stella_widget_set_bg(screen(), c, static_cast<stella_opa_t>(opa));
    return *this;
}

Window& Window::onClose(std::function<void()> cb) {
    _close_fn = new std::function<void()>(std::move(cb));
    stella_window_on_close(_win, close_trampoline, _close_fn);
    return *this;
}

void Window::run() {
    constexpr long long kFrameMs = 16LL;
    long long last = now_ms();

    while (!stella_window_should_close(_win)) {
        stella_process_events(_win);

        long long now   = now_ms();
        uint32_t  delta = static_cast<uint32_t>(now - last);
        last = now;

        if (delta > 0) stella_tick(delta);

        long long elapsed = now_ms() - now;
        long long sleep   = kFrameMs - elapsed;
        if (sleep > 0) sleep_ms(sleep);
    }
}

} // namespace stella