/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE {
    HAL_UART_Transmit(&huart1, (uint8_t *) &ch, 1, HAL_MAX_DELAY);
    return ch;
}

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define TYPE_BUTTON 0x80
#define TYPE_ENC 0x00
#define BUTTON_PRESS 0x40
#define BUTTON_RELEASE 0x00
#define ENCODER_FW 0x40
#define ENCODER_BW 0x00

#define CENTER (1<<6)

#define INTERVAL 500
#define BUTTON_DEBOUNCE_TIME 15

#define BUTTON_ID(x) (TYPE_BUTTON | (x + 16))
#define ENCODER_BUTTON_ID(x) (TYPE_BUTTON | (x))
#define ENCODER_ID(x) (TYPE_ENC | x)
#define BASE_ID(x) (x & ~TYPE_BUTTON)
#define INVALID_ID ((uint8_t)0xff)


#define BUTTON_STATE(in, mask) (in & mask ? BUTTON_RELEASE : BUTTON_PRESS)

#define SEND_EVENT(out) HAL_UART_Transmit(&huart1, &out, 1, 100);

static uint8_t LAG = 0xfe;

enum encoder_event {
    EE_NONE, EE_FWD, EE_BWD, EE_INVALID
};
enum encoder_event encoder_table[4][4] = {
        {EE_NONE,    EE_FWD,     EE_BWD,     EE_INVALID},
        {EE_BWD,     EE_NONE,    EE_INVALID, EE_FWD},
        {EE_FWD,     EE_INVALID, EE_NONE,    EE_BWD},
        {EE_INVALID, EE_BWD,     EE_FWD,     EE_NONE}
};

const uint8_t id_table[4][16] = {
        {BUTTON_ID(0), BUTTON_ID(1), BUTTON_ID(2), BUTTON_ID(3), BUTTON_ID(4), BUTTON_ID(5), BUTTON_ID(6), BUTTON_ID(7), BUTTON_ID(8), BUTTON_ID(9), BUTTON_ID(10), BUTTON_ID(11), BUTTON_ID(12), INVALID_ID, ENCODER_ID(0), ENCODER_ID(0)},
        {INVALID_ID, BUTTON_ID(13), BUTTON_ID(14), BUTTON_ID(15), BUTTON_ID(16), BUTTON_ID(17), BUTTON_ID(18), BUTTON_ID(19), ENCODER_BUTTON_ID(0), ENCODER_BUTTON_ID(1), ENCODER_BUTTON_ID(2), ENCODER_BUTTON_ID(3), ENCODER_BUTTON_ID(4), ENCODER_BUTTON_ID(5), ENCODER_BUTTON_ID(6), ENCODER_BUTTON_ID(7)},
        {ENCODER_ID(1), ENCODER_ID(1), ENCODER_ID(2), ENCODER_ID(2), ENCODER_ID(3), ENCODER_ID(3), ENCODER_ID(4), ENCODER_ID(4), ENCODER_ID(5), ENCODER_ID(5), ENCODER_ID(6), ENCODER_ID(6), ENCODER_ID(7), ENCODER_ID(7), ENCODER_ID(8), ENCODER_ID(8)},
        {ENCODER_ID(9), ENCODER_ID(9), BUTTON_ID(20), BUTTON_ID(21), BUTTON_ID(22), BUTTON_ID(23), BUTTON_ID(24), INVALID_ID, ENCODER_ID(10), ENCODER_ID(10), BUTTON_ID(25), BUTTON_ID(26), BUTTON_ID(27), BUTTON_ID(28), BUTTON_ID(29), INVALID_ID}
};


uint8_t enc_locks[9] = {};
#define encoder_lock(x) enc_locks[BASE_ID(x)]

static inline enum encoder_event decode_encoder(uint16_t in, uint16_t state, int shift){
    return encoder_table[(state >> shift) & 0x3][(in >> shift) & 0x3];
}


static inline void scan_row_0(){
    //static uint8_t button_locks[13];
    static uint16_t previous;
    
    uint16_t in = GPIOB->IDR;
    
    //strobe the next row ASAP to give it time to settle
    GPIOA->ODR = 0b1101;

    uint16_t edges = in ^ previous;
    //Scan 12 buttons

    for (int i = 0; i < 13; i++) {
        int mask = 1 << i;
        if (edges & mask) {
            uint8_t out = id_table[0][i] | BUTTON_STATE(in, mask);
            SEND_EVENT(out);
            //button_locks[i] = BUTTON_DEBOUNCE_TIME;
        }
    }

    //Scan 1 encoder
    int i = 14;
    uint8_t id = id_table[0][i];
    if (encoder_lock(id)) {
        encoder_lock(id)--;
    } else {
        switch(decode_encoder(in, previous, i)){
            case EE_FWD: {
                uint8_t out = id | ENCODER_FW;// | 0
                SEND_EVENT(out);
            }
                break;
            case EE_BWD: {
                uint8_t out = id | ENCODER_BW;// | 0
                SEND_EVENT(out);
            }
                break;
            case EE_NONE:
            case EE_INVALID:
                break;
        }
    }
    previous = in;
}

static inline void scan_row_1(){
    static uint16_t previous;
    uint16_t in = GPIOB->IDR;
    GPIOA->ODR = 0b1011;
    uint16_t edges = in ^ previous;
    previous = in;

    for (int i = 1; i < 7; i++) {
        int mask = 1 << i;
        if (edges & mask) {
            uint8_t out = id_table[1][i] | BUTTON_STATE(in, mask);
            SEND_EVENT(out);
        }
    }

    //Scan encoder buttons
    for (int i = 7; i < 16; i++) {
        int mask = 1 << i;
        if (edges & mask) {
            //This corresponds to the ID of the associated encoder...
            uint8_t id = id_table[1][i];
            uint8_t out = id | BUTTON_STATE(in, mask);
            SEND_EVENT(out);
            //...therefore we can use it to lock that encoder for a while
            encoder_lock(id) = 10;
        }
    }
    previous = in;
}

static inline void scan_row_2(){
    static uint16_t previous;
    uint16_t in = GPIOB->IDR;;
    uint16_t edges = in ^ previous;

    GPIOA->ODR = 0b0111;

    for (uint8_t i = 16; i < 6; i+= 2) {
        uint8_t id = id_table[2][i];
        if (encoder_lock(id)) {
            encoder_lock(id)--;
        } else {
            switch(decode_encoder(in, previous, i)){
                case EE_NONE:
                    break;
                case EE_FWD: {
                    uint8_t out = TYPE_ENC | ENCODER_FW | (i + 1);
                    SEND_EVENT(out);
                }
                    break;
                case EE_BWD: {
                    uint8_t out = TYPE_ENC | ENCODER_BW | (i + 1);
                    SEND_EVENT(out);
                }
                    break;
                case EE_INVALID:
                    break;
            }
        }
    }
    previous = in;
}



static inline void scan_row_3(){
    static uint16_t previous;
    uint16_t in = GPIOB->IDR;
    uint16_t edges = in ^ previous;
    GPIOA->ODR = 0b1110;
    //The encoders move 2 times per click so we need to keep track of that
    static int8_t joyenc_steps[2];

    for (int k = 0; k < 2; k ++) {
        int i = k * 8;
        uint8_t encoder_id = id_table[3][i];
        //First two pins are the encoder
        switch (decode_encoder(in, previous, i)) {
            case EE_FWD: {
                if (joyenc_steps[k] >= 2) {
                    uint8_t out = encoder_id | ENCODER_BW;
                    HAL_UART_Transmit(&huart1, &out, 1, 100);
                    joyenc_steps[k] = 0;
                }
            }
                break;
            case EE_BWD: {
                joyenc_steps[k]--;
                if (joyenc_steps[k] <= -2) {
                    uint8_t out = encoder_id | ENCODER_FW;
                    SEND_EVENT(out);
                    joyenc_steps[k] = 0;
                }
            }
                break;
            case EE_NONE:
            case EE_INVALID:
                break;
        }
        const int center_index = i + 6;
        //Check if a direction is currently depressed. If it is, that will be the input ID we send out, otherwise it will be the center button's ID
        int direction = center_index;
        for(int x = 2; x < 6; x++){
            int mask = (1 << (i + x));
            //if the direction is low, that means it's being pressed
            if((in & mask) == 0){
                direction = i+x;
            }
        }
        int id = id_table[3][direction];
        int center_mask = 1 << center_index;
        //only edges on the center button trigger an action
        if(edges & (center_mask)){
            uint8_t out = id | BUTTON_STATE(in, center_mask);
            SEND_EVENT(out);
        }
    }
    previous = in;
}

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
  MX_USART1_UART_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

    GPIOA->ODR = 0b1110;
    HAL_Delay(1);
    HAL_TIM_Base_Start(&htim1);

    while (1) {
        scan_row_0();
        //wait for the minimum strobe interval to pass
        if (__HAL_TIM_GET_COUNTER(&htim1) > INTERVAL) HAL_UART_Transmit(&huart1, &LAG, 1, 100);;
        while (__HAL_TIM_GET_COUNTER(&htim1) < INTERVAL);

        scan_row_1();

        if (__HAL_TIM_GET_COUNTER(&htim1) > 2 * INTERVAL) HAL_UART_Transmit(&huart1, &LAG, 1, 100);;
        while (__HAL_TIM_GET_COUNTER(&htim1) < 2 * INTERVAL);

        scan_row_2();

        if (__HAL_TIM_GET_COUNTER(&htim1) > 3 * INTERVAL) HAL_UART_Transmit(&huart1, &LAG, 1, 100);;
        while (__HAL_TIM_GET_COUNTER(&htim1) < 3 * INTERVAL);
        
        scan_row_3();

        //instead of waiting for 4*INTERVAL, we're waiting for the timer to wrap back to 0
        if (__HAL_TIM_GET_COUNTER(&htim1) < 3 * INTERVAL) HAL_UART_Transmit(&huart1, &LAG, 1, 100);
        while (__HAL_TIM_GET_COUNTER(&htim1) >= 3 * INTERVAL);
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    //}
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL12;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 48;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 2000;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_8;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, R0_Pin|R1_Pin|R2_Pin|R3_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : R0_Pin R1_Pin R2_Pin R3_Pin */
  GPIO_InitStruct.Pin = R0_Pin|R1_Pin|R2_Pin|R3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : C0_Pin C1_Pin C2_Pin C10_Pin
                           C11_Pin C12_Pin C13_Pin C14_Pin
                           C15_Pin C3_Pin C4_Pin C5_Pin
                           C6_Pin C7_Pin C8_Pin C9_Pin */
  GPIO_InitStruct.Pin = C0_Pin|C1_Pin|C2_Pin|C10_Pin
                          |C11_Pin|C12_Pin|C13_Pin|C14_Pin
                          |C15_Pin|C3_Pin|C4_Pin|C5_Pin
                          |C6_Pin|C7_Pin|C8_Pin|C9_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

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
    while (1) {
    }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
