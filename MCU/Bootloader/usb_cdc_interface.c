#include "bootloader.h"

#if (USE_USB_CDC == 1)
/** standaed includes */
#include <stdint.h>

/** bootloader includes */
#include "comm_interface.h"

/** st includes */
#include "usbd_cdc_if.h"

struct Ring_Buffer_t
{
    uint8_t *Storage;
    uint8_t Full_Flag;
    uint16_t Write_Index;
    uint16_t Read_Index;
    uint16_t Size;
};

uint8_t CDC_RX_Buffer[1024];
struct Ring_Buffer_t CDC_RB = {CDC_RX_Buffer, 0, 0, 0, sizeof(CDC_RX_Buffer)};

static uint16_t RB_Get_Count()
{
    if (CDC_RB.Full_Flag)
    {
        return CDC_RB.Size;
    }
    if (CDC_RB.Write_Index >= CDC_RB.Read_Index)
    {
        return CDC_RB.Write_Index - CDC_RB.Read_Index;
    }

    return CDC_RB.Size - (CDC_RB.Read_Index - CDC_RB.Write_Index);
}

/**
 * @brief send character
 * @param data char to be sent
 */
void BL_CDC_Send_Char(char data)
{
    CDC_Transmit_FS((uint8_t *)&data, 1);
}

/**
 * @brief  send string buffer
 * @param data input buffer
 * @param number of chars to send
 **/
void BL_CDC_Send_Chars(char *data, uint32_t count)
{
    CDC_Transmit_FS((uint8_t *)data, count);
}

/**
 * @brief get character
 * @param timeout
 */
int BL_CDC_Get_Char(uint32_t timeout)
{
    uint8_t ch;

    while (--timeout && CDC_RB.Write_Index == CDC_RB.Read_Index && !CDC_RB.Full_Flag)
    {
        HAL_Delay(1);
    }

    if (timeout)
    {
        ch = CDC_RB.Storage[CDC_RB.Read_Index++];
        if (CDC_RB.Read_Index == CDC_RB.Size)
        {
            CDC_RB.Read_Index = 0;
        }
        CDC_RB.Full_Flag = 0;
        return ch;
    }

    return -1;
}

/**
 * @brief get character
 * @param timeout
 * @reval number chaers received
 */
int BL_CDC_Get_Chars(char *buffer, uint32_t count, uint32_t timeout)
{
    uint32_t temp;

    while (--timeout && RB_Get_Count() < count)
    {
        HAL_Delay(1);
    }

    if (!timeout)
    {
        count = RB_Get_Count();
    }

    temp = count;

    while (count--)
    {
        *buffer++ = CDC_RB.Storage[CDC_RB.Read_Index++];
        if (CDC_RB.Read_Index == CDC_RB.Size)
        {
            CDC_RB.Read_Index = 0;
        }
        CDC_RB.Full_Flag = 0;
    }

    return temp;
}

/** called from CDC_Receive_FS() in @usbd_cdc_if.c */
void CDC_Receive_FS_ISR(uint8_t *buff, uint32_t len)
{
    while (len--)
    {
        CDC_RB.Storage[CDC_RB.Write_Index++] = *buff++;
        if (CDC_RB.Write_Index == CDC_RB.Size)
        {
            CDC_RB.Write_Index = 0;
        }
        CDC_RB.Full_Flag = (CDC_RB.Write_Index == CDC_RB.Read_Index);
    }
}

uint8_t BL_CDC_Init()
{
    return 1;
}

void BL_CDC_Deinit()
{
    extern USBD_HandleTypeDef hUsbDeviceFS;
    USBD_DeInit(&hUsbDeviceFS);
}
#endif
