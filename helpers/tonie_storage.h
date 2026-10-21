#pragma once

#include <furi.h>
#include <storage/storage.h>
#include <nfc/nfc_device.h>

#define TONIE_PATH_MAX 224
#define TONIE_MAX_ITEMS 350
#define TONIE_MAX_FAVORITES 32
#define TONIE_MAX_NAME_LEN 148
#define TONIE_MAX_SERIES_LEN 64
#define TONIE_UID_LEN 8

#define TONIE_ROOT_PATH EXT_PATH("nfc/flipper-zero-tonies")
#define TONIE_CONFIG_PATH EXT_PATH("nfc/flipper-zero-tonies/tonie_simulator.cfg")
#define TONIE_TONIES_PATH EXT_PATH("nfc/flipper-zero-tonies/tonies")
#define TONIE_GERMAN_PATH EXT_PATH("nfc/flipper-zero-tonies/tonies/German")
#define TONIE_ENGLISH_PATH EXT_PATH("nfc/flipper-zero-tonies/tonies/English")

/* Legacy fallback paths if a user placed files directly under flipper-zero-tonies/German */
#define TONIE_LEGACY_GERMAN_PATH EXT_PATH("nfc/flipper-zero-tonies/German")
#define TONIE_LEGACY_ENGLISH_PATH EXT_PATH("nfc/flipper-zero-tonies/English")

typedef enum {
    TonieLangGerman = 0, // Default beim App-Start
    TonieLangEnglish = 1,
    TonieLangFavorites = 2,
    TonieLangCount
} TonieLang;

typedef enum {
    TonieUiLangDe = 0, // Deutsch (Standard)
    TonieUiLangEn = 1, // English
    TonieUiLangCount
} TonieUiLang;

typedef enum {
    TonieBrowseLevelSeries = 0, // Ebene 1: Serien-Unterordner auswählen
    TonieBrowseLevelFiles = 1,  // Ebene 2: .nfc Dateien innerhalb der Serie
} TonieBrowseLevel;

typedef struct {
    char entry_name[TONIE_MAX_NAME_LEN];
    uint8_t fav_index;
    bool is_directory;
    bool is_favorite;
} TonieItem;

typedef struct {
    char file_path[TONIE_PATH_MAX];
    char display_name[TONIE_MAX_NAME_LEN];
    char series_name[TONIE_MAX_SERIES_LEN];
    uint8_t uid[TONIE_UID_LEN];
    bool is_slix;
    bool is_favorite;
} TonieEmulateDetails;

typedef struct {
    TonieUiLang ui_lang;
    TonieLang default_tab;
    bool led_enabled;
    bool vibro_enabled;
    uint32_t scroll_speed_ms;
    size_t favorite_count;
    char favorites[TONIE_MAX_FAVORITES][TONIE_PATH_MAX];
} TonieConfig;

typedef struct {
    TonieItem items[TONIE_MAX_ITEMS];
    size_t count;
    size_t selected_index;
    size_t saved_series_index;
    TonieLang active_lang;
    TonieBrowseLevel level;
    char current_dir_path[TONIE_PATH_MAX];
    char active_series_name[TONIE_MAX_SERIES_LEN];
} TonieLibrary;

/**
 * @brief Ensure that root, tonies/German, tonies/English directories exist.
 */
bool tonie_storage_ensure_directories(Storage* storage);

/**
 * @brief Load configuration and favorites from /ext/nfc/flipper-zero-tonies/tonie_simulator.cfg.
 *        Creates default config file if not present.
 */
bool tonie_storage_load_config(Storage* storage, TonieConfig* config);

/**
 * @brief Save configuration and favorites to /ext/nfc/flipper-zero-tonies/tonie_simulator.cfg.
 */
bool tonie_storage_save_config(Storage* storage, const TonieConfig* config);

/**
 * @brief Check if a file path is currently in the favorites list.
 */
bool tonie_storage_is_favorite(const TonieConfig* config, const char* file_path);

/**
 * @brief Toggle a .nfc file path in the favorites list and persist to tonie_simulator.cfg.
 *
 * @return true if the item is now a favorite, false if removed or full.
 */
bool tonie_storage_toggle_favorite(Storage* storage, TonieConfig* config, const char* file_path);

/**
 * @brief Resolve the full SD card path for an item at `index` in `library`.
 */
bool tonie_storage_get_item_path(
    const TonieLibrary* library,
    const TonieConfig* config,
    size_t index,
    char* out_path,
    size_t out_size);

/**
 * @brief Format a human-readable display name for `item` (strips .nfc, replaces '_' with ' ',
 *        and prefixes nested subfolder names when not redundant).
 */
void tonie_storage_get_item_display_name(
    const TonieItem* item,
    char* out_name,
    size_t max_len);

/**
 * @brief Scan Level 1 (Series folders) for German or English, or Favorites list if TonieLangFavorites.
 */
size_t tonie_storage_scan_series(
    Storage* storage,
    TonieLibrary* library,
    const TonieConfig* config,
    TonieLang lang);

/**
 * @brief Scan Level 2 (.nfc files) inside a selected series directory.
 */
size_t tonie_storage_scan_series_files(
    Storage* storage,
    TonieLibrary* library,
    const TonieConfig* config,
    const char* series_path,
    const char* series_name);

/**
 * @brief Parse a single .nfc file on demand, extracting its UID, protocol, series name, and display name.
 */
bool tonie_storage_parse_nfc_file(
    Storage* storage,
    const char* file_path,
    const char* series_name,
    const TonieConfig* config,
    TonieEmulateDetails* details);

/**
 * @brief Return human-readable language label for UI display.
 */
const char* tonie_storage_get_lang_label(TonieLang lang);

/**
 * @brief Return base directory path for the given language.
 */
const char* tonie_storage_get_lang_path(Storage* storage, TonieLang lang);

/**
 * @brief Populate `library` with `[+ Neue Serie anlegen]` / `[+ Create new series]` at index 0
 *        followed by all existing Series folders in the target language directory.
 */
size_t tonie_storage_scan_series_for_picker(
    Storage* storage,
    TonieLibrary* library,
    TonieLang lang,
    TonieUiLang ui_lang);

/**
 * @brief Sanitize a user-entered string for FAT32 directory or filename usage.
 */
void tonie_storage_sanitize_filename(const char* input, char* out, size_t out_size);

/**
 * @brief Create the target Series directory if needed and construct the full `.nfc` file path.
 */
bool tonie_storage_build_new_nfc_path(
    Storage* storage,
    TonieLang lang,
    const char* series_folder_name,
    const char* tonie_name,
    char* out_path,
    size_t out_size);
