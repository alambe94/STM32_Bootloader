#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/timeb.h>

#include "serial_port.h"

#define USER_APP_ADDRESS 0x08008000
#define FLASH_SIZE  480000 //+ 512000 //uncomment for 407VG

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

#define CMD_WRITE  0x50
#define CMD_READ   0x51
#define CMD_ERASE  0x52
#define CMD_RESET  0x53
#define CMD_JUMP   0x54
#define CMD_VERIFY 0x55

#define CMD_ACK    0x90
#define CMD_NACK   0x91
#define CMD_ERROR  0x92

#define CMD_HELP   0x40

#define SYNC_CHAR  '$'


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
#endif

#ifdef __linux__
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
   return rx_char;
}

void stm32_erase()
{

   Serial_Port_Timeout(Serial_Handle, 10000);

   stm32_send_cmd(CMD_ERASE);

   uint8_t reply = stm32_read_ack();
   if (reply == CMD_ACK)
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

   uint8_t reply = stm32_read_ack();

   if (reply == CMD_ACK)
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

   uint8_t reply = stm32_read_ack();

   if (reply == CMD_ACK)
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
   uint32_t read_len = FLASH_SIZE;
   uint32_t stm32_app_address = USER_APP_ADDRESS;
   
   uint8_t bl_packet[10];
   uint8_t rx_buffer[256];
   uint8_t bl_packet_index;
   uint8_t bytes_to_read = 248;
   uint8_t temp[1] = {0};

   fp = fopen("output_file.bin", "wb");

   if (fp == NULL)
   {
      printf("can not create bin file\n");
      return;
   }

   while (read_len > 0)
   {

      if (read_len < bytes_to_read)
      {
         bytes_to_read = read_len;
      }
      memset(bl_packet, 0x00, 10);
      memset(rx_buffer, 0x00, 256);

      bl_packet_index = 0;

      // assemble cmd
      bl_packet[bl_packet_index++] = CMD_READ;

      // no char to receive from stm32
      bl_packet[bl_packet_index++] = bytes_to_read;

      // 2 bytes padding for stm32 word alignment
      bl_packet[bl_packet_index++] = 0x00;
      bl_packet[bl_packet_index++] = 0x00;

      // assemble address
      bl_packet[bl_packet_index++] = (stm32_app_address >> 24 & 0xFF);
      bl_packet[bl_packet_index++] = (stm32_app_address >> 16 & 0xFF);
      bl_packet[bl_packet_index++] = (stm32_app_address >> 8 & 0xFF);
      bl_packet[bl_packet_index++] = (stm32_app_address >> 0 & 0xFF);

      // calculate crc
      uint8_t crc = CRC8(bl_packet, 8);

      // assemble crc
      bl_packet[bl_packet_index++] = crc;

      // send sync char
      temp[0] = SYNC_CHAR;
      Serial_Port_Write(Serial_Handle, temp, 1);

      // send no chars in bl_packet
      temp[0] = 9;
      Serial_Port_Write(Serial_Handle, temp, 1);

      // send bl_packet
      Serial_Port_Write(Serial_Handle, bl_packet, bl_packet_index);

      uint8_t reply = stm32_read_ack();
      
      if (reply == CMD_ACK)
      {
         uint8_t crc_recvd;

         Serial_Port_Read(Serial_Handle, rx_buffer, bytes_to_read);

         Serial_Port_Read(Serial_Handle, &crc_recvd, 1);

         uint8_t crc_calc = CRC8(rx_buffer, bytes_to_read);

         if (crc_recvd == crc_calc)
         {
            fwrite(rx_buffer, 1, bytes_to_read, fp);
            printf("flash read succsess at 0X%0x\n", stm32_app_address);
            read_len -= bytes_to_read;
            stm32_app_address += bytes_to_read;
            printf("remaining bytes: %i\n", read_len);
         }
         else
         {
            printf("crc mismatch\n");
         }
      }
      else
      {
         printf("flash read error at 0X%0x\n", stm32_app_address);
         break;
      }
   }

   if (read_len == 0)
   {
      printf("flash read successfull, jolly good!!!!\n");
   }

   rewind(fp);
   fseek(fp, 0L, SEEK_END);
   uint32_t f_file_size = ftell(fp);
   fclose(fp);

   uint32_t elapsed_time = system_current_time_millis() - start_time;
   printf("elapsed time = %ims\n", elapsed_time);
   printf("read speed = %ikB/S\n", f_file_size / elapsed_time);
}

void stm32_write(char *input_file)
{
   FILE *fp = NULL;

   uint32_t start_time = system_current_time_millis();
   uint32_t stm32_app_address = USER_APP_ADDRESS;
   uint32_t f_file_len;
   uint32_t f_file_size;

   uint8_t payload_length = 240;
   uint8_t bl_packet[256];
   uint8_t bl_packet_index = 0;
   uint8_t temp[1] = {0};

   printf("opening file...\n");

   fp = fopen(input_file, "rb");

   if (fp == NULL)
   {
      printf("can not open %s\n", input_file);
   }
   else
   {
      fseek(fp, 0L, SEEK_END);
      f_file_len = ftell(fp);
      rewind(fp);
      printf("file size %i\n", f_file_len);
      f_file_size = f_file_len;

      while (f_file_len > 0)
      {
         if (f_file_len < payload_length)
         {
            payload_length = f_file_len;
         }

         memset(bl_packet, 0x00, 256);
         bl_packet_index = 0;

         // assemble cmd
         bl_packet[bl_packet_index++] = CMD_WRITE;

         // no char to flash to stm32
         bl_packet[bl_packet_index++] = payload_length;

         // 2 bytes padding for stm32 word alignment
         bl_packet[bl_packet_index++] = 0x00;
         bl_packet[bl_packet_index++] = 0x00;

         // assemble address
         bl_packet[bl_packet_index++] = (stm32_app_address >> 24 & 0xFF);
         bl_packet[bl_packet_index++] = (stm32_app_address >> 16 & 0xFF);
         bl_packet[bl_packet_index++] = (stm32_app_address >> 8 & 0xFF);
         bl_packet[bl_packet_index++] = (stm32_app_address >> 0 & 0xFF);

         // assemble payload
         fread(&bl_packet[bl_packet_index], 1, payload_length, fp);
         bl_packet_index += payload_length;

         // calculate crc
         uint8_t crc = CRC8(bl_packet, (payload_length + 8));

         // assemble crc
         bl_packet[bl_packet_index++] = crc;

         printf("writing to serial port\n");

         // send sync char
         temp[0] = SYNC_CHAR;
         Serial_Port_Write(Serial_Handle, temp, 1);

         // send no char in bl_packet
         temp[0] = bl_packet_index;
         Serial_Port_Write(Serial_Handle, temp, 1);

         Serial_Port_Write(Serial_Handle, bl_packet, bl_packet_index);

         uint8_t reply = stm32_read_ack();

         if (reply == CMD_ACK)
         {
            printf("flash write success at 0X%0x\n", stm32_app_address);
         }
         else
         {
            printf("flash write error at 0X%0x\n", stm32_app_address);
            break;
         }

         f_file_len -= payload_length;
         stm32_app_address += payload_length;
         printf("remaining bytes: %i\n", f_file_len);
      }

      if (f_file_len == 0)
      {
         printf("flash write successfull, jolly good!!!!\n");
      }

      fclose(fp);
      printf("closing file\n");

      uint32_t elapsed_time = system_current_time_millis() - start_time;
      printf("elapsed time = %ims\n", elapsed_time);
      printf("write speed = %ikB/S\n", f_file_size / elapsed_time);
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
      printf("baud rate = %i\n", baud_rate);
      printf("cmd = %s\n", cmd);
   }

   if (Open_Serial_port(com_port, baud_rate))
   {
      printf("Port open success\n");

      if (strncmp(cmd, "write", strlen("write")) == 0)
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
      else if (strncmp(cmd, "erase", strlen("erase")) == 0)
      {
         stm32_erase();
      }
      else if (strncmp(cmd, "reset", strlen("reset")) == 0)
      {
         stm32_reset();
      }
      else if (strncmp(cmd, "jump", strlen("jump")) == 0)
      {
         stm32_jump();
      }
      else if (strncmp(cmd, "help", strlen("help")) == 0)
      {
         stm32_get_help();
      }
      else if (strncmp(cmd, "read", strlen("read")) == 0)
      {
         stm32_read_flash();
      }
      else
      {
         printf("Invalid cmd\n");
      }

      printf("Closing port\n");
      Serial_Port_Close(Serial_Handle);
   }
   else
   {
      printf("Not valid port\n");
   }

   return 0;
}