#include "tonie_emulator.h"

#include <furi_hal.h>
#include <furi_hal_light.h>
#include <furi_hal_vibro.h>

#define TAG "TonieEmulator"

struct TonieEmulator {
    Nfc* nfc;
    NfcDevice* nfc_device;
    NfcListener* nfc_listener;
    bool is_running;
    TonieEmulatorRxCallback rx_callback;
    void* rx_context;
};

static void furi_hal_vibro_blink(void) {
    furi_hal_vibro_on(true);
    furi_delay_ms(35);
    furi_hal_vibro_on(false);
}

static void tonie_emulator_led_emulating(void) {
    /* Cyan/Blue glow during active Tonie emulation */
    furi_hal_light_set(LightRed, 0);
    furi_hal_light_set(LightGreen, 80);
    furi_hal_light_set(LightBlue, 255);
}

static void tonie_emulator_led_off(void) {
    furi_hal_light_set(LightRed, 0);
    furi_hal_light_set(LightGreen, 0);
    furi_hal_light_set(LightBlue, 0);
}

static NfcCommand tonie_emulator_listener_callback(NfcGenericEvent event, void* context) {
    UNUSED(event);
    TonieEmulator* instance = context;
    if(instance && instance->rx_callback) {
        instance->rx_callback(instance->rx_context);
    }
    return NfcCommandContinue;
}

TonieEmulator* tonie_emulator_alloc(void) {
    TonieEmulator* instance = malloc(sizeof(TonieEmulator));
    furi_check(instance);

    instance->nfc = nfc_alloc();
    instance->nfc_device = nfc_device_alloc();
    instance->nfc_listener = NULL;
    instance->is_running = false;
    instance->rx_callback = NULL;
    instance->rx_context = NULL;

    return instance;
}

void tonie_emulator_free(TonieEmulator* instance) {
    furi_check(instance);

    if(instance->is_running) {
        tonie_emulator_stop(instance);
    }

    if(instance->nfc_device) {
        nfc_device_free(instance->nfc_device);
        instance->nfc_device = NULL;
    }

    if(instance->nfc) {
        nfc_free(instance->nfc);
        instance->nfc = NULL;
    }

    free(instance);
}

bool tonie_emulator_start(TonieEmulator* instance, const char* nfc_file_path) {
    furi_check(instance);
    furi_check(nfc_file_path);

    if(instance->is_running) {
        tonie_emulator_stop(instance);
    }

    nfc_device_clear(instance->nfc_device);

    if(!nfc_device_load(instance->nfc_device, nfc_file_path)) {
        FURI_LOG_E(TAG, "Failed to load NFC file: %s", nfc_file_path);
        return false;
    }

    NfcProtocol protocol = nfc_device_get_protocol(instance->nfc_device);
    if(protocol != NfcProtocolSlix && protocol != NfcProtocolIso15693_3) {
        FURI_LOG_W(
            TAG,
            "Non-SLIX/ISO15693 protocol (%d) in %s, attempting listener anyway",
            protocol,
            nfc_file_path);
        if(protocol == NfcProtocolInvalid) {
            return false;
        }
    }

    const NfcDeviceData* data = nfc_device_get_data(instance->nfc_device, protocol);
    if(!data) {
        FURI_LOG_E(TAG, "Failed to get protocol data for %s", nfc_file_path);
        return false;
    }

    instance->nfc_listener = nfc_listener_alloc(instance->nfc, protocol, data);
    if(!instance->nfc_listener) {
        FURI_LOG_E(TAG, "Failed to allocate NfcListener for protocol %d", protocol);
        return false;
    }

    nfc_listener_start(instance->nfc_listener, tonie_emulator_listener_callback, instance);
    instance->is_running = true;

    tonie_emulator_led_emulating();
    furi_hal_vibro_blink();

    FURI_LOG_I(TAG, "Started SLIX/ISO15693 emulation for %s", nfc_file_path);
    return true;
}

void tonie_emulator_stop(TonieEmulator* instance) {
    furi_check(instance);

    if(!instance->is_running) {
        return;
    }

    if(instance->nfc_listener) {
        nfc_listener_stop(instance->nfc_listener);
        nfc_listener_free(instance->nfc_listener);
        instance->nfc_listener = NULL;
    }

    instance->is_running = false;

    tonie_emulator_led_off();
    furi_hal_vibro_blink();

    FURI_LOG_I(TAG, "Stopped Tonie NFC emulation");
}

bool tonie_emulator_is_running(TonieEmulator* instance) {
    furi_check(instance);
    return instance->is_running;
}

void tonie_emulator_set_rx_callback(
    TonieEmulator* instance,
    TonieEmulatorRxCallback callback,
    void* context) {
    furi_check(instance);
    instance->rx_callback = callback;
    instance->rx_context = context;
}
