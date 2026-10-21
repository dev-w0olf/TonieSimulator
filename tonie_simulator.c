#include "tonie_simulator.h"

#include <furi_hal_vibro.h>
#include <stdio.h>
#include <string.h>

static void tonie_app_short_vibro(const TonieApp* app) {
    if(app && app->config.vibro_enabled) {
        furi_hal_vibro_on(true);
        furi_delay_ms(25);
        furi_hal_vibro_on(false);
    }
}

static uint32_t tonie_app_splash_previous_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static uint32_t tonie_app_browser_previous_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static uint32_t tonie_app_emulate_previous_callback(void* context) {
    UNUSED(context);
    return TonieViewIdBrowser;
}

static void tonie_app_on_splash_start(void* context) {
    TonieApp* app = context;
    furi_check(app);

    view_dispatcher_send_custom_event(app->view_dispatcher, TonieCustomEventStartApp);
}

static void tonie_app_on_browser_select(void* context, size_t selected_index) {
    TonieApp* app = context;
    furi_check(app);

    if(selected_index >= app->library.count) {
        return;
    }

    app->library.selected_index = selected_index;
    view_dispatcher_send_custom_event(app->view_dispatcher, TonieCustomEventSelect);
}

static void tonie_app_on_browser_lang_change(void* context, TonieLang new_lang) {
    TonieApp* app = context;
    furi_check(app);

    uint32_t ev = TonieCustomEventLangGerman;
    if(new_lang == TonieLangEnglish) {
        ev = TonieCustomEventLangEnglish;
    } else if(new_lang == TonieLangFavorites) {
        ev = TonieCustomEventLangFavorites;
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, ev);
}

static void tonie_app_on_browser_fav_toggle(void* context, size_t selected_index) {
    TonieApp* app = context;
    furi_check(app);

    if(selected_index >= app->library.count) {
        return;
    }

    app->library.selected_index = selected_index;
    view_dispatcher_send_custom_event(app->view_dispatcher, TonieCustomEventToggleFavorite);
}

static bool tonie_app_on_browser_back(void* context) {
    TonieApp* app = context;
    furi_check(app);

    if(app->library.level == TonieBrowseLevelFiles &&
       app->library.active_lang != TonieLangFavorites) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TonieCustomEventBackToSeries);
        return true;
    }

    return false;
}

static void tonie_app_on_emulate_stop(void* context) {
    TonieApp* app = context;
    furi_check(app);

    view_dispatcher_send_custom_event(app->view_dispatcher, TonieCustomEventStopEmulate);
}

static void tonie_app_emulator_rx_callback(void* context) {
    TonieApp* app = context;
    if(app && app->view_dispatcher) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TonieCustomEventReaderRx);
    }
}

static bool tonie_app_custom_event_callback(void* context, uint32_t event) {
    TonieApp* app = context;
    furi_check(app);

    switch(event) {
    case TonieCustomEventStartApp:
        tonie_app_short_vibro(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, TonieViewIdBrowser);
        return true;

    case TonieCustomEventLangGerman:
        tonie_storage_scan_series(
            app->storage, &app->library, &app->config, TonieLangGerman);
        tonie_view_browser_update_library(app->view_browser, &app->library);
        return true;

    case TonieCustomEventLangEnglish:
        tonie_storage_scan_series(
            app->storage, &app->library, &app->config, TonieLangEnglish);
        tonie_view_browser_update_library(app->view_browser, &app->library);
        return true;

    case TonieCustomEventLangFavorites:
        tonie_storage_scan_series(
            app->storage, &app->library, &app->config, TonieLangFavorites);
        tonie_view_browser_update_library(app->view_browser, &app->library);
        return true;

    case TonieCustomEventBackToSeries: {
        size_t prev_series_idx = app->library.saved_series_index;
        tonie_storage_scan_series(
            app->storage, &app->library, &app->config, app->library.active_lang);
        if(prev_series_idx < app->library.count) {
            app->library.selected_index = prev_series_idx;
        }
        tonie_view_browser_update_library(app->view_browser, &app->library);
        return true;
    }

    case TonieCustomEventToggleFavorite: {
        if(app->library.selected_index < app->library.count) {
            TonieItem* item = &app->library.items[app->library.selected_index];
            if(!item->is_directory) {
                char item_path[TONIE_PATH_MAX];
                if(tonie_storage_get_item_path(
                       &app->library,
                       &app->config,
                       app->library.selected_index,
                       item_path,
                       sizeof(item_path))) {
                    bool is_fav = tonie_storage_toggle_favorite(
                        app->storage, &app->config, item_path);
                    item->is_favorite = is_fav;
                    tonie_app_short_vibro(app);

                    if(app->library.active_lang == TonieLangFavorites) {
                        size_t prev_idx = app->library.selected_index;
                        tonie_storage_scan_series(
                            app->storage, &app->library, &app->config, TonieLangFavorites);
                        if(prev_idx >= app->library.count && app->library.count > 0) {
                            app->library.selected_index = app->library.count - 1;
                        } else {
                            app->library.selected_index = prev_idx;
                        }
                    }
                    tonie_view_browser_update_library(app->view_browser, &app->library);
                }
            }
        }
        return true;
    }

    case TonieCustomEventSelect: {
        if(app->library.selected_index < app->library.count) {
            const TonieItem* item = &app->library.items[app->library.selected_index];
            char selected_path[TONIE_PATH_MAX];
            if(tonie_storage_get_item_path(
                   &app->library,
                   &app->config,
                   app->library.selected_index,
                   selected_path,
                   sizeof(selected_path))) {
                if(item->is_directory) {
                    /* Enter selected Series subfolder */
                    char series_label[TONIE_MAX_SERIES_LEN];
                    tonie_storage_get_item_display_name(
                        item, series_label, sizeof(series_label));
                    app->library.saved_series_index = app->library.selected_index;
                    tonie_storage_scan_series_files(
                        app->storage,
                        &app->library,
                        &app->config,
                        selected_path,
                        series_label);
                    tonie_view_browser_update_library(app->view_browser, &app->library);
                } else {
                    /* Parse metadata & start NFC Emulation for selected .nfc file */
                    TonieEmulateDetails details;
                    tonie_storage_parse_nfc_file(
                        app->storage,
                        selected_path,
                        app->library.active_series_name,
                        &app->config,
                        &details);
                    tonie_storage_get_item_display_name(
                        item, details.display_name, sizeof(details.display_name));
                    bool started = tonie_emulator_start(app->emulator, details.file_path);
                    tonie_view_emulate_set_item(
                        app->view_emulate, &details, app->library.active_lang, started);
                    view_dispatcher_switch_to_view(app->view_dispatcher, TonieViewIdEmulate);
                }
            }
        }
        return true;
    }

    case TonieCustomEventStopEmulate:
        tonie_emulator_stop(app->emulator);
        view_dispatcher_switch_to_view(app->view_dispatcher, TonieViewIdBrowser);
        return true;

    case TonieCustomEventReaderRx:
        tonie_view_emulate_notify_rx(app->view_emulate);
        return true;

    default:
        return false;
    }
}

static TonieApp* tonie_app_alloc(void) {
    TonieApp* app = malloc(sizeof(TonieApp));
    furi_check(app);
    memset(app, 0, sizeof(TonieApp));

    /* Open core Furi records */
    app->storage = furi_record_open(RECORD_STORAGE);
    app->gui = furi_record_open(RECORD_GUI);

    /* Allocate and attach ViewDispatcher */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, tonie_app_custom_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* Allocate NFC Emulator engine */
    app->emulator = tonie_emulator_alloc();
    tonie_emulator_set_rx_callback(app->emulator, tonie_app_emulator_rx_callback, app);

    /* Allocate and register Splash Screen View */
    app->view_splash = tonie_view_splash_alloc();
    tonie_view_splash_set_start_callback(app->view_splash, tonie_app_on_splash_start, app);
    View* splash_view = tonie_view_splash_get_view(app->view_splash);
    view_set_previous_callback(splash_view, tonie_app_splash_previous_callback);
    view_dispatcher_add_view(app->view_dispatcher, TonieViewIdSplash, splash_view);

    /* Allocate and register Browser View */
    app->view_browser = tonie_view_browser_alloc();
    tonie_view_browser_set_callbacks(
        app->view_browser,
        tonie_app_on_browser_select,
        tonie_app_on_browser_lang_change,
        tonie_app_on_browser_fav_toggle,
        tonie_app_on_browser_back,
        app);
    View* browser_view = tonie_view_browser_get_view(app->view_browser);
    view_set_previous_callback(browser_view, tonie_app_browser_previous_callback);
    view_dispatcher_add_view(app->view_dispatcher, TonieViewIdBrowser, browser_view);

    /* Allocate and register Live Emulation View */
    app->view_emulate = tonie_view_emulate_alloc();
    tonie_view_emulate_set_stop_callback(app->view_emulate, tonie_app_on_emulate_stop, app);
    View* emulate_view = tonie_view_emulate_get_view(app->view_emulate);
    view_set_previous_callback(emulate_view, tonie_app_emulate_previous_callback);
    view_dispatcher_add_view(app->view_dispatcher, TonieViewIdEmulate, emulate_view);

    return app;
}

static void tonie_app_free(TonieApp* app) {
    furi_check(app);

    if(app->emulator) {
        tonie_emulator_stop(app->emulator);
    }

    view_dispatcher_remove_view(app->view_dispatcher, TonieViewIdSplash);
    view_dispatcher_remove_view(app->view_dispatcher, TonieViewIdBrowser);
    view_dispatcher_remove_view(app->view_dispatcher, TonieViewIdEmulate);

    tonie_view_emulate_free(app->view_emulate);
    tonie_view_browser_free(app->view_browser);
    tonie_view_splash_free(app->view_splash);

    tonie_emulator_free(app->emulator);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);

    free(app);
}

int32_t tonie_simulator_app(void* p) {
    UNUSED(p);
    TonieApp* app = tonie_app_alloc();

    /* 1. Ordnerstruktur prüfen / anlegen */
    tonie_storage_ensure_directories(app->storage);

    /* 2. Config & Favoriten laden (erstellt tonie_simulator.cfg falls noch nicht vorhanden) */
    tonie_storage_load_config(app->storage, &app->config);
    tonie_view_browser_set_scroll_speed(app->view_browser, app->config.scroll_speed_ms);

    /* 3. Standard-Tab (Default: German) und Serien-Ordner laden */
    tonie_storage_scan_series(
        app->storage, &app->library, &app->config, app->config.default_tab);
    tonie_view_browser_update_library(app->view_browser, &app->library);

    /* 4. GUI & ViewDispatcher mit Splash-Screen starten */
    view_dispatcher_switch_to_view(app->view_dispatcher, TonieViewIdSplash);
    view_dispatcher_run(app->view_dispatcher);

    /* 5. Aufräumen bei Beenden */
    tonie_app_free(app);
    return 0;
}
