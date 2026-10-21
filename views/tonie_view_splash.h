#pragma once

#include <furi.h>
#include <gui/view.h>

typedef struct TonieViewSplash TonieViewSplash;

typedef enum {
    TonieMenuActionSimulate = 0, // 1. Simulation
    TonieMenuActionScan = 1,     // 2. Tonie scannen
    TonieMenuActionSettings = 2, // 3. Einstellungen
    TonieMenuActionCount
} TonieMenuAction;

typedef void (*TonieViewSplashMenuCallback)(void* context, TonieMenuAction action);

/**
 * @brief Allocate the Splash Screen View instance.
 */
TonieViewSplash* tonie_view_splash_alloc(void);

/**
 * @brief Free the Splash Screen View instance.
 */
void tonie_view_splash_free(TonieViewSplash* instance);

/**
 * @brief Get the underlying Furi GUI View pointer.
 */
View* tonie_view_splash_get_view(TonieViewSplash* instance);

/**
 * @brief Set the callback invoked when the user selects a main menu item with OK.
 */
void tonie_view_splash_set_menu_callback(
    TonieViewSplash* instance,
    TonieViewSplashMenuCallback on_menu_select,
    void* context);
