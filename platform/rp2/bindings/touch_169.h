/* Private qualified onboard CST816-family protocol (ID 0xB5), not a public bus API. */
#ifndef MCUJS_TOUCH_169_H
#define MCUJS_TOUCH_169_H
#include <stdbool.h>
/* open: 0 success, 1 busy, 2 I/O/identity failure. sample: false on failure. */
int mcujs_touch_169_open(void);
bool mcujs_touch_169_sample(bool horizontal, bool *pressed, int *x, int *y);
void mcujs_touch_169_close(void);
const char *mcujs_touch_169_error(void);
#endif
