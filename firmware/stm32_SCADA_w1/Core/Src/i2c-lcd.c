#include "i2c-lcd.h"
#include "i2c.h"

// Địa chỉ I2C mặc định của chip PCF8574
static uint8_t SLAVE_ADDRESS_LCD = 0x4E;

// Gửi 1 nửa byte (Single Nibble - 4 bit) cho giai đoạn khởi tạo
static void lcd_write_nibble(char nibble, uint8_t rs)
{
    uint8_t data = (nibble & 0xF0) | (rs ? 0x01 : 0x00) | 0x08; // 0x08: Bật đèn nền (Backlight)
    uint8_t data_t[2];
    data_t[0] = data | 0x04;  // EN = 1 (Tạo xung sườn lên)
    data_t[1] = data & ~0x04; // EN = 0 (Chốt dữ liệu sườn xuống)
    HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_LCD, data_t, 2, 100);
}

void lcd_send_cmd(char cmd)
{
    char data_u, data_l;
    uint8_t data_t[4];
    data_u = (cmd & 0xf0);
    data_l = ((cmd << 4) & 0xf0);
    data_t[0] = data_u | 0x0C;  // en=1, rs=0, bl=1
    data_t[1] = data_u | 0x08;  // en=0, rs=0, bl=1
    data_t[2] = data_l | 0x0C;  // en=1, rs=0, bl=1
    data_t[3] = data_l | 0x08;  // en=0, rs=0, bl=1
    HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_LCD, (uint8_t *)data_t, 4, 100);
}

void lcd_send_data(char data)
{
    char data_u, data_l;
    uint8_t data_t[4];
    data_u = (data & 0xf0);
    data_l = ((data << 4) & 0xf0);
    data_t[0] = data_u | 0x0D;  // en=1, rs=1, bl=1
    data_t[1] = data_u | 0x09;  // en=0, rs=1, bl=1
    data_t[2] = data_l | 0x0D;  // en=1, rs=1, bl=1
    data_t[3] = data_l | 0x09;  // en=0, rs=1, bl=1
    HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_LCD, (uint8_t *)data_t, 4, 100);
}

void lcd_clear(void)
{
    lcd_send_cmd(0x01);
    HAL_Delay(2);
}

void lcd_put_cur(int row, int col)
{
    switch (row)
    {
        case 0:
            col |= 0x80;
            break;
        case 1:
            col |= 0xC0;
            break;
    }
    lcd_send_cmd(col);
}

void lcd_init(void)
{
    // 1. Tự động dò tìm địa chỉ I2C của màn hình (0x4E hoặc 0x7E)
    if (HAL_I2C_IsDeviceReady(&hi2c1, 0x4E, 1, 10) == HAL_OK)
    {
        SLAVE_ADDRESS_LCD = 0x4E;
    }
    else if (HAL_I2C_IsDeviceReady(&hi2c1, 0x7E, 1, 10) == HAL_OK)
    {
        SLAVE_ADDRESS_LCD = 0x7E;
    }

    // 2. Chờ nguồn LCD ổn định > 50ms
    HAL_Delay(50);

    // 3. Khởi tạo 4-bit CHUẨN HD44780 (Bắt buộc gửi đơn NIBBLE 4-bit, không gửi 2 nibble!)
    lcd_write_nibble(0x30, 0);
    HAL_Delay(5);
    lcd_write_nibble(0x30, 0);
    HAL_Delay(1);
    lcd_write_nibble(0x30, 0);
    HAL_Delay(1);

    // Chuyển sang chế độ 4-bit
    lcd_write_nibble(0x20, 0);
    HAL_Delay(1);

    // 4. Từ đây trở đi mới gửi lệnh 2 nibble bình thường:
    lcd_send_cmd(0x28); // 2 dòng, font 5x8
    HAL_Delay(1);
    lcd_send_cmd(0x08); // Tắt màn hình tạm thời
    HAL_Delay(1);
    lcd_send_cmd(0x01); // Xóa sạch màn hình
    HAL_Delay(3);
    lcd_send_cmd(0x06); // Tự động tăng con trỏ sang phải
    HAL_Delay(1);
    lcd_send_cmd(0x0C); // Bật hiển thị, tắt con trỏ nhấp nháy
    HAL_Delay(1);
}

void lcd_send_string(char *str)
{
    while (*str)
    {
        lcd_send_data(*str++);
    }
}
