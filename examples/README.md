# MCU.js examples

Start with [First light](blink/README.md), then [board discovery](board-discovery/index.js).
These examples target the current portable API source line; a folder name is not
proof that an older firmware supports it. Inspect `.info` and
`require('mcujs:module').builtinModules` before choosing a lesson.

## Copy and run safely

The host-visible application volume root maps to **`/app`** in the runtime.
Copy `hello/index.js` as `hello.js` on that volume, eject it safely, wait for
`require('board').storageReady()` to return `true`, then `.run /app/hello.js`.
Use `.run` for fresh CommonJS entry execution; `require()` caches modules.
Keep sibling files together. Use a non-`index.js` destination at the volume root
unless you deliberately want `/app/index.js` startup execution. Never overwrite
an existing app or settings just to try a demo. Boards without USB file access
need their supported transfer method; the runtime paths remain `/app/...`.

## Maintained common entry points

This is a finite inventory, not a claim that every script below this directory
is a portable, beginner-safe entry point. Missing capabilities should be read
as a reason to skip a lesson, not to guess a GPIO number.

| Entry | Files to copy together | Behavior / stop |
| --- | --- | --- |
| `hello/index.js` | entry only | Board info and 5-second heartbeat; 30s; `helloStop()` |
| `board-discovery/index.js` | entry only | Read-only identity, pins, modules and capabilities; immediate |
| `storage-ownership/index.js` | entry only | Read-only filesystem ownership check; immediate |
| `blink/index.js` (or `blink/blink.js`) | either entry | Declared onboard LED; 30s; [early stop](blink/README.md) |
| `config/index.js` | `config.json` beside entry | Read/log static JSON; immediate; no writes or LED pin assumptions |
| `modules/index.js` | `math.js` beside entry | Relative CommonJS exports and cache identity; immediate; no writes |
| `onboard-button/index.js` | entry only | Configured button events and optional onboard LED; 60s; `onboardButtonStop()` |
| `pwm-fade/index.js` | entry only | Advertised PWM-capable LED/pin only; 15s; no fallback pin guessing |
| `i2c-scan/index.js` | entry only | Finite scan on declared default route; requires correctly wired 3.3V target/pull-ups/common ground |

The timer demos above replace their own prior run. Stop one hardware lesson
before starting a different one that needs the same resource. Configuration and
helper modules remain cached until reset or explicit cache invalidation; `.run`
refreshes the entry, not its dependencies.

### i2c-scan/

Scans readable standard addresses using `board.capability('i2c')` and its
`defaultRoute`, not a fixed pair of Pico pins. Use the options-object `i2c.init`
contract. Connect a 3.3 V target, appropriate SDA/SCL pull-ups and common ground.
Read-incompatible targets may not appear. Only `ENXIO` means no responding target;
unexpected bus and native I/O errors stop the scan.

## Maintained configured-display entry points

**Start drawing with `devices` and `display.canvas`, not a board-specific
`screen.js` driver.** The firmware must advertise the `devices` module and a
configured display. These entries check availability before opening a display.

| Entry | Additional files | Behavior / stop |
| --- | --- | --- |
| `portable/device-display/index.js` | its sibling `draw.js` | Draw, present, close immediately |
| `portable/pointer-draw/index.js` | its sibling `draw.js` | Configured pointer input; 30s; `pointerDrawStop()` |
| `portable/app-draw/index.js` | copy `portable/pointer-draw/draw.js` beside entry | Pointer input and **intentional** validated `/app/settings.json` persistence; 60s; `appDrawStop()` |

Pointer entries replace their own old timers, listeners and handles on `.run`.
The pointer drawing consumer also has a browser demonstration: keep
`browser.html`, `browser.js`, `browser-adapter.js`, and `draw.js` together.

### Portable helper modules (not `.run` entry lessons)

[`portable/images/README.md`](portable/images/README.md) documents the image
helpers: `images/features.js`, `images/info.js`, `images/slideshow.js`,
`jpeg/show.js`, `sd-bmp/show.js`, and `sd-bmp/copy.js`. They require explicit
paths, assets and appropriate filesystem/decoder/display support. Preserve the
relative directory layout and follow their returned-handle cleanup contract.
`portable/sd-asset/show.js` is a separate explicit-path asset helper. These are
not universal onboard demos; SD access and copy operations have additional
hardware and write/ownership prerequisites.

## External-hardware / board-specific legacy and experimental examples

The remaining scripts are **not in the maintained newcomer entry inventory**.
They may depend on older APIs, fixed wiring, local drivers, specific firmware,
assets, or longer-running loops. Review each folder's instructions and source;
do not infer compatibility or safe reruns from their presence here.

- `button/`: external push button and fixed-wiring lesson, not the onboard input API.
- `waveshare-lcd-1.28/`, `waveshare-lcd-1.47/`, `waveshare-lcd-1.69/`,
  `waveshare-lcd-2.8/`, `waveshare_rp2040_pizero/`: board/controller-specific
  drivers, animations, games and experiments.
- `waveshare-epaper-1.54-v2/`, `reterminal-sticky/`: dedicated e-paper hardware,
  artwork and board-specific setup; see their own READMEs.
- `images/`: assets, not executable lessons.

There is no universal Pico pin table: use `require('board').pins`,
`require('board').devices`, and the relevant capability's declared routes.

## Verification

Host regressions execute real example sources with simulated public SDK/timer
boundaries, including fresh CommonJS entries in one persistent VM:

```sh
node --test tests/examples-newcomer.test.js tests/examples-gpio-contract.test.js tests/pointer-draw.test.js
```

These tests are not physical-board evidence. Firmware/native compatibility,
USB transfer, actual timing, and visible/electrical behavior require separate
hardware verification. New demos should be non-blocking, bounded, capability
aware, and clean up on completion and rerun.
