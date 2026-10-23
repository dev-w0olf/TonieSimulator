#include "tonie_emulator.h"

#include <furi_hal.h>
#include <furi_hal_light.h>
#include <furi_hal_vibro.h>
#include <nfc/protocols/slix/slix_poller.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>
#include <string.h>

#define TAG "TonieEmulator"

struct TonieEmulator {
    Nfc* nfc;
    NfcDevice* nfc_device;
    NfcListener* nfc_listener;
    NfcScanner* nfc_scanner;
    NfcPoller* nfc_poller;
    NfcProtocol detected_protocol;
    bool is_running;
    bool is_scanning;
    bool scan_valid;
    bool tag_detected_notified;
    uint8_t consecutive_errors;
    uint8_t privacy_pwd_idx;
    bool led_enabled;
    bool vibro_enabled;
    TonieEmulatorRxCallback rx_callback;
    void* rx_context;
    TonieEmulatorScanDetectedCallback scan_detected_callback;
    TonieEmulatorScanResultCallback scan_result_callback;
    void* scan_context;
};

static void tonie_emulator_vibro_pulse(const TonieEmulator* instance) {
    if(instance && instance->vibro_enabled) {
        furi_hal_vibro_on(true);
        furi_delay_ms(35);
        furi_hal_vibro_on(false);
    }
}

static void tonie_emulator_led_emulating(const TonieEmulator* instance) {
    if(instance && instance->led_enabled) {
        /* Cyan/Blue glow during active Tonie emulation */
        furi_hal_light_set(LightRed, 0);
        furi_hal_light_set(LightGreen, 80);
        furi_hal_light_set(LightBlue, 255);
    }
}

static void tonie_emulator_led_scanning(const TonieEmulator* instance) {
    if(instance && instance->led_enabled) {
        /* Warm orange/yellow glow while scanning for a Tonie tag */
        furi_hal_light_set(LightRed, 255);
        furi_hal_light_set(LightGreen, 120);
        furi_hal_light_set(LightBlue, 0);
    }
}

static void tonie_emulator_led_success(const TonieEmulator* instance) {
    if(instance && instance->led_enabled) {
        /* Green glow when a Tonie tag was successfully read */
        furi_hal_light_set(LightRed, 0);
        furi_hal_light_set(LightGreen, 255);
        furi_hal_light_set(LightBlue, 40);
    }
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

static NfcCommand tonie_emulator_poller_callback(NfcGenericEvent event, void* context) {
    TonieEmulator* instance = context;
    if(!instance || !instance->nfc_poller) {
        return NfcCommandStop;
    }

    if(event.protocol == NfcProtocolSlix) {
        SlixPoller* slix_poller = event.instance;
        const SlixPollerEvent* slix_event = event.event_data;
        if(slix_event->type == SlixPollerEventTypePrivacyUnlockRequest) {
            if(!instance->tag_detected_notified) {
                instance->tag_detected_notified = true;
                if(instance->scan_detected_callback) {
                    instance->scan_detected_callback(instance->scan_context);
                }
            }
            if(slix_event->data) {
                slix_event->data->privacy_password.password_set = false;
            }
            return NfcCommandContinue;
        } else if(slix_event->type == SlixPollerEventTypeReady) {
            if(!instance->tag_detected_notified) {
                instance->tag_detected_notified = true;
                if(instance->scan_detected_callback) {
                    instance->scan_detected_callback(instance->scan_context);
                }
            }
            nfc_device_set_data(
                instance->nfc_device,
                NfcProtocolSlix,
                nfc_poller_get_data(instance->nfc_poller));
            instance->scan_valid = true;
            if(instance->scan_result_callback) {
                instance->scan_result_callback(instance->scan_context, true);
            }
            return NfcCommandStop;
        } else if(slix_event->type == SlixPollerEventTypeError) {
            if(slix_event->data && slix_event->data->error == SlixErrorNotSupported) {
                const SlixData* slix_data = nfc_poller_get_data(instance->nfc_poller);
                const Iso15693_3Data* iso_data = slix_data ? slix_get_base_data(slix_data) : NULL;
                if(iso_data) {
                    if(!instance->tag_detected_notified) {
                        instance->tag_detected_notified = true;
                        if(instance->scan_detected_callback) {
                            instance->scan_detected_callback(instance->scan_context);
                        }
                    }
                    nfc_device_set_data(
                        instance->nfc_device, NfcProtocolIso15693_3, iso_data);
                    instance->scan_valid = true;
                    if(instance->scan_result_callback) {
                        instance->scan_result_callback(instance->scan_context, true);
                    }
                    return NfcCommandStop;
                }
            }

            /*
             * Two common cases reach SlixPollerEventTypeError:
             * 1. A Kreativ-Tonie / Tonie is in NXP ICODE SLIX-L Privacy Mode and ignores
             *    standard ISO15693 INVENTORY commands until unlocked via GET RANDOM NUMBER
             *    + SET PASSWORD (Privacy).
             * 2. A Tonie is entering the RF field and had a transient frame/CRC timeout.
             *    Never abort on transient errors; keep polling until Ready succeeds.
             */
            if(slix_poller) {
                SlixRandomNumber rn = 0;
                if(slix_poller_get_random_number(slix_poller, &rn) == SlixErrorNone) {
                    if(!instance->tag_detected_notified) {
                        instance->tag_detected_notified = true;
                        if(instance->scan_detected_callback) {
                            instance->scan_detected_callback(instance->scan_context);
                        }
                    }
                    instance->consecutive_errors++;
                    /*
                     * On the first transient error (consecutive_errors == 1), let the next
                     * standard INVENTORY cycle retry cleanly (avoids sending SET_PASSWORD to
                     * normal Tonies that merely had a frame glitch while entering the field).
                     * If INVENTORY still fails while GET_RANDOM_NUMBER succeeds, the tag is
                     * in SLIX-L Privacy Mode -> unlock it with the Tonie privacy passwords.
                     */
                    if((instance->consecutive_errors & 1U) == 0U) {
                        static const SlixPassword k_privacy_passwords[] = {
                            0x0F0F0F0FUL,
                            0x00000000UL,
                            0xFFFFFFFFUL,
                        };
                        SlixPassword pwd = k_privacy_passwords
                            [instance->privacy_pwd_idx % COUNT_OF(k_privacy_passwords)];
                        instance->privacy_pwd_idx++;
                        if(slix_poller_set_password(
                               slix_poller, SlixPasswordTypePrivacy, pwd, rn) != SlixErrorNone) {
                            if(slix_poller_get_random_number(slix_poller, &rn) == SlixErrorNone) {
                                pwd = k_privacy_passwords
                                    [instance->privacy_pwd_idx % COUNT_OF(k_privacy_passwords)];
                                instance->privacy_pwd_idx++;
                                (void)slix_poller_set_password(
                                    slix_poller, SlixPasswordTypePrivacy, pwd, rn);
                            }
                        }
                    }
                } else {
                    instance->consecutive_errors = 0;
                }
            }
            return NfcCommandContinue;
        }
    } else if(event.protocol == NfcProtocolIso15693_3) {
        const Iso15693_3PollerEvent* iso_event = event.event_data;
        if(iso_event->type == Iso15693_3PollerEventTypeReady) {
            nfc_device_set_data(
                instance->nfc_device,
                NfcProtocolIso15693_3,
                nfc_poller_get_data(instance->nfc_poller));
            instance->scan_valid = true;
            if(instance->scan_result_callback) {
                instance->scan_result_callback(instance->scan_context, true);
            }
            return NfcCommandStop;
        } else if(iso_event->type == Iso15693_3PollerEventTypeError) {
            return NfcCommandContinue;
        }
    }

    return NfcCommandContinue;
}

TonieEmulator* tonie_emulator_alloc(void) {
    TonieEmulator* instance = malloc(sizeof(TonieEmulator));
    furi_check(instance);

    instance->nfc = nfc_alloc();
    instance->nfc_device = nfc_device_alloc();
    instance->nfc_listener = NULL;
    instance->nfc_scanner = NULL;
    instance->nfc_poller = NULL;
    instance->detected_protocol = NfcProtocolInvalid;
    instance->is_running = false;
    instance->is_scanning = false;
    instance->scan_valid = false;
    instance->tag_detected_notified = false;
    instance->consecutive_errors = 0;
    instance->privacy_pwd_idx = 0;
    instance->led_enabled = true;
    instance->vibro_enabled = true;
    instance->rx_callback = NULL;
    instance->rx_context = NULL;
    instance->scan_detected_callback = NULL;
    instance->scan_result_callback = NULL;
    instance->scan_context = NULL;

    return instance;
}

void tonie_emulator_free(TonieEmulator* instance) {
    furi_check(instance);

    if(instance->is_running) {
        tonie_emulator_stop(instance);
    }

    if(instance->is_scanning || instance->nfc_scanner || instance->nfc_poller) {
        tonie_emulator_stop_scan(instance);
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

void tonie_emulator_set_feedback(TonieEmulator* instance, bool led_enabled, bool vibro_enabled) {
    furi_check(instance);
    instance->led_enabled = led_enabled;
    instance->vibro_enabled = vibro_enabled;
    if(!led_enabled) {
        tonie_emulator_led_off();
    }
}

bool tonie_emulator_start(TonieEmulator* instance, const char* nfc_file_path) {
    furi_check(instance);
    furi_check(nfc_file_path);

    if(instance->is_running) {
        tonie_emulator_stop(instance);
    }
    if(instance->is_scanning || instance->nfc_scanner || instance->nfc_poller) {
        tonie_emulator_stop_scan(instance);
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

    tonie_emulator_led_emulating(instance);
    tonie_emulator_vibro_pulse(instance);

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
    tonie_emulator_vibro_pulse(instance);

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

bool tonie_emulator_start_scan(
    TonieEmulator* instance,
    TonieEmulatorScanDetectedCallback on_detected,
    TonieEmulatorScanResultCallback on_result,
    void* context) {
    furi_check(instance);

    if(instance->is_running) {
        tonie_emulator_stop(instance);
    }
    tonie_emulator_stop_scan(instance);

    nfc_device_clear(instance->nfc_device);
    instance->scan_valid = false;
    instance->tag_detected_notified = false;
    instance->consecutive_errors = 0;
    instance->privacy_pwd_idx = 0;
    instance->detected_protocol = NfcProtocolSlix;
    instance->scan_detected_callback = on_detected;
    instance->scan_result_callback = on_result;
    instance->scan_context = context;

    instance->nfc_poller = nfc_poller_alloc(instance->nfc, NfcProtocolSlix);
    if(!instance->nfc_poller) {
        FURI_LOG_E(TAG, "Failed to allocate NfcPoller for SLIX scan");
        return false;
    }

    nfc_poller_start(instance->nfc_poller, tonie_emulator_poller_callback, instance);
    instance->is_scanning = true;
    tonie_emulator_led_scanning(instance);

    FURI_LOG_I(TAG, "Started continuous SLIX poller for Tonie tag");
    return true;
}

bool tonie_emulator_start_poller(TonieEmulator* instance) {
    furi_check(instance);

    if(instance->nfc_poller) {
        return true;
    }

    if(instance->nfc_scanner) {
        nfc_scanner_stop(instance->nfc_scanner);
        nfc_scanner_free(instance->nfc_scanner);
        instance->nfc_scanner = NULL;
    }

    if(instance->detected_protocol != NfcProtocolSlix &&
       instance->detected_protocol != NfcProtocolIso15693_3) {
        instance->detected_protocol = NfcProtocolSlix;
    }

    instance->nfc_poller = nfc_poller_alloc(instance->nfc, instance->detected_protocol);
    if(!instance->nfc_poller) {
        FURI_LOG_E(TAG, "Failed to allocate NfcPoller for protocol %d", instance->detected_protocol);
        return false;
    }

    nfc_poller_start(instance->nfc_poller, tonie_emulator_poller_callback, instance);
    instance->is_scanning = true;
    return true;
}

void tonie_emulator_stop_scan(TonieEmulator* instance) {
    furi_check(instance);

    if(instance->nfc_scanner) {
        nfc_scanner_stop(instance->nfc_scanner);
        nfc_scanner_free(instance->nfc_scanner);
        instance->nfc_scanner = NULL;
    }

    if(instance->nfc_poller) {
        nfc_poller_stop(instance->nfc_poller);
        nfc_poller_free(instance->nfc_poller);
        instance->nfc_poller = NULL;
    }

    if(instance->is_scanning) {
        instance->is_scanning = false;
        if(instance->scan_valid) {
            tonie_emulator_led_success(instance);
            tonie_emulator_vibro_pulse(instance);
        } else {
            tonie_emulator_led_off();
        }
    } else if(!instance->is_running) {
        tonie_emulator_led_off();
    }
}

bool tonie_emulator_get_scanned_info(
    const TonieEmulator* instance,
    uint8_t* uid_out,
    size_t uid_out_len,
    bool* is_slix_out) {
    furi_check(instance);

    if(!instance->scan_valid || !instance->nfc_device) {
        return false;
    }

    NfcProtocol proto = nfc_device_get_protocol(instance->nfc_device);
    if(is_slix_out) {
        *is_slix_out = (proto == NfcProtocolSlix);
    }

    if(uid_out && uid_out_len > 0) {
        memset(uid_out, 0, uid_out_len);
        size_t uid_len = 0;
        const uint8_t* uid_ptr = nfc_device_get_uid(instance->nfc_device, &uid_len);
        if(uid_ptr && uid_len > 0) {
            size_t copy_len = (uid_len < uid_out_len) ? uid_len : uid_out_len;
            memcpy(uid_out, uid_ptr, copy_len);
        }
    }

    return true;
}

bool tonie_emulator_save_scanned(TonieEmulator* instance, const char* file_path) {
    furi_check(instance);
    furi_check(file_path);

    if(!instance->scan_valid || !instance->nfc_device) {
        return false;
    }

    bool saved = nfc_device_save(instance->nfc_device, file_path);
    if(saved) {
        tonie_emulator_vibro_pulse(instance);
        FURI_LOG_I(TAG, "Saved scanned Tonie NFC dump to %s", file_path);
    } else {
        FURI_LOG_E(TAG, "Failed to save scanned Tonie NFC dump to %s", file_path);
    }
    return saved;
}
