#ifndef __SERIAL_PORT_H
#define __SERIAL_PORT_H

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define SERIAL_HANDLE HANDLE
#include <Windows.h>
#endif


#ifdef __linux__
#define SERIAL_HANDLE int
#include <fcntl.h>
#include <termios.h>
#endif

SERIAL_HANDLE Serial_Port_Config(char *port, int baud);
int Serial_Port_Write(SERIAL_HANDLE hComm, char *str, int len);
int Serial_Port_Read(SERIAL_HANDLE hComm, char *buf, int len);
void Serial_Port_Close(SERIAL_HANDLE hComm);

#endif
