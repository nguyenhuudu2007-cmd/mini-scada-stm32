# Sổ tiến bộ & Nhật ký sửa lỗi thực tế

> Ghi lại các lỗi thực tế đã gặp trong quá trình dựng mạch, viết firmware STM32 và phát triển Qt HMI.

| Lỗi / Triệu chứng | Nguyên nhân gốc | Cách sửa đã áp dụng | Bài học rút ra |
| :--- | :--- | :--- | :--- |
| Chạm dây breadboard khiến LCD nhảy ký tự rác hoặc hiện 16 ô trắng | Chân `GND` trên breadboard lỏng gây mất tham chiếu bus I2C | Cố định cụm mạch và kiểm tra thông mạch `GND` trước khi cấp nguồn | Luôn kiểm tra nguồn và `GND` trước khi nghi ngờ lỗi code I2C |
| Servo giật làm reset MCU / sai số DS1307 | Dòng đề-pa Servo kéo sụt nguồn `5V` khi cắm chung với mạch điều khiển | Tách nguồn `5V` riêng (`>= 1A`) cho Servo, thêm tụ lọc, giữ chung `GND` | Cơ cấu chấp hành dòng lớn phải cách ly nguồn khỏi MCU |
| Bấm `PA0` hoặc `Reset` cứng trên STM32 nhưng Qt `Current Angle` không về `0°` | Qt ban đầu chỉ parse `:SRV:` và `setAngle()` tự `emit angleChanged` ngược xuống STM32 | Tách `syncAngleFromHardware()` dùng `blockSignals(true)` và thêm `$ALM:...:SYS:BOOT` | Tách biệt luồng cập nhật UI từ phần cứng với luồng gửi lệnh |
| Bấm nút `PA0` trên mạch thật nhưng Qt HMI không khóa E-Stop | STM32 phát `$TLM:...:BTN:PRESSED` trong khi parser Qt bắt buộc `$ALM:` và tự mở khóa sau `1000 ms` | Chuẩn hóa `main.c` phát `$ALM:...:BTN:PRESSED` và bỏ tự mở khóa sau 1s | Khóa hợp đồng giao thức bằng spec & unit test giữa 2 phía |
| Rút cáp USB-RS232 lúc đang `Connected` nhưng HMI vẫn hiện đang kết nối | Slot `QSerialPort::errorOccurred` mới chỉ ghi log, chưa đóng cổng và reset state | Xử lý `ResourceError` qua `teardownActiveSession()` và bỏ `waitForBytesWritten()` chặn luồng | Luôn xử lý kịch bản mất kết nối nóng trên thiết bị nối tiếp |
| Chạy script `sync-rtc.py` ngoài terminal lúc `ServoHMI` đang mở làm mất gói tin | Hai tiến trình cùng mở `/dev/ttyUSB0` tranh đọc byte phản hồi của nhau | Tích hợp gửi `$CMD:RTC` trực tiếp trong `MainWindow` lúc `Connect` và qua nút `Sync PC Time (GMT+7)` | Tại một thời điểm chỉ một tiến trình được giữ cổng Serial |
