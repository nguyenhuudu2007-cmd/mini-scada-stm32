# Đặc tả giao thức ASCII (`USART1` / RS232 · `115200 8N1`)

Mọi khung truyền giữa **ServoHMI (PC)** và **STM32F103** đều là chuỗi ASCII kết thúc bằng ký tự xuống dòng `\n` (bộ nhận phía PC chấp nhận cả `\r\n` và `\n`, giới hạn tối đa `512` bytes/dòng).

## 1. Lệnh từ Host PC xuống STM32 (`$CMD`)

| Lệnh | Cú pháp | Điều kiện gửi trên HMI | Hành vi phía STM32 |
| :--- | :--- | :--- | :--- |
| Điều khiển LED | `$CMD:LED:<0\|1>\n` | Cổng Serial đang mở | Bật/tắt `PC13`, trả về `$TLM:<HHMMSS>:LED:<0\|1>\n` |
| Đặt góc Servo | `$CMD:SRV:<0..180>\n` | `connected && !isLocked()` | Nếu đang `is_stopped`: từ chối và trả `$ALM:<HHMMSS>:SYS:LOCKED\n`. Ngược lại quay `TIM2_CH2` và trả `$TLM:<HHMMSS>:SRV:<000..180>\n` |
| Mở khóa hệ thống | `$CMD:SYS:UNLOCK\n` | `connected && isLocked()` | Xóa `is_stopped = 0`, cập nhật lại LCD, trả `$TLM:<HHMMSS>:SYS:UNLOCKED\n` |
| Đồng bộ RTC (GMT+7) | `$CMD:RTC:HH:MM:SS\n` | Cổng Serial đang mở (kể cả khi khóa) | Ghi giờ 24h xuống DS1307 qua `I2C2`; trả `$TLM:<HHMMSS>:RTC:OK\n` hoặc `$ALM:<HHMMSS>:RTC:ERR\n` |

## 2. Bản tin từ STM32 gửi lên Host PC (`$TLM` / `$ALM`)

Cấu trúc chuẩn bắt buộc gồm **4 trường** ngăn cách bởi dấu `:`:

```text
$<PREFIX>:<HHMMSS>:<SUBSYSTEM>:<DETAIL>\n
```

- `PREFIX`: `$TLM` (Telemetry / xác nhận bình thường) hoặc `$ALM` (Alarm / sự cố an toàn).
- `HHMMSS`: Đúng 6 chữ số ASCII theo thời gian DS1307 trên STM32.

| Mẫu bản tin | Loại | Ý nghĩa | Phản ứng của `ServoHMI` |
| :--- | :--- | :--- | :--- |
| `$ALM:<HHMMSS>:SYS:BOOT` | Alarm | STM32 vừa khởi động / reset cứng | Xóa khóa E-Stop cũ (nếu có), đồng bộ góc hiển thị về `0°`, ghi Log |
| `$ALM:<HHMMSS>:BTN:PRESSED` | Alarm | Nút cứng E-Stop `PA0` vừa được nhấn | Dừng `Auto Sweep` / `Self-Test`, đưa góc về `0°`, hiện banner khóa đỏ, vô hiệu hóa mọi `$CMD:SRV` |
| `$ALM:<HHMMSS>:SYS:LOCKED` | Alarm | STM32 đang khóa an toàn (từ chối `$CMD:SRV`) | Đưa HMI vào trạng thái `Locked` giống E-Stop |
| `$TLM:<HHMMSS>:SYS:UNLOCKED` | Telemetry | STM32 đã mở khóa sau `$CMD:SYS:UNLOCK` | Ẩn banner đỏ, cho phép điều khiển Servo trở lại |
| `$TLM:<HHMMSS>:SRV:<0..180>` | Telemetry | Xác nhận góc Servo thực tế | Cập nhật `Current Angle` (và `SliderPage` nếu không đang Sweep) |
| `$TLM:<HHMMSS>:LED:<0\|1>` | Telemetry | Xác nhận trạng thái LED `PC13` | Cập nhật trạng thái LED trên tab `Hardware Test` |
| `$TLM:<HHMMSS>:RTC:OK` | Telemetry | Ghi DS1307 thành công | Cập nhật trạng thái top bar thành `RTC: Synced (<HHMMSS>)` |
| `$ALM:<HHMMSS>:RTC:ERR` | Alarm | Lỗi ghi DS1307 qua `I2C2` | Cập nhật top bar `RTC: Error (<HHMMSS>)` |
| Khung sai định dạng | Invalid | Thiếu trường, sai prefix, timestamp không phải 6 số, góc ngoài `0..180` | Giữ nguyên trạng thái hiện tại, chỉ ghi dòng thô vào Log (`SC-002` / `FR-009`) |

## 3. Cơ chế giữ an toàn & chống nghẽn UART

1. **Khóa hai đầu khi E-Stop:** STM32 giữ `is_stopped = 1` cho đến khi nhận `$CMD:SYS:UNLOCK`; phía Qt `HmiSessionState` giữ trạng thái `Locked` ngay cả khi ngắt rồi kết nối lại (chỉ mở khi nhận `$TLM:...:SYS:UNLOCKED` hoặc `$ALM:...:SYS:BOOT`).
2. **Tiết lưu & giãn lệnh:** `SliderPage` dùng bộ tiết lưu `40 ms`, `SweepPage` giữ chu kỳ `>= 45 ms`, và `MainWindow` giãn các lệnh khởi tạo lúc kết nối (`$CMD:LED:1` → `+120 ms` `$CMD:RTC` → `+260 ms` `$CMD:SRV`) để tránh tràn `rx_buf[32]` trên STM32 khi MCU đang bận giao tiếp I2C.
