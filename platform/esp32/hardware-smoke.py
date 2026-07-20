#!/usr/bin/env python3
"""Hardware smoke test for the MCU.js ESP32-S3 headless runtime."""

from __future__ import annotations

import argparse
import ctypes
import os
from pathlib import Path
import re
import stat
import time

import serial

PROMPT = "\r\n> "
SG_IO = 0x2285
SG_DXFER_NONE = -1


class SgIoHeader(ctypes.Structure):
    _fields_ = [
        ("interface_id", ctypes.c_int),
        ("dxfer_direction", ctypes.c_int),
        ("cmd_len", ctypes.c_ubyte),
        ("mx_sb_len", ctypes.c_ubyte),
        ("iovec_count", ctypes.c_ushort),
        ("dxfer_len", ctypes.c_uint),
        ("dxferp", ctypes.c_void_p),
        ("cmdp", ctypes.c_void_p),
        ("sbp", ctypes.c_void_p),
        ("timeout", ctypes.c_uint),
        ("flags", ctypes.c_uint),
        ("pack_id", ctypes.c_int),
        ("usr_ptr", ctypes.c_void_p),
        ("status", ctypes.c_ubyte),
        ("masked_status", ctypes.c_ubyte),
        ("msg_status", ctypes.c_ubyte),
        ("sb_len_wr", ctypes.c_ubyte),
        ("host_status", ctypes.c_ushort),
        ("driver_status", ctypes.c_ushort),
        ("resid", ctypes.c_int),
        ("duration", ctypes.c_uint),
        ("info", ctypes.c_uint),
    ]


def eject_storage(path: Path) -> None:
    allowed_aliases = {
        "/dev/mcujs-dev-disk",
        "/dev/disk/by-id/usb-MCUJS_Runtime_Disk_MCUJS-DEV-0:0",
    }
    if str(path) not in allowed_aliases:
        raise ValueError(f"refusing non-development-board disk alias: {path}")
    resolved = path.resolve(strict=True)
    device_stat = resolved.stat()
    if not stat.S_ISBLK(device_stat.st_mode):
        raise ValueError(f"MSC path is not a block device: {resolved}")

    sys_block = Path("/sys/class/block") / resolved.name
    vendor = (sys_block / "device/vendor").read_text().strip()
    model = (sys_block / "device/model").read_text().strip()
    size_bytes = int((sys_block / "size").read_text().strip()) * 512
    if (
        vendor != "MCUJS"
        or model != "Runtime Disk"
        or (
            size_bytes != 0
            and not 1024 * 1024 <= size_bytes <= 16 * 1024 * 1024
        )
    ):
        raise ValueError(
            f"refusing unexpected MSC device: vendor={vendor!r}, model={model!r}, "
            f"size={size_bytes}"
        )
    if size_bytes == 0:
        print(f"PASS: exact MCU.js runtime disk is already ejected: {path}")
        return

    command = (ctypes.c_ubyte * 6)(0x1B, 0, 0, 0, 0x02, 0)
    sense = (ctypes.c_ubyte * 32)()
    header = SgIoHeader(
        interface_id=ord("S"),
        dxfer_direction=SG_DXFER_NONE,
        cmd_len=len(command),
        mx_sb_len=len(sense),
        cmdp=ctypes.cast(command, ctypes.c_void_p),
        sbp=ctypes.cast(sense, ctypes.c_void_p),
        timeout=5000,
    )
    descriptor = os.open(resolved, os.O_RDWR | os.O_CLOEXEC)
    try:
        libc = ctypes.CDLL(None, use_errno=True)
        result = libc.ioctl(descriptor, SG_IO, ctypes.byref(header))
        if result < 0:
            error = ctypes.get_errno()
            raise OSError(error, os.strerror(error), str(resolved))
    finally:
        os.close(descriptor)
    if header.status or header.host_status or header.driver_status:
        raise OSError(
            f"SCSI eject failed: status={header.status}, host={header.host_status}, "
            f"driver={header.driver_status}"
        )
    print(f"PASS: ejected exact MCU.js runtime disk {path} ({size_bytes} bytes)")


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
            read_until(port, (PROMPT,))
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
    output = read_until(port, (PROMPT,), timeout=timeout)
    body = output.split(PROMPT, 1)[0].replace("\r\n", "\n")
    if body.startswith(source):
        body = body[len(source) :]
    result_lines = [line for line in body.split("\n") if line]
    result = result_lines[-1] if result_lines else ""
    if expected is not None and result != expected:
        raise AssertionError(
            f"result for {source!r} was {result!r}, expected {expected!r}; output={output!r}"
        )
    print(f"PASS: {source} -> exact result {result!r}")
    return output


def evaluate_number(port: serial.Serial, source: str) -> int:
    output = evaluate(port, source, None)
    values = re.findall(r"(?:^|\r*\n)(\d+)(?=\r*\n)", output)
    if not values:
        raise AssertionError(f"result for {source!r} was not numeric: {output!r}")
    return int(values[-1])


def output_after_prompt(output: str) -> str:
    parts = output.split(PROMPT, 1)
    return parts[1] if len(parts) == 2 else ""


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


def verify_repl_crlf_and_completion(port: serial.Serial) -> None:
    port.reset_input_buffer()
    port.write(b"boa\t.name\r\n")
    port.flush()
    output = read_until(port, (PROMPT,), timeout=5.0)
    time.sleep(0.1)
    output += port.read(4096).decode("utf-8", errors="replace")
    if "board.name" not in output or "'seeed_xiao_esp32s3'" not in output:
        raise AssertionError(f"tab completion did not produce board.name: {output!r}")
    if output.count(PROMPT) != 1:
        raise AssertionError(f"CRLF produced more than one prompt: {output!r}")
    print("PASS: tab completed board.name and CRLF submitted exactly one line")


def verify_help_surface(port: serial.Serial) -> None:
    port.reset_input_buffer()
    port.write(b".help\r")
    port.flush()
    output = read_until(port, (PROMPT,), timeout=5.0)
    for module in ("fs", "process", "gpio", "pwm", "i2c", "spi", "adc", "neopixel"):
        if f"require('{module}')" not in output:
            raise AssertionError(f".help omitted {module!r}: {output!r}")
    for module in ("image", "keyboard", "mouse"):
        if f"require('{module}')" in output:
            raise AssertionError(f".help advertised unavailable {module!r}: {output!r}")
    if "uniqueId()" not in output or " led, ids" in output:
        raise AssertionError(f".help board API is inaccurate: {output!r}")
    print("PASS: .help matches the ESP32 board and module surface")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/mcujs-dev")
    parser.add_argument("--disk", default="/dev/mcujs-dev-disk")
    parser.add_argument("--resets", type=int, default=2)
    args = parser.parse_args()

    path = Path(args.port)
    disk_path = Path(args.disk)
    port = open_console(path)
    try:
        verify_repl_crlf_and_completion(port)
        verify_help_surface(port)
        evaluate(port, "2 + 2", "4")
        evaluate(port, "board.name + ':' + board.chip", "'seeed_xiao_esp32s3:ESP32-S3'")
        evaluate(port, "board.millis() > 0", "true")
        evaluate(
            port,
            "GPIO.init(21, GPIO.OUTPUT); GPIO.set(21, false); GPIO.get(21)",
            "false",
        )
        evaluate(
            port,
            "try{GPIO.set(21,1);false}catch(e){e instanceof TypeError}",
            "true",
        )
        evaluate(
            port,
            "try{GPIO.get(3);false}catch(e){e.code==='EBUSY'}",
            "true",
        )
        evaluate(
            port,
            "try { GPIO.init(26, GPIO.OUTPUT); 'unsafe-open' } catch(e) { 'unsafe-rejected' }",
            "'unsafe-rejected'",
        )
        evaluate(
            port,
            "try{board.led(1);false}catch(e){e instanceof TypeError}",
            "true",
        )
        evaluate(port, "board.led(true); board.led()", "true")
        evaluate(
            port,
            "require('mcujs:module').builtinModules.join(',')",
            "'board,fs,process,gpio,pwm,i2c,spi,adc,neopixel,mcujs:module,node:module'",
        )
        evaluate(
            port,
            "typeof board.uniqueId==='function'&&typeof board.ids==='undefined'&&board.uniqueId().length===12",
            "true",
        )
        evaluate(
            port,
            "var adc=require('adc'),raw=adc.readPin(1),volts=adc.readVoltageChannel(0);"
            "raw>=0&&raw<=4095&&volts>=0&&volts<=3.3",
            "true",
        )
        evaluate(port, "var temp=adc.readTempC();temp>-40&&temp<125", "true")
        evaluate(port, "var raw8=adc.readChannel(8);raw8>=0&&raw8<=4095", "true")
        evaluate(
            port,
            "try{adc.readPin(10);false}catch(e){true}",
            "true",
        )
        evaluate(
            port,
            "var pwm=require('pwm');GPIO.init(1,GPIO.OUTPUT);pwm.init(1,1000);true",
            "true",
        )
        evaluate(
            port,
            "try{GPIO.set(1,true);false}catch(e){e.code==='EBUSY'}",
            "true",
        )
        evaluate(
            port,
            "pwm.stop(1);try{GPIO.set(1,true);false}catch(e){e.code==='EBUSY'}",
            "true",
        )
        evaluate(
            port,
            "GPIO.init(1,GPIO.OUTPUT);GPIO.set(1,true);true",
            "true",
        )
        evaluate(
            port,
            "try{pwm.init(43,1000);false}catch(e){true}",
            "true",
        )
        evaluate(
            port,
            "var ps=[[1,1000],[3,1300],[4,2000],[5,4000]];"
            "ps.forEach(function(x){pwm.init(x[0],x[1])});"
            "var exhausted=false;try{pwm.init(6,5000)}catch(e){exhausted=true}exhausted",
            "true",
        )
        evaluate(
            port,
            "pwm.init(6,1000);[1,3,4,5,6].forEach(function(p){pwm.stop(p)});true",
            "true",
        )
        evaluate(port, "try{pwm.init(3,NaN);false}catch(e){true}", "true")
        evaluate(
            port,
            "var neo=require('neopixel');neo.init({pin:2,length:1,order:'GRB'});"
            "neo.setPixel(0,1,2,3);neo.show();neo.clear();"
            "try{GPIO.init(2,GPIO.OUTPUT);false}catch(e){true}",
            "true",
        )
        evaluate(
            port,
            "try{neo.init({pin:21,length:1});false}catch(e){true}",
            "true",
        )
        evaluate(
            port,
            "neo.init({pin:2,length:256,order:'RGB'});neo.setPixel(255,3,2,1);"
            "neo.show();neo.clear();true",
            "true",
        )
        evaluate(
            port,
            "try{neo.init({pin:3,length:257});false}catch(e){true}",
            "true",
        )
        evaluate(
            port,
            "var i2c=require('i2c');i2c.init(0,5,6,100000);typeof i2c.read==='function'",
            "true",
        )
        evaluate(
            port,
            "var spi=require('spi');spi.init(0,7,9,8,1000000);typeof spi.transfer(0,0x55)==='number'",
            "true",
        )
        evaluate(port, "spi.transfer(0,Array(64).fill(0)).length", "64")
        evaluate(port, "try{spi.transfer(0,Array(65).fill(0));false}catch(e){true}", "true")
        timer_output = evaluate(
            port,
            "setTimeout(function(){ GPIO.set(21, true); console.log('M2_TIMER_OK'); }, 50)",
            None,
        )
        if "M2_TIMER_OK" not in output_after_prompt(timer_output):
            read_until(port, ("M2_TIMER_OK",), timeout=5.0)
        print("PASS: asynchronous timer callback produced 'M2_TIMER_OK'")
        evaluate(port, "GPIO.get(21)", "true")
        self_clear_output = evaluate(
            port,
            "var selfClear; selfClear=setTimeout(function(){ clearTimeout(selfClear); console.log('M2_SELF_CLEAR_OK'); }, 20)",
            None,
        )
        if "M2_SELF_CLEAR_OK" not in output_after_prompt(self_clear_output):
            read_until(port, ("M2_SELF_CLEAR_OK",), timeout=5.0)
        evaluate(port, "2 + 2", "4")
        print("PASS: self-clearing one-shot timer preserved runtime state")
        zero_interval_output = evaluate(
            port,
            "var zeroTicks=0, zeroTimer=setInterval(function(){ if(++zeroTicks===2){ clearInterval(zeroTimer); console.log('M2_ZERO_INTERVAL_OK'); } }, 0)",
            None,
        )
        if "M2_ZERO_INTERVAL_OK" not in output_after_prompt(zero_interval_output):
            read_until(port, ("M2_ZERO_INTERVAL_OK",), timeout=5.0)
        evaluate(port, "zeroTicks >= 2", "true")
        print("PASS: zero-delay interval remained repeating and was cleared")

        wait_for_device(disk_path)
        eject_storage(disk_path)
        time.sleep(0.2)
        evaluate(port, "board.storageReady()", "true")
        evaluate(port, "board.safeMode()", "false")
        evaluate(
            port,
            "var m3fs=require('fs'); typeof m3fs.writeFileSync==='function'",
            "true",
        )
        evaluate(
            port,
            "if(!m3fs.existsSync('/m3'))m3fs.mkdirSync('/m3');"
            "m3fs.writeFileSync('/m3/persist.txt','alpha');"
            "m3fs.appendFileSync('/m3/persist.txt','-beta');"
            "m3fs.readFileSync('/m3/persist.txt')",
            "'alpha-beta'",
        )
        evaluate(
            port,
            "var m3stat=m3fs.statSync('/m3/persist.txt');"
            "m3stat.isFile===true&&m3stat.isDirectory===false&&m3stat.size===10",
            "true",
        )
        evaluate(
            port,
            "m3fs.writeFileSync('/m3/rename.tmp','rename');"
            "m3fs.renameSync('/m3/rename.tmp','/m3/renamed.txt');"
            "m3fs.readdirSync('/m3').indexOf('renamed.txt')>=0",
            "true",
        )
        evaluate(
            port,
            "m3fs.unlinkSync('/m3/renamed.txt');!m3fs.existsSync('/m3/renamed.txt')",
            "true",
        )
        evaluate(
            port,
            "if(!m3fs.existsSync('/lib'))m3fs.mkdirSync('/lib');"
            "m3fs.writeFileSync('/lib/m3-module.js','module.exports={answer:42};');"
            "require('/lib/m3-module').answer",
            "42",
        )
        evaluate(
            port,
            "m3fs.writeFileSync('/lib/m3-child.js','exports.answer=17;');"
            "m3fs.writeFileSync('/lib/m3-deferred.js',"
            "\"exports.load=function(){return require('./m3-child').answer;};\");"
            "require('/lib/m3-deferred').load()",
            "17",
        )
        evaluate(
            port,
            "try{m3fs.writeFileSync('../escape.txt','no');false}catch(e){true}",
            "true",
        )
        evaluate(
            port,
            "var guardPath='/'+('g'.repeat(62));m3fs.writeFileSync(guardPath,'guard');"
            "var longRejected=false;try{m3fs.unlinkSync(guardPath+'x')}"
            "catch(e){longRejected=e.code==='ENAMETOOLONG'};"
            "longRejected&&m3fs.existsSync(guardPath)",
            "true",
        )
        evaluate(port, "m3fs.unlinkSync(guardPath);true", "true")
        print(
            "PASS: filesystem CRUD, nested stat, deferred modules, path confinement, "
            "and overlong-path rejection"
        )

        for index in range(args.resets):
            uptime_before = evaluate_number(port, "board.millis()")
            port = reset_and_reconnect(port, path)
            uptime_after = evaluate_number(port, "board.millis()")
            if uptime_after >= uptime_before:
                raise AssertionError(
                    f"reset did not restart uptime: before={uptime_before}, after={uptime_after}"
                )
            wait_for_device(disk_path)
            eject_storage(disk_path)
            time.sleep(0.2)
            evaluate(port, "2 + 2", "4")
            evaluate(port, "require('fs').readFileSync('/m3/persist.txt')", "'alpha-beta'")
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
