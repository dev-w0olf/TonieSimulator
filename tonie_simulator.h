#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <storage/storage.h>

#include "helpers/tonie_storage.h"
#include "helpers/tonie_emulator.h"
#include "views/tonie_view_splash.h"
#include "views/tonie_view_browser.h"
#include "views/tonie_view_emulate.h"
#include "views/tonie_view_scan.h"

#define TAG "TonieSimulator"

typedef enum {
    TonieViewIdSplash = 0,
    TonieViewIdBrowser = 1,
    TonieViewIdEmulate = 2,
    TonieViewIdScan = 3,
    TonieViewIdTextInput = 4,
    TonieViewIdSettings = 5,
} TonieViewId;

typedef enum {
    TonieCustomEventMenuSimulate = 90,
    TonieCustomEventMenuScan = 91,
    TonieCustomEventMenuSettings = 92,
    TonieCustomEventSelect = 100,
    TonieCustomEventStopEmulate = 101,
    TonieCustomEventLangGerman = 102,
    TonieCustomEventLangEnglish = 103,
    TonieCustomEventLangFavorites = 104,
    TonieCustomEventToggleFavorite = 105,
    TonieCustomEventBackToSeries = 106,
    TonieCustomEventReaderRx = 107,
    TonieCustomEventBackToSplash = 108,
    TonieCustomEventScanTagDetected = 110,
    TonieCustomEventScanSuccess = 111,
    TonieCustomEventScanFailed = 112,
    TonieCustomEventScanSaveStart = 113,
    TonieCustomEventScanRestart = 114,
    TonieCustomEventScanStop = 115,
    TonieCustomEventScanPickerBack = 116,
    TonieCustomEventTextInputDone = 117,
} TonieCustomEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Storage* storage;
    TonieConfig config;
    TonieLibrary library;
    TonieEmulator* emulator;
    TonieViewSplash* view_splash;
    TonieViewBrowser* view_browser;
    TonieViewEmulate* view_emulate;
    TonieViewScan* view_scan;
    TextInput* text_input;
    VariableItemList* settings_list;

    /* State for NFC Scan & Save workflow */
    bool browser_is_series_picker;
    bool text_input_for_series;
    TonieLang scan_target_lang;
    char scan_series_name[TONIE_MAX_SERIES_LEN];
    char scan_tonie_name[TONIE_MAX_NAME_LEN];
} TonieApp;

int32_t tonie_simulator_app(void* p);
