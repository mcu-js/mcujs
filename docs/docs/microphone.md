---
sidebar_position: 18
---

# Microphone capture

`devices.microphone` exposes explicitly started, bounded PCM recording on the
Waveshare ESP32-S3-ePaper-1.54 **V2**. Other board profiles omit the capability.
This is an input-only slice, not voice recognition or a background audio service.

```js
var microphone = require('devices').microphone;
if (!microphone) throw new Error('No configured microphone');
var input = microphone.open(); // exclusive ownership only; microphone stays off
input.record({ duration: 1000 }).then(function (pcm) {
  // Uint8Array: signed 16-bit little-endian mono PCM, 16000 samples/second.
  // No file write or network transfer happens automatically.
  console.log('Captured bytes:', pcm.length);
  input.close();
}, function (error) {
  input.close();
  console.log(error.code || error.message);
});
```

## Contract

- Discovery and `open()` do not start audio capture. `open()` accepts no options.
- One live handle and one pending recording. No mixing, queue or shared readers.
- `record({ duration, signal? })` returns a Promise for a `Uint8Array`.
- `duration` is required: an integer from **20 through 1000 milliseconds**.
  It bounds the native capture window; the returned byte count can be shorter
  than nominal duration. Actual samples are returned without padding.
- Fixed **16000 Hz**, **mono**, **PCM signed 16-bit little-endian**. No WAV header.
- At most **32000 PCM bytes** per operation, held in bounded native memory before
  transfer to a JavaScript-owned buffer. This is not unbounded streaming.
- Setup precedes the requested sample window; Promise completion also depends on
  JS servicing. Native capture shutdown does **not** depend on JS timers running.
- `stop()` aborts the pending Promise with `ABORT_ERR` and discards partial PCM.
  Calling it on an open, inactive handle is harmless.
- `close()` stops capture and relinquishes ownership; repeated close is harmless.
  A closed handle cannot record again; open a fresh handle.
- AbortSignal cancellation discards the recording; an already-aborted signal
  never starts the input path. Unknown options and invalid durations are rejected.
- `microphone.state` is `idle` or `busy` ownership, **not a listening indicator**.
  A handle is `open`, `recording` (operation pending), or `closed`.
- No gain control, playback, filesystem dependency, networking, automatic uploads,
  boot-time recording or persistent background listening is introduced.

```js
var controller = new (require('events').AbortController)();
var input = require('devices').microphone.open();
var pending = input.record({ duration: 1000, signal: controller.signal });
controller.abort();
pending.catch(function (error) {
  console.log(error.name); // AbortError
  input.close();
});
```

## Hardware source and privacy boundary

The official [V2 documentation](https://docs.waveshare.com/ESP32-S3-ePaper-1.54)
identifies the ESP32-S3-PICO-1-N8R8 variant and onboard ES8311 microphone codec.
The [vendor source](https://github.com/waveshareteam/ESP32-S3-ePaper-1.54/tree/9957d0f4fc7cd40d1d42880cb1b74a8d6782a6c2)
provides `02_Example/XiaoZhi/V2-Src-XiaoZhi.zip`, whose board `config.h` specifies
MCLK GPIO14, BCLK GPIO15, WS GPIO38, input GPIO16, control SDA GPIO47/SCL GPIO48,
and audio power GPIO42. The Arduino audio example independently agrees.

The codec uses I2C address **0x18** (the vendor's 8-bit address is 0x30).
GPIO42 is active-low audio power; GPIO46 is the speaker amplifier enable and
must remain low in this input-only slice. These are private board-driver details,
never arguments to applications. This is **not** Sticky's PDM microphone, and
external USB/webcam recordings are not evidence of board-microphone capture.

The native adapter owns input shutdown, errors and cleanup. Reset/context cleanup
must quiesce capture before destroying its resources. Failure paths must close
the input or establish the checked rail-off boundary; they must not return a
successful recording while privacy closure is unconfirmed.

## Hardware acceptance

Firmware `aeb38dc` was built in the pinned ESP SDK container and exercised on
an identified ePaper V2. Two independent full-flash backups matched before the
application-only write; readback proved the app bytes and preservation of the
bootloader, partition table, NVS, PHY and FFAT filesystem. A normal physical
restart returned the expected runtime; automatic ROM reset did not qualify.

- A requested one-second capture returned **31744 bytes / 15872 samples** and
  released microphone ownership to idle.
- The nearby RP2350 2.8 speaker played the existing synthetic chime. The ePaper's
  returned PCM contained **625, 1000 and 1500 Hz in order**, verified by windowed
  FFT against that input. This capture came from the board's ES8311 microphone,
  not a USB webcam. It is automated acoustic evidence, not an operator listening
  assessment or a frequency-response calibration.
- Stop and close rejected pending capture and discarded its samples. Abort and
  already-aborted signals rejected with their AbortError reason. All ended idle.
- Five physical close/reopen recordings completed within their requested bounds.
- A 200 ms recording while JavaScript was blocked for 1200 ms returned only
  **6144 bytes**, then closed normally. Native fault tests separately exercised
  the independent rail cutoff, including a blocked reader.
- Both microphone and test speaker were left idle; no autorun or recording files
  were added to either board. The test waveform was explicitly retrieved to the
  test host for analysis, not uploaded by the MCU.js API.

The host suite passed **241 tests**; native tests cover ownership, bounded PCM,
39 setup fault points, allocation/task failure, missing input, deadline handling,
stop/close races, mute fallback, stale tokens and the privacy-fault latch.
Reset-during-capture, long-duration soak, broader microphone hardware and
production audio-quality calibration are not qualified by this first slice.
