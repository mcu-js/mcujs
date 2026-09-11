# Configured WAV speaker

Discover support without opening hardware:

```javascript
var speaker = require('devices').speaker;
if (speaker) {
  var output = speaker.open();
  output.play('/app/chime.wav', {volume: 0.25}).then(function () {
    output.close();
    console.log('Playback finished');
  }, function (error) {
    output.close();
    console.log('Playback failed:', error);
  });
}
```

The adapter is configured only on Waveshare RP2350 Touch LCD 2.8. Other boards
omit `devices.speaker`, its capability and private speaker modules. Apps do not
choose pins or inspect board names. No microphone access, capture, codecs,
queue, mixing, resampling or Web Audio is provided.

## Contract

- `speaker.capabilities` is frozen static metadata; descriptor `state` is `idle`
  or `busy`. `open()` takes no arguments, emits no audio, and returns one
  exclusive frozen handle. A second open throws `EBUSY`.
- `play(path, {volume?, signal?})` returns a Promise. The initial format is
  RIFF/WAVE PCM format 1, signed 16-bit little-endian, **mono, 16000 Hz**,
  a 16-byte `fmt ` chunk, block alignment 2 and byte rate 32000. Mono samples
  are duplicated into left/right I2S words. No implicit conversion.
- RIFF length must match the file. Exactly one format and data chunk are
  required. Unknown chunks and odd-byte padding are skipped; data may precede
  format. At most 128 chunks are scanned before output. Files must fit the
  filesystem's signed 32-bit position range. Empty valid data completes silently.
- `volume` is a finite linear sample multiplier in `[0, 1]`, default **0.25**;
  zero streams silently. Multiplication truncates toward zero. It is not a
  calibrated loudness percentage or a guarantee against clipping in hardware.
- Invalid options reject with TypeError/RangeError. Unsupported formats reject
  `ERR_NOT_SUPPORTED`; malformed/over-limit files reject `EINVAL`; missing files
  reject `ENOENT`. Read failure, mid-play truncation or underrun rejects `EIO`.
- One active play per handle. Concurrent play rejects `EBUSY`; nothing queues
  or replaces it. The handle remains owned after completion or a playback error.
- Completion means DMA data has had time to drain from the FIFO/shift register,
  output is silenced and operation resources released, not merely file EOF.
- `signal` is an MCU.js `events.AbortSignal`. Cancellation stops output and
  rejects with its exact reason, independently of ordinary event propagation.
  A pre-aborted signal never starts output.
- `stop()` immediately silences output and rejects pending work with
  `AbortError` / `ABORT_ERR`; stopping an idle open handle is harmless.
- `close()` stops playback and releases ownership; repeated close is harmless.
  Other operations on a closed handle fail `ENXIO`. Handle `state` is `open`,
  `playing` or `closed`; Promise processing may lag physical silence.
- PIO/DMA/alarm/service-timer exhaustion rejects `ERR_RESOURCE_EXHAUSTED` and
  unwinds partial acquisition. VM/timer cleanup stops output before engine teardown.

## Bounded streaming and safety

The native adapter uses the existing filesystem backend and USB ownership gate,
not a whole-file JavaScript buffer. Eject the MCUJS host volume before playback;
USB ownership conflicts reject `EBUSY`. One native file handle, two 512-frame
buffers (4096 output bytes total), and a 1024-byte read scratch buffer bound memory
independently of file duration. Each JS service turn refills at most 1024 input
bytes. A completion poll runs nominally every 4 ms, leaving the REPL serviceable.
The first qualification path is `/app`; SD playback is **not qualified**.

PIO generates clocks and sends digital zero automatically if its FIFO empties,
even if JS and native IRQ service are both delayed. A native alarm checks DMA
nominally every 250 microseconds and rejects missing/late next-buffer handoffs
rather than silently resuming after a gap. It silences pins after the remaining
FIFO/shift-register drain window (at least 1 ms). This is fail-silent behavior,
not a hard real-time fidelity guarantee. Long synchronous JS work can deliberately
cause an underrun; the Promise rejects when JS resumes. Resources remain bounded
until JS cleanup or reset if the engine never resumes. Stop/close do not wait for
drain: queued samples are discarded immediately.

The adapter follows the [official Waveshare demo](https://files.waveshare.com/wiki/RP2350-Touch-LCD-2.8/RP2350-Touch-LCD-2.8-Demo.zip):
BCLK GPIO2, LRCLK GPIO3, DIN GPIO4, PIO1 and DMA; no codec setup is present in
that demo. Public generic pins remain unexposed on this board. This is a
board-specific internal adapter, not a public I2S bus API.

## Qualification

Physical acceptance on firmware `5f25b78b2d964d711670187535fa6d909dd99601`:

- All 13 firmware targets compiled; 235 host tests, native speaker/buzzer/runtime
  lifecycle checks, storage ownership, REPL and SD SPI regressions passed.
- Two matching full-flash backups preceded the update. Firmware readback matched,
  and both original scripts were restored and hash-verified again after resets.
- The operator heard and confirmed the three-note chime from `/app`.
- A local C920 microphone recording captured both complete 625/1000/1500 Hz chimes
  and three 750 Hz playback bursts cut short by abort, stop and close. USB/REPL
  replies continued during playback, and reopen/replay completed successfully.
- A 60-second, 1.92 MB PCM payload streamed at volume zero while the console
  answered 12 probes. Twenty consecutive play/close/reopen cycles then passed.
  This is large-file/lifecycle evidence, not a minute of acoustic fidelity testing.
- During a bounded 700 ms synchronous JS loop, output fell silent and playback
  rejected `EIO` when JS resumed. A reset requested during another playback
  returned the same firmware and permitted a fresh chime after USB host ejection.
- Recordings show the short starvation/reset bursts followed by quiet, then the
  fresh three-note chime. Fourteen expected frequency-band segments were found
  across the two successful recordings. FFT windows/ringing broaden intervals;
  these are functional checks, not calibrated frequency, timing or loudness tests.

The board was left idle, with no audio autorun. The 1.47 board was not modified.
Cold-power-cycle qualification, SD audio and microphone capture remain outside
this slice. The existing software-reset USB delay is not an immediate mute API.

Harness corrections: a REPL prompt prefixed one valid completion marker; USB
re-enumeration must wait for a new device instance rather than the old tty path;
automount after reboot correctly caused `EBUSY` until ejection. The large WAV
installation exceeded the local 180-second SSH wait, but its remote writer
finished, remounted, hash-verified all files and ejected without recovery actions.
None of these observations justified changing the speaker implementation.

Local evidence lives under `plans/mcujs-speaker/`: UART logs, timing observations,
original microphone recordings, fixture manifest, flash/restore verification and
`acoustic-analysis.json`. Recording SHA-256 values:

- Lifecycle: `13dc35e3d7c7dee33fa43e10ef5a7ff1224fd9ca9b7605016b0065e48c4efa47`.
- Starvation/reset: `9c10692090b5e8b49f993afe0e30b54e434effc74681fc4edb8c534e6738cb94`.

No microphone API or board microphone qualification is implied by these external
microphone recordings.
