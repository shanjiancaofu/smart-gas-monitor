/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    tim.c
  * @brief   This file provides code for the configuration
  *          of the TIM instances.
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
#include "tim.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

TIM_HandleTypeDef htim2;

/* TIM2 init function */
void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 71;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 9999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* tim_baseHandle)
{

  if(tim_baseHandle->Instance==TIM2)
  {
  /* USER CODE BEGIN TIM2_MspInit 0 */

  /* USER CODE END TIM2_MspInit 0 */
    /* TIM2 clock enable */
    __HAL_RCC_TIM2_CLK_ENABLE();

    /* TIM2 interrupt Init */
    HAL_NVIC_SetPriority(TIM2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
  /* USER CODE BEGIN TIM2_MspInit 1 */

  /* USER CODE END TIM2_MspInit 1 */
  }
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef* tim_baseHandle)
{

  if(tim_baseHandle->Instance==TIM2)
  {
  /* USER CODE BEGIN TIM2_MspDeInit 0 */

  /* USER CODE END TIM2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_TIM2_CLK_DISABLE();

    /* TIM2 interrupt Deinit */
    HAL_NVIC_DisableIRQ(TIM2_IRQn);
  /* USER CODE BEGIN TIM2_MspDeInit 1 */

  /* USER CODE END TIM2_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* TIM4_CH3 驱动舵机阀门，但 CubeMX 的 .ioc 里没有 TIM4：它的键名格式逆推过
 * 两种写法，CubeMX 都拒绝并丢弃，所以这里干脆不依赖生成器——句柄、初始化函数
 * 和声明全部放在 USER CODE 段，CubeMX 重新生成代码时不会碰。
 *
 * 同理，TIM4 的时钟使能由 bsp_servo_init() 自己做，不依赖 msp.c 里的
 * HAL_TIM_PWM_MspInit（那个函数在 .ioc 没有 PWM 外设时会被整个删掉）。
 *
 * 如果以后在 CubeMX 图形界面里把 TIM4 补进 .ioc，这段就可以删掉，改用生成版。 */
TIM_HandleTypeDef htim4;

void MX_TIM4_Init(void)
{
    TIM_OC_InitTypeDef channel = {0};

    htim4.Instance = TIM4;
    htim4.Init.Prescaler = 71;   /* 72 MHz / 72 = 1 MHz 计数 */
    htim4.Init.Period = 19999;   /* 1 MHz / 20000 = 50 Hz */
    htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    if (HAL_TIM_PWM_Init(&htim4) != HAL_OK) {
        Error_Handler();
    }
    channel.OCMode = TIM_OCMODE_PWM1;
    channel.Pulse = 1000;        /* 先装载关阀位置，PWM 一启动就是关的 */
    channel.OCPolarity = TIM_OCPOLARITY_HIGH;
    if (HAL_TIM_PWM_ConfigChannel(&htim4, &channel, TIM_CHANNEL_3) != HAL_OK) {
        Error_Handler();
    }
}

/* USER CODE END 1 */



