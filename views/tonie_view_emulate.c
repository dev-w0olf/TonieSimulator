#include "tonie_view_emulate.h"

#include <gui/canvas.h>
#include <gui/elements.h>
#include <input/input.h>
#include <stdio.h>
#include <string.h>

#define TONIE_EMULATE_ANIM_PERIOD_MS 300

/*
 * 16x20 Asymmetric-Ear Toniehead Logo with Smile (XBM format, LSB first per byte).
 * Matches the official Toniehead silhouette uploaded by the user.
 */
static const uint8_t tonie_logo_16x20[] = {
    0x00, 0x60, /* .............##. */
    0x00, 0x70, /* ............###. */
    0x08, 0x78, /* ...#.......####. */
    0x1C, 0x7C, /* ..###.....#####. */
    0x3C, 0x7E, /* ..####...######. */
    0x7C, 0x7F, /* ..#####.#######. */
    0xFE, 0x7F, /* .##############. */
    0xFE, 0x7F, /* .##############. */
    0xFF, 0x7F, /* ###############. */
    0xFF, 0x7F, /* ###############. */
    0xFF, 0x7F, /* ###############. */
    0xFF, 0x7F, /* ###############. */
    0xFF, 0x7F, /* ###############. */
    0xCF, 0x7F, /* ####..#########. */
    0x9F, 0x1F, /* #####..######... */
    0x3F, 0x70, /* ######......###. */
    0xFF, 0x7F, /* ###############. */
    0xFE, 0x7F, /* .##############. */
    0xFE, 0x3F, /* .#############.. */
    0xFC, 0x1F, /* ..###########... */
};

typedef struct {
    char display_name[TONIE_MAX_NAME_LEN];
    char series_name[TONIE_MAX_SERIES_LEN];
    uint8_t uid[TONIE_UID_LEN];
    TonieLang lang;
    bool is_slix;
    bool is_favorite;
    bool emulation_running;
    uint8_t anim_step;
    uint16_t marquee_tick;
    uint32_t rx_count;
} TonieEmulateModel;

struct TonieViewEmulate {
    View* view;
    FuriTimer* anim_timer;
    TonieViewEmulateStopCallback on_stop;
    void* callback_context;
};

static const char* tonie_signal_wave_frames[4] = {
    "[     ]",
    "[~    ]",
    "[~ ~  ]",
    "[~ ~ ~]",
};

static void tonie_view_emulate_marquee_str(
    Canvas* canvas,
    const char* prefix,
    const char* value,
    char* out,
    size_t out_size,
    size_t max_px,
    uint16_t tick) {
    snprintf(out, out_size, "%s%s", prefix, value);
    if(canvas_string_width(canvas, out) <= max_px) {
        return;
    }

    size_t val_len = strlen(value);
    size_t start_idx = 0;
    if(val_len > 4) {
        size_t max_shift = val_len - 4;
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

    snprintf(out, out_size, "%s%s", prefix, value + start_idx);
    size_t len = strlen(out);
    while(len > 0 && canvas_string_width(canvas, out) > max_px) {
        out[--len] = '\0';
    }
}

static void tonie_view_emulate_timer_callback(void* context) {
    TonieViewEmulate* instance = context;
    furi_check(instance);

    with_view_model(
        instance->view,
        TonieEmulateModel * model,
        {
            model->anim_step = (model->anim_step + 1) & 0x03;
            model->marquee_tick++;
        },
        true);
}

static void tonie_view_emulate_enter_callback(void* context) {
    TonieViewEmulate* instance = context;
    furi_check(instance);

    if(instance->anim_timer) {
        furi_timer_start(instance->anim_timer, furi_ms_to_ticks(TONIE_EMULATE_ANIM_PERIOD_MS));
    }
}

static void tonie_view_emulate_exit_callback(void* context) {
    TonieViewEmulate* instance = context;
    furi_check(instance);

    if(instance->anim_timer) {
        furi_timer_stop(instance->anim_timer);
    }
}

static void tonie_view_emulate_draw_callback(Canvas* canvas, void* context) {
    const TonieEmulateModel* model = context;
    furi_check(model);

    canvas_clear(canvas);

    /* 1. Top Header Bar (y: 0..12) */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 12);

    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "TONIE ACTIVE");

    char sig_buf[20];
    const char* wave = tonie_signal_wave_frames[model->anim_step & 0x03];
    snprintf(sig_buf, sizeof(sig_buf), "SIG: %s", wave);
    canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, sig_buf);

    /* 2. Left Column: Official Toniehead Logo + Animated RF Signal Arcs */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_xbm(canvas, 3, 24, 16, 20, tonie_logo_16x20);

    if(model->emulation_running) {
        if(model->anim_step >= 1) {
            canvas_draw_line(canvas, 7, 21, 14, 21);
        }
        if(model->anim_step >= 2) {
            canvas_draw_line(canvas, 5, 18, 16, 18);
        }
        if(model->anim_step >= 3) {
            canvas_draw_line(canvas, 3, 15, 18, 15);
        }
    }

    /* Vertical separator between Tonie logo and metadata */
    canvas_draw_line(canvas, 22, 14, 22, 50);

    /* 3. Right Column (x: 25..126, max width: 101px) */
    canvas_set_font(canvas, FontSecondary);
    const size_t max_line_px = 101;

    char line_name[80];
    tonie_view_emulate_marquee_str(
        canvas,
        "Name: ",
        model->display_name,
        line_name,
        sizeof(line_name),
        max_line_px,
        model->marquee_tick);
    canvas_draw_str(canvas, 25, 21, line_name);

    char series_raw[64];
    const char* lang_short = (model->lang == TonieLangEnglish) ? "EN" : "DE";
    if(model->series_name[0] != '\0') {
        snprintf(series_raw, sizeof(series_raw), "%s (%s)", model->series_name, lang_short);
    } else {
        snprintf(series_raw, sizeof(series_raw), "%s", tonie_storage_get_lang_label(model->lang));
    }

    char line_series[80];
    tonie_view_emulate_marquee_str(
        canvas,
        "Serie: ",
        series_raw,
        line_series,
        sizeof(line_series),
        max_line_px,
        model->marquee_tick);
    canvas_draw_str(canvas, 25, 30, line_series);

    /* Compact 8-Byte Hex UID so all 16 hex digits fit cleanly inside 101px */
    char line_uid[32];
    snprintf(
        line_uid,
        sizeof(line_uid),
        "UID: %02X%02X%02X%02X%02X%02X%02X%02X",
        model->uid[0],
        model->uid[1],
        model->uid[2],
        model->uid[3],
        model->uid[4],
        model->uid[5],
        model->uid[6],
        model->uid[7]);
    canvas_draw_str(canvas, 25, 39, line_uid);

    if(model->emulation_running) {
        canvas_draw_str(canvas, 25, 48, "Mode: SLIX EMULATION");
    } else {
        canvas_draw_str(canvas, 25, 48, "Mode: NFC LOAD ERROR");
    }

    /* 4. Footer Bar (y: 52..63) */
    canvas_draw_line(canvas, 0, 52, 127, 52);
    canvas_draw_rbox(canvas, 2, 54, 124, 10, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(
        canvas, 64, 62, AlignCenter, AlignBottom, "[OK/Back] Emulation stoppen");
}

static bool tonie_view_emulate_input_callback(InputEvent* event, void* context) {
    TonieViewEmulate* instance = context;
    furi_check(instance);

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyOk || event->key == InputKeyBack) {
            if(instance->on_stop) {
                instance->on_stop(instance->callback_context);
            }
            return true;
        }
    }

    return false;
}

TonieViewEmulate* tonie_view_emulate_alloc(void) {
    TonieViewEmulate* instance = malloc(sizeof(TonieViewEmulate));
    furi_check(instance);

    instance->view = view_alloc();
    instance->on_stop = NULL;
    instance->callback_context = NULL;

    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(TonieEmulateModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, tonie_view_emulate_draw_callback);
    view_set_input_callback(instance->view, tonie_view_emulate_input_callback);
    view_set_enter_callback(instance->view, tonie_view_emulate_enter_callback);
    view_set_exit_callback(instance->view, tonie_view_emulate_exit_callback);

    instance->anim_timer =
        furi_timer_alloc(tonie_view_emulate_timer_callback, FuriTimerTypePeriodic, instance);

    with_view_model(
        instance->view,
        TonieEmulateModel * model,
        {
            memset(model, 0, sizeof(TonieEmulateModel));
            model->anim_step = 3;
        },
        false);

    return instance;
}

void tonie_view_emulate_free(TonieViewEmulate* instance) {
    furi_check(instance);

    if(instance->anim_timer) {
        furi_timer_stop(instance->anim_timer);
        furi_timer_free(instance->anim_timer);
        instance->anim_timer = NULL;
    }

    view_free(instance->view);
    free(instance);
}

View* tonie_view_emulate_get_view(TonieViewEmulate* instance) {
    furi_check(instance);
    return instance->view;
}

void tonie_view_emulate_set_stop_callback(
    TonieViewEmulate* instance,
    TonieViewEmulateStopCallback on_stop,
    void* context) {
    furi_check(instance);
    instance->on_stop = on_stop;
    instance->callback_context = context;
}

void tonie_view_emulate_set_item(
    TonieViewEmulate* instance,
    const TonieEmulateDetails* details,
    TonieLang lang,
    bool emulation_running) {
    furi_check(instance);
    furi_check(details);

    with_view_model(
        instance->view,
        TonieEmulateModel * model,
        {
            snprintf(
                model->display_name, sizeof(model->display_name), "%s", details->display_name);
            snprintf(
                model->series_name, sizeof(model->series_name), "%s", details->series_name);
            memcpy(model->uid, details->uid, TONIE_UID_LEN);
            model->lang = lang;
            model->is_slix = details->is_slix;
            model->is_favorite = details->is_favorite;
            model->emulation_running = emulation_running;
            model->anim_step = 1;
            model->marquee_tick = 0;
            model->rx_count = 0;
        },
        true);
}

void tonie_view_emulate_notify_rx(TonieViewEmulate* instance) {
    furi_check(instance);

    with_view_model(
        instance->view,
        TonieEmulateModel * model,
        {
            model->rx_count++;
            model->anim_step = (model->anim_step + 1) & 0x03;
        },
        true);
}
