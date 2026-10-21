#pragma once

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include <nfc/nfc_scanner.h>
#include <nfc/nfc_poller.h>

typedef struct TonieEmulator TonieEmulator;

typedef void (*TonieEmulatorRxCallback)(void* context);
typedef void (*TonieEmulatorScanDetectedCallback)(void* context);
typedef void (*TonieEmulatorScanResultCallback)(void* context, bool success);

/**
 * @brief Allocate a TonieEmulator instance and underlying NFC resources.
 *
 * @return Pointer to allocated TonieEmulator instance.
 */
TonieEmulator* tonie_emulator_alloc(void);

/**
 * @brief Free a TonieEmulator instance, stopping any active emulation or scan first.
 *
 * @param instance Pointer to TonieEmulator instance.
 */
void tonie_emulator_free(TonieEmulator* instance);

/**
 * @brief Configure whether LED and haptic vibration feedback are enabled.
 */
void tonie_emulator_set_feedback(TonieEmulator* instance, bool led_enabled, bool vibro_enabled);

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

/**
 * @brief Start NFC scanning for an ISO15693 / NXP ICODE SLIX Tonie figure.
 */
bool tonie_emulator_start_scan(
    TonieEmulator* instance,
    TonieEmulatorScanDetectedCallback on_detected,
    TonieEmulatorScanResultCallback on_result,
    void* context);

/**
 * @brief Transition from NfcScanner to NfcPoller on the main thread after a tag was detected.
 */
bool tonie_emulator_start_poller(TonieEmulator* instance);

/**
 * @brief Stop active NFC scanner and/or poller.
 */
void tonie_emulator_stop_scan(TonieEmulator* instance);

/**
 * @brief Retrieve the UID and protocol of the most recently scanned Tonie tag.
 */
bool tonie_emulator_get_scanned_info(
    const TonieEmulator* instance,
    uint8_t* uid_out,
    size_t uid_out_len,
    bool* is_slix_out);

/**
 * @brief Save the most recently scanned Tonie tag to a .nfc file on the SD card.
 */
bool tonie_emulator_save_scanned(TonieEmulator* instance, const char* file_path);
