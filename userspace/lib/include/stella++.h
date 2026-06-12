// stella++.h
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
#ifndef VESPERAOS_STELLA_PLUS_PLUS_LIB_H
#define VESPERAOS_STELLA_PLUS_PLUS_LIB_H

#include <stella.h>
#include <cstdint>
#include <functional>

namespace stella {

    class Color {
    public:
        Color(uint32_t hex) noexcept : _c(stella_hex(hex)) {
        }

        Color(uint8_t r, uint8_t g, uint8_t b) noexcept : _c(stella_rgb(r, g, b)) {
        }

        Color(stella_color_t c) noexcept : _c(c) {
        }

        operator stella_color_t() const noexcept { return _c; }

    private:
        stella_color_t _c;
    };

    enum class Align : int {
        Default = STELLA_ALIGN_DEFAULT,
        TopLeft = STELLA_ALIGN_TOP_LEFT,
        TopMid = STELLA_ALIGN_TOP_MID,
        TopRight = STELLA_ALIGN_TOP_RIGHT,
        BottomLeft = STELLA_ALIGN_BOTTOM_LEFT,
        BottomMid = STELLA_ALIGN_BOTTOM_MID,
        BottomRight = STELLA_ALIGN_BOTTOM_RIGHT,
        LeftMid = STELLA_ALIGN_LEFT_MID,
        RightMid = STELLA_ALIGN_RIGHT_MID,
        Center = STELLA_ALIGN_CENTER,
    };

    enum class Flex : int {
        Start = STELLA_FLEX_START,
        End = STELLA_FLEX_END,
        Center = STELLA_FLEX_CENTER,
        SpaceEvenly = STELLA_FLEX_SPACE_EVENLY,
        SpaceAround = STELLA_FLEX_SPACE_AROUND,
        SpaceBetween = STELLA_FLEX_SPACE_BETWEEN,
    };

    enum class Text : int {
        Auto = STELLA_TEXT_ALIGN_AUTO,
        Left = STELLA_TEXT_ALIGN_LEFT,
        Center = STELLA_TEXT_ALIGN_CENTER,
        Right = STELLA_TEXT_ALIGN_RIGHT,
    };

    enum class Opa : uint8_t {
        Transp = STELLA_OPA_TRANSP,
        p10 = STELLA_OPA_10,
        p20 = STELLA_OPA_20,
        p30 = STELLA_OPA_30,
        p40 = STELLA_OPA_40,
        p50 = STELLA_OPA_50,
        p60 = STELLA_OPA_60,
        p70 = STELLA_OPA_70,
        p80 = STELLA_OPA_80,
        p90 = STELLA_OPA_90,
        Cover = STELLA_OPA_COVER,
    };


    inline constexpr int32_t Full = STELLA_SIZE_FULL;
    inline constexpr int32_t Content = STELLA_SIZE_CONTENT;

    class Widget {
    public:
        explicit Widget(stella_widget_t h) noexcept;

        operator stella_widget_t() const noexcept;
        stella_widget_t handle() const noexcept;

        // Size & position
        Widget& size(int32_t w, int32_t h);
        Widget& width(int32_t w);
        Widget& height(int32_t h);
        Widget& pos(int32_t x, int32_t y);
        Widget& center();
        Widget& align(Align a, int32_t dx = 0, int32_t dy = 0);

        // Background
        Widget& bg(Color c, Opa opa = Opa::Cover);
        Widget& bgTransp();
        Widget& gradient(Color top, Color bot,
                         uint8_t main_stop = 128, uint8_t grad_stop = 255,
                         Opa opa = Opa::Cover);
        Widget& hoverBg(Color c, Opa opa = Opa::Cover);

        // Border
        Widget& border(Color c, int32_t w, Opa opa = Opa::Cover);
        Widget& borderBottom(Color c, int32_t w);
        Widget& borderTop(Color c, int32_t w);
        Widget& noBorder();

        // Shape
        Widget& radius(int32_t r);

        // Padding
        Widget& pad(int32_t p);
        Widget& padHor(int32_t p);
        Widget& padVer(int32_t p);
        Widget& padTop(int32_t p);
        Widget& padLeft(int32_t p);
        Widget& padRow(int32_t p);
        Widget& padCol(int32_t p);

        // Flex layout
        Widget& flexRow(Flex main = Flex::Start,
                        Flex cross = Flex::Center,
                        Flex track = Flex::Start);
        Widget& flexCol(Flex main = Flex::Start,
                        Flex cross = Flex::Center,
                        Flex track = Flex::Start);

        // Events
        Widget& onClick(std::function<void()> cb);

        // Misc
        Widget& noScroll();
        void destroy() const;

    protected:
        stella_widget_t _w;
    };

    class Label : public Widget {
    public:
        Label(stella_widget_t parent, const char* text = "");

        Label& text(const char* t);
        Label& font(const stella_font_t* f);
        Label& color(Color c);
        Label& textAlign(Text a);
        Label& textPadTop(int32_t p);
        Label& longDot();
    };

    class Button : public Widget {
    public:
        Button(stella_widget_t parent, const char* text,
               int32_t w = STELLA_SIZE_CONTENT,
               int32_t h = STELLA_SIZE_CONTENT);
    };

    class Container : public Widget {
    public:
        explicit Container(stella_widget_t parent);

        void clean() const;
    };

    class Bar : public Widget {
    public:
        Bar(stella_widget_t parent, int32_t w = 200, int32_t h = 20);

        Bar& range(int32_t min, int32_t max);
        Bar& value(int32_t v);
        Bar& trackColor(Color c);
        Bar& trackRadius(int32_t r);
        Bar& indicatorColor(Color c);
        Bar& indicatorRadius(int32_t r);
    };

    class Image : public Widget {
    public:
        // From a compiled-in data pointer (LVGL image array).
        Image(stella_widget_t parent, const void* src,
              int32_t w = STELLA_SIZE_CONTENT,
              int32_t h = STELLA_SIZE_CONTENT);

        // From a VesperaOS filesystem path at runtime.
        Image(stella_widget_t parent, const char* path,
              int32_t w = STELLA_SIZE_CONTENT,
              int32_t h = STELLA_SIZE_CONTENT);
    };

    // RAII: timer is deleted when the object goes out of scope.
    //
    //   stella::Timer clock([&] { refresh_clock_label(); }, 1000);

    class Timer {
    public:
        Timer(std::function<void()> cb, uint32_t period_ms);
        ~Timer();

        Timer(const Timer&) = delete;
        Timer& operator=(const Timer&) = delete;

        void fireNow();

    private:
        stella_timer_t* _t = nullptr;
        std::function<void()>* _fn = nullptr;
    };

    // ─── Window ───────────────────────────────────────────────────────────────────
    // Owns the stella_window_t*. Call run() to enter the main loop.
    //
    //   stella::Window win("My App", 800, 480);
    //   win.bg(0x1A1A2E).onClose([] { config_save(); });
    //   win.run();

    class Window {
    public:
        Window(const char* title, uint32_t w, uint32_t h, uint32_t flags = 0);
        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        stella_widget_t screen() const;

        Window& bg(Color c, Opa opa = Opa::Cover);

        Window& onClose(std::function<void()> cb);

        // Blocking ~60 fps loop. Returns when the window is closed.
        void run() const;

    private:
        stella_window_t* _win = nullptr;
        std::function<void()>* _close_fn = nullptr;
    };
} // namespace stella

#endif //VESPERAOS_STELLA_PLUS_PLUS_LIB_H
