#pragma once

#include <furi.h>
#include <gui/view.h>
#include "../helpers/tonie_storage.h"

typedef struct TonieViewBrowser TonieViewBrowser;

typedef void (*TonieViewBrowserSelectCallback)(void* context, size_t selected_index);
typedef void (*TonieViewBrowserLangCallback)(void* context, TonieLang new_lang);
typedef void (*TonieViewBrowserFavCallback)(void* context, size_t selected_index);
typedef bool (*TonieViewBrowserBackCallback)(void* context);

/**
 * @brief Allocate the Browser View instance (with marquee scroll timer).
 */
TonieViewBrowser* tonie_view_browser_alloc(void);

/**
 * @brief Free the Browser View instance.
 */
void tonie_view_browser_free(TonieViewBrowser* instance);

/**
 * @brief Get the underlying Furi GUI View pointer.
 */
View* tonie_view_browser_get_view(TonieViewBrowser* instance);

/**
 * @brief Set callbacks for selection (OK), language/tab switch (Left/Right),
 *        favorite toggle (Hold OK), and level back navigation (Back).
 */
void tonie_view_browser_set_callbacks(
    TonieViewBrowser* instance,
    TonieViewBrowserSelectCallback on_select,
    TonieViewBrowserLangCallback on_lang_change,
    TonieViewBrowserFavCallback on_fav_toggle,
    TonieViewBrowserBackCallback on_back,
    void* context);

/**
 * @brief Configure marquee scroll speed in milliseconds.
 */
void tonie_view_browser_set_scroll_speed(TonieViewBrowser* instance, uint32_t scroll_speed_ms);

/**
 * @brief Synchronize the browser view model with the current TonieLibrary state.
 */
void tonie_view_browser_update_library(TonieViewBrowser* instance, const TonieLibrary* library);
