#include "tonie_view_splash.h"

#include <gui/canvas.h>
#include <gui/elements.h>
#include <input/input.h>
#include <string.h>

#define TONIE_SPLASH_ANIM_MS 320

/*
 * 32x32 Pixel-Art: Lion Tonie Figure standing on an Isometric 3D Toniebox
 * (with Toniehead logo on left face and 50% checkerboard dithering on right face).
 * XBM format: LSB first per byte, 4 bytes per row.
 */
static const uint8_t tonie_splash_art_32x32[] = {
    0x00, 0xFC, 0x03, 0x00, /*  0: ..........########.............. */
    0x00, 0xFF, 0x0F, 0x00, /*  1: ........############............ */
    0x80, 0x63, 0x1C, 0x00, /*  2: .......###...##...###........... */
    0x80, 0x01, 0x18, 0x00, /*  3: .......##..........##........... */
    0xC0, 0x09, 0x39, 0x00, /*  4: ......###..#....#..###.......... */
    0xC0, 0x61, 0x38, 0x00, /*  5: ......###....##....###.......... */
    0x80, 0xF3, 0x1C, 0x03, /*  6: .......###..####..###...##...... */
    0x00, 0xFF, 0x8F, 0x04, /*  7: ........############...#..#..... */
    0x00, 0xFE, 0x87, 0x03, /*  8: .........##########....###...... */
    0x00, 0x6E, 0x47, 0x00, /*  9: .........###.##.###...#......... */
    0x00, 0x67, 0x2E, 0x00, /* 10: ........###..##..###.#.......... */
    0xC0, 0xFD, 0x3B, 0x00, /* 11: ......###.########.###.......... */
    0x70, 0x04, 0xE2, 0x00, /* 12: ....###...#......#...###........ */
    0x1C, 0xF0, 0x80, 0x03, /* 13: ..###.......####.......###...... */
    0x0F, 0x00, 0x00, 0x0F, /* 14: ####....................####.... */
    0x3D, 0x00, 0xC0, 0x0B, /* 15: #.####................####.#.... */
    0xF1, 0x00, 0xF0, 0x0A, /* 16: #...####............####.#.#.... */
    0xC1, 0x03, 0xBC, 0x0A, /* 17: #.....####........####.#.#.#.... */
    0x01, 0x0F, 0xAF, 0x0A, /* 18: #.......####....####.#.#.#.#.... */
    0x21, 0xFC, 0xAB, 0x0A, /* 19: #....#....########.#.#.#.#.#.... */
    0x31, 0x61, 0xAA, 0x0A, /* 20: #...##..#....##..#.#.#.#.#.#.... */
    0xF1, 0x61, 0x55, 0x0D, /* 21: #...#####....##.#.#.#.#.#.##.... */
    0xF9, 0x61, 0xAA, 0x0A, /* 22: #..######....##..#.#.#.#.#.#.... */
    0xF9, 0x61, 0x55, 0x0D, /* 23: #..######....##.#.#.#.#.#.##.... */
    0xF9, 0x61, 0xAA, 0x0A, /* 24: #..######....##..#.#.#.#.#.#.... */
    0xF1, 0x60, 0x55, 0x0D, /* 25: #...####.....##.#.#.#.#.#.##.... */
    0x07, 0x60, 0xAA, 0x0E, /* 26: ###..........##..#.#.#.#.###.... */
    0x1C, 0x60, 0x55, 0x07, /* 27: ..###........##.#.#.#.#.###..... */
    0x70, 0x60, 0xEA, 0x00, /* 28: ....###......##..#.#.###........ */
    0xC0, 0x61, 0x75, 0x00, /* 29: ......###....##.#.#.###......... */
    0x00, 0x67, 0x3A, 0x00, /* 30: ........###..##..#.###.......... */
    0x00, 0xFC, 0x03, 0x00, /* 31: ..........########.............. */
};

/* 4x7 Left Arrow (<) Icon for Menu Selector */
static const uint8_t tonie_menu_arrow_left_4x7[] = {
    0x08, /* ...# */
    0x0C, /* ..## */
    0x0E, /* .### */
    0x0F, /* #### */
    0x0E, /* .### */
    0x0C, /* ..## */
    0x08, /* ...# */
};

/* 4x7 Right Arrow (>) Icon for Menu Selector */
static const uint8_t tonie_menu_arrow_right_4x7[] = {
    0x01, /* #... */
    0x03, /* ##.. */
    0x07, /* ###. */
    0x0F, /* #### */
    0x07, /* ###. */
    0x03, /* ##.. */
    0x01, /* #... */
};

typedef struct {
    uint8_t wave_step;
    TonieMenuAction selected_action;
} TonieSplashModel;

struct TonieViewSplash {
    View* view;
    FuriTimer* anim_timer;
    TonieViewSplashMenuCallback on_menu_select;
    void* callback_context;
};

/* Helper to draw a curved 2px-thick vertical NFC signal arc */
static void tonie_view_splash_draw_arc(
    Canvas* canvas,
    int16_t x,
    int16_t y_top,
    int16_t height,
    bool left_side) {
    int16_t dir = left_side ? 1 : -1;
    /* Top curved tip */
    canvas_draw_box(canvas, x + (2 * dir), y_top, 2, 2);
    canvas_draw_box(canvas, x + (1 * dir), y_top + 2, 2, 2);
    /* Center vertical spine */
    if(height > 8) {
        canvas_draw_box(canvas, x, y_top + 4, 2, height - 8);
    }
    /* Bottom curved tip */
    canvas_draw_box(canvas, x + (1 * dir), y_top + height - 4, 2, 2);
    canvas_draw_box(canvas, x + (2 * dir), y_top + height - 2, 2, 2);
}

static void tonie_view_splash_draw_callback(Canvas* canvas, void* context) {
    const TonieSplashModel* model = context;
    furi_check(model);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    /* 1. Large Title: "Tonie Simulator" */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "Tonie Simulator");

    /* 2. Subtitle: "by W0olf" */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 17, AlignCenter, AlignBottom, "by W0olf");

    /* 3. Center Illustration: Lion Tonie on Isometric 3D Toniebox (y: 19..50) */
    canvas_draw_xbm(canvas, 50, 19, 32, 32, tonie_splash_art_32x32);

    /* 4. Animated NFC Signal Waves (((  ))) on Left and Right */
    if(model->wave_step >= 1) {
        tonie_view_splash_draw_arc(canvas, 43, 29, 12, true);
        tonie_view_splash_draw_arc(canvas, 83, 29, 12, false);
    }
    if(model->wave_step >= 2) {
        tonie_view_splash_draw_arc(canvas, 38, 26, 18, true);
        tonie_view_splash_draw_arc(canvas, 88, 26, 18, false);
    }
    if(model->wave_step >= 3) {
        tonie_view_splash_draw_arc(canvas, 33, 23, 24, true);
        tonie_view_splash_draw_arc(canvas, 93, 23, 24, false);
    }

    /* 5. Bottom Black Bar: Interactive Main Menu [< 1. Simulation >   [OK]] */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 53, 128, 11);

    const char* menu_label = "1. Simulation";
    if(model->selected_action == TonieMenuActionScan) {
        menu_label = "2. Tonie scannen";
    } else if(model->selected_action == TonieMenuActionSettings) {
        menu_label = "3. Einstellungen";
    }

    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_xbm(canvas, 3, 55, 4, 7, tonie_menu_arrow_left_4x7);
    canvas_draw_str_aligned(canvas, 53, 62, AlignCenter, AlignBottom, menu_label);
    canvas_draw_xbm(canvas, 100, 55, 4, 7, tonie_menu_arrow_right_4x7);

    /* Inverted [OK] button pill on the right edge */
    canvas_draw_rbox(canvas, 108, 54, 18, 9, 2);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str_aligned(canvas, 117, 62, AlignCenter, AlignBottom, "OK");
}

static void tonie_view_splash_timer_callback(void* context) {
    TonieViewSplash* instance = context;
    furi_check(instance);

    with_view_model(
        instance->view,
        TonieSplashModel * model,
        {
            model->wave_step = (model->wave_step % 3) + 1;
        },
        true);
}

static void tonie_view_splash_enter_callback(void* context) {
    TonieViewSplash* instance = context;
    furi_check(instance);

    if(instance->anim_timer) {
        furi_timer_start(instance->anim_timer, furi_ms_to_ticks(TONIE_SPLASH_ANIM_MS));
    }
}

static void tonie_view_splash_exit_callback(void* context) {
    TonieViewSplash* instance = context;
    furi_check(instance);

    if(instance->anim_timer) {
        furi_timer_stop(instance->anim_timer);
    }
}

static bool tonie_view_splash_input_callback(InputEvent* event, void* context) {
    TonieViewSplash* instance = context;
    furi_check(instance);

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyRight || event->key == InputKeyDown) {
            with_view_model(
                instance->view,
                TonieSplashModel * model,
                {
                    model->selected_action =
                        (TonieMenuAction)(((int)model->selected_action + 1) %
                                          (int)TonieMenuActionCount);
                },
                true);
            return true;
        } else if(event->key == InputKeyLeft || event->key == InputKeyUp) {
            with_view_model(
                instance->view,
                TonieSplashModel * model,
                {
                    model->selected_action =
                        (TonieMenuAction)(((int)model->selected_action +
                                           (int)TonieMenuActionCount - 1) %
                                          (int)TonieMenuActionCount);
                },
                true);
            return true;
        } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
            TonieMenuAction action = TonieMenuActionSimulate;
            with_view_model(
                instance->view,
                TonieSplashModel * model,
                { action = model->selected_action; },
                false);

            if(instance->on_menu_select) {
                instance->on_menu_select(instance->callback_context, action);
            }
            return true;
        }
    }

    return false;
}

TonieViewSplash* tonie_view_splash_alloc(void) {
    TonieViewSplash* instance = malloc(sizeof(TonieViewSplash));
    furi_check(instance);

    instance->view = view_alloc();
    instance->on_menu_select = NULL;
    instance->callback_context = NULL;

    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(TonieSplashModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, tonie_view_splash_draw_callback);
    view_set_input_callback(instance->view, tonie_view_splash_input_callback);
    view_set_enter_callback(instance->view, tonie_view_splash_enter_callback);
    view_set_exit_callback(instance->view, tonie_view_splash_exit_callback);

    instance->anim_timer =
        furi_timer_alloc(tonie_view_splash_timer_callback, FuriTimerTypePeriodic, instance);

    with_view_model(
        instance->view,
        TonieSplashModel * model,
        {
            model->wave_step = 3;
            model->selected_action = TonieMenuActionSimulate;
        },
        false);

    return instance;
}

void tonie_view_splash_free(TonieViewSplash* instance) {
    furi_check(instance);

    if(instance->anim_timer) {
        furi_timer_stop(instance->anim_timer);
        furi_timer_free(instance->anim_timer);
        instance->anim_timer = NULL;
    }

    view_free(instance->view);
    free(instance);
}

View* tonie_view_splash_get_view(TonieViewSplash* instance) {
    furi_check(instance);
    return instance->view;
}

void tonie_view_splash_set_menu_callback(
    TonieViewSplash* instance,
    TonieViewSplashMenuCallback on_menu_select,
    void* context) {
    furi_check(instance);
    instance->on_menu_select = on_menu_select;
    instance->callback_context = context;
}
