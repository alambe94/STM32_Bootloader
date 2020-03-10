#ifndef __SERIAL_PORT_H
#define __SERIAL_PORT_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#define SERIAL_HANDLE HANDLE
#include <Windows.h>
#endif


#ifdef __linux__
#define SERIAL_HANDLE int
#include <fcntl.h>
#include <termios.h>
#endif

SERIAL_HANDLE Serial_Port_Config(uint8_t *port, uint32_t baud);
uint32_t Serial_Port_Write(SERIAL_HANDLE hComm, uint8_t *str, uint32_t len);
uint32_t Serial_Port_Read(SERIAL_HANDLE hComm, uint8_t *buf, uint32_t len);
void Serial_Port_Close(SERIAL_HANDLE hComm);
void Serial_Port_Timeout(SERIAL_HANDLE hComm, uint32_t len);

#endif
