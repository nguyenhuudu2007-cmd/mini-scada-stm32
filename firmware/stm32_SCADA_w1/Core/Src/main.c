/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "i2c-lcd.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
  uint8_t sec;
  uint8_t min;
  uint8_t hour;
  uint8_t day;
  uint8_t date;
  uint8_t month;
  uint8_t year;
} DS1307_Time_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DS1307_ADDR (0x68 << 1)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t rx_byte; //hứng từng ký tự nhận đươc
char rx_buf[32]; // mảng chứa chuỗi lệnh hoàn chỉnh
uint8_t rx_idx=0; // con trỏ vị trí mảng
volatile uint8_t flag_cmd_ready=0; //cờ báo có lệnh từ máy ảo

//--Biến phục vụ nút nhấn EXTI & Khóa an toàn E-Stop-----//
volatile uint8_t flag_btn_pressed = 0; // Cờ báo có người nhấn PA0
volatile uint8_t is_stopped = 0;       // Cờ khóa dừng khẩn cấp E-Stop
volatile uint32_t stop_tick = 0;       // Thời điểm kích hoạt E-Stop

//--Biến phục vụ DS1307 RTC trên I2C2 (PB10/PB11)--------//
DS1307_Time_t rtc_time = {0};
DS1307_Time_t stop_time = {0};
uint8_t ds1307_ok = 0;

//--Biến phục vụ LCD 2 trang & đo thời gian sweep_servo---//
uint8_t lcd_page = 0;               // 0: Giờ Việt Nam (DS1307), 1: Thời gian sweep_servo
uint32_t servo_active_ms = 0;       // Tổng thời gian servo quay (sweep_servo)
uint32_t last_srv_cmd_tick = 0;     // Mốc thời gian nhận lệnh CMD:SRV gần nhất
int16_t last_servo_angle = 0;       // Góc servo gần nhất để nhận biết đang quay
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Set_Servo_Angle(int16_t angle);
uint8_t DS1307_BcdToDec(uint8_t bcd);
uint8_t DS1307_DecToBcd(uint8_t dec);
HAL_StatusTypeDef DS1307_EnsureRunning(void);
HAL_StatusTypeDef DS1307_SetTime(const DS1307_Time_t *t);
HAL_StatusTypeDef DS1307_GetTime(DS1307_Time_t *t);
void Update_LCD_Header(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */
  // 0. Đảm bảo chu kỳ PWM luôn là 20ms (ARR = 19999 cho 50Hz)
  __HAL_TIM_SET_AUTORELOAD(&htim2, 19999);

  // 1. Kích hoạt xuất xung PWM trên Timer 2 Kênh 2 (chân PA1)
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);

  // 2. Đưa Servo về góc 0 độ mặc định ban đâu
  Set_Servo_Angle(0);

  // 3. Kích hoạt ngắt nhận byte UART đàu tiên
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

  // 4. Khởi động màn hình LCD 1602 và in dòng chữ chào mừng
  lcd_init();
  lcd_put_cur(0, 0);
  lcd_send_string("STM32 Mini-SCADA");
  lcd_put_cur(1, 0);
  lcd_send_string("Servo: 000 deg  ");

  // 5. Kiểm tra DS1307 trên I2C2 (PB10/PB11) và đánh thức bộ dao động (giờ VN nhận qua $CMD:RTC)
  if (HAL_I2C_IsDeviceReady(&hi2c2, DS1307_ADDR, 2, 50) == HAL_OK)
  {
    ds1307_ok = 1;
    DS1307_EnsureRunning();
    DS1307_GetTime(&rtc_time);
    Update_LCD_Header();
  }

  // 6. Phát bản tin khởi động lên HMI để đồng bộ trạng thái boot
  {
    char boot_msg[40];
    sprintf(boot_msg, "$ALM:%02d%02d%02d:SYS:BOOT\n", rtc_time.hour, rtc_time.min, rtc_time.sec);
    HAL_UART_Transmit(&huart1, (uint8_t*)boot_msg, strlen(boot_msg), 50);
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t last_rtc_tick = HAL_GetTick();
  uint32_t last_page_tick = HAL_GetTick();
  while (1)
  {
	// -------------------------------------------------------------
	// TÁC VỤ 1: Kiểm tra xem có lệnh mới từ máy ảo gửi xuống không
	// -------------------------------------------------------------
	  if (flag_cmd_ready)
	  {
		int val=0;
		char resp[40];

		// Kiểm tra nếu là lệnh LED: $CMD:LED:1\n hoặc 0\n
		if (sscanf(rx_buf, "CMD:LED:%d", &val)==1)
		{
			// Bật ( kéo xuống 0 ) hoặc Tắt ( kéo lên 1 ) PC13
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, val ? GPIO_PIN_RESET : GPIO_PIN_SET );

			// Đóng gói chuỗi phản hồi ( ACK ) gửi ngược lên máy ảo
			sprintf(resp, "$TLM:%02d%02d%02d:LED:%d\n", rtc_time.hour, rtc_time.min, rtc_time.sec, val);
			HAL_UART_Transmit (&huart1,(uint8_t*)resp,strlen(resp), 50 );
		}

		// Kiểm tra nếu là lệnh Servo $CMD:SRV:090\n
		else if (sscanf(rx_buf, "CMD:SRV:%d", &val)==1)
		{
			// Khi đang khóa an toàn E-Stop -> Từ chối mọi lệnh Servo cho tới khi nhận $CMD:SYS:UNLOCK
			if (is_stopped)
			{
				sprintf(resp, "$ALM:%02d%02d%02d:SYS:LOCKED\n", rtc_time.hour, rtc_time.min, rtc_time.sec);
				HAL_UART_Transmit(&huart1, (uint8_t*)resp, strlen(resp), 50);
			}
			else
			{
				uint8_t was_stopped = is_stopped;
				uint32_t now_tick = HAL_GetTick();

				int clamped_val = val;
				if (clamped_val < 0) clamped_val = 0;
				if (clamped_val > 180) clamped_val = 180;

				// Chỉ cộng dồn thời gian quay khi servo thực sự đổi góc trong đợt sweep (<= 2s)
				if (last_srv_cmd_tick != 0 &&
					(now_tick - last_srv_cmd_tick) <= 2000U &&
					clamped_val != last_servo_angle)
				{
					servo_active_ms += (now_tick - last_srv_cmd_tick);
				}
				last_srv_cmd_tick = now_tick;
				last_servo_angle = (int16_t)clamped_val;

				Set_Servo_Angle((int16_t)clamped_val);

				// 1. Chỉ vẽ lại dòng 0 ngay khi vừa thoát trạng thái STOPPED (giảm tải I2C khi sweep)
				if (was_stopped)
				{
					Update_LCD_Header();
				}

				// 2. Cập nhật góc mới lên Dòng 2 LCD (có chặn 0-180)
				char lcd_str[17];
				sprintf(lcd_str, "Servo: %03d deg  ", clamped_val);
				lcd_put_cur(1, 0);
				lcd_send_string(lcd_str);

				// 3. Đóng gói chuỗi phản hồi (ACK)
				sprintf(resp, "$TLM:%02d%02d%02d:SRV:%03d\n", rtc_time.hour, rtc_time.min, rtc_time.sec, clamped_val);
				HAL_UART_Transmit(&huart1, (uint8_t*)resp, strlen(resp), 50);
			}
		}
		// Lệnh mở khóa hệ thống: $CMD:SYS:UNLOCK\n
		else if (strncmp(rx_buf, "CMD:SYS:UNLOCK", 14) == 0)
		{
			is_stopped = 0;
			last_srv_cmd_tick = 0;
			Update_LCD_Header();
			lcd_put_cur(1, 0);
			lcd_send_string("Servo: 000 deg  ");
			sprintf(resp, "$TLM:%02d%02d%02d:SYS:UNLOCKED\n", rtc_time.hour, rtc_time.min, rtc_time.sec);
			HAL_UART_Transmit(&huart1, (uint8_t*)resp, strlen(resp), 50);
		}
		// Lệnh cài giờ DS1307: $CMD:RTC:HH:MM:SS\n
		else
		{
			int hh = 0, mm = 0, ss = 0;
			if (sscanf(rx_buf, "CMD:RTC:%d:%d:%d", &hh, &mm, &ss) == 3)
			{
				if (hh >= 0 && hh <= 23 && mm >= 0 && mm <= 59 && ss >= 0 && ss <= 59)
				{
					DS1307_Time_t new_time = rtc_time;
					new_time.hour = (uint8_t)hh;
					new_time.min = (uint8_t)mm;
					new_time.sec = (uint8_t)ss;
					if (new_time.day == 0) new_time.day = 1;
					if (new_time.date == 0) new_time.date = 1;
					if (new_time.month == 0) new_time.month = 1;
					if (DS1307_SetTime(&new_time) == HAL_OK)
					{
						ds1307_ok = 1;
						rtc_time = new_time;
						if (!is_stopped) Update_LCD_Header();
						sprintf(resp, "$TLM:%02d%02d%02d:RTC:OK\n", rtc_time.hour, rtc_time.min, rtc_time.sec);
					}
					else
					{
						sprintf(resp, "$ALM:%02d%02d%02d:RTC:ERR\n", rtc_time.hour, rtc_time.min, rtc_time.sec);
					}
					HAL_UART_Transmit(&huart1, (uint8_t*)resp, strlen(resp), 50);
				}
			}
		}
		flag_cmd_ready=0;
	  }

	  // -------------------------------------------------------------
	  // TÁC VỤ 2: Xử lý sự kiện bấm nút PA0 (DỪNG KHẨN CẤP E-STOP)
	  // -------------------------------------------------------------
	  if (flag_btn_pressed)
	  {
		  static uint32_t last_btn_tick = 0;
		  // Chống dội 200ms bằng SysTick - NHẤN 1 LẦN ĂN NGAY!
		  if (HAL_GetTick() - last_btn_tick > 200)
		  {
			  last_btn_tick = HAL_GetTick();

			  // 1. Bật cờ khóa cứng hệ thống và chốt thời điểm E-Stop
			  is_stopped = 1;
			  stop_tick = HAL_GetTick();
			  last_srv_cmd_tick = 0;
			  if (ds1307_ok && DS1307_GetTime(&rtc_time) == HAL_OK)
			  {
				  stop_time = rtc_time;
			  }

			  // 2. STM32 TỰ ĐỘNG thu Servo về 0 độ ngay lập tức!
			  Set_Servo_Angle(0);

			  // 3. Màn hình LCD đóng băng thời điểm xảy ra ngừng đột ngột
			  char line0[17];
			  char line1[17];
			  if (ds1307_ok)
			  {
				  snprintf(line0, sizeof(line0), "STOP AT %02u:%02u:%02u",
						   (unsigned)(stop_time.hour % 24U),
						   (unsigned)(stop_time.min % 60U),
						   (unsigned)(stop_time.sec % 60U));
			  }
			  else
			  {
				  snprintf(line0, sizeof(line0), "** STOPPED! **  ");
			  }

			  uint32_t swp_s = servo_active_ms / 1000U;
			  if (swp_s > 0U)
			  {
				  uint32_t swp_m = (swp_s / 60U) % 100U;
				  uint32_t swp_rem_s = swp_s % 60U;
				  snprintf(line1, sizeof(line1), "SWP:%02lu:%02lu (SAFE)",
						   (unsigned long)swp_m, (unsigned long)swp_rem_s);
			  }
			  else
			  {
				  snprintf(line1, sizeof(line1), "Servo: 000(SAFE)");
			  }

			  lcd_put_cur(0, 0);
			  lcd_send_string(line0);
			  lcd_put_cur(1, 0);
			  lcd_send_string(line1);

			  // 4. Báo tin lên máy tính kèm đúng timestamp lúc xảy ra sự cố
			  char btn_msg[40];
			  sprintf(btn_msg, "$ALM:%02d%02d%02d:BTN:PRESSED\n",
					  stop_time.hour, stop_time.min, stop_time.sec);
			  HAL_UART_Transmit(&huart1, (uint8_t*)btn_msg, strlen(btn_msg), 50);
		  }
		  flag_btn_pressed = 0;
	  }

	  // -------------------------------------------------------------
	  // TÁC VỤ 3: Đọc DS1307 mỗi 1000ms & xoay vòng 2 trang LCD mỗi 3000ms
	  // -------------------------------------------------------------
	  if (HAL_GetTick() - last_page_tick >= 3000)
	  {
		  last_page_tick = HAL_GetTick();
		  if (!is_stopped)
		  {
			  lcd_page ^= 1U;
			  Update_LCD_Header();
		  }
	  }

	  if (HAL_GetTick() - last_rtc_tick >= 1000)
	  {
		  last_rtc_tick = HAL_GetTick();
		  if (ds1307_ok)
		  {
			  if (DS1307_GetTime(&rtc_time) == HAL_OK)
			  {
				  if (!is_stopped)
				  {
					  Update_LCD_Header();
				  }
			  }
			  else
			  {
				  ds1307_ok = 0;
			  }
		  }
		  else
		  {
			  if (HAL_I2C_IsDeviceReady(&hi2c2, DS1307_ADDR, 1, 20) == HAL_OK)
			  {
				  ds1307_ok = 1;
				  DS1307_EnsureRunning();
				  DS1307_GetTime(&rtc_time);
			  }
			  if (!is_stopped)
			  {
				  Update_LCD_Header();
			  }
		  }
	  }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

// 1. Hàm Callback ngắt nhận UART (Chạy tự động mỗi khi có 1 byte bay tới)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance==USART1)
	{
		if (rx_byte == '$')
		{
			rx_idx=0; // Ký tự bắt đầu -> Reset con trỏ về đầu
		}
		else if ( rx_byte=='\n')
		{
			rx_buf[rx_idx]='\0'; // Ký tự kết thúc -> Đóng chuỗi C
			flag_cmd_ready=1; // bật cờ thông báo cho while(1)
		}
		else
		{
			if (rx_idx<sizeof(rx_buf)-1)
			{
				rx_buf[rx_idx++]=rx_byte; //Lưu ký tự vào bộ đệm
			}
		}
		// Mồi lại để sẵn sàng nhận ký tự tiếp theo
		HAL_UART_Receive_IT(huart, &rx_byte, 1);
	}
}

// 2. Hàm Callback ngắt ngoài nút EXTI ( chạy tự động hi ấn nút PA0 )
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == GPIO_PIN_0)
	{
		flag_btn_pressed = 1; // Bật cờ, thoát ngắt ngay lập tức
	}
}

// 3. Hàm quy đổi góc 0-180 độ sang xung PWM nạp vào thanh ghi CCR2
void Set_Servo_Angle(int16_t angle)
{
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    // Đổi công thức sang dải 500us (0 độ) -> 2500us (180 độ)
    uint32_t ccr_val = 500 + ((uint32_t)angle * 2000) / 180;
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, ccr_val);
}

// 4. Chuyển đổi BCD <-> Decimal cho DS1307
uint8_t DS1307_BcdToDec(uint8_t bcd)
{
    return (uint8_t)(((bcd >> 4) * 10U) + (bcd & 0x0FU));
}

uint8_t DS1307_DecToBcd(uint8_t dec)
{
    return (uint8_t)(((dec / 10U) << 4) | (dec % 10U));
}

// 5. Đảm bảo bit CH (bit 7 thanh ghi 0x00) được xóa để DS1307 chạy dao động
HAL_StatusTypeDef DS1307_EnsureRunning(void)
{
    uint8_t sec_reg = 0;
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&hi2c2, DS1307_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, &sec_reg, 1, 50);
    if (st != HAL_OK)
    {
        return st;
    }
    if (sec_reg & 0x80U)
    {
        sec_reg &= 0x7FU;
        st = HAL_I2C_Mem_Write(&hi2c2, DS1307_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, &sec_reg, 1, 50);
    }
    return st;
}

// 6. Ghi thời gian 24h xuống DS1307 qua I2C2 (PB10/PB11)
HAL_StatusTypeDef DS1307_SetTime(const DS1307_Time_t *t)
{
    uint8_t buf[7];
    buf[0] = DS1307_DecToBcd(t->sec) & 0x7FU; // Bit 7 = 0 -> Bật dao động
    buf[1] = DS1307_DecToBcd(t->min);
    buf[2] = DS1307_DecToBcd(t->hour) & 0x3FU; // Bit 6 = 0 -> Chế độ 24h
    buf[3] = DS1307_DecToBcd(t->day);
    buf[4] = DS1307_DecToBcd(t->date);
    buf[5] = DS1307_DecToBcd(t->month);
    buf[6] = DS1307_DecToBcd(t->year);
    return HAL_I2C_Mem_Write(&hi2c2, DS1307_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, buf, 7, 50);
}

// 7. Đọc thời gian từ DS1307 qua I2C2 (PB10/PB11)
HAL_StatusTypeDef DS1307_GetTime(DS1307_Time_t *t)
{
    uint8_t buf[7] = {0};
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&hi2c2, DS1307_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, buf, 7, 50);
    if (st == HAL_OK)
    {
        t->sec   = DS1307_BcdToDec(buf[0] & 0x7FU);
        t->min   = DS1307_BcdToDec(buf[1] & 0x7FU);
        t->hour  = DS1307_BcdToDec(buf[2] & 0x3FU);
        t->day   = DS1307_BcdToDec(buf[3] & 0x07U);
        t->date  = DS1307_BcdToDec(buf[4] & 0x3FU);
        t->month = DS1307_BcdToDec(buf[5] & 0x1FU);
        t->year  = DS1307_BcdToDec(buf[6]);
    }
    return st;
}

// 8. Cập nhật dòng 1 LCD theo 2 trang (Trang 0: Giờ Việt Nam, Trang 1: Thời gian sweep_servo)
void Update_LCD_Header(void)
{
    char line0[17];
    if (lcd_page == 0U)
    {
        if (ds1307_ok)
        {
            snprintf(line0, sizeof(line0), "VN: %02u:%02u:%02u    ",
                     (unsigned)(rtc_time.hour % 24U),
                     (unsigned)(rtc_time.min % 60U),
                     (unsigned)(rtc_time.sec % 60U));
        }
        else
        {
            snprintf(line0, sizeof(line0), "STM32 Mini-SCADA");
        }
    }
    else
    {
        uint32_t tot_s = servo_active_ms / 1000U;
        uint32_t h = (tot_s / 3600U) % 100U;
        uint32_t m = (tot_s / 60U) % 60U;
        uint32_t s = tot_s % 60U;
        snprintf(line0, sizeof(line0), "SWP: %02lu:%02lu:%02lu   ",
                 (unsigned long)h, (unsigned long)m, (unsigned long)s);
    }
    lcd_put_cur(0, 0);
    lcd_send_string(line0);
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
