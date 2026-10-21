#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <storage/storage.h>

#include "helpers/tonie_storage.h"
#include "helpers/tonie_emulator.h"
#include "views/tonie_view_splash.h"
#include "views/tonie_view_browser.h"
#include "views/tonie_view_emulate.h"

#define TAG "TonieSimulator"

typedef enum {
    TonieViewIdSplash = 0,
    TonieViewIdBrowser = 1,
    TonieViewIdEmulate = 2,
} TonieViewId;

typedef enum {
    TonieCustomEventStartApp = 99,
    TonieCustomEventSelect = 100,
    TonieCustomEventStopEmulate = 101,
    TonieCustomEventLangGerman = 102,
    TonieCustomEventLangEnglish = 103,
    TonieCustomEventLangFavorites = 104,
    TonieCustomEventToggleFavorite = 105,
    TonieCustomEventBackToSeries = 106,
    TonieCustomEventReaderRx = 107,
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
} TonieApp;

int32_t tonie_simulator_app(void* p);
