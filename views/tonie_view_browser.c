#include "tonie_view_browser.h"

#include <gui/canvas.h>
#include <gui/elements.h>
#include <input/input.h>
#include <stdio.h>
#include <string.h>

#define TONIE_BROWSER_VISIBLE_ROWS 4
#define TONIE_BROWSER_DEFAULT_SCROLL_MS 250

/* 7x7 Asymmetric-Ear Toniehead Logo Icon (XBM format, LSB first per byte) */
static const uint8_t tonie_head_icon_7x7[] = {
    0x20, /* .....#. */
    0x32, /* .#..##. */
    0x3B, /* ##.###. */
    0x3F, /* ######. */
    0x31, /* #...##. */
    0x3F, /* ######. */
    0x1E, /* .####.. */
};

/* 7x7 Folder Icon for Series Directories */
static const uint8_t tonie_folder_icon_7x7[] = {
    0x07, /* ###.... */
    0x7F, /* ####### */
    0x41, /* #.....# */
    0x7F, /* ####### */
    0x7F, /* ####### */
    0x7F, /* ####### */
    0x3E, /* .#####. */
};

/* 7x7 Star Icon for Favorited Tonies */
static const uint8_t tonie_star_icon_7x7[] = {
    0x08, /* ...#... */
    0x08, /* ...#... */
    0x7F, /* ####### */
    0x3E, /* .#####. */
    0x1C, /* ..###.. */
    0x36, /* .##.##. */
    0x41, /* #.....# */
};

typedef struct {
    const TonieLibrary* library;
    size_t selected_index;
    size_t scroll_offset;
    TonieLang active_lang;
    TonieBrowseLevel level;
    TonieUiLang ui_lang;
    uint16_t marquee_tick;
    bool picker_mode;
} TonieBrowserModel;

struct TonieViewBrowser {
    View* view;
    FuriTimer* scroll_timer;
    uint32_t scroll_speed_ms;
    TonieViewBrowserSelectCallback on_select;
    TonieViewBrowserLangCallback on_lang_change;
    TonieViewBrowserFavCallback on_fav_toggle;
    TonieViewBrowserBackCallback on_back;
    void* callback_context;
    bool ignore_next_ok_short;
};

static void tonie_view_browser_adjust_scroll(TonieBrowserModel* model) {
    size_t count = model->library ? model->library->count : 0;
    if(count == 0) {
        model->selected_index = 0;
        model->scroll_offset = 0;
        return;
    }

    if(model->selected_index >= count) {
        model->selected_index = count - 1;
    }

    if(model->selected_index < model->scroll_offset) {
        model->scroll_offset = model->selected_index;
    } else if(model->selected_index >= model->scroll_offset + TONIE_BROWSER_VISIBLE_ROWS) {
        model->scroll_offset = model->selected_index - TONIE_BROWSER_VISIBLE_ROWS + 1;
    }
}

/* Format text to fit within max_px, scrolling horizontally if active_scroll is true */
static void tonie_view_browser_fit_string(
    Canvas* canvas,
    const char* src,
    char* out,
    size_t out_size,
    size_t max_px,
    bool active_scroll,
    uint16_t tick) {
    if(!src || !out || out_size == 0) {
        return;
    }

    size_t full_w = canvas_string_width(canvas, src);
    if(full_w <= max_px) {
        snprintf(out, out_size, "%s", src);
        return;
    }

    size_t src_len = strlen(src);
    size_t start_idx = 0;

    if(active_scroll && src_len > 4) {
        /* Pause 3 ticks at start, then advance 1 char per tick, pause 3 ticks at end */
        size_t max_shift = src_len - 4;
        size_t cycle = max_shift + 6;
        size_t pos = tick % cycle;
        if(pos < 3) {
            start_idx = 0;
        } else if(pos - 3 < max_shift) {
            start_idx = pos - 3;
        } else {
            start_idx = max_shift;
        }
    }

    const char* sub = src + start_idx;
    snprintf(out, out_size, "%s", sub);

    size_t len = strlen(out);
    while(len > 0 && canvas_string_width(canvas, out) > max_px) {
        out[--len] = '\0';
    }

    /* Append subtle '..' indicator on static unselected rows that were truncated */
    if(!active_scroll && len >= 2) {
        out[len - 1] = '.';
        out[len - 2] = '.';
        while(len > 2 && canvas_string_width(canvas, out) > max_px) {
            out[len - 3] = '.';
            out[--len] = '\0';
        }
    }
}

static void tonie_view_browser_draw_header(Canvas* canvas, const TonieBrowserModel* model) {
    /* Top Header Bar (y: 0..12) */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 13);

    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);

    /* Left side: Item Counter [X/Y] (e.g. [244/268]) */
    size_t count = model->library ? model->library->count : 0;
    size_t current = (count > 0) ? (model->selected_index + 1) : 0;
    char counter_str[24];
    snprintf(counter_str, sizeof(counter_str), "[%zu/%zu]", current, count);
    canvas_draw_str(canvas, 2, 10, counter_str);

    /* Right side: either Series Picker title or Interactive 3-Tab Selector [DE | EN | FAV] */
    if(model->picker_mode) {
        const char* picker_title =
            (model->ui_lang == TonieUiLangEn) ? "SELECT SERIES" : "SERIE WAEHLEN";
        canvas_draw_str_aligned(canvas, 125, 10, AlignRight, AlignBottom, picker_title);
        return;
    }

    const int16_t tab_x[3] = {61, 83, 105};
    const int16_t tab_w[3] = {21, 21, 22};
    const char* tab_lbl[3] = {"DE", "EN", "FAV"};

    for(int i = 0; i < 3; i++) {
        bool active = ((int)model->active_lang == i);
        if(active) {
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_rbox(canvas, tab_x[i], 1, tab_w[i], 11, 2);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str_aligned(
            canvas,
            tab_x[i] + (tab_w[i] / 2),
            10,
            AlignCenter,
            AlignBottom,
            tab_lbl[i]);
    }
}

static void tonie_view_browser_draw_empty(Canvas* canvas, const TonieBrowserModel* model) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, 3, 15, 122, 35, 3);
    canvas_set_font(canvas, FontSecondary);
    bool en = (model->ui_lang == TonieUiLangEn);

    if(model->active_lang == TonieLangFavorites) {
        canvas_draw_str_aligned(
            canvas, 64, 26, AlignCenter, AlignBottom, en ? "No favorites yet!" : "Keine Favoriten!");
        canvas_draw_str_aligned(
            canvas,
            64,
            36,
            AlignCenter,
            AlignBottom,
            en ? "Hold [OK] on a Tonie inside" : "In einer Serie [OK] gedrueckt");
        canvas_draw_str_aligned(
            canvas,
            64,
            46,
            AlignCenter,
            AlignBottom,
            en ? "a series for favorite (*)" : "halten fuer Favorit (*)");
    } else if(model->level == TonieBrowseLevelSeries) {
        canvas_draw_str_aligned(
            canvas,
            64,
            26,
            AlignCenter,
            AlignBottom,
            en ? "No series folders in:" : "Keine Serien-Ordner in:");
        const char* hint = (model->active_lang == TonieLangGerman) ?
                               ".../tonies/German/{Serie}" :
                               ".../tonies/English/{Serie}";
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignBottom, hint);
        canvas_draw_str_aligned(
            canvas,
            64,
            46,
            AlignCenter,
            AlignBottom,
            en ? "[< / >] Switch tab" : "[< / >] Tab wechseln");
    } else {
        canvas_draw_str_aligned(
            canvas,
            64,
            28,
            AlignCenter,
            AlignBottom,
            en ? "No .nfc files in series:" : "Keine .nfc Dateien in Serie:");
        const char* series_str =
            model->library ? model->library->active_series_name : "";
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignBottom, series_str);
    }
}

static void tonie_view_browser_draw_list(Canvas* canvas, const TonieBrowserModel* model) {
    if(!model->library || model->library->count == 0) {
        return;
    }

    const size_t count = model->library->count;
    canvas_set_font(canvas, FontSecondary);

    /* Full horizontal space (102px) for all rows since [X/Y] is now in the header bar */
    const size_t max_title_px = 121 - 19;

    for(size_t row = 0; row < TONIE_BROWSER_VISIBLE_ROWS; row++) {
        size_t idx = model->scroll_offset + row;
        if(idx >= count) {
            break;
        }

        const TonieItem* item = &model->library->items[idx];
        const int16_t row_y = 14 + (int16_t)(row * 9);
        const bool is_selected = (idx == model->selected_index);

        /* Choose icon: Folder for Series, Star for Favorite, Toniehead logo for regular NFC */
        const uint8_t* icon = tonie_head_icon_7x7;
        if(item->is_directory) {
            icon = tonie_folder_icon_7x7;
        } else if(item->is_favorite) {
            icon = tonie_star_icon_7x7;
        }

        char raw_display[TONIE_MAX_NAME_LEN];
        tonie_storage_get_item_display_name(item, raw_display, sizeof(raw_display));

        char fitted_name[TONIE_MAX_NAME_LEN];
        tonie_view_browser_fit_string(
            canvas,
            raw_display,
            fitted_name,
            sizeof(fitted_name),
            max_title_px,
            is_selected,
            model->marquee_tick);

        if(is_selected) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_rbox(canvas, 1, row_y, 122, 9, 2);
            canvas_set_color(canvas, ColorWhite);

            canvas_draw_str(canvas, 3, row_y + 8, ">");
            canvas_draw_xbm(canvas, 10, row_y + 1, 7, 7, icon);
            canvas_draw_str(canvas, 19, row_y + 8, fitted_name);
        } else {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_xbm(canvas, 10, row_y + 1, 7, 7, icon);
            canvas_draw_str(canvas, 19, row_y + 8, fitted_name);
        }
    }

    /* Scrollbar on right edge (x = 125..127, y = 14..50) */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 126, 14, 126, 50);
    if(count > 1) {
        const int16_t track_h = 36;
        int16_t thumb_h = (int16_t)((track_h * TONIE_BROWSER_VISIBLE_ROWS) / count);
        if(thumb_h < 5) thumb_h = 5;
        if(thumb_h > track_h) thumb_h = track_h;

        int16_t thumb_y =
            14 + (int16_t)(((track_h - thumb_h) * model->selected_index) / (count - 1));
        canvas_draw_box(canvas, 125, thumb_y, 3, thumb_h);
    }
}

static void tonie_view_browser_draw_footer(Canvas* canvas, const TonieBrowserModel* model) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 0, 52, 127, 52);
    canvas_set_font(canvas, FontSecondary);
    bool en = (model->ui_lang == TonieUiLangEn);

    if(model->picker_mode) {
        canvas_draw_str_aligned(
            canvas,
            64,
            62,
            AlignCenter,
            AlignBottom,
            en ? "[OK] Select series  [Back]" : "[OK] Serie waehlen  [Back]");
        return;
    }

    size_t count = model->library ? model->library->count : 0;
    bool selected_is_dir =
        (count > 0 && model->selected_index < count &&
         model->library->items[model->selected_index].is_directory);

    if(model->level == TonieBrowseLevelSeries && selected_is_dir) {
        canvas_draw_str_aligned(
            canvas,
            64,
            62,
            AlignCenter,
            AlignBottom,
            en ? "[OK] Series  [</>] Tab  [Back]" : "[OK] Serie  [</>] Tab  [Back]");
    } else {
        canvas_draw_str_aligned(
            canvas, 64, 62, AlignCenter, AlignBottom, "[OK] Emul  [Hold OK] Fav  [Back]");
    }
}

static void tonie_view_browser_draw_callback(Canvas* canvas, void* context) {
    const TonieBrowserModel* model = context;
    furi_check(model);

    canvas_clear(canvas);
    tonie_view_browser_draw_header(canvas, model);

    size_t count = model->library ? model->library->count : 0;
    if(count == 0) {
        tonie_view_browser_draw_empty(canvas, model);
    } else {
        tonie_view_browser_draw_list(canvas, model);
    }

    tonie_view_browser_draw_footer(canvas, model);
}

static void tonie_view_browser_timer_callback(void* context) {
    TonieViewBrowser* instance = context;
    furi_check(instance);

    with_view_model(
        instance->view,
        TonieBrowserModel * model,
        { model->marquee_tick++; },
        true);
}

static void tonie_view_browser_enter_callback(void* context) {
    TonieViewBrowser* instance = context;
    furi_check(instance);

    if(instance->scroll_timer) {
        furi_timer_start(instance->scroll_timer, furi_ms_to_ticks(instance->scroll_speed_ms));
    }
}

static void tonie_view_browser_exit_callback(void* context) {
    TonieViewBrowser* instance = context;
    furi_check(instance);

    if(instance->scroll_timer) {
        furi_timer_stop(instance->scroll_timer);
    }
}

static bool tonie_view_browser_input_callback(InputEvent* event, void* context) {
    TonieViewBrowser* instance = context;
    furi_check(instance);

    /* Handle Back key to go from Files level back to Series level */
    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        if(instance->on_back && instance->on_back(instance->callback_context)) {
            return true;
        }
        return false;
    }

    bool is_picker = false;
    with_view_model(
        instance->view,
        TonieBrowserModel * model,
        { is_picker = model->picker_mode; },
        false);

    /* Handle Long-Press OK to toggle Favorite on a Tonie .nfc item */
    if(!is_picker && event->key == InputKeyOk && event->type == InputTypeLong) {
        size_t count = 0;
        size_t selected = 0;
        bool is_dir = false;
        with_view_model(
            instance->view,
            TonieBrowserModel * model,
            {
                count = model->library ? model->library->count : 0;
                selected = model->selected_index;
                if(selected < count && model->library) {
                    is_dir = model->library->items[selected].is_directory;
                }
            },
            false);

        if(count > 0 && !is_dir && instance->on_fav_toggle) {
            instance->ignore_next_ok_short = true;
            instance->on_fav_toggle(instance->callback_context, selected);
            return true;
        }
    }

    if(event->key == InputKeyOk && event->type == InputTypeRelease) {
        instance->ignore_next_ok_short = false;
    }

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    bool consumed = false;

    if(!is_picker && (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        if(event->type == InputTypeShort) {
            TonieLang next_lang = TonieLangGerman;
            with_view_model(
                instance->view,
                TonieBrowserModel * model,
                {
                    int cur = (int)model->active_lang;
                    if(event->key == InputKeyRight) {
                        cur = (cur + 1) % (int)TonieLangCount;
                    } else {
                        cur = (cur + (int)TonieLangCount - 1) % (int)TonieLangCount;
                    }
                    next_lang = (TonieLang)cur;
                    model->active_lang = next_lang;
                    model->selected_index = 0;
                    model->scroll_offset = 0;
                    model->marquee_tick = 0;
                },
                true);

            if(instance->on_lang_change) {
                instance->on_lang_change(instance->callback_context, next_lang);
            }
            consumed = true;
        }
    } else if(event->key == InputKeyUp) {
        with_view_model(
            instance->view,
            TonieBrowserModel * model,
            {
                size_t count = model->library ? model->library->count : 0;
                if(count > 0) {
                    if(model->selected_index == 0) {
                        model->selected_index = count - 1;
                    } else {
                        model->selected_index--;
                    }
                    model->marquee_tick = 0;
                    tonie_view_browser_adjust_scroll(model);
                }
            },
            true);
        consumed = true;
    } else if(event->key == InputKeyDown) {
        with_view_model(
            instance->view,
            TonieBrowserModel * model,
            {
                size_t count = model->library ? model->library->count : 0;
                if(count > 0) {
                    model->selected_index = (model->selected_index + 1) % count;
                    model->marquee_tick = 0;
                    tonie_view_browser_adjust_scroll(model);
                }
            },
            true);
        consumed = true;
    } else if(event->key == InputKeyOk) {
        if(event->type == InputTypeShort && !instance->ignore_next_ok_short) {
            size_t count = 0;
            size_t selected = 0;
            with_view_model(
                instance->view,
                TonieBrowserModel * model,
                {
                    count = model->library ? model->library->count : 0;
                    selected = model->selected_index;
                },
                false);

            if(count > 0 && instance->on_select) {
                instance->on_select(instance->callback_context, selected);
            }
            consumed = true;
        }
    }

    return consumed;
}

TonieViewBrowser* tonie_view_browser_alloc(void) {
    TonieViewBrowser* instance = malloc(sizeof(TonieViewBrowser));
    furi_check(instance);

    instance->view = view_alloc();
    instance->scroll_speed_ms = TONIE_BROWSER_DEFAULT_SCROLL_MS;
    instance->on_select = NULL;
    instance->on_lang_change = NULL;
    instance->on_fav_toggle = NULL;
    instance->on_back = NULL;
    instance->callback_context = NULL;
    instance->ignore_next_ok_short = false;

    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(TonieBrowserModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, tonie_view_browser_draw_callback);
    view_set_input_callback(instance->view, tonie_view_browser_input_callback);
    view_set_enter_callback(instance->view, tonie_view_browser_enter_callback);
    view_set_exit_callback(instance->view, tonie_view_browser_exit_callback);

    instance->scroll_timer =
        furi_timer_alloc(tonie_view_browser_timer_callback, FuriTimerTypePeriodic, instance);

    with_view_model(
        instance->view,
        TonieBrowserModel * model,
        {
            memset(model, 0, sizeof(TonieBrowserModel));
            model->active_lang = TonieLangGerman;
            model->level = TonieBrowseLevelSeries;
        },
        true);

    return instance;
}

void tonie_view_browser_free(TonieViewBrowser* instance) {
    furi_check(instance);

    if(instance->scroll_timer) {
        furi_timer_stop(instance->scroll_timer);
        furi_timer_free(instance->scroll_timer);
        instance->scroll_timer = NULL;
    }

    view_free(instance->view);
    free(instance);
}

View* tonie_view_browser_get_view(TonieViewBrowser* instance) {
    furi_check(instance);
    return instance->view;
}

void tonie_view_browser_set_callbacks(
    TonieViewBrowser* instance,
    TonieViewBrowserSelectCallback on_select,
    TonieViewBrowserLangCallback on_lang_change,
    TonieViewBrowserFavCallback on_fav_toggle,
    TonieViewBrowserBackCallback on_back,
    void* context) {
    furi_check(instance);
    instance->on_select = on_select;
    instance->on_lang_change = on_lang_change;
    instance->on_fav_toggle = on_fav_toggle;
    instance->on_back = on_back;
    instance->callback_context = context;
}

void tonie_view_browser_set_scroll_speed(TonieViewBrowser* instance, uint32_t scroll_speed_ms) {
    furi_check(instance);
    if(scroll_speed_ms >= 50 && scroll_speed_ms <= 2000) {
        instance->scroll_speed_ms = scroll_speed_ms;
    }
}

void tonie_view_browser_update_library(TonieViewBrowser* instance, const TonieLibrary* library) {
    furi_check(instance);
    furi_check(library);

    with_view_model(
        instance->view,
        TonieBrowserModel * model,
        {
            model->library = library;
            model->selected_index = library->selected_index;
            model->active_lang = library->active_lang;
            model->level = library->level;
            model->marquee_tick = 0;
            tonie_view_browser_adjust_scroll(model);
        },
        true);
}

void tonie_view_browser_set_picker_mode(TonieViewBrowser* instance, bool picker_mode) {
    furi_check(instance);

    with_view_model(
        instance->view,
        TonieBrowserModel * model,
        { model->picker_mode = picker_mode; },
        true);
}

void tonie_view_browser_set_ui_lang(TonieViewBrowser* instance, TonieUiLang ui_lang) {
    furi_check(instance);

    with_view_model(
        instance->view,
        TonieBrowserModel * model,
        { model->ui_lang = ui_lang; },
        true);
}
