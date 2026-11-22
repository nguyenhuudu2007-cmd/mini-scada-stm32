# Đặc tả giao thức gói tin *(viết ở Chặng 3)*

> Học bài 6 khóa **Deviot STM32 nâng cao** (CRC + MIN Protocol) trước, rồi điền tài liệu này bằng lời của bạn.

## 1. Lớp vật lý
- UART: ___ baud, 8N1 · RS232 qua MAX3232

## 2. Khung gói tin
| Byte | Trường | Kích thước | Mô tả |
|---|---|---|---|
| 0 | Header | | |
| | Opcode | | |
| | Length | | |
| | Payload | | |
| | CRC8 | | Đa thức: ___ · Tính trên các trường: ___ |

## 3. Bảng lệnh
| Opcode | Hướng | Payload | Ý nghĩa |
|---|---|---|---|
| | Host → MCU | | Đặt thời gian DS1307 |
| | Host → MCU | | Đặt góc servo |
| | MCU → Host | | Báo thời gian hiện tại |
| | MCU → Host | | ACK / NACK |

## 4. Xử lý lỗi
- Sai CRC → ___
- Timeout / mất đồng bộ khung → ___

## 5. Câu hỏi phải tự trả lời được
- Nếu byte dữ liệu trùng giá trị Header thì sao? (gợi ý: byte stuffing trong MIN)
- Ring Buffer đầy thì làm gì?
