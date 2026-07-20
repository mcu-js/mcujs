---
sidebar_position: 2
---

# Error reference

MCU.js 0.2 separates programming mistakes from failures of a supported
operation. Portable code should inspect the JavaScript class for argument
mistakes and the stable `code` property for operational failures. Do not branch
on a board or chip name.

The frozen schema defines this contract for every portable module. The current
native rollout enforces it through the RP2040/RP2350 and ESP32-S3 `gpio`, `i2c`,
and `pwm` factories and through shared filesystem failures. Other native module
handlers are not covered by this implementation slice yet; their presence must
not be treated as evidence that their legacy validation and errors have already
been migrated.

## Programming errors

Programming mistakes use JavaScript built-in errors and do not carry an
operational `code`:

| Class | When it is used |
|---|---|
| `TypeError` | A required argument is missing, a value has the wrong JavaScript type, or a numeric value is `NaN`, `Infinity`, or `-Infinity`. |
| `RangeError` | An integer is fractional or outside the native integer range; a finite value, enum, route, byte, array length, or other value violates a documented constraint. |

In the migrated handlers, validation happens before native conversion or
dynamic resource-state checks. Numbers are not parsed from strings. Booleans do
not accept truthy or falsy substitutes. Integers and bytes are not truncated or
wrapped. Enum strings are case-sensitive unless an API explicitly documents
normalization. Arrays above a capability limit throw instead of being
shortened. Indexed byte-array accessor exceptions propagate unchanged.

```js
(function () {
  var pwm = require('pwm');
  var pin = require('board').pins.LED;

  try {
    pwm.setDuty(pin, 1.5);
  } catch (error) {
    if (error instanceof RangeError) {
      console.log('PWM duty must be a finite ratio from 0 to 1');
      return;
    }
    throw error;
  }
}());
```

## Operational errors

An operational failure means the API exists, so its module or method remains
present. The error is an `Error` with one of these stable identities:

| `code` | `name` | Meaning |
|---|---|---|
| `ERR_NOT_SUPPORTED` | `NotSupportedError` | Every supplied argument passes its public constraints, but the backend cannot implement that valid configuration or exact representation. |
| `EBUSY` | `ResourceBusyError` | A pin, bus, timer, filesystem, or other resource is owned, uninitialized, or temporarily unavailable. |
| `ERR_RESOURCE_EXHAUSTED` | `ResourceExhaustedError` | A finite hardware pool, such as channels or timers, has no free entry. |
| `ENXIO` | `Error` | An external device absence or NACK can be distinguished from other I/O failures. |
| `EIO` | `Error` | A native driver or hardware I/O operation failed and no more specific portable code applies. |

`ERR_NOT_SUPPORTED` is not a replacement for `RangeError`. For example, an SPI
mode absent from the capability descriptor is outside the public range and
throws `RangeError`. A mode and frequency that are each advertised but cannot
be represented together may throw `ERR_NOT_SUPPORTED`.

```js
(function () {
  var boardApi = require('board');
  var modules = require('mcujs:module');

  if (!modules.has('i2c')) {
    console.log('I2C is not compiled into this firmware');
    return;
  }

  var i2c = require('i2c');
  var caps = boardApi.capability('i2c');

  try {
    var route = caps.defaultRoute;
    i2c.init(route.bus, route.sda, route.scl, 400000);
    i2c.read(caps.defaultBus, 0x50, 1);
  } catch (error) {
    switch (error.code) {
      case 'EBUSY':
        console.log('The selected resource is currently unavailable');
        break;
      case 'ERR_RESOURCE_EXHAUSTED':
        console.log('Release another channel or timer before retrying');
        break;
      case 'ENXIO':
        console.log('The external device did not acknowledge');
        break;
      case 'EIO':
        console.log('The native I/O operation failed');
        break;
      default:
        throw error;
    }
  }
}());
```

## Diagnostic properties

Operational errors may add these properties:

| Property | Type | Meaning |
|---|---|---|
| `resource` | string | Portable resource category, such as `gpio`, `i2c`, or `filesystem`. |
| `pin` | number | The validated MCU pin involved in the failure. |
| `owner` | string | The current owner or subsystem when it is safe and useful to report. |
| `bus` | number | The validated bus index involved in the operation. |
| `limit` | number | The relevant advertised or finite resource limit. |
| `nativeCode` | number | Backend-native diagnostic value for logs and debugging. |

These fields are optional diagnostics. Portable control flow must branch on
`code`, because a backend may not be able to identify the owner, native code,
or exact failed resource. `nativeCode` is intentionally not a portable API and
must not be used to select RP or ESP behavior.

For example, the ESP32-S3 filesystem reports host ownership as
`ResourceBusyError` with `code === 'EBUSY'`, `resource === 'filesystem'`, and
`owner === 'usb-host'`. Ejecting the MCUJS volume transfers ownership back to
the runtime; the filesystem API remains present throughout the transition.

## Unsupported APIs are absent

A module or optional method that is not implemented by the current firmware is
absent rather than represented by `ERR_NOT_SUPPORTED`. Detect modules through
`require('mcujs:module').has(name)` and optional methods through property
existence. Reserve operational errors for calls to APIs that are actually
present.

See the [portable API contract](./portable-api-contract.md) for capability and
availability rules, and [Migrating from MCU.js 0.1 to 0.2](../migration/0.2.md)
for behavior that became strict in 0.2.
