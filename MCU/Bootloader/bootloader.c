/**
 * @file bootloader.c
 * @brief Implements bootloader control commands
 * @author xyz
 * @version 0.1.7
 */

/**
 * Change Log
 ****V0.1.4***
 *   1. Get_Version cmd added
 *   2. jump cmd fixed for f103 mcu
 *****V0.1.5***
 *   1. auto baud detection added
 *   2. bootloader size changed to 16K from 32K
 ******V0.1.6***
 *   1. independent comm ineterface from bootloader
 *   2. usb cdc interface
 ******V0.1.7***
 *   1.using magic number to decide to bootloader
 * */

/** stdandard includes */
#include <string.h>
#include <stdlib.h>

/** booloader includes */
#include "bootloader.h"
#include "comm_interface.h"

#include "usart.h"
#include "main.h"

/**
 * @addtogroup Bootloader
 * @{
 */

/**
 * @defgroup Bootloader_Defines
 * @{
 */

#define BL_VERSION_MAJOR (0)
#define BL_VERSION_MINOR (1)
#define BL_VERSION_BUILD (7)

#define BL_RX_BUFFER_SIZE (1024)
#define BL_TX_BUFFER_SIZE (1024)

static uint8_t BL_RX_Buffer[BL_RX_BUFFER_SIZE];
static uint8_t BL_TX_Buffer[BL_TX_BUFFER_SIZE];

#ifdef STM32F103xE
#include "stm32f1xx_hal.h"
/**
 *  STM32F103RE -- 512KB total flash.
 *  Bootloader resides in pages 0,1--7 2KB*8 Flash.
 *  Total pages in 103RE are 256.
 *  pages to erase 8 to 255, except pages 0 to 7 (bootloader).
 *  Erase type- pages.
 *  Programaing voltage- 2.7v to 3.3v.
 *  Flash writing width - double word.
 */
#define BL_PAGE_SIZE (2 * 1024) // 2KB
#define BL_TOTAL_PAGES 255      // 255*2KB
#define BL_USED_PAGES 8         // 8*2KB

#define USER_FLASH_START_ADDRESS (0x08000000 + BL_USED_PAGES * BL_PAGE_SIZE)
#define USER_FLASH_END_ADDRESS (0x08000000 + 512 * 1024)
#endif

#ifdef STM32F103xB
#include "stm32f1xx_hal.h"
/**
 *  STM32F103C8 -- 64KB total flash.
 *  Bootloader resides in pages 0,1--7 1KB*8 Flash.
 *  Total pages in 103C* are 64.
 *  pages to erase 8 to 64, except pages 0 to 7 (bootloader).
 *  Erase type- pages.
 *  Programaing voltage- 2.7v to 3.3v.
 *  Flash writing width - double word.
 */
#define BL_PAGE_SIZE (1 * 1024) // 1KB
#define BL_TOTAL_PAGES 64       // 64*1KB
#define BL_USED_PAGES 8         // 8*1KB

#define USER_FLASH_START_ADDRESS (0x08000000 + BL_USED_PAGES * BL_PAGE_SIZE)
#define USER_FLASH_END_ADDRESS (0x08000000 + 64 * 1024)
#endif

#ifdef STM32F401xE
#include "stm32f4xx_hal.h"
/**
 *  STM32F401RE -- 512KB total flash.
 *  Bootloader resides in sector 0 -- 16KB Flash.
 *  Total sectors in 401RE are 8.
 *  Sectors to erase 7, except sector 0 (bootloader).
 *  Erase type- sector by sector.
 *  Programaing voltage- 2.7v to 3.3v.
 *  Flash writing width - byte by byte for 2.7v to 3.3v.
 */
#define BL_SECTOR_SIZE (16 * 1024) //16KB
#define BL_TOTAL_SECTORS 8         // 16KB+16KB+16KB+16KB+64KB+128KB+128KB+128KB
#define BL_USED_SECTORS 1          // 16KB

#define USER_FLASH_START_ADDRESS (0x08000000 + BL_USED_SECTORS * BL_SECTOR_SIZE)
#define USER_FLASH_END_ADDRESS (0x08000000 + 512 * 1024)
#endif

#ifdef STM32F407xx
#include "stm32f4xx_hal.h"
/**
 *  STM32F407xx -- 1024K total flash.
 *  Bootloader resides in sector 0 -- 16KB Flash.
 *  Total sectors in 407VG are 12.
 *  Sectors to erase 11, except sector 0 (bootloader).
 *  Erase type- sector by sector.
 *  Programaing voltage- 2.7v to 3.3v.
 *  Flash writing width - byte by byte for 2.7v to 3.3v.
 */
#define BL_SECTOR_SIZE (16 * 1024) //16KB
#define BL_TOTAL_SECTORS 12        // 16KB+16KB+16KB+16KB+64KB+128KB+128KB+128KB...128KB
#define BL_USED_SECTORS 2          // 16KB*2

#define USER_FLASH_START_ADDRESS (0x08000000 + BL_USED_SECTORS * BL_SECTOR_SIZE)
#define USER_FLASH_END_ADDRESS (0x08000000 + 1024 * 1024) // 32KB used by bootloader remaining
#endif

/*
CMD_WRITE, CMD_VERIFY Frame
[SYNC_CHAR + frame len] frame len = 9 + payload len
[1-byte cmd + 1-byte no of bytes to write + 0x00 + 0x00 + 4-byte address +  payload + 1-byte CRC]
*/

/*
CMD_READ Frame
[SYNC_CHAR + frame len] frame len = 9
[1-byte cmd + 1-byte no of bytes to read + 0x00 + 0x00 + 4-byte address + 1-byte CRC]
*/

/*
CMD_ERASE, CMD_RESET, CMD_JUMP, CMD_GETVER Frame
[SYNC_CHAR + frame len] frame len = 2
[1-byte cmd + 1-byte CRC]
*/

#define BL_CMD_WRITE 0x50
#define BL_CMD_READ 0x51
#define BL_CMD_ERASE 0x52
#define BL_CMD_RESET 0x53
#define BL_CMD_JUMP 0x54
#define BL_CMD_VERIFY 0x55
#define BL_CMD_GETVER 0x56

#define BL_CMD_ACK 0x90
#define BL_CMD_NACK 0x91
#define BL_CMD_ERROR 0x92

#define BL_SYNC_CHAR '$'

/* used for auto baud detection ST AN4908*/
#define BL_CMD_CONNECT 0x7F

/**
 * @}
 */
/* End of Bootloader_Defines */

/**
 * @defgroup Bootloader_extern_Variables
 * @{
 */

/**
  * @}
  */
/* End of Bootloader_extern_Variables */

/**
  * @defgroup Bootloader_Global_Variables
  * @{
  */

/* bootloader version number */
/* Major, minor, build  eg 0.1.2*/
static uint8_t BL_Version[3] = {BL_VERSION_MAJOR, BL_VERSION_MINOR, BL_VERSION_BUILD};

/* Maxim APPLICATION NOTE 27 */
uint8_t BL_CRC8_Table[] =
    {
        0x00, 0x5E, 0xBC, 0xE2, 0x61, 0x3F, 0xDD, 0x83, 0xC2, 0x9C, 0x7E, 0x20, 0xA3, 0xFD, 0x1F, 0x41,
        0x9D, 0xC3, 0x21, 0x7F, 0xFC, 0xA2, 0x40, 0x1E, 0x5F, 0x01, 0xE3, 0xBD, 0x3E, 0x60, 0x82, 0xDC,
        0x23, 0x7D, 0x9F, 0xC1, 0x42, 0x1C, 0xFE, 0xA0, 0xE1, 0xBF, 0x5D, 0x03, 0x80, 0xDE, 0x3C, 0x62,
        0xBE, 0xE0, 0x02, 0x5C, 0xDF, 0x81, 0x63, 0x3D, 0x7C, 0x22, 0xC0, 0x9E, 0x1D, 0x43, 0xA1, 0xFF,
        0x46, 0x18, 0xFA, 0xA4, 0x27, 0x79, 0x9B, 0xC5, 0x84, 0xDA, 0x38, 0x66, 0xE5, 0xBB, 0x59, 0x07,
        0xDB, 0x85, 0x67, 0x39, 0xBA, 0xE4, 0x06, 0x58, 0x19, 0x47, 0xA5, 0xFB, 0x78, 0x26, 0xC4, 0x9A,
        0x65, 0x3B, 0xD9, 0x87, 0x04, 0x5A, 0xB8, 0xE6, 0xA7, 0xF9, 0x1B, 0x45, 0xC6, 0x98, 0x7A, 0x24,
        0xF8, 0xA6, 0x44, 0x1A, 0x99, 0xC7, 0x25, 0x7B, 0x3A, 0x64, 0x86, 0xD8, 0x5B, 0x05, 0xE7, 0xB9,
        0x8C, 0xD2, 0x30, 0x6E, 0xED, 0xB3, 0x51, 0x0F, 0x4E, 0x10, 0xF2, 0xAC, 0x2F, 0x71, 0x93, 0xCD,
        0x11, 0x4F, 0xAD, 0xF3, 0x70, 0x2E, 0xCC, 0x92, 0xD3, 0x8D, 0x6F, 0x31, 0xB2, 0xEC, 0x0E, 0x50,
        0xAF, 0xF1, 0x13, 0x4D, 0xCE, 0x90, 0x72, 0x2C, 0x6D, 0x33, 0xD1, 0x8F, 0x0C, 0x52, 0xB0, 0xEE,
        0x32, 0x6C, 0x8E, 0xD0, 0x53, 0x0D, 0xEF, 0xB1, 0xF0, 0xAE, 0x4C, 0x12, 0x91, 0xCF, 0x2D, 0x73,
        0xCA, 0x94, 0x76, 0x28, 0xAB, 0xF5, 0x17, 0x49, 0x08, 0x56, 0xB4, 0xEA, 0x69, 0x37, 0xD5, 0x8B,
        0x57, 0x09, 0xEB, 0xB5, 0x36, 0x68, 0x8A, 0xD4, 0x95, 0xCB, 0x29, 0x77, 0xF4, 0xAA, 0x48, 0x16,
        0xE9, 0xB7, 0x55, 0x0B, 0x88, 0xD6, 0x34, 0x6A, 0x2B, 0x75, 0x97, 0xC9, 0x4A, 0x14, 0xF6, 0xA8,
        0x74, 0x2A, 0xC8, 0x96, 0x15, 0x4B, 0xA9, 0xF7, 0xB6, 0xE8, 0x0A, 0x54, 0xD7, 0x89, 0x6B, 0x35};

/**
 * @}
 */
/* End of Bootloader_Global_Variables */

/**
 * @defgroup Bootloader_Function_Protypes
 * @{
 */

static uint8_t ST_Erase_Flash(void);
static uint8_t BL_CRC8(uint8_t *data, uint8_t len);
static void BL_Write_Callback(uint32_t address, const uint8_t *data, uint32_t len);
static void BL_Verify_Callback(uint32_t address, const uint8_t *data, uint8_t len);
static void BL_Read_Callback(uint32_t address, uint32_t len);
static void BL_Erase_Callback(void);
static void BL_Jump_Callback(void);
static void BL_Jump(void);
static void BL_Get_Version_Callback(void);
static void BL_Loop(void);

static void (*BL_COMM_Deinit)(void);

static void (*BL_Send_Char)(char data);
static void (*BL_Send_Chars)(char *data, uint32_t count);

static int (*BL_Get_Char)(uint32_t timeout);
static uint32_t (*BL_Get_Chars)(char *buffer, uint32_t count, uint32_t timeout);

/**
 * @}
 */
/* End of Bootloader_Function_Protypes */

/**
 * @defgroup Bootloader_Implementation
 * @{
 */

/**
 * @brief calculate 8 bit crc on input buffer
 * @param data input buffer
 * @param len number of bytes in input buffer
 * @retval return calculated crc
 */
static uint8_t BL_CRC8(uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;

    for (uint8_t i = 0; i < len; i++)
    {
        crc = BL_CRC8_Table[crc ^ data[i]];
    }

    return crc;
}

/**
 * @brief erase stm32 flash
 * @note erase mode depends upon mcu family
 */
static uint8_t ST_Erase_Flash(void)
{
    uint8_t status = 0;
    uint32_t error = 0;

    FLASH_EraseInitTypeDef flash_erase_handle;

#if defined(STM32F103xE) || defined(STM32F103xB)
    flash_erase_handle.TypeErase = FLASH_TYPEERASE_PAGES;
    flash_erase_handle.Banks = FLASH_BANK_1;
    flash_erase_handle.NbPages = (BL_TOTAL_PAGES - BL_USED_PAGES);
    flash_erase_handle.PageAddress = USER_FLASH_START_ADDRESS;
#elif defined(STM32F407xx) || defined(STM32F401xE)
    flash_erase_handle.TypeErase = FLASH_TYPEERASE_SECTORS;
    flash_erase_handle.Banks = FLASH_BANK_1;
    flash_erase_handle.Sector = BL_USED_SECTORS;
    flash_erase_handle.NbSectors = (BL_TOTAL_SECTORS - BL_USED_SECTORS);
    flash_erase_handle.VoltageRange = FLASH_VOLTAGE_RANGE_3;
#endif

    HAL_FLASH_Unlock();

    if (HAL_FLASHEx_Erase(&flash_erase_handle, &error) == HAL_OK)
    {
        status = 1;
    }

    HAL_FLASH_Lock();

    return status;
}

/**
 * @brief write data in given flash address
 * @param address address where flash is to be written
 * @param data input data buffer
 * @param len amount of data to be written
 */
static void BL_Write_Callback(uint32_t address, const uint8_t *data, uint32_t len)
{
    uint32_t *sram_ptr = (uint32_t *)data;
    uint8_t status = 1;
    len /= 4;

    if (address >= USER_FLASH_START_ADDRESS && address <= USER_FLASH_END_ADDRESS - len)
    {
        /* Unlock the Flash to enable the flash control register access */
        HAL_FLASH_Unlock();

        for (uint32_t i = 0; i < len; i++)
        {
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, *sram_ptr) == HAL_OK)
            {
                /* Check the written value */
                if (*(uint32_t *)address != *sram_ptr++)
                {
                    /* Flash content doesn't match SRAM content */
                    status = 0;
                    break;
                }
                address += 4;
            }
            else
            {
                /* Error occurred while writing data in Flash memory */
                status = 0;
                break;
            }
        }

        /* Lock the Flash to disable the flash control register access (recommended
     to protect the FLASH memory against possible unwanted operation) */
        HAL_FLASH_Lock();
    }
    else
    {
        status = 0;
    }

    if (status)
    {
        BL_Send_Char(BL_CMD_ACK);
    }
    else
    {
        BL_Send_Char(BL_CMD_NACK);
    }
}

/**
 * @brief verify data at given flash address
 * @param address address where flash is to be verified
 * @param data input data buffer
 * @param len amount of data to be written
 */
static void BL_Verify_Callback(uint32_t address, const uint8_t *data, uint8_t len)
{
    uint8_t ok_flag = 1;
    len /= 4;

    if (address >= USER_FLASH_START_ADDRESS && address <= USER_FLASH_END_ADDRESS - len)
    {

        for (uint8_t i = 0; i < len; i++)
        {
            if (*(uint32_t *)address != *(uint32_t *)data)
            {
                ok_flag = 0;
                break;
            }

            address += 4;
            data += 4;
        }
    }
    else
    {
        ok_flag = 0;
    }

    if (ok_flag)
    {
        BL_Send_Char(BL_CMD_ACK);
    }
    else
    {
        BL_Send_Char(BL_CMD_NACK);
    }
}

/**
 * @brief read data in given flash address and send
 * @param address address where flash read from
 * @param len number of bytes to be read
 */
static void BL_Read_Callback(uint32_t address, uint32_t len)
{
    uint8_t crc;
    uint8_t *add_ptr = (uint8_t *)address;

    if (address >= USER_FLASH_START_ADDRESS && address <= USER_FLASH_END_ADDRESS - len)
    {
        BL_Send_Char(BL_CMD_ACK);

        for (uint8_t i = 0; i < len; i++)
        {
            BL_TX_Buffer[i] = add_ptr[i];
        }
        crc = BL_CRC8(BL_TX_Buffer, len);
        BL_TX_Buffer[len] = crc;

        BL_Send_Chars((char *)BL_TX_Buffer, len + 1);
    }
    else
    {
        BL_Send_Char(BL_CMD_NACK);
    }
}

/**
 * @brief erase stm32 flash and ack if success
 */
static void BL_Erase_Callback(void)
{
    if (ST_Erase_Flash())
    {
        BL_Send_Char(BL_CMD_ACK);
    }
    else
    {
        BL_Send_Char(BL_CMD_NACK);
    }
}

/**
 * @brief reset stm32 device
 */
static void BL_Reset_Callback(void)
{
    // reset mcu
    BL_Send_Char(BL_CMD_ACK);
    HAL_NVIC_SystemReset();
}

/**
 * @brief jump to user application
 */
static void BL_Jump_Callback(void)
{
    if (BL_Send_Char)
    {
        BL_Send_Char(BL_CMD_ACK);
    }

    HAL_DeInit();

    /** deinit uart or cdc */
    BL_COMM_Deinit();

    BL_Jump();
}

/**
 * @brief send current bootloader version
 */
static void BL_Get_Version_Callback(void)
{
    uint8_t crc;

    BL_Send_Char(BL_CMD_ACK);

    crc = BL_CRC8(BL_Version, 3);

    BL_Send_Char(BL_Version[0]);
    BL_Send_Char(BL_Version[1]);
    BL_Send_Char(BL_Version[2]);
    BL_Send_Char(crc);
}

/**
 * @brief jump to user application
 */
static void BL_Jump(void)
{
    uint32_t reset_vector = 0;
    uint32_t stack_pointer = 0;
    void (*pFunction)(void);

    /** disable interrupts */
    __disable_irq();

    stack_pointer = *(__IO uint32_t *)USER_FLASH_START_ADDRESS;
    reset_vector = *(__IO uint32_t *)(USER_FLASH_START_ADDRESS + 4);

    pFunction = (void (*)(void))reset_vector;

    /** a code is considered valid if the MSB of the initial Main Stack Pointer (MSP) value located in
     *  the first address of the application area is equal to 0x2000 */
    if ((stack_pointer & 0x20000000) == 0x20000000)
    {
        /* Initialize user application's Stack Pointer */
        __set_MSP(stack_pointer);

        /* Jump to user application */
        pFunction();
    }
}

/**
 * @brief bootloader main process loop
 */

static void BL_Loop(void)
{
    /** try auto baud if enabled */
    BL_UART_Init();

#if (USE_USB_CDC == 1)
    BL_CDC_Init();
#endif

    while (1)
    {
        int uart_char = BL_UART_Get_Char(100);
        if (uart_char == BL_CMD_CONNECT)
        {
            /* send ack for connect cmd*/
            BL_UART_Send_Char(BL_CMD_ACK);

            /** if any activity is detected on uart choose uart interface */
            BL_COMM_Deinit = BL_UART_Deinit;

            BL_Send_Char = BL_UART_Send_Char;
            BL_Send_Chars = BL_UART_Send_Chars;

            BL_Get_Char = BL_UART_Get_Char;
            BL_Get_Chars = BL_UART_Get_Chars;
            break;
        }
#if (USE_USB_CDC == 1)
        int cdc_char = BL_CDC_Get_Char(100);
        if (cdc_char == BL_CMD_CONNECT)
        {
            /* send ack for connect cmd*/
            BL_CDC_Send_Char(BL_CMD_ACK);

            /** choose usb cdc */
            BL_COMM_Deinit = BL_CDC_Deinit;

            BL_Send_Char = BL_CDC_Send_Char;
            BL_Send_Chars = BL_CDC_Send_Chars;

            BL_Get_Char = BL_CDC_Get_Char;
            BL_Get_Chars = BL_CDC_Get_Chars;
            break;
        }
#endif
    }

    while (1)
    {
        /* wait for sync char*/
        int sync_char = BL_Get_Char(10);

        if (sync_char != -1)
        {
            /* if BL_CMD_CONNECT received again send ack*/
            /* can be used to test connection*/
            if (sync_char == BL_CMD_CONNECT)
            {
                BL_Send_Char(BL_CMD_ACK);
            }

            if (sync_char == BL_SYNC_CHAR)
            {
                /* wait for packet_len char*/
                int packet_len = BL_Get_Char(100);

                if (packet_len != -1)
                {
                    if (BL_Get_Chars((char *)BL_RX_Buffer, packet_len, 5000) == packet_len)
                    {
                        uint8_t cmd = BL_RX_Buffer[0];

                        /* only applicable to CMD_WRITE, CMD_READ, CMD_VERIFY cmds,  dont care for other cmd*/
                        /* no of bytes to read or write*/
                        uint32_t len = BL_RX_Buffer[1];
                        uint32_t address = BL_RX_Buffer[4] << 24 | BL_RX_Buffer[5] << 16 |
                                           BL_RX_Buffer[6] << 8 | BL_RX_Buffer[7] << 0;

                        /* dont care*/
                        /* padding for stm32 word alignment*/
                        (void)BL_RX_Buffer[2];
                        (void)BL_RX_Buffer[3];

                        /* last byte is crc*/
                        uint8_t crc_recvd = BL_RX_Buffer[packet_len - 1];

                        /* calculate crc */
                        uint8_t crc_calc = BL_CRC8(BL_RX_Buffer, (packet_len - 1));

                        if (crc_calc == crc_recvd)
                        {
                            switch (cmd)
                            {
                            case BL_CMD_WRITE:
                                BL_Write_Callback(address, (BL_RX_Buffer + 8), len);
                                break;

                            case BL_CMD_READ:
                                BL_Read_Callback(address, len);
                                break;

                            case BL_CMD_ERASE:
                                BL_Erase_Callback();
                                break;

                            case BL_CMD_RESET:
                                BL_Reset_Callback();
                                break;

                            case BL_CMD_JUMP:
                                BL_Jump_Callback();
                                break;

                            case BL_CMD_VERIFY:
                                BL_Verify_Callback(address, (BL_RX_Buffer + 8), len);
                                break;

                            case BL_CMD_GETVER:
                                BL_Get_Version_Callback();
                                break;

                            default:
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

/**
 * @brief bootloader entry point
 */
void BL_Main(void)
{
    HAL_Delay(1);
    uint8_t magic_number;

    /** enable backup register access */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

#if defined(STM32F103xE) || defined(STM32F103xB)
    __HAL_RCC_BKP_CLK_ENABLE();
    magic_number = BKP->DR1 & 0x0000FFFF;
    BKP->DR1 = 0x00;
#elif defined(STM32F407xx)
    __HAL_RCC_BKPSRAM_CLK_ENABLE();
    magic_number = *(__IO uint8_t *)0x40024000;
    *(__IO uint8_t *)0x40024000 = 0x00;
#elif defined(STM32F401xE)
    __HAL_RCC_RTC_CONFIG(RCC_RTCCLKSOURCE_LSI);
    __HAL_RCC_RTC_ENABLE();
    magic_number = RTC->BKP0R;
    RTC->BKP0R = 0x00;
#endif

    /** if pin is low enter bootloader, magic number is set or debug flag is enabled */
    if (HAL_GPIO_ReadPin(Boot_GPIO_Port, Boot_Pin) == GPIO_PIN_RESET || magic_number == 0xA5 || BL_DEBUG)
    {
        BL_Loop();
    }
    /** else jump to application */
    BL_Jump();

    /** if jump failed stay in bootloader */
    BL_Loop();
}

/**
 * @}
 */
/* End of Bootloader_Implementation */

/**
 * @}
 */
/* End of Bootloader */
