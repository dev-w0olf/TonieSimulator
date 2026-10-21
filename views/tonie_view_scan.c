#include "tonie_view_scan.h"

#include <gui/canvas.h>
#include <gui/elements.h>
#include <input/input.h>
#include <stdio.h>
#include <string.h>

#define TONIE_SCAN_ANIM_MS 280

/* 14x14 Asymmetric-Ear Toniehead Silhouette Icon (XBM format, LSB first per byte) */
static const uint8_t tonie_scan_head_14x14[] = {
    0x00, 0x06, /* .........##... */
    0x00, 0x07, /* ........###... */
    0x18, 0x07, /* ...##...###... */
    0x38, 0x07, /* ...###..###... */
    0xF0, 0x07, /* ....#######... */
    0xFC, 0x0F, /* ..##########.. */
    0xFE, 0x1F, /* .############. */
    0x36, 0x1B, /* .##.##..##.##. */
    0x36, 0x1B, /* .##.##..##.##. */
    0xFE, 0x1F, /* .############. */
    0xFC, 0x0F, /* ..##########.. */
    0xF8, 0x07, /* ...########... */
    0xF0, 0x03, /* ....######.... */
    0x00, 0x00, /* .............. */
};

typedef struct {
    TonieScanState state;
    TonieLang target_lang;
    uint8_t uid[TONIE_UID_LEN];
    bool is_slix;
    uint8_t anim_step;
    char saved_series[TONIE_MAX_SERIES_LEN];
    char saved_name[TONIE_MAX_NAME_LEN];
} TonieScanModel;

struct TonieViewScan {
    View* view;
    FuriTimer* anim_timer;
    TonieViewScanSaveCallback on_save;
    TonieViewScanRescanCallback on_rescan;
    TonieViewScanBackCallback on_back;
    void* callback_context;
};

static void tonie_view_scan_fit_text(
    Canvas* canvas,
    const char* src,
    char* out,
    size_t out_size,
    size_t max_px) {
    if(!src || !out || out_size == 0) {
        return;
    }
    snprintf(out, out_size, "%s", src);
    size_t len = strlen(out);
    if(canvas_string_width(canvas, out) <= max_px) {
        return;
    }
    while(len > 0 && canvas_string_width(canvas, out) > max_px) {
        out[--len] = '\0';
    }
    if(len >= 2) {
        out[len - 1] = '.';
        out[len - 2] = '.';
    }
}

static void tonie_view_scan_draw_header(Canvas* canvas, const TonieScanModel* model) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 13);

    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 10, "TONIE SCANNEN");

    /* Right side: Language selector [DE | EN] (x: 83..126) */
    const int16_t tab_x[2] = {83, 105};
    const int16_t tab_w = 21;
    const char* tab_lbl[2] = {"DE", "EN"};

    for(int i = 0; i < 2; i++) {
        bool active = ((i == 0 && model->target_lang != TonieLangEnglish) ||
                       (i == 1 && model->target_lang == TonieLangEnglish));
        if(active) {
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_rbox(canvas, tab_x[i], 1, tab_w, 11, 2);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str_aligned(
            canvas, tab_x[i] + (tab_w / 2), 10, AlignCenter, AlignBottom, tab_lbl[i]);
    }
}

static void tonie_view_scan_draw_callback(Canvas* canvas, void* context) {
    const TonieScanModel* model = context;
    furi_check(model);

    canvas_clear(canvas);
    tonie_view_scan_draw_header(canvas, model);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, 2, 15, 124, 36, 3);
    canvas_draw_xbm(canvas, 8, 25, 14, 14, tonie_scan_head_14x14);

    if(model->state == TonieScanStateScanning || model->state == TonieScanStateReading) {
        /* Animated RF scan arcs next to Toniehead icon */
        if(model->anim_step >= 1) {
            canvas_draw_line(canvas, 25, 29, 25, 34);
        }
        if(model->anim_step >= 2) {
            canvas_draw_line(canvas, 28, 27, 28, 36);
        }
        if(model->anim_step >= 3) {
            canvas_draw_line(canvas, 31, 25, 31, 38);
        }

        canvas_set_font(canvas, FontPrimary);
        const char* status_title = (model->state == TonieScanStateReading) ?
                                       "Lese Tonie..." :
                                       "Suche Tonie...";
        canvas_draw_str(canvas, 36, 27, status_title);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 36, 37, "Figur an Flipper");
        canvas_draw_str(canvas, 36, 46, "Rueckseite halten");

        /* Footer */
        canvas_draw_line(canvas, 0, 52, 127, 52);
        canvas_draw_str_aligned(
            canvas, 64, 62, AlignCenter, AlignBottom, "[</>] Ziel: DE/EN   [Back]");
    } else if(model->state == TonieScanStateSuccess) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 26, 26, "TONIE ERKANNT!");

        canvas_set_font(canvas, FontSecondary);
        const char* proto_str = model->is_slix ? "Typ: ICODE SLIX" : "Typ: ISO15693";
        canvas_draw_str(canvas, 26, 36, proto_str);

        char uid_str[32];
        snprintf(
            uid_str,
            sizeof(uid_str),
            "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
            model->uid[0],
            model->uid[1],
            model->uid[2],
            model->uid[3],
            model->uid[4],
            model->uid[5],
            model->uid[6],
            model->uid[7]);
        canvas_draw_str(canvas, 26, 46, uid_str);

        /* Footer */
        canvas_draw_line(canvas, 0, 52, 127, 52);
        canvas_draw_str_aligned(
            canvas, 64, 62, AlignCenter, AlignBottom, "[OK] Speichern [Up] Neu [</>]");
    } else if(model->state == TonieScanStateSaved) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 26, 26, "GESPEICHERT!");

        canvas_set_font(canvas, FontSecondary);
        char fitted_series[TONIE_MAX_SERIES_LEN];
        char fitted_name[TONIE_MAX_NAME_LEN];
        tonie_view_scan_fit_text(
            canvas, model->saved_series, fitted_series, sizeof(fitted_series), 94);
        tonie_view_scan_fit_text(
            canvas, model->saved_name, fitted_name, sizeof(fitted_name), 94);

        canvas_draw_str(canvas, 26, 36, fitted_series);
        canvas_draw_str(canvas, 26, 46, fitted_name);

        /* Footer */
        canvas_draw_line(canvas, 0, 52, 127, 52);
        canvas_draw_str_aligned(
            canvas, 64, 62, AlignCenter, AlignBottom, "[OK] Weiterer Scan  [Back]");
    } else {
        /* Error state */
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 26, 27, "Lesefehler!");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 26, 38, "Tonie ruhig an die");
        canvas_draw_str(canvas, 26, 47, "Rueckseite halten.");

        /* Footer */
        canvas_draw_line(canvas, 0, 52, 127, 52);
        canvas_draw_str_aligned(
            canvas, 64, 62, AlignCenter, AlignBottom, "[OK] Neu scannen    [Back]");
    }
}

static void tonie_view_scan_timer_callback(void* context) {
    TonieViewScan* instance = context;
    furi_check(instance);

    with_view_model(
        instance->view,
        TonieScanModel * model,
        { model->anim_step = (model->anim_step % 3) + 1; },
        true);
}

static void tonie_view_scan_enter_callback(void* context) {
    TonieViewScan* instance = context;
    furi_check(instance);

    if(instance->anim_timer) {
        furi_timer_start(instance->anim_timer, furi_ms_to_ticks(TONIE_SCAN_ANIM_MS));
    }
}

static void tonie_view_scan_exit_callback(void* context) {
    TonieViewScan* instance = context;
    furi_check(instance);

    if(instance->anim_timer) {
        furi_timer_stop(instance->anim_timer);
    }
}

static bool tonie_view_scan_input_callback(InputEvent* event, void* context) {
    TonieViewScan* instance = context;
    furi_check(instance);

    if(event->type != InputTypeShort) {
        return false;
    }

    if(event->key == InputKeyBack) {
        if(instance->on_back) {
            instance->on_back(instance->callback_context);
            return true;
        }
        return false;
    }

    if(event->key == InputKeyLeft || event->key == InputKeyRight) {
        with_view_model(
            instance->view,
            TonieScanModel * model,
            {
                model->target_lang = (model->target_lang == TonieLangEnglish) ?
                                         TonieLangGerman :
                                         TonieLangEnglish;
            },
            true);
        return true;
    }

    TonieScanState state = TonieScanStateScanning;
    TonieLang target_lang = TonieLangGerman;
    with_view_model(
        instance->view,
        TonieScanModel * model,
        {
            state = model->state;
            target_lang = model->target_lang;
        },
        false);

    if(event->key == InputKeyOk) {
        if(state == TonieScanStateSuccess) {
            if(instance->on_save) {
                instance->on_save(instance->callback_context, target_lang);
            }
            return true;
        } else if(state == TonieScanStateSaved || state == TonieScanStateError) {
            if(instance->on_rescan) {
                instance->on_rescan(instance->callback_context);
            }
            return true;
        }
    } else if((event->key == InputKeyUp || event->key == InputKeyDown) &&
              state == TonieScanStateSuccess) {
        if(instance->on_rescan) {
            instance->on_rescan(instance->callback_context);
        }
        return true;
    }

    return false;
}

TonieViewScan* tonie_view_scan_alloc(void) {
    TonieViewScan* instance = malloc(sizeof(TonieViewScan));
    furi_check(instance);

    instance->view = view_alloc();
    instance->on_save = NULL;
    instance->on_rescan = NULL;
    instance->on_back = NULL;
    instance->callback_context = NULL;

    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(TonieScanModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, tonie_view_scan_draw_callback);
    view_set_input_callback(instance->view, tonie_view_scan_input_callback);
    view_set_enter_callback(instance->view, tonie_view_scan_enter_callback);
    view_set_exit_callback(instance->view, tonie_view_scan_exit_callback);

    instance->anim_timer =
        furi_timer_alloc(tonie_view_scan_timer_callback, FuriTimerTypePeriodic, instance);

    with_view_model(
        instance->view,
        TonieScanModel * model,
        {
            memset(model, 0, sizeof(TonieScanModel));
            model->state = TonieScanStateScanning;
            model->target_lang = TonieLangGerman;
            model->anim_step = 1;
        },
        false);

    return instance;
}

void tonie_view_scan_free(TonieViewScan* instance) {
    furi_check(instance);

    if(instance->anim_timer) {
        furi_timer_stop(instance->anim_timer);
        furi_timer_free(instance->anim_timer);
        instance->anim_timer = NULL;
    }

    view_free(instance->view);
    free(instance);
}

View* tonie_view_scan_get_view(TonieViewScan* instance) {
    furi_check(instance);
    return instance->view;
}

void tonie_view_scan_set_callbacks(
    TonieViewScan* instance,
    TonieViewScanSaveCallback on_save,
    TonieViewScanRescanCallback on_rescan,
    TonieViewScanBackCallback on_back,
    void* context) {
    furi_check(instance);
    instance->on_save = on_save;
    instance->on_rescan = on_rescan;
    instance->on_back = on_back;
    instance->callback_context = context;
}

void tonie_view_scan_set_state(
    TonieViewScan* instance,
    TonieScanState state,
    TonieLang default_lang) {
    furi_check(instance);

    with_view_model(
        instance->view,
        TonieScanModel * model,
        {
            model->state = state;
            if(default_lang == TonieLangEnglish) {
                model->target_lang = TonieLangEnglish;
            } else if(default_lang == TonieLangGerman) {
                model->target_lang = TonieLangGerman;
            }
        },
        true);
}

void tonie_view_scan_set_tag_info(
    TonieViewScan* instance,
    const uint8_t* uid,
    bool is_slix) {
    furi_check(instance);

    with_view_model(
        instance->view,
        TonieScanModel * model,
        {
            model->state = TonieScanStateSuccess;
            model->is_slix = is_slix;
            if(uid) {
                memcpy(model->uid, uid, TONIE_UID_LEN);
            } else {
                memset(model->uid, 0, TONIE_UID_LEN);
            }
        },
        true);
}

void tonie_view_scan_set_saved_info(
    TonieViewScan* instance,
    const char* series_name,
    const char* tonie_name) {
    furi_check(instance);

    with_view_model(
        instance->view,
        TonieScanModel * model,
        {
            model->state = TonieScanStateSaved;
            snprintf(
                model->saved_series,
                sizeof(model->saved_series),
                "Serie: %s",
                series_name ? series_name : "");
            snprintf(
                model->saved_name,
                sizeof(model->saved_name),
                "Tonie: %s",
                tonie_name ? tonie_name : "");
        },
        true);
}

TonieLang tonie_view_scan_get_target_lang(TonieViewScan* instance) {
    furi_check(instance);
    TonieLang lang = TonieLangGerman;
    with_view_model(
        instance->view,
        TonieScanModel * model,
        { lang = model->target_lang; },
        false);
    return lang;
}
