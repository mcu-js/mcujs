#!/usr/bin/env python3
"""Persistent boot-script and safe-mode hardware tests for ESP32-S3."""

from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path
import time

_HARDWARE_SMOKE_PATH = Path(__file__).with_name("hardware-smoke.py")
_HARDWARE_SMOKE_SPEC = importlib.util.spec_from_file_location(
    "mcujs_hardware_smoke", _HARDWARE_SMOKE_PATH
)
if _HARDWARE_SMOKE_SPEC is None or _HARDWARE_SMOKE_SPEC.loader is None:
    raise RuntimeError(f"unable to load {_HARDWARE_SMOKE_PATH}")
_hardware_smoke = importlib.util.module_from_spec(_HARDWARE_SMOKE_SPEC)
_HARDWARE_SMOKE_SPEC.loader.exec_module(_hardware_smoke)

evaluate = _hardware_smoke.evaluate
open_console = _hardware_smoke.open_console
reset_and_reconnect = _hardware_smoke.reset_and_reconnect


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/mcujs-dev")
    args = parser.parse_args()

    path = Path(args.port)
    port = open_console(path)
    try:
        evaluate(port, "board.safeMode(false);board.safeMode()", "false")

        # A healthy boot script must run on every reset and survive the
        # five-second healthy-loop qualification window.
        evaluate(
            port,
            "var bootfs=require('fs');"
            "bootfs.writeFileSync('/app/boot-count.txt','0');"
            "bootfs.writeFileSync('/app/index.js',"
            "\"var fs=require('fs');var n=Number(fs.readFileSync('/app/boot-count.txt'));\"+"
            "\"fs.writeFileSync('/app/boot-count.txt',String(n+1));\");true",
            "true",
        )
        port = reset_and_reconnect(port, path)
        time.sleep(6.0)
        evaluate(port, "require('fs').readFileSync('/app/boot-count.txt')", "'1'")
        port = reset_and_reconnect(port, path)
        time.sleep(6.0)
        evaluate(port, "require('fs').readFileSync('/app/boot-count.txt')", "'2'")
        evaluate(port, "board.safeMode()", "false")
        print("PASS: healthy /app/index.js persisted and passed the qualification window")

        # A syntax failure leaves the attempt pending; the following reset must
        # skip the broken script and expose a working REPL in persistent safe mode.
        evaluate(port, "require('fs').writeFileSync('/app/index.js','function (');true", "true")
        evaluate(port, "board.safeMode(false);true", "true")
        port = reset_and_reconnect(port, path)
        evaluate(port, "2+2", "4")
        evaluate(port, "board.safeMode()", "false")
        port = reset_and_reconnect(port, path)
        evaluate(port, "board.safeMode()", "true")
        evaluate(port, "2+2", "4")
        print("PASS: malformed /app/index.js entered persistent safe mode on next reset")

        evaluate(
            port,
            "require('fs').unlinkSync('/app/index.js');board.safeMode(false);true",
            "true",
        )
        port = reset_and_reconnect(port, path)
        evaluate(port, "board.safeMode()", "false")

        # The explicit software flag must skip an otherwise valid script.
        evaluate(
            port,
            "var fs2=require('fs');"
            "if(fs2.existsSync('/app/explicit-ran.txt'))fs2.unlinkSync('/app/explicit-ran.txt');"
            "fs2.writeFileSync('/app/index.js',"
            "\"require('fs').writeFileSync('/app/explicit-ran.txt','bad');\");"
            "board.safeMode(true);true",
            "true",
        )
        port = reset_and_reconnect(port, path)
        evaluate(port, "board.safeMode()", "true")
        evaluate(port, "require('fs').existsSync('/app/explicit-ran.txt')", "false")
        print("PASS: explicit persistent safe mode skipped /app/index.js")

        evaluate(
            port,
            "require('fs').unlinkSync('/app/index.js');board.safeMode(false);true",
            "true",
        )
        port = reset_and_reconnect(port, path)

        # A boot script cannot clear its own pending marker to evade recovery.
        evaluate(
            port,
            "require('fs').writeFileSync('/app/index.js',"
            "'board.safeMode(false);while(true){}');true",
            "true",
        )
        port = reset_and_reconnect(port, path)
        evaluate(port, "2+2", "4")
        port = reset_and_reconnect(port, path)
        evaluate(port, "board.safeMode()", "true")
        print("PASS: /app/index.js could not clear its own pending recovery marker")
        evaluate(
            port,
            "require('fs').unlinkSync('/app/index.js');board.safeMode(false);true",
            "true",
        )
        port = reset_and_reconnect(port, path)

        # A yielding non-returning script must be recovered by the runtime-task
        # watchdog, not merely by starving the idle task.
        evaluate(
            port,
            "require('fs').writeFileSync('/app/index.js','while(true){board.delay(1)}');true",
            "true",
        )
        evaluate(port, "board.safeMode(false);true", "true")
        port = reset_and_reconnect(port, path)
        evaluate(port, "board.safeMode()", "true", timeout=5.0)
        evaluate(port, "2+2", "4")
        print("PASS: yielding looping /app/index.js recovered automatically into safe mode")

        evaluate(
            port,
            "require('fs').unlinkSync('/app/index.js');board.safeMode(false);true",
            "true",
        )
        port = reset_and_reconnect(port, path)

        # Even a yielding loop delayed until after the healthy window must
        # recover from the persisted watchdog reset reason.
        evaluate(
            port,
            "require('fs').writeFileSync('/app/index.js',"
            "'setTimeout(function(){while(true){board.delay(1)}},6000);');true",
            "true",
        )
        port = reset_and_reconnect(port, path)
        time.sleep(19.0)
        port.close()
        port = open_console(path)
        evaluate(port, "board.safeMode()", "true")
        evaluate(port, "2+2", "4")
        print(
            "PASS: delayed yielding post-qualification loop recovered from "
            "watchdog reset reason"
        )

        evaluate(
            port,
            "var fs3=require('fs');fs3.unlinkSync('/app/index.js');"
            "if(fs3.existsSync('/app/boot-count.txt'))fs3.unlinkSync('/app/boot-count.txt');"
            "board.safeMode(false);true",
            "true",
        )
        port = reset_and_reconnect(port, path)
        evaluate(port, "board.safeMode()", "false")
        evaluate(port, "2+2", "4")
    finally:
        if port.is_open:
            port.close()

    print("ESP32-S3 boot safety smoke passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
