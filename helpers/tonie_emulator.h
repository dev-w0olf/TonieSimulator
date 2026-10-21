#pragma once

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>

typedef struct TonieEmulator TonieEmulator;

typedef void (*TonieEmulatorRxCallback)(void* context);

/**
 * @brief Allocate a TonieEmulator instance and underlying NFC resources.
 *
 * @return Pointer to allocated TonieEmulator instance.
 */
TonieEmulator* tonie_emulator_alloc(void);

/**
 * @brief Free a TonieEmulator instance, stopping any active emulation first.
 *
 * @param instance Pointer to TonieEmulator instance.
 */
void tonie_emulator_free(TonieEmulator* instance);

/**
 * @brief Load a .nfc dump from SD card and start ISO15693 / ICODE SLIX NFC emulation.
 *        Also activates cyan/blue status LED and triggers a short haptic pulse.
 *
 * @param instance Pointer to TonieEmulator instance.
 * @param nfc_file_path Full path to the .nfc dump file.
 * @return true if emulation started successfully, false otherwise.
 */
bool tonie_emulator_start(TonieEmulator* instance, const char* nfc_file_path);

/**
 * @brief Stop active NFC emulation, release listener, turn off LED, and trigger a short haptic pulse.
 *
 * @param instance Pointer to TonieEmulator instance.
 */
void tonie_emulator_stop(TonieEmulator* instance);

/**
 * @brief Check whether NFC emulation is currently running.
 *
 * @param instance Pointer to TonieEmulator instance.
 * @return true if listener is active, false otherwise.
 */
bool tonie_emulator_is_running(TonieEmulator* instance);

/**
 * @brief Register a callback invoked whenever an NFC reader (Toniebox) interacts with the emulated tag.
 *
 * @param instance Pointer to TonieEmulator instance.
 * @param callback Callback function pointer.
 * @param context User-defined context passed to callback.
 */
void tonie_emulator_set_rx_callback(
    TonieEmulator* instance,
    TonieEmulatorRxCallback callback,
    void* context);
