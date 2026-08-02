#include "msc_ownership.h"

#include <stddef.h>

static bool hooks_valid(const mcujs_msc_ownership_hooks_t *hooks) {
    return hooks != NULL && hooks->begin_host_access != NULL &&
           hooks->end_host_access != NULL && hooks->host_owned != NULL;
}

void mcujs_msc_ownership_init(mcujs_msc_ownership_t *ownership) {
    if (ownership == NULL) return;
    atomic_init(&ownership->media_ready, false);
    atomic_init(&ownership->io_inflight, 0);
    atomic_init(&ownership->owner_request, MCUJS_MSC_OWNER_REQUEST_NONE);
}

bool mcujs_msc_ownership_expose(mcujs_msc_ownership_t *ownership,
                                const mcujs_msc_ownership_hooks_t *hooks) {
    if (ownership == NULL || !hooks_valid(hooks)) return false;
    atomic_store(&ownership->owner_request, MCUJS_MSC_OWNER_REQUEST_HOST);
    mcujs_msc_ownership_task(ownership, hooks);
    return mcujs_msc_ownership_media_ready(ownership);
}

void mcujs_msc_ownership_event(mcujs_msc_ownership_t *ownership,
                               mcujs_msc_event_t event) {
    if (ownership == NULL) return;
    switch (event) {
        case MCUJS_MSC_EVENT_LOAD:
            atomic_store(&ownership->owner_request, MCUJS_MSC_OWNER_REQUEST_HOST);
            break;
        case MCUJS_MSC_EVENT_EJECT:
        case MCUJS_MSC_EVENT_DETACH:
            atomic_store(&ownership->media_ready, false);
            atomic_store(&ownership->owner_request, MCUJS_MSC_OWNER_REQUEST_DEVICE);
            break;
        case MCUJS_MSC_EVENT_RESET:
        case MCUJS_MSC_EVENT_SUSPEND:
        case MCUJS_MSC_EVENT_RESUME:
            /* A transport reset or suspend is not a confirmed media eject. */
            break;
    }
}

void mcujs_msc_ownership_task(mcujs_msc_ownership_t *ownership,
                              const mcujs_msc_ownership_hooks_t *hooks) {
    if (ownership == NULL || !hooks_valid(hooks)) return;
    mcujs_msc_owner_request_t request =
        (mcujs_msc_owner_request_t)atomic_load(&ownership->owner_request);

    if (request == MCUJS_MSC_OWNER_REQUEST_DEVICE) {
        atomic_store(&ownership->media_ready, false);
        if (atomic_load(&ownership->io_inflight) != 0) return;

        fs_result_t result = FS_OK;
        if (hooks->host_owned(hooks->context)) {
            result = hooks->end_host_access(hooks->context);
        }
        if (result != FS_OK) return;

        int expected = MCUJS_MSC_OWNER_REQUEST_DEVICE;
        (void)atomic_compare_exchange_strong(
            &ownership->owner_request, &expected, MCUJS_MSC_OWNER_REQUEST_NONE);
        return;
    }

    if (request == MCUJS_MSC_OWNER_REQUEST_HOST) {
        if (!hooks->host_owned(hooks->context)) {
            fs_result_t result = hooks->begin_host_access(hooks->context);
            if (result != FS_OK) {
                atomic_store(&ownership->media_ready, false);
                return;
            }
        }
        if (!hooks->host_owned(hooks->context)) {
            atomic_store(&ownership->media_ready, false);
            return;
        }

        atomic_store(&ownership->media_ready, true);
        int expected = MCUJS_MSC_OWNER_REQUEST_HOST;
        (void)atomic_compare_exchange_strong(
            &ownership->owner_request, &expected, MCUJS_MSC_OWNER_REQUEST_NONE);
    }
}

bool mcujs_msc_ownership_begin_io(mcujs_msc_ownership_t *ownership,
                                  const mcujs_msc_ownership_hooks_t *hooks) {
    if (ownership == NULL || !hooks_valid(hooks) ||
        !atomic_load(&ownership->media_ready) ||
        atomic_load(&ownership->owner_request) == MCUJS_MSC_OWNER_REQUEST_DEVICE) {
        return false;
    }

    atomic_fetch_add(&ownership->io_inflight, 1);
    if (!atomic_load(&ownership->media_ready) ||
        atomic_load(&ownership->owner_request) == MCUJS_MSC_OWNER_REQUEST_DEVICE ||
        !hooks->host_owned(hooks->context)) {
        atomic_fetch_sub(&ownership->io_inflight, 1);
        return false;
    }
    return true;
}

void mcujs_msc_ownership_end_io(mcujs_msc_ownership_t *ownership) {
    if (ownership == NULL) return;
    unsigned inflight = atomic_load(&ownership->io_inflight);
    if (inflight != 0) atomic_fetch_sub(&ownership->io_inflight, 1);
}

bool mcujs_msc_ownership_media_ready(const mcujs_msc_ownership_t *ownership) {
    return ownership != NULL && atomic_load(&ownership->media_ready);
}
