#include <stdint.h>

#include "bootloader.h"
#include "comm_interface.h"
#include "usart.h"

/** default baud */
#define BL_BAUD 115200

UART_HandleTypeDef *BL_UART = &huart6; // huart2 or huart6

static volatile uint8_t BL_UART_RX_INT_Count;
static volatile uint32_t Tick_Value;

/**
 * @brief send character
 * @param data char to be sent
 */
void BL_UART_Send_Char(char data)
{
    BL_UART->Instance->DR = data;
    while (__HAL_UART_GET_FLAG(BL_UART, UART_FLAG_TC) == 0)
        ;
}

/**
 * @brief  send string buffer
 * @param data input buffer
 * @param number of chars to send
 **/
void BL_UART_Send_Chars(char *data, uint32_t count)
{
    while (count--)
    {
        BL_UART_Send_Char(*data);
        data++;
    }
}

/**
 * @brief get character
 * @param timeout
 */
int BL_UART_Get_Char(uint32_t timeout)
{
    uint8_t ch;

    if (HAL_UART_Receive(BL_UART, &ch, 1, timeout) == HAL_OK)
    {
        return ch;
    }

    return -1;
}

/**
 * @brief get character
 * @param timeout
 * @retval number chars received
 */
int BL_UART_Get_Chars(char *buffer, uint32_t count, uint32_t timeout)
{
    if (HAL_UART_Receive(BL_UART, (uint8_t*)buffer, count, timeout) == HAL_OK)
    {
        return count;
    }

    return 0;
}

#if (BL_AUTO_BAUD == 1)
static void BL_UART_RX_INT_Reset(void)
{
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_3);
}

static void BL_UART_RX_INT_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /*Configure GPIO pin : PA3 */
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* EXTI interrupt init*/
    HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);
}
#endif

uint8_t BL_UART_Init()
{
    uint32_t baud = BL_BAUD;
    uint8_t xreturn = 0;

#if (BL_AUTO_BAUD == 1)
    /* auto baud detection ST AN4908 */

    /* temporarily suspend systick interrupt */
    HAL_SuspendTick();

    /* temporarily deinit uart*/
    HAL_UART_DeInit(BL_UART);

    /* temporarily enable rising edge interrupt on uart rx pin */
    BL_UART_RX_INT_Init();

    /* wait for two rising edge interrupts on uart rx pin */
    uint32_t timeout = HAL_RCC_GetHCLKFreq()/5;
    while (BL_UART_RX_INT_Count < 2 && --timeout)
        ;

    if (timeout)
    {
        baud = (8 * HAL_RCC_GetHCLKFreq()) / (0xFFFFFF - Tick_Value + 1);
        xreturn = 1;
    }

    /* reset uart rx pin */
    BL_UART_RX_INT_Reset();

    /* reconfigure systick to generate 1ms interrupt */
    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);

    HAL_ResumeTick();
#endif

    /* update baud rate */
    BL_UART->Init.BaudRate = baud;

    /* init uart with new baud */
    if (HAL_UART_Init(BL_UART) != HAL_OK)
    {
        Error_Handler();
    }

    return xreturn;
}

void BL_UART_Deinit()
{
    HAL_UART_DeInit(BL_UART);
}

#if (BL_AUTO_BAUD == 1)
/**
 * @brief This function handles uart rx pin interrupt.
 */
void EXTI3_IRQHandler(void)
{
    /* USER CODE BEGIN EXTI3_IRQn 0 */
    if (BL_UART_RX_INT_Count == 0)
    {
        /* reset systic, systick is down counter*/
        SysTick->LOAD = 0xFFFFFF;
        SysTick->VAL = 0;
        BL_UART_RX_INT_Count++;
    }
    else
    {
        Tick_Value = SysTick->VAL;
        BL_UART_RX_INT_Count++;
    }
    /* USER CODE END EXTI3_IRQn 0 */
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3);
    /* USER CODE BEGIN EXTI3_IRQn 1 */

    /* USER CODE END EXTI3_IRQn 1 */
}
#endif
