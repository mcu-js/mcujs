#ifndef MCUJS_FATAL_RECOVERY_H
#define MCUJS_FATAL_RECOVERY_H

/* Native reset handoff, not a JavaScript API. Call once at boot, before JS.
 * Consumes a warm-reset marker; does not allocate or write either filesystem. */
void mcujs_fatal_recovery_init(void);
/* -1 on ordinary boot; otherwise the fatal engine code from the previous boot. */
int mcujs_fatal_recovery_code(void);

#endif
