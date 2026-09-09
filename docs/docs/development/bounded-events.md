---
sidebar_position: 13
---

# Bounded events and cancellation (development)

This is a second, limited slice of issue #6, stacked on
[cooperative Promise jobs](./promise-jobs.md). It supplies a shared software
event surface. It does **not** add device handles, button or touch drivers,
native event queues, or asynchronous hardware operations.

## Start here

The classes are module exports, not new globals. The firmware packages the
source and evaluates it only on the first `require('events')`. Normal repeated
imports return the same module and constructors. Use
`require('mcujs:module').has('events')` to check availability.

```js
const { Event, EventTarget, AbortController } = require('events');
const target = new EventTarget();
const controller = new AbortController();

function changed(event) {
  console.log(event.type);
}

target.addEventListener('change', changed, { signal: controller.signal });
target.dispatchEvent(new Event('change')); // prints change, synchronously
controller.abort();                       // removes the subscription
target.dispatchEvent(new Event('change')); // no callback
console.log(controller.signal.aborted);    // true
```

Despite the module name, this is **not Node's `events` module or EventEmitter**.
It is a documented, flat subset of web-shaped interfaces. There is no
`node:events` alias, DOM tree, capture/bubble phase, `CustomEvent`, `onabort`
property, `AbortSignal.any()` or `AbortSignal.timeout()`.

## Supported contract

- `new Event(type, { cancelable: false })`: the type is converted to a string.
  `type`, `cancelable`, `defaultPrevented`, `target` and `currentTarget` are
  read-only properties. `target` retains the last dispatch target;
  `currentTarget` is `null` outside dispatch.
- `event.preventDefault()` sets `defaultPrevented` only for a cancelable event.
  `event.stopImmediatePropagation()` skips the remaining listeners in the
  current dispatch. Its stop flag is cleared after dispatch.
- `new EventTarget()` creates an independent target.
- `target.addEventListener(type, callback, { once, signal })` accepts a function
  or an object with `handleEvent(event)`. Function listeners receive the target
  as `this`; object listeners receive their listener object as `this`.
  A null or undefined listener does nothing. Duplicate type/callback pairs
  do not create another registration or change its original options.
- `target.removeEventListener(type, callback)` removes that exact registration.
  There is no subscription return value. Removal is idempotent.
- `target.dispatchEvent(event)` is synchronous. It returns `false` if the event
  has been canceled, otherwise `true`. Only this module's Event instances are
  accepted. Dispatch of an event that is already being dispatched throws an
  Error named `InvalidStateError`.
- `new AbortController()` owns one stable `signal`. Calling `abort(reason)`
  sets `signal.aborted` and `signal.reason`, removes signal-bound listeners,
  then dispatches one `abort` event. The first reason wins. Repeated calls do
  nothing. An omitted or undefined reason becomes an Error named `AbortError`.
- `signal.throwIfAborted()` throws the stored reason, including non-Error
  reasons. Signals are created by AbortController, not `new AbortSignal()`.
  An already-aborted signal prevents a new listener from being registered.

`capture: false` and `passive: false` are accepted for registration; true values
throw RangeError. Removal accepts only `capture: false` (or boolean `false`).
Event construction accepts false `bubbles` and `composed` values, but rejects
true values. Unknown enumerable option keys throw RangeError rather than being
silently accepted. Invalid receivers, callbacks, option types and signals throw
TypeError. Browser/Node signals from another implementation are not accepted.

## Mutation, errors and cancellation

Dispatch visits a snapshot in registration order. A listener removed before
its turn is skipped. A listener added during dispatch waits until a subsequent
dispatch. A `once` listener is removed **before** it runs, including from its
signal's subscription list, so nested dispatch cannot call it again.

Listener exceptions are reported through `console.error`; later listeners
still run. Even a failing error reporter cannot bypass dispatch cleanup.
An exception from the dispatch machinery itself, such as a capacity error,
is not a listener exception at that level.

Cancellation cleanup is separate from public `abort` listeners. Dispatching a
synthetic event named `abort` does not change `signal.aborted` or remove its
subscriptions. A real abort removes subscriptions before any user listener can
throw or stop further notification. Explicit listener removal and `once` also
release their cancellation subscriptions.

**Only listener registration accepts cancellation in this slice.** Existing
filesystem, timer and hardware APIs do not gain signal arguments or become
asynchronous. Application-written Promise wrappers must explicitly connect and
remove their own completion/abort listeners; `await` alone does not cancel work.

## Fixed limits and overflow

`require('events').limits` reports a frozen object:

- `listeners: 16`: total registrations per EventTarget, across all event types.
- `subscriptions: 16`: registrations attached to one AbortSignal, across targets.
  The signal's own public event listeners have the separate listener limit.
- `depth: 4`: simultaneous nested dispatches, shared across all targets.
- `callbacks: 32`: listener invocation attempts across one outer dispatch and
  all its nested dispatches.

A registration that exceeds a limit throws RangeError and leaves existing
registrations intact. Duplicate registrations do not consume another slot.
A dispatch that would exceed depth or callback limits throws RangeError; pending
callbacks are **not queued or retried**. If that error reaches a calling listener,
it is reported under the listener-error rule above. Target state and the shared
dispatch depth are restored on every exit.

An abort that encounters a dispatch limit has **already committed cancellation**:
its reason is stored and its subscriptions have been removed. Some public abort
listeners may not be notified, and repeating `abort()` does not retry them.

These are registration and invocation-count limits, not time or total-heap
limits. One callback can still block or allocate too much memory. The module
adds no native event queue, coalescing, queue backpressure, callback preemption,
recoverable out-of-memory handling or unhandled-rejection reporting. The
Promise-job and timer checkpoints from the parent slice are unchanged.

## Memory and lifecycle

State is non-enumerable and instance-owned. This avoids the pinned JerryScript
3.0 weak-container backing buffers, which retain empty slots after deleted or
collected entries. Normal GC can reclaim discarded instances and their cycles.
These internals provide application encapsulation, not a hostile-code security
boundary; do not reflect on or mutate them.

File-module cache clearing keeps built-in identity intact. Engine teardown
releases both file and built-in module references before `jerry_cleanup()`.
A fresh VM creates fresh constructors and cancellation state.

## Verification and manual gate

`node --test tests/events.test.js` checks the public contract. The portable
conformance runner also replays those same cases through the production
`require('events')` factory in real, pinned JerryScript with a **64KiB heap**,
for full RP, constrained RP2350 and ESP32 binding configurations. Only hardware
and unrelated native modules are stubbed; the console uses its production
formatter with a stubbed transport.

The native checks include ten repeated workloads with bounded retained heap
after GC, cancellation-to-Promise rejection, stable identity after file-cache
clearing, and complete VM teardown/reinitialization. The production engine and
both timer backends also verify that module cleanup runs before VM destruction.

These checks do not prove physical input, USB responsiveness, display behavior,
watchdog behavior or cold boot. After tests and exact-candidate board builds,
manually run the example above on approved RP2040, RP2350 and ESP32 hardware,
then check Promise/timer progress and REPL access. Record the board, firmware
build ID and result. Do not flash or change startup files without separate
approval and the device's recovery/preservation plan.
