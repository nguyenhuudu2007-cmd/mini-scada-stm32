#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Script quét góc Servo tự động (Sweep Servo) và đồng bộ 2 chiều với STM32
Dự án: Mini SCADA STM32F103 (Blue Pill)

Giao thức:
- PC -> STM32: $CMD:SRV:<angle>\n (angle từ 0 đến 180)
- STM32 -> PC: Báo trạng thái nút nhấn (STOP / BTN_PRESSED) hoặc Telemetry
"""

import sys
import os
import time

CANDIDATE_PORTS = [
    "/dev/scada_rs232",
    "/dev/ttyUSB0",
    "/dev/ttyUSB1",
    "/dev/ttyAMA1"
]
BAUDRATE = 115200

# Thêm đường dẫn sysroot nếu cần
for p in [
    "/home/nguyenhuudu2007/rpi-qemu/qt/sysroot/usr/lib/python3/dist-packages",
    "/usr/lib/python3/dist-packages"
]:
    if os.path.exists(p) and p not in sys.path:
        sys.path.insert(0, p)

try:
    import serial
except ImportError:
    print("[!] Lỗi: Chưa tìm thấy thư viện pyserial! Cài đặt bằng: pip install pyserial")
    sys.exit(1)

def find_serial_port():
    for port in CANDIDATE_PORTS:
        if os.path.exists(port):
            return port
    return None

def draw_progress_bar(angle, status="RUNNING"):
    bar_len = 36
    filled = int(round(bar_len * (angle / 180.0)))
    bar = "█" * filled + "░" * (bar_len - filled)
    print(f"\r[SERVO] {angle:3d}° |{bar}| Trạng thái: {status:<10}", end="", flush=True)

def main():
    port = find_serial_port()
    if not port:
        print("[!] Không tìm thấy cổng nối tiếp khả dụng (/dev/scada_rs232, /dev/ttyUSB*)!")
        return 1

    print(f"[*] Đang kết nối tới STM32 qua: {port} @ {BAUDRATE} bps...")
    try:
        ser = serial.Serial(port, BAUDRATE, timeout=0.05)
        time.sleep(0.3)
        ser.reset_input_buffer()
        ser.reset_output_buffer()
    except Exception as e:
        print(f"[!] Không thể mở cổng {port}: {e}")
        return 1

    print("[*] Đã kết nối thành công! Bắt đầu chu trình quét Servo (0° -> 180° -> 0°).")
    print("[*] Nhấn Button trên kit STM32 hoặc Ctrl+C trên terminal để dừng.\n")

    current_angle = 90
    step = 5            # Bước quét góc (5 độ mỗi nhịp)
    direction = 1       # 1: tăng dần (0->180), -1: giảm dần (180->0)
    delay_step = 0.06   # 60ms mỗi bước (đủ thời gian servo cơ khí di chuyển và không làm nghẽn UART)
    stopped = False

    try:
        while not stopped:
            # 1. GỬI LỆNH ĐIỀU KHIỂN GÓC XUỐNG STM32
            cmd = f"$CMD:SRV:{current_angle}\n"
            ser.write(cmd.encode("ascii"))
            
            # Hiển thị thanh tiến trình góc quay
            draw_progress_bar(current_angle, "QUÉT GÓC" if not stopped else "DỪNG")

            # 2. ĐỌC PHẢN HỒI LIÊN TỤC TỪ STM32 (BẮT SỰ KIỆN NÚT NHẤN DỪNG)
            start_wait = time.time()
            while (time.time() - start_wait) < delay_step:
                while ser.in_waiting > 0:
                    raw_line = ser.readline().decode("ascii", errors="ignore").strip()
                    if raw_line:
                        # Kiểm tra xem STM32 có báo nút nhấn hoặc dừng/khóa không:
                        # - $TLM:hhmmss:BTN:PRESSED (nút bấm PA0)
                        # - $ALM:hhmmss:SYS:LOCKED  (trạng thái khóa an toàn)
                        # - Các từ khóa khác: STOP, PAUSE...
                        if any(k in raw_line.upper() for k in ["STOP", "BTN", "LOCKED", "ALM", "PAUSE"]):
                            stopped = True
                            draw_progress_bar(current_angle, "STOPPED")
                            print(f"\n\n[✓] PHÁT HIỆN TÍN HIỆU TỪ STM32: \"{raw_line}\"")
                            print(f"[!] STM32 đã kích hoạt nút dừng / Khóa an toàn E-Stop (LCD hiển thị STOP).")
                            print(f"[!] Đã dừng chương trình Python tại góc: {current_angle}°.")
                            break
                        elif ":SRV:" in raw_line:
                            # STM32 phản hồi xác nhận góc quay: $TLM:hhmmss:SRV:xxx
                            pass
                if stopped:
                    break
                time.sleep(0.01)

            if stopped:
                break

            # 3. TÍNH TOÁN GÓC CHO CHU KỲ TIẾP THEO
            current_angle += direction * step
            if current_angle >= 180:
                current_angle = 180
                direction = -1  # Đổi chiều quay ngược lại
            elif current_angle <= 0:
                current_angle = 0
                direction = 1   # Đổi chiều quay tiến lên

    except KeyboardInterrupt:
        print("\n\n[!] Người dùng bấm Ctrl+C để dừng chương trình.")
    finally:
        # Đưa servo về góc an toàn 90 độ nếu chưa dừng
        print("\n[*] Đóng cổng Serial.")
        try:
            ser.close()
        except:
            pass
        print("[*] Hoàn tất.")

    return 0

if __name__ == "__main__":
    sys.exit(main())
