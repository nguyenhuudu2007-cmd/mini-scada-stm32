# Sơ đồ chân (STM32F103C8T6 · Blue Pill)

> Mục tiêu: chốt chân **trước** khi mở CubeMX để không phải đổi lại giữa chừng. Chân có ký hiệu **FT** chịu được 5V.

| Chức năng | Chân STM32 | Nối tới | Ghi chú |
|---|---|---|---|
| I2C1 SCL | **PB6** (FT) | DS1307 SCL, PCF8574 SCL | Pull-up có sẵn trên module (lên 5V) → tắt pull-up nội |
| I2C1 SDA | **PB7** (FT) | DS1307 SDA, PCF8574 SDA | Kiểm tra bằng `I2C_Scanner`: 0x27 + 0x68 |
| PWM servo | **PA0** · TIM2_CH1 | SG90 dây cam (tín hiệu) | 50Hz, xung 0.5–2.5 ms |
| USART1 TX | **PA9** | MAX3232 **RXD** (T1IN) | Chéo TX↔RX |
| USART1 RX | **PA10** (FT) | MAX3232 **TXD** (R1OUT) | |
| LED trạng thái | **PC13** (LED trên board) | — | Tích cực mức thấp. Nháy khi có gói TX/RX |
| LED lỗi CRC | **PB12** | LED + điện trở 330Ω → GND | Chặng 3 |
| SWD | PA13 / PA14 | ST-Link SWDIO / SWCLK | Không dùng hai chân này làm GPIO |

## Nguồn

| Khối | Nguồn | Lưu ý |
|---|---|---|
| Blue Pill | 5V từ ST-Link hoặc USB | |
| DS1307 + LCD | **5V** | DS1307 cần VCC ≥ 4.5V. Pin dự phòng **LIR2032** (TinyRTC có mạch sạc) |
| SG90 | **5V riêng ≥ 1A** | Tụ hóa 470–1000µF + 100nF sát servo. **GND chung** với Blue Pill |
| MAX3232 | **3.3V** từ chân 3V3 của Blue Pill | |

## Đầu ra cần đính kèm
- [ ] Ảnh chụp mạch breadboard → `hardware/`
- [ ] Ảnh PulseView khung I2C có ACK → `hardware/`
- [ ] Schematic KiCad (chỉ cần schematic, không cần PCB) → `hardware/`
