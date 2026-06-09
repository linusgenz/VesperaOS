-- crepusculum.lua

display = {
    target_fps   = 120,
    bg_color     = 0xFF1A1A24,
}

ssd = {
    titlebar_h      = 34,
    border_w        = 1,
    color_titlebar  = 0xFF252535,
    color_border    = 0xFF3A3A52,
    color_title_fg  = 0xFFD0D0E8,

    color_btn_close    = 0xFFED8796,   -- muted red
    color_btn_maximize = 0xFF8EC994,   -- muted green
    color_btn_minimize = 0xFFF8D080,   -- muted yellow

    btn_size      = 16,   -- circle diameter in pixels
    btn_margin    = 4,    -- horizontal gap between buttons
    btn_right_pad = 8,    -- distance from the right edge of the titlebar

}

cursor = {
    xcursor_path        = "/usr/share/icons/Bibata-Modern-Ice/cursors/left_ptr",
    xcursor_target_size = 24,
}

compositor = {
    desktop_binary = "/bin/firmament",
}