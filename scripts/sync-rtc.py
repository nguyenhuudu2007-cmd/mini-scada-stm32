#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Script đồng bộ thời gian thực chuẩn GMT+7 (Việt Nam) từ máy tính/máy ảo 
xuống vi điều khiển STM32 và chip DS1307 RTC, hiển thị lên LCD1602.
Định dạng gói tin giao thức: $CMD:RTC:HH:MM:SS\\n
"""

import sys
import os
import time
from datetime import datetime
from zoneinfo import ZoneInfo

# Thêm đường dẫn thư viện serial nếu chạy trong môi trường sysroot/custom
for p in [
    "/home/nguyenhuudu2007/rpi-qemu/qt/sysroot/usr/lib/python3/dist-packages",
    "/usr/lib/python3/dist-packages"
]:
    if os.path.exists(p) and p not in sys.path:
        sys.path.insert(0, p)

try:
    import serial
except ImportError:
    print("[!] Lỗi: Chưa tìm thấy thư viện pyserial!")
    sys.exit(1)

CANDIDATE_PORTS = [
    "/dev/scada_rs232",
    "/dev/ttyUSB0",
    "/dev/ttyUSB1",
    "/dev/ttyAMA1"
]
BAUDRATE = 115200

def find_serial_port():
    for port in CANDIDATE_PORTS:
        if os.path.exists(port):
            return port
    return None

def main():
    port = find_serial_port()
    if not port:
        print("[!] Không tìm thấy cổng nối tiếp nào khả dụng (/dev/scada_rs232, /dev/ttyUSB*)!")
        return 1

    # Ép múi giờ chuẩn Việt Nam (GMT+7)
    tz = ZoneInfo("Asia/Ho_Chi_Minh")
    now = datetime.now(tz)
    
    cmd = f"$CMD:RTC:{now.hour:02d}:{now.minute:02d}:{now.second:02d}\n"

    print(f"[*] Đang kết nối tới STM32 qua: {port} @ {BAUDRATE} bps...")
    try:
        ser = serial.Serial(port, BAUDRATE, timeout=2.0)
        time.sleep(0.3)  # Chờ ổn định kết nối

        # Đọc bỏ byte rác tồn đọng
        if ser.in_waiting:
            ser.read(ser.in_waiting)

        print(f"[TX] Gửi lệnh cài giờ: {cmd.strip()} (Giờ thực tế: {now.strftime('%H:%M:%S %d/%m/%Y %Z')})")
        ser.write(cmd.encode("ascii"))

        # Chờ phản hồi ACK từ STM32
        resp = ser.readline().decode("ascii", errors="ignore").strip()
        if "RTC:OK" in resp or "TLM" in resp:
            print(f"[✓] STM32 phản hồi thành công: {resp}")
            print(f"[✓] Đã cập nhật xong giờ thực tế và đánh thức DS1307! Màn hình LCD1602 đang hiển thị VN: {now.strftime('%H:%M:%S')}")
        else:
            print(f"[?] Phản hồi từ STM32: {resp if resp else '(Không có phản hồi)'}")

        ser.close()
        return 0

    except Exception as e:
        print(f"[!] Lỗi giao tiếp Serial: {e}")
        return 1

if __name__ == "__main__":
    sys.exit(main())
