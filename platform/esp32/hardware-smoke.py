#!/usr/bin/env python3
"""Hardware smoke test for the MCU.js ESP32-S3 headless runtime."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import time

import serial


def wait_for_device(path: Path, timeout: float = 20.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if path.exists():
            return
        time.sleep(0.1)
    raise TimeoutError(f"device did not appear: {path}")


def read_until(port: serial.Serial, needles: tuple[str, ...], timeout: float = 5.0) -> str:
    deadline = time.monotonic() + timeout
    data = bytearray()
    while time.monotonic() < deadline:
        chunk = port.read(4096)
        if chunk:
            data.extend(chunk)
            text = data.decode("utf-8", errors="replace")
            if all(needle in text for needle in needles):
                return text
        else:
            time.sleep(0.01)
    text = data.decode("utf-8", errors="replace")
    raise AssertionError(f"timed out waiting for {needles!r}; received {text!r}")


def open_console(path: Path) -> serial.Serial:
    deadline = time.monotonic() + 20.0
    while time.monotonic() < deadline:
        wait_for_device(path, timeout=max(0.1, deadline - time.monotonic()))
        port: serial.Serial | None = None
        try:
            port = serial.Serial(str(path), 115200, timeout=0.05, write_timeout=2)
            port.reset_input_buffer()
            port.write(b"\r")
            port.flush()
            read_until(port, ("> ",))
            return port
        except (OSError, serial.SerialException, AssertionError):
            if port is not None:
                try:
                    port.close()
                except OSError:
                    pass
            time.sleep(0.1)
    raise TimeoutError(f"console did not become ready: {path}")


def evaluate(
    port: serial.Serial, source: str, expected: str | None, timeout: float = 5.0
) -> str:
    port.reset_input_buffer()
    port.write(source.encode("utf-8") + b"\r")
    port.flush()
    output = read_until(port, ("> ",), timeout=timeout)
    result_output = output.replace(source, "", 1)
    if expected is not None and expected not in result_output:
        raise AssertionError(
            f"result for {source!r} did not contain {expected!r}: {result_output!r}"
        )
    print(f"PASS: {source} -> result contains {expected!r}")
    return result_output


def evaluate_number(port: serial.Serial, source: str) -> int:
    output = evaluate(port, source, None)
    values = re.findall(r"(?:^|\r*\n)(\d+)(?=\r*\n)", output)
    if not values:
        raise AssertionError(f"result for {source!r} was not numeric: {output!r}")
    return int(values[-1])


def reset_and_reconnect(port: serial.Serial, path: Path) -> serial.Serial:
    port.reset_input_buffer()
    port.write(b".reset\r")
    port.flush()
    try:
        read_until(port, ("Resetting...",), timeout=2.0)
    except (AssertionError, serial.SerialException):
        pass
    port.close()

    # Fixed USB Serial/JTAG may remain enumerated across esp_restart(), while
    # other transports disappear and return. Give either path time to reboot.
    time.sleep(0.5)
    return open_console(path)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/mcujs-dev")
    parser.add_argument("--resets", type=int, default=2)
    args = parser.parse_args()

    path = Path(args.port)
    port = open_console(path)
    try:
        evaluate(port, "2 + 2", "4")
        evaluate(port, "board.name + ':' + board.chip", "seeed_xiao_esp32s3:ESP32-S3")
        evaluate(port, "board.millis() > 0", "true")
        evaluate(
            port,
            "GPIO.init(21, GPIO.OUTPUT); GPIO.set(21, false); GPIO.get(21)",
            "false",
        )
        evaluate(
            port,
            "try { GPIO.init(26, GPIO.OUTPUT); 'unsafe-open' } catch(e) { 'unsafe-rejected' }",
            "unsafe-rejected",
        )
        evaluate(port, "board.led(true); board.led()", "true")
        timer_output = evaluate(
            port,
            "setTimeout(function(){ GPIO.set(21, true); console.log('M2_TIMER_OK'); }, 50)",
            None,
        )
        if "M2_TIMER_OK" not in timer_output:
            read_until(port, ("M2_TIMER_OK",), timeout=5.0)
        print("PASS: asynchronous timer callback produced 'M2_TIMER_OK'")
        evaluate(port, "GPIO.get(21)", "true")
        self_clear_output = evaluate(
            port,
            "var selfClear; selfClear=setTimeout(function(){ clearTimeout(selfClear); console.log('M2_SELF_CLEAR_OK'); }, 20)",
            None,
        )
        if "M2_SELF_CLEAR_OK" not in self_clear_output:
            read_until(port, ("M2_SELF_CLEAR_OK",), timeout=5.0)
        evaluate(port, "2 + 2", "4")
        print("PASS: self-clearing one-shot timer preserved runtime state")
        zero_interval_output = evaluate(
            port,
            "var zeroTicks=0, zeroTimer=setInterval(function(){ if(++zeroTicks===2){ clearInterval(zeroTimer); console.log('M2_ZERO_INTERVAL_OK'); } }, 0)",
            None,
        )
        if "M2_ZERO_INTERVAL_OK" not in zero_interval_output:
            read_until(port, ("M2_ZERO_INTERVAL_OK",), timeout=5.0)
        evaluate(port, "zeroTicks >= 2", "true")
        print("PASS: zero-delay interval remained repeating and was cleared")

        for index in range(args.resets):
            uptime_before = evaluate_number(port, "board.millis()")
            port = reset_and_reconnect(port, path)
            uptime_after = evaluate_number(port, "board.millis()")
            if uptime_after >= uptime_before:
                raise AssertionError(
                    f"reset did not restart uptime: before={uptime_before}, after={uptime_after}"
                )
            evaluate(port, "2 + 2", "4")
            print(
                f"PASS: reset/reconnect cycle {index + 1} restarted uptime "
                f"({uptime_before} -> {uptime_after})"
            )

        port.reset_input_buffer()
        time.sleep(12.0)
        watchdog_output = port.read(16384).decode("utf-8", errors="replace")
        if "task_wdt" in watchdog_output or "Task watchdog" in watchdog_output:
            raise AssertionError(f"watchdog output detected: {watchdog_output!r}")
        evaluate(port, "2 + 2", "4")
        print("PASS: runtime remained responsive with no watchdog output for 12 seconds")
    finally:
        if port.is_open:
            port.close()

    print("ESP32-S3 hardware smoke passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
