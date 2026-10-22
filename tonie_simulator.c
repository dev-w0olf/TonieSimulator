#include "tonie_simulator.h"

#include <furi_hal_vibro.h>
#include <stdio.h>
#include <string.h>

static const char* const tonie_setting_ui_lang_names[] = {"DE", "EN"};
static const char* const tonie_setting_tab_names[] = {"DE", "EN", "FAV"};
static const char* const tonie_setting_speed_names_de[] = {"Schnell", "Normal", "Langsam"};
static const char* const tonie_setting_speed_names_en[] = {"Fast", "Normal", "Slow"};
static const uint32_t tonie_setting_speed_values[] = {150, 250, 400};
static const char* const tonie_setting_bool_names_de[] = {"Aus", "Ein"};
static const char* const tonie_setting_bool_names_en[] = {"Off", "On"};

static void tonie_app_apply_ui_lang(TonieApp* app) {
    furi_check(app);
    tonie_view_splash_set_ui_lang(app->view_splash, app->config.ui_lang);
    tonie_view_browser_set_ui_lang(app->view_browser, app->config.ui_lang);
    tonie_view_emulate_set_ui_lang(app->view_emulate, app->config.ui_lang);
    tonie_view_scan_set_ui_lang(app->view_scan, app->config.ui_lang);
}

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
    return TonieViewIdSplash;
}

static uint32_t tonie_app_emulate_previous_callback(void* context) {
    UNUSED(context);
    return TonieViewIdBrowser;
}

static uint32_t tonie_app_scan_previous_callback(void* context) {
    UNUSED(context);
    return TonieViewIdSplash;
}

static uint32_t tonie_app_text_input_previous_callback(void* context) {
    UNUSED(context);
    return TonieViewIdBrowser;
}

static uint32_t tonie_app_settings_previous_callback(void* context) {
    UNUSED(context);
    return TonieViewIdSplash;
}

/* --- Splash Menu Callback --- */
static void tonie_app_on_splash_menu(void* context, TonieMenuAction action) {
    TonieApp* app = context;
    furi_check(app);

    uint32_t ev = TonieCustomEventMenuSimulate;
    if(action == TonieMenuActionScan) {
        ev = TonieCustomEventMenuScan;
    } else if(action == TonieMenuActionSettings) {
        ev = TonieCustomEventMenuSettings;
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, ev);
}

/* --- Browser Callbacks --- */
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

    if(app->browser_is_series_picker) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TonieCustomEventScanPickerBack);
        return true;
    }

    if(app->library.level == TonieBrowseLevelFiles &&
       app->library.active_lang != TonieLangFavorites) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TonieCustomEventBackToSeries);
        return true;
    }

    return false;
}

/* --- Emulate Callbacks --- */
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

/* --- Scan Callbacks --- */
static void tonie_app_emulator_scan_detected_callback(void* context) {
    TonieApp* app = context;
    if(app && app->view_dispatcher) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TonieCustomEventScanTagDetected);
    }
}

static void tonie_app_emulator_scan_result_callback(void* context, bool success) {
    TonieApp* app = context;
    if(app && app->view_dispatcher) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher,
            success ? TonieCustomEventScanSuccess : TonieCustomEventScanFailed);
    }
}

static void tonie_app_on_scan_save(void* context, TonieLang target_lang) {
    TonieApp* app = context;
    furi_check(app);

    app->scan_target_lang = target_lang;
    view_dispatcher_send_custom_event(app->view_dispatcher, TonieCustomEventScanSaveStart);
}

static void tonie_app_on_scan_rescan(void* context) {
    TonieApp* app = context;
    furi_check(app);

    view_dispatcher_send_custom_event(app->view_dispatcher, TonieCustomEventScanRestart);
}

static void tonie_app_on_scan_back(void* context) {
    TonieApp* app = context;
    furi_check(app);

    view_dispatcher_send_custom_event(app->view_dispatcher, TonieCustomEventScanStop);
}

/* --- TextInput Callback --- */
static void tonie_app_on_text_input_done(void* context) {
    TonieApp* app = context;
    furi_check(app);

    view_dispatcher_send_custom_event(app->view_dispatcher, TonieCustomEventTextInputDone);
}

/* --- Settings Callbacks --- */
static void tonie_app_refresh_settings_labels(TonieApp* app) {
    furi_check(app);
    bool en = (app->config.ui_lang == TonieUiLangEn);
    const char* const* speed_names =
        en ? tonie_setting_speed_names_en : tonie_setting_speed_names_de;
    const char* const* bool_names =
        en ? tonie_setting_bool_names_en : tonie_setting_bool_names_de;

#ifdef FW_ORIGIN_Momentum
    variable_item_list_set_header(app->settings_list, en ? "Settings" : "Einstellungen");
#endif

    if(app->setting_item_ui_lang) {
        uint8_t idx = (app->config.ui_lang == TonieUiLangEn) ? 1 : 0;
        variable_item_set_item_label(app->setting_item_ui_lang, en ? "Language" : "Sprache");
        variable_item_set_current_value_index(app->setting_item_ui_lang, idx);
        variable_item_set_current_value_text(
            app->setting_item_ui_lang, tonie_setting_ui_lang_names[idx]);
    }

    if(app->setting_item_tab) {
        uint8_t idx = (uint8_t)app->config.default_tab;
        if(idx >= 3) idx = 0;
        variable_item_set_item_label(app->setting_item_tab, en ? "Default Tab" : "Start-Tab");
        variable_item_set_current_value_index(app->setting_item_tab, idx);
        variable_item_set_current_value_text(app->setting_item_tab, tonie_setting_tab_names[idx]);
    }

    if(app->setting_item_speed) {
        uint8_t idx = 1;
        if(app->config.scroll_speed_ms <= 180) {
            idx = 0;
        } else if(app->config.scroll_speed_ms >= 320) {
            idx = 2;
        }
        variable_item_set_item_label(app->setting_item_speed, en ? "Scroll Speed" : "Scroll-Tempo");
        variable_item_set_current_value_index(app->setting_item_speed, idx);
        variable_item_set_current_value_text(app->setting_item_speed, speed_names[idx]);
    }

    if(app->setting_item_vibro) {
        uint8_t idx = app->config.vibro_enabled ? 1 : 0;
        variable_item_set_item_label(app->setting_item_vibro, "Vibration");
        variable_item_set_current_value_index(app->setting_item_vibro, idx);
        variable_item_set_current_value_text(app->setting_item_vibro, bool_names[idx]);
    }

    if(app->setting_item_led) {
        uint8_t idx = app->config.led_enabled ? 1 : 0;
        variable_item_set_item_label(app->setting_item_led, "Status-LED");
        variable_item_set_current_value_index(app->setting_item_led, idx);
        variable_item_set_current_value_text(app->setting_item_led, bool_names[idx]);
    }
}

static void tonie_app_setting_ui_lang_changed(VariableItem* item) {
    TonieApp* app = variable_item_get_context(item);
    furi_check(app);

    uint8_t idx = variable_item_get_current_value_index(item);
    app->config.ui_lang = (idx == 1) ? TonieUiLangEn : TonieUiLangDe;
    tonie_app_apply_ui_lang(app);
    tonie_app_refresh_settings_labels(app);
    tonie_storage_save_config(app->storage, &app->config);
}

static void tonie_app_setting_tab_changed(VariableItem* item) {
    TonieApp* app = variable_item_get_context(item);
    furi_check(app);

    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= TonieLangCount) {
        idx = 0;
    }
    variable_item_set_current_value_text(item, tonie_setting_tab_names[idx]);
    app->config.default_tab = (TonieLang)idx;
    tonie_storage_save_config(app->storage, &app->config);
}

static void tonie_app_setting_speed_changed(VariableItem* item) {
    TonieApp* app = variable_item_get_context(item);
    furi_check(app);

    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= 3) {
        idx = 1;
    }
    const char* const* speed_names = (app->config.ui_lang == TonieUiLangEn) ?
                                         tonie_setting_speed_names_en :
                                         tonie_setting_speed_names_de;
    variable_item_set_current_value_text(item, speed_names[idx]);
    app->config.scroll_speed_ms = tonie_setting_speed_values[idx];
    tonie_view_browser_set_scroll_speed(app->view_browser, app->config.scroll_speed_ms);
    tonie_storage_save_config(app->storage, &app->config);
}

static void tonie_app_setting_vibro_changed(VariableItem* item) {
    TonieApp* app = variable_item_get_context(item);
    furi_check(app);

    uint8_t idx = variable_item_get_current_value_index(item);
    bool enabled = (idx != 0);
    const char* const* bool_names = (app->config.ui_lang == TonieUiLangEn) ?
                                        tonie_setting_bool_names_en :
                                        tonie_setting_bool_names_de;
    variable_item_set_current_value_text(item, bool_names[enabled ? 1 : 0]);
    app->config.vibro_enabled = enabled;
    tonie_emulator_set_feedback(app->emulator, app->config.led_enabled, app->config.vibro_enabled);
    tonie_storage_save_config(app->storage, &app->config);
    if(enabled) {
        tonie_app_short_vibro(app);
    }
}

static void tonie_app_setting_led_changed(VariableItem* item) {
    TonieApp* app = variable_item_get_context(item);
    furi_check(app);

    uint8_t idx = variable_item_get_current_value_index(item);
    bool enabled = (idx != 0);
    const char* const* bool_names = (app->config.ui_lang == TonieUiLangEn) ?
                                        tonie_setting_bool_names_en :
                                        tonie_setting_bool_names_de;
    variable_item_set_current_value_text(item, bool_names[enabled ? 1 : 0]);
    app->config.led_enabled = enabled;
    tonie_emulator_set_feedback(app->emulator, app->config.led_enabled, app->config.vibro_enabled);
    tonie_storage_save_config(app->storage, &app->config);
}

static void tonie_app_setup_settings_list(TonieApp* app) {
    variable_item_list_reset(app->settings_list);
    bool en = (app->config.ui_lang == TonieUiLangEn);

    /* 1. Sprache / Language (DE / EN) */
    app->setting_item_ui_lang = variable_item_list_add(
        app->settings_list,
        en ? "Language" : "Sprache",
        2,
        tonie_app_setting_ui_lang_changed,
        app);

    /* 2. Start-Tab / Default Tab */
    app->setting_item_tab = variable_item_list_add(
        app->settings_list,
        en ? "Default Tab" : "Start-Tab",
        3,
        tonie_app_setting_tab_changed,
        app);

    /* 3. Scroll-Tempo / Scroll Speed */
    app->setting_item_speed = variable_item_list_add(
        app->settings_list,
        en ? "Scroll Speed" : "Scroll-Tempo",
        3,
        tonie_app_setting_speed_changed,
        app);

    /* 4. Vibration */
    app->setting_item_vibro = variable_item_list_add(
        app->settings_list,
        "Vibration",
        2,
        tonie_app_setting_vibro_changed,
        app);

    /* 5. Status-LED */
    app->setting_item_led = variable_item_list_add(
        app->settings_list,
        "Status-LED",
        2,
        tonie_app_setting_led_changed,
        app);

    tonie_app_refresh_settings_labels(app);
}

static void tonie_app_prompt_text_input(
    TonieApp* app,
    const char* header_text,
    char* buffer,
    size_t buffer_size) {
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, header_text);
    text_input_set_result_callback(
        app->text_input,
        tonie_app_on_text_input_done,
        app,
        buffer,
        buffer_size,
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, TonieViewIdTextInput);
}

static void tonie_app_save_scanned_tonie(TonieApp* app) {
    furi_check(app);
    bool en = (app->config.ui_lang == TonieUiLangEn);

    if(app->scan_series_name[0] == '\0') {
        snprintf(
            app->scan_series_name,
            sizeof(app->scan_series_name),
            "%s",
            en ? "My Series" : "Meine Serie");
    }
    if(app->scan_tonie_name[0] == '\0') {
        snprintf(
            app->scan_tonie_name,
            sizeof(app->scan_tonie_name),
            "%s",
            en ? "New Tonie" : "Neuer Tonie");
    }

    char new_nfc_path[TONIE_PATH_MAX];
    bool ok = false;
    if(tonie_storage_build_new_nfc_path(
           app->storage,
           app->scan_target_lang,
           app->scan_series_name,
           app->scan_tonie_name,
           new_nfc_path,
           sizeof(new_nfc_path))) {
        ok = tonie_emulator_save_scanned(app->emulator, new_nfc_path);
    }

    app->browser_is_series_picker = false;
    tonie_view_browser_set_picker_mode(app->view_browser, false);

    if(ok) {
        uint8_t uid[TONIE_UID_LEN] = {0};
        if(tonie_emulator_get_scanned_info(app->emulator, uid, sizeof(uid), NULL)) {
            tonie_storage_register_uid(
                app->storage,
                uid,
                app->scan_target_lang,
                app->scan_series_name,
                app->scan_tonie_name);
        }
        tonie_view_scan_set_saved_info(
            app->view_scan, app->scan_series_name, app->scan_tonie_name);
    } else {
        tonie_view_scan_set_state(
            app->view_scan, TonieScanStateError, app->scan_target_lang);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, TonieViewIdScan);
}

static bool tonie_app_custom_event_callback(void* context, uint32_t event) {
    TonieApp* app = context;
    furi_check(app);

    switch(event) {
    case TonieCustomEventMenuSimulate:
        tonie_app_short_vibro(app);
        app->browser_is_series_picker = false;
        tonie_view_browser_set_picker_mode(app->view_browser, false);
        tonie_storage_scan_series(
            app->storage, &app->library, &app->config, app->config.default_tab);
        tonie_view_browser_update_library(app->view_browser, &app->library);
        view_dispatcher_switch_to_view(app->view_dispatcher, TonieViewIdBrowser);
        return true;

    case TonieCustomEventMenuScan: {
        tonie_app_short_vibro(app);
        TonieLang default_scan_lang = (app->config.default_tab == TonieLangEnglish) ?
                                          TonieLangEnglish :
                                          TonieLangGerman;
        app->scan_target_lang = default_scan_lang;
        tonie_view_scan_set_state(app->view_scan, TonieScanStateScanning, default_scan_lang);
        tonie_emulator_start_scan(
            app->emulator,
            tonie_app_emulator_scan_detected_callback,
            tonie_app_emulator_scan_result_callback,
            app);
        view_dispatcher_switch_to_view(app->view_dispatcher, TonieViewIdScan);
        return true;
    }

    case TonieCustomEventMenuSettings:
        tonie_app_short_vibro(app);
        tonie_app_setup_settings_list(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, TonieViewIdSettings);
        return true;

    case TonieCustomEventScanTagDetected:
        tonie_view_scan_set_state(
            app->view_scan,
            TonieScanStateReading,
            tonie_view_scan_get_target_lang(app->view_scan));
        if(!tonie_emulator_start_poller(app->emulator)) {
            tonie_emulator_stop_scan(app->emulator);
            tonie_view_scan_set_state(
                app->view_scan,
                TonieScanStateError,
                tonie_view_scan_get_target_lang(app->view_scan));
        }
        return true;

    case TonieCustomEventScanSuccess: {
        tonie_emulator_stop_scan(app->emulator);
        uint8_t uid[TONIE_UID_LEN] = {0};
        bool is_slix = true;
        tonie_emulator_get_scanned_info(app->emulator, uid, sizeof(uid), &is_slix);

        TonieLang matched_lang = app->scan_target_lang;
        if(tonie_storage_find_by_uid(
               app->storage,
               uid,
               &matched_lang,
               app->scan_series_name,
               sizeof(app->scan_series_name),
               app->scan_tonie_name,
               sizeof(app->scan_tonie_name))) {
            app->scan_target_lang = matched_lang;
            tonie_view_scan_set_state(app->view_scan, TonieScanStateSuccess, matched_lang);
            tonie_view_scan_set_tag_info(
                app->view_scan, uid, is_slix, app->scan_series_name, app->scan_tonie_name);
        } else {
            app->scan_series_name[0] = '\0';
            app->scan_tonie_name[0] = '\0';
            tonie_view_scan_set_tag_info(app->view_scan, uid, is_slix, NULL, NULL);
        }
        return true;
    }

    case TonieCustomEventScanFailed:
        tonie_emulator_stop_scan(app->emulator);
        tonie_view_scan_set_state(
            app->view_scan,
            TonieScanStateError,
            tonie_view_scan_get_target_lang(app->view_scan));
        return true;

    case TonieCustomEventScanRestart: {
        TonieLang cur_lang = tonie_view_scan_get_target_lang(app->view_scan);
        app->scan_series_name[0] = '\0';
        app->scan_tonie_name[0] = '\0';
        tonie_view_scan_set_state(app->view_scan, TonieScanStateScanning, cur_lang);
        tonie_emulator_start_scan(
            app->emulator,
            tonie_app_emulator_scan_detected_callback,
            tonie_app_emulator_scan_result_callback,
            app);
        return true;
    }

    case TonieCustomEventScanStop:
        tonie_emulator_stop_scan(app->emulator);
        view_dispatcher_switch_to_view(app->view_dispatcher, TonieViewIdSplash);
        return true;

    case TonieCustomEventScanSaveStart:
        tonie_storage_scan_series_for_picker(
            app->storage, &app->library, app->scan_target_lang, app->config.ui_lang);
        if(app->scan_series_name[0] != '\0') {
            for(size_t i = 1; i < app->library.count; i++) {
                if(strcasecmp(app->library.items[i].entry_name, app->scan_series_name) == 0) {
                    app->library.selected_index = i;
                    break;
                }
            }
        }
        app->browser_is_series_picker = true;
        tonie_view_browser_set_picker_mode(app->view_browser, true);
        tonie_view_browser_update_library(app->view_browser, &app->library);
        view_dispatcher_switch_to_view(app->view_dispatcher, TonieViewIdBrowser);
        return true;

    case TonieCustomEventScanPickerBack:
        if(app->library.level == TonieBrowseLevelFiles) {
            /* Return from Level 2 (Tonie Picker) back to Level 1 (Series Picker) */
            size_t prev_series_idx = app->library.saved_series_index;
            tonie_storage_scan_series_for_picker(
                app->storage, &app->library, app->scan_target_lang, app->config.ui_lang);
            if(prev_series_idx < app->library.count) {
                app->library.selected_index = prev_series_idx;
            }
            tonie_view_browser_update_library(app->view_browser, &app->library);
        } else {
            /* Return from Level 1 (Series Picker) back to Scan View */
            app->browser_is_series_picker = false;
            tonie_view_browser_set_picker_mode(app->view_browser, false);
            view_dispatcher_switch_to_view(app->view_dispatcher, TonieViewIdScan);
        }
        return true;

    case TonieCustomEventTextInputDone: {
        bool en = (app->config.ui_lang == TonieUiLangEn);
        if(app->text_input_for_series) {
            /* User entered a new Series name; now ask for the Tonie name */
            if(app->scan_series_name[0] == '\0') {
                snprintf(
                    app->scan_series_name,
                    sizeof(app->scan_series_name),
                    "%s",
                    en ? "My Series" : "Meine Serie");
            }
            app->text_input_for_series = false;
            char series_copy[TONIE_MAX_SERIES_LEN];
            snprintf(series_copy, sizeof(series_copy), "%s", app->scan_series_name);
            snprintf(
                app->scan_tonie_name, sizeof(app->scan_tonie_name), "%s", series_copy);
            tonie_app_prompt_text_input(
                app,
                en ? "Enter Tonie name:" : "Tonie-Name eingeben:",
                app->scan_tonie_name,
                sizeof(app->scan_tonie_name));
        } else {
            /* Both Series and Tonie name are set -> save .nfc file to SD card */
            tonie_app_save_scanned_tonie(app);
        }
        return true;
    }

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
        if(app->library.selected_index >= app->library.count) {
            return true;
        }

        const TonieItem* item = &app->library.items[app->library.selected_index];

        /* Handle selection when Browser is in Two-Level Picker mode for saving a scanned Tonie */
        if(app->browser_is_series_picker) {
            bool en = (app->config.ui_lang == TonieUiLangEn);
            if(app->library.level == TonieBrowseLevelSeries) {
                /* Level 1: Series selection (Freetext OR Series from list) */
                if(app->library.selected_index == 0 &&
                   item->fav_index == TONIE_PICKER_ACTION_FREETEXT) {
                    app->text_input_for_series = true;
                    snprintf(
                        app->scan_series_name,
                        sizeof(app->scan_series_name),
                        "%s",
                        en ? "New Series" : "Neue Serie");
                    tonie_app_prompt_text_input(
                        app,
                        en ? "Enter new series:" : "Neue Serie eingeben:",
                        app->scan_series_name,
                        sizeof(app->scan_series_name));
                } else {
                    app->library.saved_series_index = app->library.selected_index;
                    snprintf(
                        app->scan_series_name,
                        sizeof(app->scan_series_name),
                        "%s",
                        item->entry_name);
                    tonie_storage_scan_episodes_for_picker(
                        app->storage,
                        &app->library,
                        app->scan_target_lang,
                        app->scan_series_name,
                        app->config.ui_lang);
                    if(app->scan_tonie_name[0] != '\0') {
                        for(size_t i = 1; i < app->library.count; i++) {
                            if(strcasecmp(
                                   app->library.items[i].entry_name, app->scan_tonie_name) == 0) {
                                app->library.selected_index = i;
                                break;
                            }
                        }
                    }
                    tonie_view_browser_update_library(app->view_browser, &app->library);
                }
            } else {
                /* Level 2: Tonie/Episode selection (Freetext OR Tonie from tonies.json list) */
                if(app->library.selected_index == 0 &&
                   item->fav_index == TONIE_PICKER_ACTION_FREETEXT) {
                    app->text_input_for_series = false;
                    if(app->scan_tonie_name[0] == '\0') {
                        char series_copy[TONIE_MAX_SERIES_LEN];
                        snprintf(series_copy, sizeof(series_copy), "%s", app->scan_series_name);
                        snprintf(
                            app->scan_tonie_name,
                            sizeof(app->scan_tonie_name),
                            "%s",
                            series_copy);
                    }
                    tonie_app_prompt_text_input(
                        app,
                        en ? "Enter Tonie name:" : "Tonie-Name eingeben:",
                        app->scan_tonie_name,
                        sizeof(app->scan_tonie_name));
                } else {
                    snprintf(
                        app->scan_tonie_name,
                        sizeof(app->scan_tonie_name),
                        "%s",
                        item->entry_name);
                    tonie_app_save_scanned_tonie(app);
                }
            }
            return true;
        }

        /* Normal Simulation Browser selection */
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

    /* Allocate NFC Emulator & Scanner engine */
    app->emulator = tonie_emulator_alloc();
    tonie_emulator_set_rx_callback(app->emulator, tonie_app_emulator_rx_callback, app);

    /* Allocate and register Splash Screen View (with integrated Main Menu) */
    app->view_splash = tonie_view_splash_alloc();
    tonie_view_splash_set_menu_callback(app->view_splash, tonie_app_on_splash_menu, app);
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

    /* Allocate and register NFC Tonie Scanner View */
    app->view_scan = tonie_view_scan_alloc();
    tonie_view_scan_set_callbacks(
        app->view_scan,
        tonie_app_on_scan_save,
        tonie_app_on_scan_rescan,
        tonie_app_on_scan_back,
        app);
    View* scan_view = tonie_view_scan_get_view(app->view_scan);
    view_set_previous_callback(scan_view, tonie_app_scan_previous_callback);
    view_dispatcher_add_view(app->view_dispatcher, TonieViewIdScan, scan_view);

    /* Allocate and register TextInput View (for Series & Tonie naming) */
    app->text_input = text_input_alloc();
    View* text_input_view = text_input_get_view(app->text_input);
    view_set_previous_callback(text_input_view, tonie_app_text_input_previous_callback);
    view_dispatcher_add_view(app->view_dispatcher, TonieViewIdTextInput, text_input_view);

    /* Allocate and register Settings VariableItemList View */
    app->settings_list = variable_item_list_alloc();
    View* settings_view = variable_item_list_get_view(app->settings_list);
    view_set_previous_callback(settings_view, tonie_app_settings_previous_callback);
    view_dispatcher_add_view(app->view_dispatcher, TonieViewIdSettings, settings_view);

    return app;
}

static void tonie_app_free(TonieApp* app) {
    furi_check(app);

    if(app->emulator) {
        tonie_emulator_stop(app->emulator);
        tonie_emulator_stop_scan(app->emulator);
    }

    view_dispatcher_remove_view(app->view_dispatcher, TonieViewIdSplash);
    view_dispatcher_remove_view(app->view_dispatcher, TonieViewIdBrowser);
    view_dispatcher_remove_view(app->view_dispatcher, TonieViewIdEmulate);
    view_dispatcher_remove_view(app->view_dispatcher, TonieViewIdScan);
    view_dispatcher_remove_view(app->view_dispatcher, TonieViewIdTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, TonieViewIdSettings);

    variable_item_list_free(app->settings_list);
    text_input_free(app->text_input);
    tonie_view_scan_free(app->view_scan);
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
    tonie_app_apply_ui_lang(app);
    tonie_view_browser_set_scroll_speed(app->view_browser, app->config.scroll_speed_ms);
    tonie_emulator_set_feedback(
        app->emulator, app->config.led_enabled, app->config.vibro_enabled);

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
