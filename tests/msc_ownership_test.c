#include "msc_ownership.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

typedef enum {
    FAKE_DEVICE_OWNED = 0,
    FAKE_HOST_OWNED,
    FAKE_FAULT,
} fake_state_t;

typedef struct {
    fake_state_t state;
    unsigned open_handles;
    unsigned flush_calls;
    unsigned unmount_calls;
    unsigned mount_calls;
    unsigned format_calls;
    fs_result_t flush_result;
    fs_result_t unmount_result;
    fs_result_t mount_result;
} fake_storage_t;

static fs_result_t fake_begin_host_access(void *context) {
    fake_storage_t *storage = context;
    if (storage->state == FAKE_FAULT) return FS_ERROR_IO;
    if (storage->state == FAKE_HOST_OWNED) return FS_OK;
    if (storage->open_handles != 0) return FS_ERROR_BUSY;

    storage->flush_calls++;
    if (storage->flush_result != FS_OK) {
        storage->state = FAKE_FAULT;
        return storage->flush_result;
    }
    storage->unmount_calls++;
    if (storage->unmount_result != FS_OK) {
        storage->state = FAKE_FAULT;
        return storage->unmount_result;
    }
    storage->state = FAKE_HOST_OWNED;
    return FS_OK;
}

static fs_result_t fake_end_host_access(void *context) {
    fake_storage_t *storage = context;
    if (storage->state == FAKE_FAULT) return FS_ERROR_IO;
    if (storage->state == FAKE_DEVICE_OWNED) return FS_OK;

    storage->mount_calls++;
    if (storage->mount_result != FS_OK) {
        storage->state = FAKE_FAULT;
        return storage->mount_result;
    }
    storage->state = FAKE_DEVICE_OWNED;
    return FS_OK;
}

static bool fake_host_owned(void *context) {
    return ((fake_storage_t *)context)->state == FAKE_HOST_OWNED;
}

static fake_storage_t fresh_storage(void) {
    return (fake_storage_t) {
        .state = FAKE_DEVICE_OWNED,
        .flush_result = FS_OK,
        .unmount_result = FS_OK,
        .mount_result = FS_OK,
    };
}

static mcujs_msc_ownership_hooks_t hooks_for(fake_storage_t *storage) {
    return (mcujs_msc_ownership_hooks_t) {
        .context = storage,
        .begin_host_access = fake_begin_host_access,
        .end_host_access = fake_end_host_access,
        .host_owned = fake_host_owned,
    };
}

static void test_load_waits_for_open_handles_then_claims(void) {
    fake_storage_t storage = fresh_storage();
    storage.open_handles = 1;
    mcujs_msc_ownership_t ownership;
    mcujs_msc_ownership_init(&ownership);
    mcujs_msc_ownership_hooks_t hooks = hooks_for(&storage);

    assert(!mcujs_msc_ownership_expose(&ownership, &hooks));
    assert(storage.state == FAKE_DEVICE_OWNED);
    assert(storage.flush_calls == 0);
    assert(!mcujs_msc_ownership_media_ready(&ownership));

    storage.open_handles = 0;
    mcujs_msc_ownership_task(&ownership, &hooks);
    assert(storage.state == FAKE_HOST_OWNED);
    assert(storage.flush_calls == 1);
    assert(storage.unmount_calls == 1);
    assert(mcujs_msc_ownership_media_ready(&ownership));
}

static void test_eject_waits_for_inflight_io_then_remounts(void) {
    fake_storage_t storage = fresh_storage();
    mcujs_msc_ownership_t ownership;
    mcujs_msc_ownership_init(&ownership);
    mcujs_msc_ownership_hooks_t hooks = hooks_for(&storage);
    assert(mcujs_msc_ownership_expose(&ownership, &hooks));
    assert(mcujs_msc_ownership_begin_io(&ownership, &hooks));

    mcujs_msc_ownership_event(&ownership, MCUJS_MSC_EVENT_EJECT);
    assert(!mcujs_msc_ownership_media_ready(&ownership));
    mcujs_msc_ownership_task(&ownership, &hooks);
    assert(storage.state == FAKE_HOST_OWNED);
    assert(storage.mount_calls == 0);

    mcujs_msc_ownership_end_io(&ownership);
    mcujs_msc_ownership_task(&ownership, &hooks);
    assert(storage.state == FAKE_DEVICE_OWNED);
    assert(storage.mount_calls == 1);
    assert(!mcujs_msc_ownership_media_ready(&ownership));
}

static void test_last_request_wins(void) {
    fake_storage_t storage = fresh_storage();
    mcujs_msc_ownership_t ownership;
    mcujs_msc_ownership_init(&ownership);
    mcujs_msc_ownership_hooks_t hooks = hooks_for(&storage);

    mcujs_msc_ownership_event(&ownership, MCUJS_MSC_EVENT_LOAD);
    mcujs_msc_ownership_event(&ownership, MCUJS_MSC_EVENT_EJECT);
    mcujs_msc_ownership_task(&ownership, &hooks);
    assert(storage.state == FAKE_DEVICE_OWNED);
    assert(storage.unmount_calls == 0);

    mcujs_msc_ownership_event(&ownership, MCUJS_MSC_EVENT_EJECT);
    mcujs_msc_ownership_event(&ownership, MCUJS_MSC_EVENT_LOAD);
    mcujs_msc_ownership_task(&ownership, &hooks);
    assert(storage.state == FAKE_HOST_OWNED);
    assert(storage.unmount_calls == 1);
}

static void test_reset_suspend_and_resume_preserve_host_ownership(void) {
    fake_storage_t storage = fresh_storage();
    mcujs_msc_ownership_t ownership;
    mcujs_msc_ownership_init(&ownership);
    mcujs_msc_ownership_hooks_t hooks = hooks_for(&storage);
    assert(mcujs_msc_ownership_expose(&ownership, &hooks));

    mcujs_msc_ownership_event(&ownership, MCUJS_MSC_EVENT_RESET);
    mcujs_msc_ownership_event(&ownership, MCUJS_MSC_EVENT_SUSPEND);
    mcujs_msc_ownership_event(&ownership, MCUJS_MSC_EVENT_RESUME);
    mcujs_msc_ownership_task(&ownership, &hooks);
    assert(storage.state == FAKE_HOST_OWNED);
    assert(storage.mount_calls == 0);
    assert(mcujs_msc_ownership_media_ready(&ownership));
}

static void test_detach_returns_storage_to_device(void) {
    fake_storage_t storage = fresh_storage();
    mcujs_msc_ownership_t ownership;
    mcujs_msc_ownership_init(&ownership);
    mcujs_msc_ownership_hooks_t hooks = hooks_for(&storage);
    assert(mcujs_msc_ownership_expose(&ownership, &hooks));

    mcujs_msc_ownership_event(&ownership, MCUJS_MSC_EVENT_DETACH);
    mcujs_msc_ownership_task(&ownership, &hooks);
    assert(storage.state == FAKE_DEVICE_OWNED);
    assert(storage.mount_calls == 1);
    assert(!mcujs_msc_ownership_media_ready(&ownership));
}

static void test_transition_failures_enter_data_preserving_fault(void) {
    fake_storage_t storage = fresh_storage();
    storage.unmount_result = FS_ERROR_IO;
    mcujs_msc_ownership_t ownership;
    mcujs_msc_ownership_init(&ownership);
    mcujs_msc_ownership_hooks_t hooks = hooks_for(&storage);

    assert(!mcujs_msc_ownership_expose(&ownership, &hooks));
    assert(storage.state == FAKE_FAULT);
    assert(storage.format_calls == 0);
    assert(!mcujs_msc_ownership_media_ready(&ownership));

    storage = fresh_storage();
    hooks = hooks_for(&storage);
    mcujs_msc_ownership_init(&ownership);
    assert(mcujs_msc_ownership_expose(&ownership, &hooks));
    storage.mount_result = FS_ERROR_IO;
    mcujs_msc_ownership_event(&ownership, MCUJS_MSC_EVENT_EJECT);
    mcujs_msc_ownership_task(&ownership, &hooks);
    assert(storage.state == FAKE_FAULT);
    assert(storage.format_calls == 0);
    assert(!mcujs_msc_ownership_media_ready(&ownership));
}

int main(void) {
    test_load_waits_for_open_handles_then_claims();
    test_eject_waits_for_inflight_io_then_remounts();
    test_last_request_wins();
    test_reset_suspend_and_resume_preserve_host_ownership();
    test_detach_returns_storage_to_device();
    test_transition_failures_enter_data_preserving_fault();
    puts("MSC ownership state-machine test passed");
    return 0;
}
