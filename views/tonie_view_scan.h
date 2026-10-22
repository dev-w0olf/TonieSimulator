#pragma once

#include <furi.h>
#include <gui/view.h>
#include "../helpers/tonie_storage.h"

typedef struct TonieViewScan TonieViewScan;

typedef enum {
    TonieScanStateScanning = 0,
    TonieScanStateReading = 1,
    TonieScanStateSuccess = 2,
    TonieScanStateSaved = 3,
    TonieScanStateError = 4,
} TonieScanState;

typedef void (*TonieViewScanSaveCallback)(void* context, TonieLang target_lang);
typedef void (*TonieViewScanRescanCallback)(void* context);
typedef void (*TonieViewScanBackCallback)(void* context);

/**
 * @brief Allocate the NFC Tonie Scanner View instance.
 */
TonieViewScan* tonie_view_scan_alloc(void);

/**
 * @brief Free the NFC Tonie Scanner View instance.
 */
void tonie_view_scan_free(TonieViewScan* instance);

/**
 * @brief Get the underlying Furi GUI View pointer.
 */
View* tonie_view_scan_get_view(TonieViewScan* instance);

/**
 * @brief Register callbacks for saving a scanned Tonie, restarting a scan, and going back.
 */
void tonie_view_scan_set_callbacks(
    TonieViewScan* instance,
    TonieViewScanSaveCallback on_save,
    TonieViewScanRescanCallback on_rescan,
    TonieViewScanBackCallback on_back,
    void* context);

/**
 * @brief Update the scanner state (Scanning, Reading, Error).
 */
void tonie_view_scan_set_state(
    TonieViewScan* instance,
    TonieScanState state,
    TonieLang default_lang);

/**
 * @brief Populate the view with the UID, protocol, and optional matched library info
 *        of a successfully read Tonie tag.
 */
void tonie_view_scan_set_tag_info(
    TonieViewScan* instance,
    const uint8_t* uid,
    bool is_slix,
    const char* matched_series,
    const char* matched_name);

/**
 * @brief Show confirmation screen after saving the scanned Tonie to SD card.
 */
void tonie_view_scan_set_saved_info(
    TonieViewScan* instance,
    const char* series_name,
    const char* tonie_name);

/**
 * @brief Get the currently selected target language (German or English).
 */
TonieLang tonie_view_scan_get_target_lang(TonieViewScan* instance);

/**
 * @brief Set the UI display language (DE or EN).
 */
void tonie_view_scan_set_ui_lang(TonieViewScan* instance, TonieUiLang ui_lang);
