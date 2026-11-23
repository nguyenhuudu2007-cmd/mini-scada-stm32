# Sơ đồ chân thực tế (STM32F103C8T6 · Blue Pill)

> Tài liệu này được cập nhật khớp với cấu hình ngoại vi trong [stm32_SCADA_w1.ioc](../firmware/stm32_SCADA_w1/stm32_SCADA_w1.ioc) và [main.c](../firmware/stm32_SCADA_w1/Core/Src/main.c). Chân có ký hiệu **FT** chịu được mức logic 5V.

## 1. Bảng kết nối ngoại vi

| Chức năng | Chân STM32 | Giao thức / Timer | Nối tới | Ghi chú |
| :--- | :--- | :--- | :--- | :--- |
| Nút dừng khẩn cấp E-Stop | **PA0** | `GPIO_EXTI0` | Nút nhấn vật lý → `GND` | Ngắt ngoài kích hoạt E-Stop, chống dội `200 ms` trong `while(1)` |
| Tín hiệu Servo SG90 | **PA1** | `TIM2_CH2` (PWM) | Dây tín hiệu (cam/vàng) SG90 | Chu kỳ `20 ms` (`50 Hz`, `ARR = 19999`), xung `500–2500 µs` (`0°–180°`) |
| UART1 TX | **PA9** | `USART1_TX` | `MAX3232` chân `RXD` (`T1IN`) | Baud `115200 8N1` |
| UART1 RX | **PA10** (FT) | `USART1_RX` | `MAX3232` chân `TXD` (`R1OUT`) | Dùng ngắt `HAL_UART_Receive_IT` từng byte |
| LCD 1602 (PCF8574) | **PB6 / PB7** (FT) | `I2C1_SCL / SDA` | Module LCD I2C (`0x27`) | Hiển thị 2 trang luân phiên mỗi `3000 ms` (Giờ VN & `SWP`) |
| DS1307 RTC | **PB10 / PB11** (FT) | `I2C2_SCL / SDA` | Module TinyRTC DS1307 (`0x68`) | Tách riêng bus `I2C2` giúp giảm xung đột với LCD |
| LED trạng thái | **PC13** | `GPIO_Output` | LED onboard Blue Pill | Tích cực mức thấp (`0` = sáng, `1` = tắt) |
| Mạch nạp SWD | **PA13 / PA14** | `SWDIO / SWCLK` | ST-Link V2 | Không dùng cho GPIO thường |

## 2. Sơ đồ nguồn & Lưu ý chống nhiễu

| Khối | Nguồn khuyên dùng | Lưu ý kỹ thuật |
| :--- | :--- | :--- |
| **STM32 Blue Pill** | `5V` từ ST-Link hoặc cổng USB | Cấp ổn định `3.3V` qua LDO trên board |
| **MAX3232** | **`3.3V`** từ chân `3V3` của Blue Pill | Không cấp `5V` để đảm bảo mức TTL `3.3V` chuẩn cho MCU |
| **LCD 1602 + DS1307** | **`5V`** | DS1307 cần `VCC >= 4.5V`; dùng pin `LIR2032` nếu module có mạch sạc |
| **Servo SG90** | **Nguồn `5V` rời (`>= 1A–2A`)** | Không lấy từ `5V` ST-Link; gắn tụ `470–1000 µF` + `100 nF` sát servo và **nối chung `GND`** với Blue Pill |
