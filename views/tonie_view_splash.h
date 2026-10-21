#pragma once

#include <furi.h>
#include <gui/view.h>

typedef struct TonieViewSplash TonieViewSplash;

typedef void (*TonieViewSplashStartCallback)(void* context);

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
 * @brief Set the callback invoked when the user presses OK to start.
 */
void tonie_view_splash_set_start_callback(
    TonieViewSplash* instance,
    TonieViewSplashStartCallback on_start,
    void* context);
