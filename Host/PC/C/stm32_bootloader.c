#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/timeb.h>

#include "serial_port.h"

#define USER_APP_ADDRESS 0x08008000 // 0x08004000->8K, 0x08004000->16k, 0x08008000->32k botloader size
#define FLASH_SIZE 496000           //+ 512000 //uncomment for 407VG

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

#define CMD_WRITE 0x50
#define CMD_READ 0x51
#define CMD_ERASE 0x52
#define CMD_RESET 0x53
#define CMD_JUMP 0x54
#define CMD_VERIFY 0x55

#define CMD_ACK 0x90
#define CMD_NACK 0x91
#define CMD_ERROR 0x92

#define CMD_HELP 0x40

#define CMD_CONNECT 0x7F

#define SYNC_CHAR '$'

char *com_port = NULL;
uint32_t baud_rate = 0;
char *cmd = NULL;

SERIAL_HANDLE Serial_Handle;

uint8_t Open_Serial_port(char *port, uint32_t baud)
{
   uint8_t status = 1;
   Serial_Handle = Serial_Port_Config(port, baud);

#ifdef _WIN32
   if (Serial_Handle == INVALID_HANDLE_VALUE)
   {
      status = 0;
   }
#elif __linux__
   if (Serial_Handle == -1)
   {
      status = 0;
   }
#endif

   return status;
}

uint64_t system_current_time_millis()
{
#if defined(_WIN32)
   struct _timeb timebuffer;
   _ftime(&timebuffer);
#else
   struct timeb timebuffer;
   ftime(&timebuffer);
#endif

   return (uint64_t)(((timebuffer.time * 1000) + timebuffer.millitm));
}

/*Maxim APPLICATION NOTE 27 */
uint8_t CRC8_Table[] =
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

uint8_t CRC8(uint8_t *data, uint8_t len)
{

   uint8_t crc = 0;

   for (uint8_t i = 0; i < len; i++)
   {
      crc = CRC8_Table[crc ^ data[i]];
   }

   return crc;
}

void stm32_send_cmd(uint8_t cmd)
{
   uint8_t temp[1];

   temp[0] = cmd;
   uint8_t crc = CRC8(temp, 1);

   temp[0] = SYNC_CHAR;
   Serial_Port_Write(Serial_Handle, temp, 1);

   temp[0] = 2;
   Serial_Port_Write(Serial_Handle, temp, 1);

   temp[0] = cmd;
   Serial_Port_Write(Serial_Handle, temp, 1);

   temp[0] = crc;
   Serial_Port_Write(Serial_Handle, temp, 1);
}

void stm32_send_ack()
{
   uint8_t temp[1];
   temp[0] = CMD_ACK;
   Serial_Port_Write(Serial_Handle, temp, 1);
}

uint8_t stm32_read_ack()
{
   uint8_t rx_char;
   Serial_Port_Read(Serial_Handle, &rx_char, 1);

   return (rx_char == CMD_ACK);
}

void stm32_erase()
{

   Serial_Port_Timeout(Serial_Handle, 10000);

   stm32_send_cmd(CMD_ERASE);

   if (stm32_read_ack())
   {
      printf("flash erase success\n");
   }
   else
   {
      printf("flash erase error\n");
   }

   Serial_Port_Timeout(Serial_Handle, 100);
}

void stm32_get_help()
{
   printf("supported commands\n"
          "write  -> write application to mcu.\n"
          "erase  -> erase mcu flash.\n"
          "reset  -> reset mcu.\n"
          "jump   -> jump to user application.\n"
          "read   -> read flash from mcu.\n"
          "verify -> verify mcu content.\n");
}

void stm32_reset()
{

   stm32_send_cmd(CMD_RESET);

   if (stm32_read_ack())
   {
      printf("mcu reset\n");
   }
   else
   {
      printf("mcu reset failed\n");
   }
}

void stm32_jump()
{
   stm32_send_cmd(CMD_JUMP);

   if (stm32_read_ack())
   {
      printf("entering user application\n");
   }
   else
   {
      printf("user application failed\n");
   }
}

void stm32_read_flash()
{

   FILE *fp = NULL;

   uint32_t start_time = system_current_time_millis();
   uint32_t remaining_bytes = FLASH_SIZE;
   uint32_t stm32_app_address = USER_APP_ADDRESS;

   uint8_t bl_packet[10];
   uint8_t rx_buffer[256];

   fp = fopen("output_file.bin", "wb");

   if (fp == NULL)
   {
      printf("can not create bin file\n");
      return;
   }

   while (remaining_bytes > 0)
   {

      uint8_t bl_packet_index;
      uint8_t read_block_size = 240;
      uint8_t temp[1];

      if (remaining_bytes < read_block_size)
      {
         read_block_size = remaining_bytes;
      }

      memset(bl_packet, 0x00, 10);
      memset(rx_buffer, 0x00, 256);

      bl_packet_index = 0;

      // assemble cmd
      bl_packet[bl_packet_index++] = CMD_READ;

      // no of char to receive from stm32
      bl_packet[bl_packet_index++] = read_block_size;

      // 2 bytes padding for stm32 word alignment
      bl_packet[bl_packet_index++] = 0x00;
      bl_packet[bl_packet_index++] = 0x00;

      // assemble address
      bl_packet[bl_packet_index++] = (stm32_app_address >> 24 & 0xFF);
      bl_packet[bl_packet_index++] = (stm32_app_address >> 16 & 0xFF);
      bl_packet[bl_packet_index++] = (stm32_app_address >> 8 & 0xFF);
      bl_packet[bl_packet_index++] = (stm32_app_address >> 0 & 0xFF);

      // calculate crc
      uint8_t crc = CRC8(bl_packet, bl_packet_index);

      // assemble crc
      bl_packet[bl_packet_index++] = crc;

      // send sync char
      temp[0] = SYNC_CHAR;
      Serial_Port_Write(Serial_Handle, temp, 1);

      // send no of chars in bl_packet
      temp[0] = bl_packet_index;
      Serial_Port_Write(Serial_Handle, temp, 1);

      // send bl_packet
      Serial_Port_Write(Serial_Handle, bl_packet, bl_packet_index);

      if (stm32_read_ack())
      {
         uint8_t crc_recvd;

         for (size_t i = 0; i < read_block_size; i++)
         {
            Serial_Port_Read(Serial_Handle, rx_buffer + i, 1);
         }

         Serial_Port_Read(Serial_Handle, &crc_recvd, 1);

         uint8_t crc_calc = CRC8(rx_buffer, read_block_size);

         if (crc_recvd == crc_calc)
         {
            fwrite(rx_buffer, 1, read_block_size, fp);
            //printf("flash read succsess at 0X%0x\n", stm32_app_address);
            remaining_bytes -= read_block_size;
            stm32_app_address += read_block_size;
            if ((100 * remaining_bytes) % FLASH_SIZE == 0)
            {
               printf("remaining %u %%\n", (100 * remaining_bytes / FLASH_SIZE));
            }
         }
         else
         {
            printf("crc mismatch\n");
            break;
         }
      }
      else
      {
         printf("flash read error at 0X%0x\n", stm32_app_address);
         break;
      }
   }

   if (remaining_bytes == 0)
   {
      printf("flash read successfull, jolly good!!!!\n");
      rewind(fp);
      fseek(fp, 0L, SEEK_END);
      uint32_t f_file_size = ftell(fp);
      uint32_t elapsed_time = system_current_time_millis() - start_time;
      printf("elapsed time = %ums\n", elapsed_time);
      printf("read speed = %ukB/S\n", f_file_size / elapsed_time);
   }

   fclose(fp);
}

void stm32_write(char *input_file)
{
   FILE *fp = NULL;

   uint32_t start_time = system_current_time_millis();
   uint32_t stm32_app_address = USER_APP_ADDRESS;

   uint8_t bl_packet[256];

   printf("opening file...\n");

   fp = fopen(input_file, "rb");

   if (fp == NULL)
   {
      printf("can not open %s\n", input_file);
   }
   else
   {
      fseek(fp, 0L, SEEK_END);
      uint32_t f_file_size = ftell(fp);
      rewind(fp);
      printf("file size %u\n", f_file_size);
      uint32_t remaining_bytes = f_file_size;
      uint8_t write_block_size = 240;

      while (remaining_bytes > 0)
      {
         uint8_t bl_packet_index;
         uint8_t temp[1];

         if (remaining_bytes < write_block_size)
         {
            write_block_size = remaining_bytes;
         }

         memset(bl_packet, 0x00, 256);
         bl_packet_index = 0;

         // assemble cmd
         bl_packet[bl_packet_index++] = CMD_WRITE;

         // no of char to flash to stm32
         bl_packet[bl_packet_index++] = write_block_size;

         // 2 bytes padding for stm32 word alignment
         bl_packet[bl_packet_index++] = 0x00;
         bl_packet[bl_packet_index++] = 0x00;

         // assemble address
         bl_packet[bl_packet_index++] = (stm32_app_address >> 24 & 0xFF);
         bl_packet[bl_packet_index++] = (stm32_app_address >> 16 & 0xFF);
         bl_packet[bl_packet_index++] = (stm32_app_address >> 8 & 0xFF);
         bl_packet[bl_packet_index++] = (stm32_app_address & 0xFF);

         // assemble payload
         fread(&bl_packet[bl_packet_index], 1, write_block_size, fp);
         bl_packet_index += write_block_size;

         // calculate crc
         uint8_t crc = CRC8(bl_packet, bl_packet_index);

         // assemble crc
         bl_packet[bl_packet_index++] = crc;

         // send sync char
         temp[0] = SYNC_CHAR;
         Serial_Port_Write(Serial_Handle, temp, 1);

         // send no of char in bl_packet
         temp[0] = bl_packet_index;
         Serial_Port_Write(Serial_Handle, temp, 1);

         // send in bl_packet
         Serial_Port_Write(Serial_Handle, bl_packet, bl_packet_index);

         if (stm32_read_ack())
         {
            //printf("flash write success at 0X%0x\n", stm32_app_address);
         }
         else
         {
            printf("flash write error at 0X%0x\n", stm32_app_address);
            break;
         }

         remaining_bytes -= write_block_size;
         stm32_app_address += write_block_size;

         if ((100 * remaining_bytes) % f_file_size == 0)
         {
            printf("remaining %u %%\n", (100 * remaining_bytes / f_file_size));
         }
      }

      if (remaining_bytes == 0)
      {
         printf("flash write successfull, jolly good!!!!\n");
         uint32_t elapsed_time = system_current_time_millis() - start_time;
         printf("elapsed time = %ums\n", elapsed_time);
         printf("write speed = %ukB/S\n", f_file_size / elapsed_time);
      }

      fclose(fp);
      printf("closing file\n");
   }
}

void stm32_verify(char *input_file)
{
   FILE *fp = NULL;

   uint32_t start_time = system_current_time_millis();
   uint32_t stm32_app_address = USER_APP_ADDRESS;

   uint8_t bl_packet[256];

   printf("opening file...\n");

   fp = fopen(input_file, "rb");

   if (fp == NULL)
   {
      printf("can not open %s\n", input_file);
   }
   else
   {
      fseek(fp, 0L, SEEK_END);
      uint32_t f_file_size = ftell(fp);
      rewind(fp);
      printf("file size %u\n", f_file_size);
      uint32_t remaining_bytes = f_file_size;
      uint8_t write_block_size = 240;

      while (remaining_bytes > 0)
      {

         uint8_t bl_packet_index;
         uint8_t temp[1];

         if (remaining_bytes < write_block_size)
         {
            write_block_size = remaining_bytes;
         }

         memset(bl_packet, 0x00, 256);
         bl_packet_index = 0;

         // assemble cmd
         bl_packet[bl_packet_index++] = CMD_VERIFY;

         // no of char to flash to stm32
         bl_packet[bl_packet_index++] = write_block_size;

         // 2 bytes padding for stm32 word alignment
         bl_packet[bl_packet_index++] = 0x00;
         bl_packet[bl_packet_index++] = 0x00;

         // assemble address
         bl_packet[bl_packet_index++] = (stm32_app_address >> 24 & 0xFF);
         bl_packet[bl_packet_index++] = (stm32_app_address >> 16 & 0xFF);
         bl_packet[bl_packet_index++] = (stm32_app_address >> 8 & 0xFF);
         bl_packet[bl_packet_index++] = (stm32_app_address >> 0 & 0xFF);

         // assemble payload
         fread(&bl_packet[bl_packet_index], 1, write_block_size, fp);
         bl_packet_index += write_block_size;

         // calculate crc
         uint8_t crc = CRC8(bl_packet, bl_packet_index);

         // assemble crc
         bl_packet[bl_packet_index++] = crc;

         // send sync char
         temp[0] = SYNC_CHAR;
         Serial_Port_Write(Serial_Handle, temp, 1);

         // send no of char in bl_packet
         temp[0] = bl_packet_index;
         Serial_Port_Write(Serial_Handle, temp, 1);

         // send bl_packet
         Serial_Port_Write(Serial_Handle, bl_packet, bl_packet_index);

         if (stm32_read_ack())
         {
            //printf("verify success at 0X%0x\n", stm32_app_address);
         }
         else
         {
            printf("verify error at 0X%0x\n", stm32_app_address);
            break;
         }

         remaining_bytes -= write_block_size;
         stm32_app_address += write_block_size;
         if ((100 * remaining_bytes) % f_file_size == 0)
         {
            printf("remaining %u %%\n", (100 * remaining_bytes / f_file_size));
         }
      }

      if (remaining_bytes == 0)
      {
         printf("verify successfull, jolly good!!!!\n");
         uint32_t elapsed_time = system_current_time_millis() - start_time;
         printf("elapsed time = %ums\n", elapsed_time);
         printf("verify speed = %ukB/S\n", f_file_size / elapsed_time);
      }

      fclose(fp);
      printf("closing file\n");
   }
}

int main(int argc, char *argv[])
{
   printf("path = %s\n", argv[0]);

   if (argc == 1)
   {
      printf("please enter port, baud, cmd and optional input file\n");
   }

   if (argc >= 4)
   {
      com_port = argv[1];
      baud_rate = atoi(argv[2]);
      cmd = argv[3];

      printf("com port = %s\n", com_port);
      printf("baud rate = %u\n", baud_rate);
      printf("cmd = %s\n", cmd);
   }

   if (Open_Serial_port(com_port, baud_rate))
   {
      printf("port open success\n");

      // CMD_CONNECT used for auto baud detection on stm32
      uint8_t temp = CMD_CONNECT;
      uint8_t retry = 10;
      uint8_t connected = 0;

      while (retry--)
      {
         Serial_Port_Write(Serial_Handle, &temp, 1);

         if (!stm32_read_ack())
         {
            uint32_t delay = system_current_time_millis();
            while (system_current_time_millis() - delay < 100)
               ;
         }
         else
         {
            connected = 1;
            break;
         }
      }

      if (!connected)
      {
         printf("stm32 device connection failed\n");
      }
      else
      {
         printf("connected to stm32 device\n");

         if (strncmp(cmd, "write", strnlen("write", 10)) == 0)
         {
            if (argc >= 5)
            {
               char *bin_file = argv[4];
               printf("input file = %s\n", bin_file);
               stm32_write(bin_file);
            }
            else
            {
               printf("please enter input file\n");
            }
         }
         else if (strncmp(cmd, "erase", 10) == 0)
         {
            stm32_erase();
         }
         else if (strncmp(cmd, "reset", 10) == 0)
         {
            stm32_reset();
         }
         else if (strncmp(cmd, "jump", 10) == 0)
         {
            stm32_jump();
         }
         else if (strncmp(cmd, "help", 10) == 0)
         {
            stm32_get_help();
         }
         else if (strncmp(cmd, "read", 10) == 0)
         {
            stm32_read_flash();
         }
         else if (strncmp(cmd, "verify", 10) == 0)
         {
            if (argc >= 5)
            {
               char *bin_file = argv[4];
               printf("input file = %s\n", bin_file);
               stm32_verify(bin_file);
            }
            else
            {
               printf("please enter input file to verfy\n");
            }
         }
         else
         {
            printf("Invalid cmd\n");
         }
      }

      printf("closing port\n");
      Serial_Port_Close(Serial_Handle);
   }
   else
   {
      printf("Not valid port\n");
   }

   return 0;
}