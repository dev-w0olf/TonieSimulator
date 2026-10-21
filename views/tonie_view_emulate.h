#pragma once

#include <furi.h>
#include <gui/view.h>
#include "../helpers/tonie_storage.h"

typedef struct TonieViewEmulate TonieViewEmulate;

typedef void (*TonieViewEmulateStopCallback)(void* context);

/**
 * @brief Allocate the Live Emulation View instance (including 500ms animation timer).
 *
 * @return Pointer to allocated TonieViewEmulate instance.
 */
TonieViewEmulate* tonie_view_emulate_alloc(void);

/**
 * @brief Free the Live Emulation View instance and its animation timer.
 *
 * @param instance Pointer to TonieViewEmulate instance.
 */
void tonie_view_emulate_free(TonieViewEmulate* instance);

/**
 * @brief Get the underlying Furi GUI View pointer for registration with ViewDispatcher.
 *
 * @param instance Pointer to TonieViewEmulate instance.
 * @return Pointer to View.
 */
View* tonie_view_emulate_get_view(TonieViewEmulate* instance);

/**
 * @brief Set the callback invoked when the user stops emulation via OK or Back.
 *
 * @param instance Pointer to TonieViewEmulate instance.
 * @param on_stop Callback function pointer.
 * @param context User context pointer passed to callback.
 */
void tonie_view_emulate_set_stop_callback(
    TonieViewEmulate* instance,
    TonieViewEmulateStopCallback on_stop,
    void* context);

/**
 * @brief Configure the active Tonie item to display in the emulation view.
 *
 * @param instance Pointer to TonieViewEmulate instance.
 * @param details Pointer to parsed TonieEmulateDetails.
 * @param lang Active language (German or English).
 * @param emulation_running True if the NFC listener started successfully.
 */
void tonie_view_emulate_set_item(
    TonieViewEmulate* instance,
    const TonieEmulateDetails* details,
    TonieLang lang,
    bool emulation_running);

/**
 * @brief Notify the emulation view that an RF reader frame was received (boosts wave animation).
 *
 * @param instance Pointer to TonieViewEmulate instance.
 */
void tonie_view_emulate_notify_rx(TonieViewEmulate* instance);
