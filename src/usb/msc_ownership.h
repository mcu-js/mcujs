#ifndef MCUJS_MSC_OWNERSHIP_H
#define MCUJS_MSC_OWNERSHIP_H

#include "fs.h"

#include <stdatomic.h>
#include <stdbool.h>

typedef enum {
    MCUJS_MSC_EVENT_LOAD = 0,
    MCUJS_MSC_EVENT_EJECT,
    MCUJS_MSC_EVENT_RESET,
    MCUJS_MSC_EVENT_SUSPEND,
    MCUJS_MSC_EVENT_RESUME,
    MCUJS_MSC_EVENT_DETACH,
} mcujs_msc_event_t;

typedef enum {
    MCUJS_MSC_OWNER_REQUEST_NONE = 0,
    MCUJS_MSC_OWNER_REQUEST_DEVICE,
    MCUJS_MSC_OWNER_REQUEST_HOST,
} mcujs_msc_owner_request_t;

typedef struct {
    atomic_bool media_ready;
    atomic_uint io_inflight;
    atomic_int owner_request;
} mcujs_msc_ownership_t;

typedef struct {
    void *context;
    fs_result_t (*begin_host_access)(void *context);
    fs_result_t (*end_host_access)(void *context);
    bool (*host_owned)(void *context);
} mcujs_msc_ownership_hooks_t;

void mcujs_msc_ownership_init(mcujs_msc_ownership_t *ownership);
bool mcujs_msc_ownership_expose(mcujs_msc_ownership_t *ownership,
                                const mcujs_msc_ownership_hooks_t *hooks);
void mcujs_msc_ownership_event(mcujs_msc_ownership_t *ownership,
                               mcujs_msc_event_t event);
void mcujs_msc_ownership_task(mcujs_msc_ownership_t *ownership,
                              const mcujs_msc_ownership_hooks_t *hooks);
bool mcujs_msc_ownership_begin_io(mcujs_msc_ownership_t *ownership,
                                  const mcujs_msc_ownership_hooks_t *hooks);
void mcujs_msc_ownership_end_io(mcujs_msc_ownership_t *ownership);
bool mcujs_msc_ownership_media_ready(const mcujs_msc_ownership_t *ownership);

#endif /* MCUJS_MSC_OWNERSHIP_H */
