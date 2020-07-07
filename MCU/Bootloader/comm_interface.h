#include <stdint.h>

uint8_t BL_UART_Init();
void BL_UART_Deinit();

void BL_UART_Send_Char(char data);
void BL_UART_Send_Chars(char *data, uint32_t count);

int BL_UART_Get_Char(uint32_t timeout);
int BL_UART_Get_Chars(char *buffer, uint32_t count, uint32_t timeout);

uint8_t BL_CDC_Init();
void BL_CDC_Deinit();

void BL_CDC_Send_Char(char data);
void BL_CDC_Send_Chars(char *data, uint32_t count);

int BL_CDC_Get_Char(uint32_t timeout);
int BL_CDC_Get_Chars(char *buffer, uint32_t count, uint32_t timeout);
