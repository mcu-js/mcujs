---
sidebar_position: 12
---

# Cooperative Promise jobs (development)

This is the first scheduling slice of issue #6, stacked on the ESP32 PSRAM
heap work. It is not a release or a complete asynchronous device API.

## Application behavior

Promise reactions and continuations inside `async` functions now run when the
runtime returns to its main loop. They do not run inline in the initial script.
CommonJS scripts and the REPL do not gain top-level `await`.

```js
Promise.resolve(42).then(value => console.log(value));

(async function () {
  const value = await new Promise(resolve => {
    setTimeout(() => resolve(7), 100);
  });
  console.log(value);
})().catch(error => console.log(error.message));
```

Use `.catch()` or `try`/`catch` inside an async function. Promise rejection is
not a synchronous `js_engine_exec()` failure. Unhandled-rejection events and
reporting are not implemented by this slice; a rejection without a handler can
remain silent. A returned engine-level job exception is logged with CRLF and
released, without discarding other queued jobs.

## Scheduling contract

Both RP and ESP loop through the existing `js_engine_process_timers()` seam:

1. Run up to **16 queued Promise jobs** in FIFO order.
2. Run one platform timer pass (at most 16 timer callbacks).
3. Run up to **16 more Promise jobs**, including those scheduled by timers.
4. Return to platform services: REPL/USB, watchdog/boot tasks and display work
   in their existing platform order.

Remaining jobs stay in the engine queue. A timer batch is not interrupted by
a microtask checkpoint after each callback. Thus a Promise queued by timer A
can run after another due timer B. A long Promise chain can span main-loop
turns, with timers and input serviced between batches.

**This is an intentional cooperative MCU scheduling policy, not the browser
or Node drain-to-empty microtask-ordering contract.** It trades exact host
ordering for progress on a constrained device. Don't depend on Node/browser
ordering beyond the documented FIFO and checkpoints.

The budget counts jobs, not milliseconds or instructions. An individual
callback, native operation or synchronous loop can still block. There is no
preemption. The queue consumes the existing JS heap; this slice does not add
an independent queue-length cap, backpressure or recoverable out-of-memory
handling. A workload that creates jobs faster than it consumes them can still
exhaust memory. Neither a Promise wrapper nor `await` makes synchronous file
I/O asynchronous.

Timer callbacks retain their function independently while running, so clearing
and reusing their slot is safe. Engine cleanup releases active timer references
before JerryScript teardown; queued jobs are discarded with their context.

## Engine integration

Pinned JerryScript 3.0.0 exposes `jerry_run_jobs()`, which drains until empty and
can starve platform work with a self-replenishing chain. The build therefore
stages a private source copy using `scripts/prepare-jerry-jobs.py` and adds the
small **MCU.js-only** C API:

```c
jerry_value_t mcujs_jerry_run_jobs(uint32_t max_jobs, bool *pending_p);
```

A zero budget queries pending state without executing work. Other budgets
execute complete jobs and retain the rest in FIFO order. `pending_p` is required.
The normal upstream `jerry_run_jobs()` keeps its drain-to-empty behavior. This
extension is not presented as an upstream JerryScript API.

The four edited upstream files are checked against pinned SHA-256 digests;
changed versions fail configuration instead of receiving a guessed patch.
The SDK checkout is never modified. RP source builds and ESP amalgamated builds
consume the same staged extension. Old pre-generated RP amalgams are rejected;
use the pinned source build. Reconfiguring an unchanged staging directory is
idempotent; a changed patch or staged source requires a clean build directory.

## Verification and manual gate

`bash scripts/test-promise-jobs.sh` builds real JerryScript with a 64KiB heap and
links the production engine and both production timer backends. Only hardware
time and unrelated binding/module initialization are stubbed. Tests cover:

- The original missing completion (42 remained 0 before the fix).
- Catch handlers, async/await, thenable and async-generator jobs.
- FIFO batches and exact Promise/timer ordering.
- A self-replenishing chain, timer progress and another host evaluation between
  turns; sustained turns with bounded retained heap usage after GC.
- Zero/one-job extension boundaries and unchanged upstream drain-to-empty.
- Callback slot reuse, pending work at cleanup, and engine reinitialization.
- Separate queued reaction, async-function, suspended async-generator and thenable
  lifecycle cases. A host-owned callback counter survives VM teardown; positive
  controls execute each callback, while cleanup must discard the queued repeat
  without running it, and the fresh VM must start empty and accept new work.
  These are public producer-workflow tests, not instrumentation of every internal
  queue-item type or proof of overload/backpressure semantics.
- Pin validation, repeat configure and staged mutation rejection.

Host tests are not physical USB, display, watchdog or cold-boot qualification.
After automated checks and affected-board builds pass, manually test the exact
candidate on RP2040, RP2350 and ESP32 hardware. Record board, artifact/build ID,
pass/fail and limitations. Verify the example above, timer/Promise progress,
REPL commands between turns, filesystem persistence, relevant display work and
recovery. Do not flash or change startup files without the device-specific
approval and preservation plan. Existing safe-boot behavior is unchanged;
completion of an asynchronous startup task is not a new healthy-boot signal.

## Pending-job admission follow-up (#23)

**A queue cap and overload/recovery policy are still unimplemented.** The native
lifecycle tests above are prerequisites, not proof of safe overload handling.

The admission audit uses JerryScript commit
`50200152feb724a74a5f64e44d7885151537cfad` and MCU.js
`62ab0e68e2a4296e64bd52a9ac72ef021eb84b01`. Relevant sources are the pinned
[Jerry queue producers/processors](https://github.com/jerryscript-project/jerryscript/blob/50200152feb724a74a5f64e44d7885151537cfad/jerry-core/ecma/operations/ecma-jobqueue.c),
[Promise objects](https://github.com/jerryscript-project/jerryscript/blob/50200152feb724a74a5f64e44d7885151537cfad/jerry-core/ecma/operations/ecma-promise-object.c),
and [MCU.js engine lifecycle](https://github.com/mcu-js/mcujs/blob/62ab0e68e2a4296e64bd52a9ac72ef021eb84b01/host/engine.c):

- All four enqueue producer families (reaction, async reaction, async-generator
  continuation and thenable assimilation) allocate their job and retain its
  payload before the common `ecma_enqueue_job()` call. Their enqueue interfaces
  return `void`. A drop-on-full check at that common call neither prevents the
  allocation nor propagates a completion policy to the caller.
- Promise settlement can enqueue many reactions, and rejection can enqueue more
  work. Rejecting an excess job is not automatically nonrecursive backpressure.
  Reactions attached to a still-pending Promise and async-generator request tasks
  also retain memory before they become runnable jobs. A runnable-job cap would
  not bound all Promise-related memory or make general OOM recoverable.
- A Jerry abort is not by itself a persistent poisoned-context policy. Some
  Promise paths take the exception and convert it into rejection;
  `jcontext_take_exception()` clears the abort flag too. A bounded native probe
  returning `jerry_throw_abort()` observed different paths: a direct call skipped
  its JS catch/finally, an async-function continuation returned an abort from the
  job runner, while reaction, thenable and async-generator fixtures ran rejection
  handlers. Both production timer backends also continued to a later due timer
  after a native abort. Every tested context still accepted a later evaluation.
  These are injected-abort observations, **not an implemented admission failure**.
- Current job-error handling logs and continues to timers and another batch.
  Ordinary Canvas teardown deliberately calls lifecycle listeners while the VM
  is valid, and error/result formatting can invoke JavaScript. Neither is a
  silent, no-further-callback poisoned-context shutdown path.

A finite host-only branching probe also confirmed the existing gap without
exhausting the heap: after four turns it had executed 128 callbacks and accepted
257, leaving 129 outstanding. A timer ran after the first 16 jobs; stopping the
producer from another evaluation allowed all 257 callbacks to finish in FIFO
order. A separate 200-reaction pending-Promise fixture retained heap while the
runnable queue was empty, then completed all callbacks after settlement. These
characterize the current build, not a promised capacity or hardware result.

The next implementation needs one coherent observable policy, not an enqueue
counter alone. If terminal context cancellation is chosen, prove pre-allocation,
context-local admission/accounting; abort propagation through all producers;
no further user callbacks after failure (including timer, cleanup and diagnostic
paths); fully unwound native-resource release; and explicit fresh-VM recovery.
Use finite real-engine boundary tests, including fulfilled/rejected async paths,
in-flight enqueue, pending collections, excess work and teardown. Preserve normal
FIFO/draining behavior and the upstream API below the chosen limit. No terminal
policy has been selected or approved by this audit; do not stress a live board to
choose one.

The next [bounded events slice](./bounded-events.md) supplies module-scoped
EventTarget and AbortSignal subsets. Queue backpressure, unhandled-rejection
reporting, asynchronous device I/O, touch, sensors, audio and networking remain
later work, not implied by either foundation.
