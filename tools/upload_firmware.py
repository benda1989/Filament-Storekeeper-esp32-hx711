#!/usr/bin/env python3
"""ESP32-C3 API uploader. Standard library only. / 仅通过 API 上传，失败自动重试。"""
from __future__ import annotations

import argparse
import hashlib
import http.client
import json
from pathlib import Path
import socket
import sys
import threading
import time
import uuid

DEFAULT_FIRMWARE = "/Users/gkk/Library/Caches/arduino/sketches/37C5763FF7D23FE6EFD1481373E406F0/HX711WebScale.ino.bin"


def read_firmware(path: Path) -> bytes:
    image = path.read_bytes()
    if (len(image) < 36 or image[0] != 0xE9 or
            int.from_bytes(image[12:14], "little") != 5 or
            image[32:36] != bytes.fromhex("3254cdab")):
        raise ValueError("请使用 ESP32-C3 应用 .bin，不可使用引导程序或合并镜像")
    if len(image) > 2 * 1024 * 1024 - 1024:
        raise ValueError("固件超过上传脚本允许的 2 MiB 大小")
    return image


def remaining(deadline: float) -> float:
    seconds = deadline - time.monotonic()
    if seconds <= 0:
        raise TimeoutError("本次上传超过总时间限制 / overall upload timeout")
    return seconds


def upload_once(host: str, port: int, image: bytes, *, rate: float = 65536,
                idle_timeout: float = 15, total_timeout: float = 180,
                progress=print) -> dict:
    """POST exact image bytes as multipart firmware; no other endpoints are called."""
    started = time.monotonic()
    deadline = started + total_timeout
    boundary = "PythonOTA" + uuid.uuid4().hex
    prefix = (f'--{boundary}\r\nContent-Disposition: form-data; name="firmware"; '
              'filename="firmware.bin"\r\nContent-Type: application/octet-stream\r\n\r\n').encode()
    suffix = f"\r\n--{boundary}--\r\n".encode()
    conn = http.client.HTTPConnection(host, port, timeout=min(8, remaining(deadline)))
    sent = 0
    deadline_timer = None
    deadline_expired = threading.Event()
    result = {"httpStatus": 0, "response": "", "sentBytes": 0, "success": False}
    try:
        conn.connect()
        connected_socket = conn.sock

        def expire() -> None:
            deadline_expired.set()
            try:
                connected_socket.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass

        deadline_timer = threading.Timer(remaining(deadline), expire)
        deadline_timer.daemon = True
        deadline_timer.start()
        conn.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        conn.sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 65536)

        def send(data: bytes) -> None:
            conn.sock.settimeout(min(idle_timeout, remaining(deadline)))
            conn.send(data)

        conn.putrequest("POST", "/api/update")
        conn.putheader("Content-Type", f"multipart/form-data; boundary={boundary}")
        conn.putheader("Content-Length", str(len(prefix) + len(image) + len(suffix)))
        conn.putheader("Connection", "close")
        conn.sock.settimeout(min(idle_timeout, remaining(deadline)))
        conn.endheaders()
        send(prefix)
        transfer_started = last_progress = time.monotonic()
        for offset in range(0, len(image), 4096):
            chunk = image[offset:offset + 4096]
            send(chunk)
            sent += len(chunk)
            now = time.monotonic()
            if now - last_progress >= 5:
                progress(f"  已发送 {sent:,}/{len(image):,} 字节 ({sent / len(image):.0%})")
                last_progress = now
            if rate:
                pause = sent / rate - (time.monotonic() - transfer_started)
                if pause > 0:
                    time.sleep(min(pause, remaining(deadline)))
        send(suffix)
        conn.sock.settimeout(min(idle_timeout, remaining(deadline)))
        response = conn.getresponse()
        result["httpStatus"] = response.status
        result["response"] = response.read(16384).decode("utf-8", errors="replace")
        if deadline_expired.is_set():
            raise TimeoutError("本次上传超过总时间限制")
        result["success"] = response.status == 200 and "更新成功" in result["response"]
    except (OSError, http.client.HTTPException) as error:
        result["error"] = f"{type(error).__name__}: {error}"
    finally:
        if deadline_timer:
            deadline_timer.cancel()
        conn.close()
        result["sentBytes"] = sent
        result["seconds"] = round(time.monotonic() - started, 3)
    return result


def verify_api(host: str, port: int, progress=print) -> bool:
    """Check API recovery after acknowledged success; never resend firmware here."""
    for attempt in range(6):
        time.sleep(3 if attempt == 0 else 5)
        conn = http.client.HTTPConnection(host, port, timeout=8)
        try:
            conn.request("GET", "/api/reading", headers={"Connection": "close"})
            response = conn.getresponse()
            reading = json.loads(response.read(16384))
            if response.status == 200:
                progress("重启后 API 已恢复: " + json.dumps(reading, ensure_ascii=False))
                return True
        except (OSError, ValueError, http.client.HTTPException):
            pass
        finally:
            conn.close()
    progress("上传已明确成功，但尚未确认重启后的 API 恢复；不再重复刷写。")
    return False


def run_upload(host: str, port: int, image: bytes, *, interval: float, max_attempts: int,
               rate: float, idle_timeout: float, total_timeout: float,
               log_path: Path | None = None, progress=print) -> dict:
    sha = hashlib.sha256(image).hexdigest()
    attempt = 0
    while True:
        attempt += 1
        progress(f"第 {attempt} 次上传 → http://{host}:{port}/api/update")
        result = upload_once(host, port, image, rate=rate, idle_timeout=idle_timeout,
                             total_timeout=total_timeout, progress=progress)
        result.update(attempt=attempt, sha256=sha)
        progress(json.dumps(result, ensure_ascii=False))
        if log_path:
            with log_path.open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(result, ensure_ascii=False) + "\n")
        if result["success"] or (max_attempts and attempt >= max_attempts):
            return result
        progress(f"等待 {interval:g} 秒后重试；Ctrl+C 可停止。")
        time.sleep(interval)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("firmware", nargs="?", type=Path, default=Path(DEFAULT_FIRMWARE), help="ESP32-C3 application .bin")
    parser.add_argument("--ip", default="192.168.3.105", help="device IP/hostname (no http:// prefix)")
    parser.add_argument("--port", type=int, default=80)
    parser.add_argument("--interval", type=float, default=30, help="seconds between failures (default 10)")
    parser.add_argument("--rate", type=float, default=64, help="KiB/s, 0=unlimited (default 64)")
    parser.add_argument("--idle-timeout", type=float, default=60)
    parser.add_argument("--total-timeout", type=float, default=360)
    parser.add_argument("--max-attempts", type=int, default=0, help="0=retries forever (default)")
    parser.add_argument("--log", type=Path, help="optional JSONL result log")
    parser.add_argument("--no-verify", action="store_true", help="skip post-success reading API check")
    args = parser.parse_args()
    if args.interval < 0 or args.rate < 0 or min(args.idle_timeout, args.total_timeout) <= 0 or args.max_attempts < 0 or not 1 <= args.port <= 65535:
        parser.error("时间、速率、端口或重试次数无效")
    try:
        image = read_firmware(args.firmware)  # Fixed bytes; later cache rebuilds cannot change retries.
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(f"文件: {args.firmware.resolve()}\n大小: {len(image):,} 字节\nSHA-256: {hashlib.sha256(image).hexdigest()}", flush=True)
    if args.log:
        args.log.parent.mkdir(parents=True, exist_ok=True)
    try:
        result = run_upload(args.ip, args.port, image, interval=args.interval,
                            max_attempts=args.max_attempts, rate=args.rate * 1024,
                            idle_timeout=args.idle_timeout, total_timeout=args.total_timeout,
                            log_path=args.log, progress=lambda text: print(text, flush=True))
        if not result["success"]:
            return 1
        print("固件上传成功，已停止重试。", flush=True)
        if not args.no_verify:
            verify_api(args.ip, args.port, progress=lambda text: print(text, flush=True))
        return 0
    except KeyboardInterrupt:
        print("\n已停止上传。", flush=True)
        return 130


if __name__ == "__main__":
    sys.exit(main())
