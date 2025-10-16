/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include <string.h>
#include <stdio.h>
#define MSG_SIZE 12
#define REPLY_SIZE 2
uint8_t rxBuffer[MSG_SIZE];
uint8_t replyBuffer[REPLY_SIZE] = {'O','K'};
volatile uint8_t txRequested = 0;
//uint8_t rxBuffer[32];
uint16_t rxSize = 0;
int ret;
char dataBuffer[] = "New Hello world!";
I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart2;
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_I2C1_Init();

    HAL_UART_Transmit(&huart2, (uint8_t*)"STM32 I2C Slave Ready...\r\n", 26, HAL_MAX_DELAY);

    uint8_t rxBuf[MSG_SIZE];
    uint8_t reply[2] = {'O', 'K'};

    while (1)
    {
        // 1️⃣ Wait for master write
        if (HAL_I2C_Slave_Receive(&hi2c1, rxBuf, MSG_SIZE, HAL_MAX_DELAY) == HAL_OK)
        {
            HAL_UART_Transmit(&huart2, (uint8_t*)"Received: ", 10, HAL_MAX_DELAY);
            HAL_UART_Transmit(&huart2, rxBuf, MSG_SIZE, HAL_MAX_DELAY);
            HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);
        }
        else
        {
            HAL_UART_Transmit(&huart2, (uint8_t*)"Receive Error...\r\n", 15, HAL_MAX_DELAY);
            continue; // try again
        }

        // 2️⃣ Wait for master read
        if (HAL_I2C_Slave_Transmit(&hi2c1, reply, sizeof(reply), HAL_MAX_DELAY) == HAL_OK)
        {
            HAL_UART_Transmit(&huart2, (uint8_t*)"Replied OK\r\n", 12, HAL_MAX_DELAY);
        }
        else
        {
            HAL_UART_Transmit(&huart2, (uint8_t*)"Reply Error\r\n", 13, HAL_MAX_DELAY);
        }

        HAL_Delay(500);
    }
}

 /* Called when master addresses STM32. TransferDirection: 1 = master->slave (WRITE), 0 = master<-slave (READ) */
 void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode)
 {
     char msg[128];
     int n = snprintf(msg, sizeof(msg), "AddrCallback: dir=%d addr=0x%02X state=0x%08lX\r\n",
                      TransferDirection, (unsigned int)(AddrMatchCode >> 1),
                      (unsigned long)hi2c->State);
     HAL_UART_Transmit(&huart2, (uint8_t*)msg, n, HAL_MAX_DELAY);

     if (TransferDirection == I2C_DIRECTION_TRANSMIT) {
         /* Master -> Slave (write). Disable listen so we can start receive safely */
         HAL_I2C_DisableListen_IT(hi2c);

         /* wait small time for peripheral to become ready (guarded timeout) */
         uint32_t to = HAL_GetTick() + 50;
         while (hi2c->State != HAL_I2C_STATE_READY && HAL_GetTick() < to) { /* spin */ }

         /* Try starting receive with a few retries */
         int attempts = 3;
         while (attempts--) {
             if (HAL_I2C_Slave_Receive_IT(hi2c, rxBuffer, MSG_SIZE) == HAL_OK) {
                 return; /* success */
             } else {
                 int err = (int)hi2c->ErrorCode;
                 int m = snprintf(msg, sizeof(msg), "Slave Receive IT attempt fail, err=0x%08lX tries_left=%d\r\n",
                                  (unsigned long)err, attempts);
                 HAL_UART_Transmit(&huart2, (uint8_t*)msg, m, HAL_MAX_DELAY);
                 HAL_Delay(5);
             }
         }

         /* If we reach here, all attempts failed -> re-enable listen to recover */
         HAL_I2C_EnableListen_IT(hi2c);
         HAL_UART_Transmit(&huart2, (uint8_t*)"Slave Receive: permanent fail, re-enabled listen\r\n", 45, HAL_MAX_DELAY);
     }
     else if (TransferDirection == I2C_DIRECTION_RECEIVE) {
         /* Master -> reading from slave: transmit immediately */
         if (HAL_I2C_Slave_Transmit_IT(hi2c, replyBuffer, REPLY_SIZE) != HAL_OK) {
             int err = (int)hi2c->ErrorCode;
             int m = snprintf(msg, sizeof(msg), "Slave Transmit IT failed err=0x%08lX\r\n", (unsigned long)err);
             HAL_UART_Transmit(&huart2, (uint8_t*)msg, m, HAL_MAX_DELAY);
         }
     }
 }

 /* Called when slave receive completed */
 void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
 {
     HAL_UART_Transmit(&huart2, (uint8_t*)"Received: ", 10, HAL_MAX_DELAY);
     HAL_UART_Transmit(&huart2, rxBuffer, MSG_SIZE, HAL_MAX_DELAY);
     HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);

     txRequested = 1;

     /* Re-enable listen so we can accept the next master addressing */
     if (HAL_I2C_EnableListen_IT(hi2c) != HAL_OK) {
         HAL_UART_Transmit(&huart2, (uint8_t*)"Re-enable Listen failed after RxCplt\r\n", 38, HAL_MAX_DELAY);
     }
 }

 /* Called after listen mode transaction finishes (STOP detected): re-enable listen */
 void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c)
 {
     /* It's OK to re-enable listen here as well (guarded in callbacks too) */
     if (HAL_I2C_EnableListen_IT(hi2c) != HAL_OK) {
         HAL_UART_Transmit(&huart2, (uint8_t*)"Re-enable Listen failed in ListenCplt\r\n", 38, HAL_MAX_DELAY);
     }
 }

 /* Error callback (prints driver error and attempts recovery) */
 void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
 {
     char msg[80];
     int n = snprintf(msg, sizeof(msg), "I2C Error: 0x%08lX\r\n", (unsigned long)hi2c->ErrorCode);
     HAL_UART_Transmit(&huart2, (uint8_t*)msg, n, HAL_MAX_DELAY);

     /* Try soft recovery: deinit/re-init and re-enable listen */
     HAL_I2C_DeInit(hi2c);
     HAL_Delay(10);
     MX_I2C1_Init();
     HAL_I2C_EnableListen_IT(hi2c);
 }

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
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

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = (0x04 << 1);
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
