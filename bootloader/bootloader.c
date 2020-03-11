/*
 * bootloader.c
 *
 *  Created on: Sep 25, 2019
 *      Author: medprime
 */

#include "bootloader.h"
#include "string.h"
#include "stdlib.h"
#include "main.h"

#define BL_ENABLE_CRC   1

#ifdef STM32F103xE
#include "stm32f1xx_hal.h"
/**
 *  STM32F103RE -- 512KB total flash.
 *  Bootloader resides in pages 0,1--15  2KB*16 Flash.
 *  Total pages in 103RE are 256.
 *  pages to erase 16 to 255, except pages 0 to 15 (bootloader).
 *  Erase type- pages.
 *  Programaing voltage- 2.7v to 3.3v.
 *  Flash writing width - double word.
 */
#define BL_PAGE_SIZE   2048    // 2KB
#define BL_TOTAL_PAGES 255// 255*2KB
#define BL_USED_PAGES  16 // 16*2KB

#define USER_FLASH_START_ADDRESS (0x08000000 + BL_USED_PAGES*BL_PAGE_SIZE)
#define USER_FLASH_END_ADDRESS   (0x08000000 + 512*1024)

/* uart used for bootloader*/
extern UART_HandleTypeDef huart2;
UART_HandleTypeDef* BL_UART = &huart2;

#endif


#ifdef STM32F401xE
#include "stm32f4xx_hal.h"
/**
 *  STM32F401RE -- 512KB total flash.
 *  Bootloader resides in sector 0,1 -- 16KB + 16KB Flash.
 *  Total sectors in 401RE are 8.
 *  Sectors to erase 6, except sector 0,1 (bootloader).
 *  Erase type- sector by sector.
 *  Programaing voltage- 2.7v to 3.3v.
 *  Flash writing width - byte by byte for 2.7v to 3.3v.
 */
#define BL_SECTOR_SIZE   (16*1024) //16KB
#define BL_TOTAL_SECTORS 8 // 16KB+16KB+16KB+16KB+64KB+128KB+128KB+128KB
#define BL_USED_SECTORS  2 // 16KB + 16KB

#define USER_FLASH_START_ADDRESS (0x08000000 + BL_USED_SECTORS*BL_SECTOR_SIZE)
#define USER_FLASH_END_ADDRESS   (0x08000000 + 512*1024) // 32KB used by bootloader remaing 480K

/* uart used for bootloader*/
extern UART_HandleTypeDef huart2;
UART_HandleTypeDef* BL_UART = &huart2;

#endif


#ifdef STM32F407xx
#include "stm32f4xx_hal.h"
/**
 *  STM32F407xx -- 1024K total flash.
 *  Bootloader resides in sector 0,1 -- 16KB + 16KB Flash.
 *  Total sectors in 407VG are 12.
 *  Sectors to erase 10, except sector 0,1 (bootloader).
 *  Erase type- sector by sector.
 *  Programaing voltage- 2.7v to 3.3v.
 *  Flash writing width - byte by byte for 2.7v to 3.3v.
 */
#define BL_SECTOR_SIZE   (16*1024) //16KB
#define BL_TOTAL_SECTORS 12 // 16KB+16KB+16KB+16KB+64KB+128KB+128KB+128KB...128KB
#define BL_USED_SECTORS  2 // 16KB + 16KB

#define USER_FLASH_START_ADDRESS (0x08000000 + BL_USED_SECTORS*BL_SECTOR_SIZE)
#define USER_FLASH_END_ADDRESS   (0x08000000 + 1024*1024) // 32KB used by bootloader remaing

/* uart used for bootloader*/
extern UART_HandleTypeDef huart6;
UART_HandleTypeDef* BL_UART = &huart6;

#endif


/*
CMD_WRITE, CMD_VERIFY Frame
[SYNC_CHAR + frame len] frame len = 9 + payload len
[1-byte cmd + 1-byte no of bytes to write + 0x00 + 0x00 + 4-byte addes +  payload + 1-byte CRC]
*/

/*
CMD_READ Frame
[SYNC_CHAR + frame len] frame len = 9
[1-byte cmd + 1-byte no of bytes to read + 0x00 + 0x00 + 4-byte addes + 1-byte CRC]
*/

/*
CMD_ERASE, CMD_RESET, CMD_JUMP Frame
[SYNC_CHAR + frame len] frame len = 2
[1-byte cmd + 1-byte CRC]
*/

#define BL_CMD_WRITE  0x50
#define BL_CMD_READ   0x51
#define BL_CMD_ERASE  0x52
#define BL_CMD_RESET  0x53
#define BL_CMD_JUMP   0x54
#define BL_CMD_VERIFY 0x55

#define BL_CMD_ACK    0x90
#define BL_CMD_NACK   0x91
#define BL_CMD_ERROR  0x92

#define BL_CMD_HELP   0x40

#define BL_SYNC_CHAR  '$'

#define  BL_RX_BUFFER_SIZE   (256)
#define  BL_TX_BUFFER_SIZE   (256)

uint8_t  BL_RX_Buffer[BL_RX_BUFFER_SIZE];
uint8_t  BL_TX_Buffer[BL_TX_BUFFER_SIZE];

/*Maxim APPLICATION NOTE 27 */
uint8_t BL_CRC8_Table[] =
    {
	0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126, 32, 163, 253, 31, 65,
	157, 195, 33, 127, 252, 162, 64, 30, 95, 1, 227, 189, 62, 96, 130, 220,
	35, 125, 159, 193, 66, 28, 254, 160, 225, 191, 93, 3, 128, 222, 60, 98,
	190, 224, 2, 92, 223, 129, 99, 61, 124, 34, 192, 158, 29, 67, 161, 255,
	70, 24, 250, 164, 39, 121, 155, 197, 132, 218, 56, 102, 229, 187, 89, 7,
	219, 133, 103, 57, 186, 228, 6, 88, 25, 71, 165, 251, 120, 38, 196, 154,
	101, 59, 217, 135, 4, 90, 184, 230, 167, 249, 27, 69, 198, 152, 122, 36,
	248, 166, 68, 26, 153, 199, 37, 123, 58, 100, 134, 216, 91, 5, 231, 185,
	140, 210, 48, 110, 237, 179, 81, 15, 78, 16, 242, 172, 47, 113, 147, 205,
	17, 79, 173, 243, 112, 46, 204, 146, 211, 141, 111, 49, 178, 236, 14, 80,
	175, 241, 19, 77, 206, 144, 114, 44, 109, 51, 209, 143, 12, 82, 176, 238,
	50, 108, 142, 208, 83, 13, 239, 177, 240, 174, 76, 18, 145, 207, 45, 115,
	202, 148, 118, 40, 171, 245, 23, 73, 8, 86, 180, 234, 105, 55, 213, 139,
	87, 9, 235, 181, 54, 104, 138, 212, 149, 203, 41, 119, 244, 170, 72, 22,
	233, 183, 85, 11, 136, 214, 52, 106, 43, 117, 151, 201, 74, 20, 246, 168,
	116, 42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107, 53
    };

uint8_t BL_CRC8(uint8_t* data, uint8_t len)
    {

    uint8_t crc = 0;

    for(uint8_t i=0; i<len; i++)
	{
	crc = BL_CRC8_Table[crc^ data[i]];
	}

    return crc;
    }

uint8_t ST_Erase_Flash()
    {

    uint8_t status = 0;
    uint32_t error = 0;

    FLASH_EraseInitTypeDef flash_erase_handle;

#ifdef STM32F103xE
    flash_erase_handle.TypeErase = FLASH_TYPEERASE_PAGES;
    flash_erase_handle.Banks = FLASH_BANK_1;
    flash_erase_handle.NbPages = (BL_TOTAL_PAGES - BL_USED_PAGES);
    flash_erase_handle.PageAddress = USER_FLASH_START_ADDRESS;
#else
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

void BL_UART_Send_Char(char data)
    {
    BL_UART->Instance->DR = data;
    while (__HAL_UART_GET_FLAG(BL_UART,UART_FLAG_TC) == 0);
    }

void BL_UART_Send_String(char *data)
    {

    while (*data)
	{
	BL_UART_Send_Char(*data);
	data++;
	}
    }

void BL_Write_Callback(uint32_t address, const uint8_t *data, uint32_t len)
    {

    uint8_t status = 1;
    uint32_t *aligned_data = (uint32_t*)data;
    len /= 4;

    if (address >= USER_FLASH_START_ADDRESS
	    && address <= USER_FLASH_END_ADDRESS - len)
	{

	/* Unlock the Flash to enable the flash control register access *************/
	HAL_FLASH_Unlock();

	for (uint32_t i = 0; i < len ; i++)
	    {

	    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, *aligned_data) == HAL_OK)
		{
		/* Check the written value */
		if (*(uint32_t*) address != *aligned_data)
		    {
		    /* Flash content doesn't match SRAM content */
		    status = 0;
		    break;
		    }
		/* Increment FLASH destination address */
		address += 4;
		aligned_data++;
		}
	    else
		{
		/* Error occurred while writing data in Flash memory */
		status = 0;
		break;
		}
	    }

	/* Lock the Flash to disable the flash control register access (recommended
	 to protect the FLASH memory against possible unwanted operation) *********/
	HAL_FLASH_Lock();
	}
    else
	{
	status = 0;
	}

    if (status)
	{
	BL_UART_Send_Char(BL_CMD_ACK);
	}
    else
	{
	BL_UART_Send_Char(BL_CMD_NACK);
	}

    }

void BL_Read_Callback(uint32_t address, uint32_t len)
    {

    uint8_t crc;
    uint8_t* add_ptr = (uint8_t*)address;

    if (address >= USER_FLASH_START_ADDRESS
	    && address <= USER_FLASH_END_ADDRESS - len)
	{

	BL_UART_Send_Char(BL_CMD_ACK);


	for (uint8_t i = 0; i < len; i++)
	    {
	    BL_TX_Buffer[i] = *add_ptr++;
	    BL_UART_Send_Char(BL_TX_Buffer[i]);
	    }

	crc = BL_CRC8(BL_TX_Buffer, len);

	BL_UART_Send_Char(crc);

	}
    else
	{
	BL_UART_Send_Char(BL_CMD_NACK);
	}

    }

void BL_Erase_Callback()
    {

    if (ST_Erase_Flash())
	{
	BL_UART_Send_Char(BL_CMD_ACK);
	}
    else
	{
	BL_UART_Send_Char(BL_CMD_NACK);
	}
    }

void BL_Reset_Callback()
    {
    // reset mcu
    BL_UART_Send_Char(BL_CMD_ACK);
    HAL_NVIC_SystemReset();
    }

void BL_Jump_Callback()
    {

    uint32_t app_adress = 0;

    void (*pFunction)(void);

    BL_UART_Send_Char(BL_CMD_ACK);

    HAL_DeInit();

    /* execute the new program */
    app_adress = *(__IO uint32_t*) (USER_FLASH_START_ADDRESS + 4);

    /* Jump to user application */
    pFunction = (void (*)(void))app_adress;

    /* Initialize user application's Stack Pointer */
    __set_MSP(*(__IO uint32_t*) USER_FLASH_START_ADDRESS);

    pFunction();

    }


void BL_Verify_Callback(uint32_t address, const uint8_t* data, uint8_t len)
    {
    // verify flash
    }


void BL_Help_Callback()
    {

    }

void BL_Loop()
    {

    uint32_t address = 0;
    uint32_t len = 0;

    while (1)
	{

	memset(BL_RX_Buffer, 0x00, BL_RX_BUFFER_SIZE);

	/* wait for sync char*/
	if (HAL_UART_Receive(BL_UART, BL_RX_Buffer, 1, HAL_MAX_DELAY)
		== HAL_OK)
	    {

	    uint8_t sync_char = BL_RX_Buffer[0];

	    if (sync_char == BL_SYNC_CHAR)
		{

		/* wait for packet_len char*/
		if (HAL_UART_Receive(BL_UART, BL_RX_Buffer, 1, 100) == HAL_OK)
		    {

		    uint8_t packet_len = BL_RX_Buffer[0];

		    if (HAL_UART_Receive(BL_UART, BL_RX_Buffer, packet_len, 5000) == HAL_OK)
			{

			uint8_t cmd = BL_RX_Buffer[0];
			len = BL_RX_Buffer[1];

			/* dont care*/
			(void)BL_RX_Buffer[2];
			(void)BL_RX_Buffer[3];

			/* last byte is crc*/
			uint8_t crc_recvd = BL_RX_Buffer[packet_len - 1];

#if(BL_ENABLE_CRC == 1)
			/* calculate crc */
			uint8_t crc_calc = BL_CRC8(BL_RX_Buffer, (packet_len - 1));
#else
			uint8_t crc_calc = crc_recvd;
#endif

			address = BL_RX_Buffer[4] << 24|
				  BL_RX_Buffer[5] << 16|
				  BL_RX_Buffer[6] << 8 |
				  BL_RX_Buffer[7] << 0;

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

			    case BL_CMD_HELP:
				BL_Help_Callback();
				break;

			    default:
				//print CMD_ERROR
				break;
				}
			    }
			}
		    }
		}
	    }
	}
    }



void BL_Main()
    {

    HAL_Delay(1);

    /* if pin is reset enter bootloader*/
    //if(HAL_GPIO_ReadPin(Boot_GPIO_Port, Boot_Pin) == GPIO_PIN_RESET)
	{
	BL_Loop();
	}

    /* else jump to application */
	BL_Jump_Callback();

    }