#include "tonie_storage.h"

#include <flipper_format/flipper_format.h>
#include <furi_hal.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define TAG "TonieStorage"
#define TONIE_CONFIG_FILETYPE "TonieSimulator Config"
#define TONIE_CONFIG_VERSION 1

static bool tonie_storage_ensure_single_dir(Storage* storage, const char* path) {
    FileInfo file_info;
    FS_Error err = storage_common_stat(storage, path, &file_info);
    if(err == FSE_OK) {
        return (file_info.flags & FSF_DIRECTORY) != 0;
    }

    err = storage_common_mkdir(storage, path);
    if(err != FSE_OK && err != FSE_EXIST) {
        FURI_LOG_E(TAG, "Failed to create directory %s (err=%d)", path, err);
        return false;
    }
    return true;
}

bool tonie_storage_ensure_directories(Storage* storage) {
    furi_check(storage);

    bool ok = true;
    ok &= tonie_storage_ensure_single_dir(storage, EXT_PATH("nfc"));
    ok &= tonie_storage_ensure_single_dir(storage, TONIE_ROOT_PATH);
    ok &= tonie_storage_ensure_single_dir(storage, TONIE_TONIES_PATH);
    ok &= tonie_storage_ensure_single_dir(storage, TONIE_GERMAN_PATH);
    ok &= tonie_storage_ensure_single_dir(storage, TONIE_ENGLISH_PATH);

    return ok;
}

const char* tonie_storage_get_lang_label(TonieLang lang) {
    switch(lang) {
    case TonieLangEnglish:
        return "English (EN)";
    case TonieLangFavorites:
        return "Favorites";
    case TonieLangGerman:
    default:
        return "German (DE)";
    }
}

static bool tonie_storage_dir_has_entries(Storage* storage, const char* path) {
    bool has_entries = false;
    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, path)) {
        FileInfo fi;
        char name[256];
        while(storage_dir_read(dir, &fi, name, sizeof(name))) {
            if(name[0] != '.') {
                has_entries = true;
                break;
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    return has_entries;
}

const char* tonie_storage_get_lang_path(Storage* storage, TonieLang lang) {
    if(lang == TonieLangEnglish) {
        if(storage && !tonie_storage_dir_has_entries(storage, TONIE_ENGLISH_PATH) &&
           tonie_storage_dir_has_entries(storage, TONIE_LEGACY_ENGLISH_PATH)) {
            return TONIE_LEGACY_ENGLISH_PATH;
        }
        return TONIE_ENGLISH_PATH;
    }

    if(storage && !tonie_storage_dir_has_entries(storage, TONIE_GERMAN_PATH) &&
       tonie_storage_dir_has_entries(storage, TONIE_LEGACY_GERMAN_PATH)) {
        return TONIE_LEGACY_GERMAN_PATH;
    }
    return TONIE_GERMAN_PATH;
}

static void tonie_storage_config_set_defaults(TonieConfig* config) {
    memset(config, 0, sizeof(TonieConfig));
    config->default_tab = TonieLangGerman;
    config->led_enabled = true;
    config->vibro_enabled = true;
    config->scroll_speed_ms = 250;
    config->favorite_count = 0;
}

bool tonie_storage_save_config(Storage* storage, const TonieConfig* config) {
    furi_check(storage);
    furi_check(config);

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool saved = false;

    do {
        if(!flipper_format_file_open_always(ff, TONIE_CONFIG_PATH)) {
            FURI_LOG_E(TAG, "Failed to open config for writing: %s", TONIE_CONFIG_PATH);
            break;
        }

        if(!flipper_format_write_header_cstr(ff, TONIE_CONFIG_FILETYPE, TONIE_CONFIG_VERSION)) {
            break;
        }

        flipper_format_write_comment_cstr(
            ff, "Default tab at startup: German, English, or Favorites");
        const char* tab_str = "German";
        if(config->default_tab == TonieLangEnglish) {
            tab_str = "English";
        } else if(config->default_tab == TonieLangFavorites) {
            tab_str = "Favorites";
        }
        if(!flipper_format_write_string_cstr(ff, "DefaultTab", tab_str)) {
            break;
        }

        flipper_format_write_comment_cstr(
            ff, "Enable cyan/blue LED and haptic vibration during emulation");
        bool led = config->led_enabled;
        bool vibro = config->vibro_enabled;
        if(!flipper_format_write_bool(ff, "LedEnabled", &led, 1)) break;
        if(!flipper_format_write_bool(ff, "VibroEnabled", &vibro, 1)) break;

        flipper_format_write_comment_cstr(ff, "Marquee title scroll speed in milliseconds");
        uint32_t speed = config->scroll_speed_ms;
        if(!flipper_format_write_uint32(ff, "ScrollSpeedMs", &speed, 1)) break;

        flipper_format_write_comment_cstr(
            ff, "Favorite Tonie .nfc file paths (Hold OK in app to toggle)");
        uint32_t fav_count = (uint32_t)config->favorite_count;
        if(!flipper_format_write_uint32(ff, "FavoriteCount", &fav_count, 1)) break;

        for(size_t i = 0; i < config->favorite_count && i < TONIE_MAX_FAVORITES; i++) {
            if(!flipper_format_write_string_cstr(ff, "Favorite", config->favorites[i])) {
                break;
            }
        }

        saved = true;
    } while(false);

    flipper_format_free(ff);
    return saved;
}

bool tonie_storage_load_config(Storage* storage, TonieConfig* config) {
    furi_check(storage);
    furi_check(config);

    tonie_storage_config_set_defaults(config);

    FileInfo fi;
    if(storage_common_stat(storage, TONIE_CONFIG_PATH, &fi) != FSE_OK) {
        tonie_storage_save_config(storage, config);
        return true;
    }

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* temp_str = furi_string_alloc();
    bool loaded = false;

    do {
        if(!flipper_format_file_open_existing(ff, TONIE_CONFIG_PATH)) {
            break;
        }

        uint32_t version = 0;
        if(!flipper_format_read_header(ff, temp_str, &version)) {
            break;
        }

        if(furi_string_cmp_str(temp_str, TONIE_CONFIG_FILETYPE) != 0) {
            break;
        }

        if(flipper_format_read_string(ff, "DefaultTab", temp_str)) {
            if(furi_string_cmpi_str(temp_str, "English") == 0) {
                config->default_tab = TonieLangEnglish;
            } else if(furi_string_cmpi_str(temp_str, "Favorites") == 0) {
                config->default_tab = TonieLangFavorites;
            } else {
                config->default_tab = TonieLangGerman;
            }
        }

        bool flag = true;
        if(flipper_format_read_bool(ff, "LedEnabled", &flag, 1)) {
            config->led_enabled = flag;
        }
        if(flipper_format_read_bool(ff, "VibroEnabled", &flag, 1)) {
            config->vibro_enabled = flag;
        }

        uint32_t speed = 250;
        if(flipper_format_read_uint32(ff, "ScrollSpeedMs", &speed, 1) && speed >= 50 &&
           speed <= 2000) {
            config->scroll_speed_ms = speed;
        }

        uint32_t fav_count = 0;
        if(flipper_format_read_uint32(ff, "FavoriteCount", &fav_count, 1)) {
            if(fav_count > TONIE_MAX_FAVORITES) {
                fav_count = TONIE_MAX_FAVORITES;
            }
            config->favorite_count = 0;
            for(uint32_t i = 0; i < fav_count; i++) {
                if(flipper_format_read_string(ff, "Favorite", temp_str)) {
                    const char* cstr = furi_string_get_cstr(temp_str);
                    if(cstr && cstr[0] != '\0') {
                        snprintf(
                            config->favorites[config->favorite_count],
                            TONIE_PATH_MAX,
                            "%s",
                            cstr);
                        config->favorite_count++;
                    }
                } else {
                    break;
                }
            }
        }

        loaded = true;
    } while(false);

    furi_string_free(temp_str);
    flipper_format_free(ff);
    return loaded;
}

bool tonie_storage_is_favorite(const TonieConfig* config, const char* file_path) {
    if(!config || !file_path || file_path[0] == '\0') {
        return false;
    }

    for(size_t i = 0; i < config->favorite_count && i < TONIE_MAX_FAVORITES; i++) {
        if(strcmp(config->favorites[i], file_path) == 0) {
            return true;
        }
    }
    return false;
}

bool tonie_storage_toggle_favorite(Storage* storage, TonieConfig* config, const char* file_path) {
    furi_check(storage);
    furi_check(config);
    furi_check(file_path);

    for(size_t i = 0; i < config->favorite_count && i < TONIE_MAX_FAVORITES; i++) {
        if(strcmp(config->favorites[i], file_path) == 0) {
            for(size_t j = i; j + 1 < config->favorite_count; j++) {
                memcpy(
                    config->favorites[j],
                    config->favorites[j + 1],
                    sizeof(config->favorites[j]));
            }
            config->favorite_count--;
            tonie_storage_save_config(storage, config);
            return false;
        }
    }

    if(config->favorite_count >= TONIE_MAX_FAVORITES) {
        FURI_LOG_W(TAG, "Favorites list is full (%d)", TONIE_MAX_FAVORITES);
        return false;
    }

    snprintf(config->favorites[config->favorite_count], TONIE_PATH_MAX, "%s", file_path);
    config->favorite_count++;
    tonie_storage_save_config(storage, config);
    return true;
}

bool tonie_storage_get_item_path(
    const TonieLibrary* library,
    const TonieConfig* config,
    size_t index,
    char* out_path,
    size_t out_size) {
    if(!library || !out_path || out_size == 0 || index >= library->count) {
        return false;
    }

    const TonieItem* item = &library->items[index];
    if(library->active_lang == TonieLangFavorites) {
        if(!config || item->fav_index >= config->favorite_count) {
            return false;
        }
        snprintf(out_path, out_size, "%s", config->favorites[item->fav_index]);
        return true;
    }

    int written =
        snprintf(out_path, out_size, "%s/%s", library->current_dir_path, item->entry_name);
    return (written > 0 && (size_t)written < out_size);
}

static bool tonie_storage_has_nfc_extension(const char* filename) {
    if(!filename || filename[0] == '.') {
        return false;
    }
    size_t len = strlen(filename);
    if(len <= 4) {
        return false;
    }
    const char* ext = filename + (len - 4);
    return strcasecmp(ext, ".nfc") == 0;
}

static void tonie_storage_format_clean_name(
    const char* raw_name,
    char* out_name,
    size_t max_len,
    bool strip_nfc) {
    if(!raw_name || !out_name || max_len == 0) {
        return;
    }

    const char* base = strrchr(raw_name, '/');
    base = base ? (base + 1) : raw_name;

    size_t copy_len = strlen(base);
    if(strip_nfc && copy_len > 4 && strcasecmp(base + copy_len - 4, ".nfc") == 0) {
        copy_len -= 4;
    }
    if(copy_len >= max_len) {
        copy_len = max_len - 1;
    }

    for(size_t i = 0; i < copy_len; i++) {
        out_name[i] = (base[i] == '_') ? ' ' : base[i];
    }
    out_name[copy_len] = '\0';
}

void tonie_storage_get_item_display_name(
    const TonieItem* item,
    char* out_name,
    size_t max_len) {
    if(!item || !out_name || max_len == 0) {
        return;
    }
    out_name[0] = '\0';

    if(item->is_directory) {
        tonie_storage_format_clean_name(item->entry_name, out_name, max_len, false);
        return;
    }

    const char* last_slash = strrchr(item->entry_name, '/');
    if(!last_slash || last_slash == item->entry_name) {
        tonie_storage_format_clean_name(item->entry_name, out_name, max_len, true);
        return;
    }

    /* Nested file inside a subfolder of a Series (e.g. "GEOlino/Alles ueber Vulkane.nfc") */
    char clean_file[TONIE_MAX_NAME_LEN];
    tonie_storage_format_clean_name(last_slash + 1, clean_file, sizeof(clean_file), true);

    const char* sub_start = last_slash - 1;
    while(sub_start > item->entry_name && *sub_start != '/') {
        sub_start--;
    }
    if(*sub_start == '/') {
        sub_start++;
    }

    size_t sub_len = (size_t)(last_slash - sub_start);
    char clean_sub[TONIE_MAX_SERIES_LEN];
    if(sub_len >= sizeof(clean_sub)) {
        sub_len = sizeof(clean_sub) - 1;
    }
    for(size_t i = 0; i < sub_len; i++) {
        clean_sub[i] = (sub_start[i] == '_') ? ' ' : sub_start[i];
    }
    clean_sub[sub_len] = '\0';

    if(clean_sub[0] != '\0' && strncasecmp(clean_file, clean_sub, strlen(clean_sub)) != 0) {
        char combined[TONIE_MAX_SERIES_LEN + TONIE_MAX_NAME_LEN + 4];
        snprintf(combined, sizeof(combined), "%s - %s", clean_sub, clean_file);
        snprintf(out_name, max_len, "%s", combined);
    } else {
        snprintf(out_name, max_len, "%s", clean_file);
    }
}

static void tonie_storage_extract_series_from_path(
    const char* file_path,
    char* out_series,
    size_t max_len) {
    if(!file_path || !out_series || max_len == 0) {
        return;
    }
    out_series[0] = '\0';

    const char* last_slash = strrchr(file_path, '/');
    if(!last_slash || last_slash == file_path) {
        return;
    }

    const char* p = last_slash - 1;
    while(p > file_path && *p != '/') {
        p--;
    }
    if(*p == '/') {
        p++;
    }

    size_t len = (size_t)(last_slash - p);
    if(len >= max_len) {
        len = max_len - 1;
    }

    char raw_folder[TONIE_MAX_SERIES_LEN];
    memcpy(raw_folder, p, len);
    raw_folder[len] = '\0';

    tonie_storage_format_clean_name(raw_folder, out_series, max_len, false);
}

bool tonie_storage_parse_nfc_file(
    Storage* storage,
    const char* file_path,
    const char* series_name,
    const TonieConfig* config,
    TonieEmulateDetails* details) {
    furi_check(storage);
    furi_check(file_path);
    furi_check(details);

    memset(details, 0, sizeof(TonieEmulateDetails));
    snprintf(details->file_path, sizeof(details->file_path), "%s", file_path);
    tonie_storage_format_clean_name(
        file_path, details->display_name, sizeof(details->display_name), true);

    if(series_name && series_name[0] != '\0') {
        snprintf(details->series_name, sizeof(details->series_name), "%s", series_name);
    } else {
        tonie_storage_extract_series_from_path(
            file_path, details->series_name, sizeof(details->series_name));
    }

    details->is_favorite = tonie_storage_is_favorite(config, file_path);

    bool parsed = false;
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* temp_str = furi_string_alloc();

    do {
        if(!flipper_format_file_open_existing(ff, file_path)) {
            break;
        }

        uint32_t version = 0;
        if(!flipper_format_read_header(ff, temp_str, &version)) {
            break;
        }

        if(furi_string_cmp_str(temp_str, "Flipper NFC device") != 0) {
            break;
        }

        if(flipper_format_read_string(ff, "Device type", temp_str)) {
            if(furi_string_search_str(temp_str, "SLIX") != FURI_STRING_FAILURE ||
               furi_string_search_str(temp_str, "ISO15693") != FURI_STRING_FAILURE) {
                details->is_slix = true;
            }
        }

        if(!flipper_format_read_hex(ff, "UID", details->uid, TONIE_UID_LEN)) {
            memset(details->uid, 0, TONIE_UID_LEN);
        }

        parsed = true;
    } while(false);

    furi_string_free(temp_str);
    flipper_format_free(ff);

    return parsed;
}

static void tonie_storage_sort_library(TonieLibrary* library) {
    if(!library || library->count < 2) {
        return;
    }

    char key_display[TONIE_MAX_NAME_LEN];
    char cur_display[TONIE_MAX_NAME_LEN];

    for(size_t i = 1; i < library->count; i++) {
        TonieItem key = library->items[i];
        tonie_storage_get_item_display_name(&key, key_display, sizeof(key_display));
        size_t j = i;
        while(j > 0) {
            if(library->items[j - 1].is_directory != key.is_directory) {
                if(!library->items[j - 1].is_directory && key.is_directory) {
                    library->items[j] = library->items[j - 1];
                    j--;
                    continue;
                }
                break;
            }
            tonie_storage_get_item_display_name(
                &library->items[j - 1], cur_display, sizeof(cur_display));
            if(strcasecmp(cur_display, key_display) > 0) {
                library->items[j] = library->items[j - 1];
                j--;
            } else {
                break;
            }
        }
        library->items[j] = key;
    }
}

size_t tonie_storage_scan_series(
    Storage* storage,
    TonieLibrary* library,
    const TonieConfig* config,
    TonieLang lang) {
    furi_check(storage);
    furi_check(library);

    if(lang >= TonieLangCount) {
        lang = TonieLangGerman;
    }

    library->count = 0;
    library->selected_index = 0;
    library->active_lang = lang;
    library->current_dir_path[0] = '\0';
    library->active_series_name[0] = '\0';

    /* Favorites tab directly lists all favorited .nfc items */
    if(lang == TonieLangFavorites) {
        library->level = TonieBrowseLevelFiles;
        snprintf(library->active_series_name, sizeof(library->active_series_name), "Favoriten");
        if(!config) {
            return 0;
        }

        FileInfo fi;
        for(size_t i = 0; i < config->favorite_count && library->count < TONIE_MAX_ITEMS; i++) {
            const char* fav_path = config->favorites[i];
            if(storage_common_stat(storage, fav_path, &fi) == FSE_OK &&
               (fi.flags & FSF_DIRECTORY) == 0) {
                TonieItem* item = &library->items[library->count];
                memset(item, 0, sizeof(TonieItem));
                const char* base = strrchr(fav_path, '/');
                base = base ? (base + 1) : fav_path;
                snprintf(item->entry_name, sizeof(item->entry_name), "%s", base);
                item->fav_index = (uint8_t)i;
                item->is_directory = false;
                item->is_favorite = true;
                library->count++;
            }
        }
        return library->count;
    }

    library->level = TonieBrowseLevelSeries;
    const char* dir_path = tonie_storage_get_lang_path(storage, lang);
    snprintf(library->current_dir_path, sizeof(library->current_dir_path), "%s", dir_path);

    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, dir_path)) {
        FileInfo file_info;
        char raw_name[256];

        while(library->count < TONIE_MAX_ITEMS &&
              storage_dir_read(dir, &file_info, raw_name, sizeof(raw_name))) {
            if(raw_name[0] == '.') {
                continue;
            }

            size_t raw_len = strlen(raw_name);
            if(raw_len >= TONIE_MAX_NAME_LEN) {
                continue;
            }

            if((file_info.flags & FSF_DIRECTORY) != 0) {
                /* Series subfolder */
                TonieItem* item = &library->items[library->count];
                memset(item, 0, sizeof(TonieItem));
                memcpy(item->entry_name, raw_name, raw_len + 1);
                item->fav_index = 0xFF;
                item->is_directory = true;
                item->is_favorite = false;
                library->count++;
            } else if(tonie_storage_has_nfc_extension(raw_name)) {
                /* Loose .nfc file directly in language directory */
                char full_path[TONIE_PATH_MAX];
                int written = snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, raw_name);
                if(written < 0 || (size_t)written >= sizeof(full_path)) {
                    continue;
                }

                TonieItem* item = &library->items[library->count];
                memset(item, 0, sizeof(TonieItem));
                memcpy(item->entry_name, raw_name, raw_len + 1);
                item->fav_index = 0xFF;
                item->is_directory = false;
                item->is_favorite = tonie_storage_is_favorite(config, full_path);
                library->count++;
            }
        }
    }

    storage_dir_close(dir);
    storage_file_free(dir);

    tonie_storage_sort_library(library);
    return library->count;
}

typedef struct {
    FileInfo file_info;
    char raw_name[256];
    char rel_entry[TONIE_MAX_NAME_LEN];
    char full_path[TONIE_PATH_MAX];
} TonieDirScanFrame;

static void tonie_storage_scan_series_dir_recursive(
    Storage* storage,
    TonieLibrary* library,
    const TonieConfig* config,
    const char* dir_path,
    const char* rel_prefix,
    uint8_t depth) {
    if(depth > 3 || library->count >= TONIE_MAX_ITEMS) {
        return;
    }

    TonieDirScanFrame* frame = malloc(sizeof(TonieDirScanFrame));
    if(!frame) {
        return;
    }

    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, dir_path)) {
        while(library->count < TONIE_MAX_ITEMS &&
              storage_dir_read(
                  dir, &frame->file_info, frame->raw_name, sizeof(frame->raw_name))) {
            if(frame->raw_name[0] == '.') {
                continue;
            }

            int rel_written = 0;
            if(rel_prefix && rel_prefix[0] != '\0') {
                rel_written = snprintf(
                    frame->rel_entry,
                    sizeof(frame->rel_entry),
                    "%s/%s",
                    rel_prefix,
                    frame->raw_name);
            } else {
                rel_written =
                    snprintf(frame->rel_entry, sizeof(frame->rel_entry), "%s", frame->raw_name);
            }
            if(rel_written <= 0 || (size_t)rel_written >= sizeof(frame->rel_entry)) {
                continue;
            }

            int path_written = snprintf(
                frame->full_path,
                sizeof(frame->full_path),
                "%s/%s",
                dir_path,
                frame->raw_name);
            if(path_written <= 0 || (size_t)path_written >= sizeof(frame->full_path)) {
                continue;
            }

            if((frame->file_info.flags & FSF_DIRECTORY) != 0) {
                /* Recurse into nested subfolder inside Series (e.g. Book Tonies, Clever Tonies/GEOlino) */
                tonie_storage_scan_series_dir_recursive(
                    storage,
                    library,
                    config,
                    frame->full_path,
                    frame->rel_entry,
                    depth + 1);
            } else if(tonie_storage_has_nfc_extension(frame->raw_name)) {
                TonieItem* item = &library->items[library->count];
                memset(item, 0, sizeof(TonieItem));
                snprintf(item->entry_name, sizeof(item->entry_name), "%s", frame->rel_entry);
                item->fav_index = 0xFF;
                item->is_directory = false;
                item->is_favorite = tonie_storage_is_favorite(config, frame->full_path);
                library->count++;
            }
        }
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    free(frame);
}

size_t tonie_storage_scan_series_files(
    Storage* storage,
    TonieLibrary* library,
    const TonieConfig* config,
    const char* series_path,
    const char* series_name) {
    furi_check(storage);
    furi_check(library);
    furi_check(series_path);

    char path_copy[TONIE_PATH_MAX];
    char name_copy[TONIE_MAX_SERIES_LEN];
    snprintf(path_copy, sizeof(path_copy), "%s", series_path);
    snprintf(name_copy, sizeof(name_copy), "%s", series_name ? series_name : "");

    library->count = 0;
    library->selected_index = 0;
    library->level = TonieBrowseLevelFiles;
    snprintf(library->current_dir_path, sizeof(library->current_dir_path), "%s", path_copy);
    snprintf(library->active_series_name, sizeof(library->active_series_name), "%s", name_copy);

    tonie_storage_scan_series_dir_recursive(storage, library, config, path_copy, "", 0);

    tonie_storage_sort_library(library);
    return library->count;
}

size_t tonie_storage_scan_series_for_picker(
    Storage* storage,
    TonieLibrary* library,
    TonieLang lang) {
    furi_check(storage);
    furi_check(library);

    if(lang != TonieLangEnglish) {
        lang = TonieLangGerman;
    }

    library->count = 0;
    library->selected_index = 0;
    library->active_lang = lang;
    library->level = TonieBrowseLevelSeries;
    snprintf(library->active_series_name, sizeof(library->active_series_name), "Serie waehlen");

    const char* dir_path = tonie_storage_get_lang_path(storage, lang);
    snprintf(library->current_dir_path, sizeof(library->current_dir_path), "%s", dir_path);

    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, dir_path)) {
        FileInfo file_info;
        char raw_name[256];

        while(library->count + 1 < TONIE_MAX_ITEMS &&
              storage_dir_read(dir, &file_info, raw_name, sizeof(raw_name))) {
            if(raw_name[0] == '.') {
                continue;
            }
            size_t raw_len = strlen(raw_name);
            if(raw_len >= TONIE_MAX_NAME_LEN) {
                continue;
            }
            if((file_info.flags & FSF_DIRECTORY) != 0) {
                TonieItem* item = &library->items[library->count];
                memset(item, 0, sizeof(TonieItem));
                memcpy(item->entry_name, raw_name, raw_len + 1);
                item->fav_index = 0xFF;
                item->is_directory = true;
                item->is_favorite = false;
                library->count++;
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);

    tonie_storage_sort_library(library);

    /* Shift sorted series down by 1 and insert [+ Neue Serie anlegen] at index 0 */
    if(library->count < TONIE_MAX_ITEMS) {
        for(size_t i = library->count; i > 0; i--) {
            library->items[i] = library->items[i - 1];
        }
        TonieItem* first = &library->items[0];
        memset(first, 0, sizeof(TonieItem));
        snprintf(
            first->entry_name, sizeof(first->entry_name), "[+ Neue Serie anlegen]");
        first->fav_index = 0xFE;
        first->is_directory = true;
        first->is_favorite = false;
        library->count++;
    }

    return library->count;
}

void tonie_storage_sanitize_filename(const char* input, char* out, size_t out_size) {
    if(!out || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if(!input) {
        snprintf(out, out_size, "Neuer_Tonie");
        return;
    }

    while(*input == ' ' || *input == '.') {
        input++;
    }

    size_t j = 0;
    for(size_t i = 0; input[i] != '\0' && j + 1 < out_size; i++) {
        char c = input[i];
        if(c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
           c == '>' || c == '|') {
            c = '_';
        }
        out[j++] = c;
    }
    while(j > 0 && (out[j - 1] == ' ' || out[j - 1] == '.')) {
        j--;
    }
    out[j] = '\0';

    if(out[0] == '\0') {
        snprintf(out, out_size, "Neuer_Tonie");
    }
}

bool tonie_storage_build_new_nfc_path(
    Storage* storage,
    TonieLang lang,
    const char* series_folder_name,
    const char* tonie_name,
    char* out_path,
    size_t out_size) {
    furi_check(storage);
    if(!out_path || out_size == 0) {
        return false;
    }

    const char* lang_base =
        (lang == TonieLangEnglish) ? TONIE_ENGLISH_PATH : TONIE_GERMAN_PATH;
    tonie_storage_ensure_directories(storage);

    char clean_series[TONIE_MAX_SERIES_LEN];
    tonie_storage_sanitize_filename(series_folder_name, clean_series, sizeof(clean_series));

    char series_dir_path[TONIE_PATH_MAX];
    int dir_written =
        snprintf(series_dir_path, sizeof(series_dir_path), "%s/%s", lang_base, clean_series);
    if(dir_written <= 0 || (size_t)dir_written >= sizeof(series_dir_path)) {
        return false;
    }

    if(!tonie_storage_ensure_single_dir(storage, series_dir_path)) {
        return false;
    }

    char clean_tonie[TONIE_MAX_NAME_LEN];
    tonie_storage_sanitize_filename(tonie_name, clean_tonie, sizeof(clean_tonie));

    /* Strip trailing .nfc if user typed it manually */
    size_t tlen = strlen(clean_tonie);
    if(tlen > 4 && strcasecmp(clean_tonie + tlen - 4, ".nfc") == 0) {
        clean_tonie[tlen - 4] = '\0';
    }

    int file_written =
        snprintf(out_path, out_size, "%s/%s.nfc", series_dir_path, clean_tonie);
    return (file_written > 0 && (size_t)file_written < out_size);
}
