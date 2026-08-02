#include "runtime_features.h"
#include "runtime_registry.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const mcujs_runtime_registry_t *registry = mcujs_runtime_registry();
    assert(registry != NULL);
    assert(strcmp(registry->board_id, MCUJS_EXPECTED_BOARD) == 0);
    assert(strcmp(registry->api_version, "0.2") == 0);
    assert(strstr(registry->board_json, MCUJS_EXPECTED_MANIFEST_NAME) != NULL);
    assert(strstr(registry->manifest_json, MCUJS_EXPECTED_MANIFEST_NAME) != NULL);
    assert(registry->capability_count > 0);
    assert(mcujs_runtime_find_capability("gpio") != NULL);
    assert(strstr(mcujs_runtime_find_capability("gpio")->json, "\"pins\"") != NULL);
    assert(mcujs_runtime_find_capability("not-a-capability") == NULL);
    assert(mcujs_runtime_has_module("board"));
    assert(mcujs_runtime_has_module("fs"));
    assert(mcujs_runtime_has_module("mcujs:module"));
    assert(mcujs_runtime_has_module("node:module"));
    assert(!mcujs_runtime_has_module("not-a-module"));
    const mcujs_runtime_capability_t *boot = mcujs_runtime_find_capability("boot");
    assert(registry->safe_mode ==
           (boot != NULL && strstr(boot->json, "\"safeMode\":true") != NULL));
    assert(registry->storage_ready ==
           (mcujs_runtime_find_capability("fs") != NULL));

#if MCUJS_FEATURE_IMAGE
    assert(mcujs_runtime_has_module("image"));
#else
    assert(!mcujs_runtime_has_module("image"));
#endif

#if MCUJS_FEATURE_KEYBOARD
    assert(mcujs_runtime_has_module("keyboard"));
#else
    assert(!mcujs_runtime_has_module("keyboard"));
#endif

#if MCUJS_FEATURE_MOUSE
    assert(mcujs_runtime_has_module("mouse"));
#else
    assert(!mcujs_runtime_has_module("mouse"));
#endif

    const mcujs_runtime_capability_t *usb = mcujs_runtime_find_capability("usb");
    assert(usb != NULL);
    assert((strstr(usb->json, "\"cdc\"") != NULL) == (MCUJS_USB_CDC != 0));
    assert((strstr(usb->json, "\"msc\"") != NULL) == (MCUJS_USB_MSC != 0));
    assert((strstr(usb->json, "\"keyboardHid\"") != NULL) ==
           (MCUJS_USB_KEYBOARD_HID != 0));
    assert((strstr(usb->json, "\"mouseHid\"") != NULL) ==
           (MCUJS_USB_MOUSE_HID != 0));
    assert((MCUJS_FEATURE_KEYBOARD != 0) == (MCUJS_USB_KEYBOARD_HID != 0));
    assert((MCUJS_FEATURE_MOUSE != 0) == (MCUJS_USB_MOUSE_HID != 0));

#if MCUJS_REGISTRY_ONBOARD_NEOPIXEL
    assert(registry->onboard_neopixel);
#else
    assert(!registry->onboard_neopixel);
#endif

    puts("runtime registry native test passed");
    return 0;
}
