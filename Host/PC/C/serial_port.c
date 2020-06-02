#include "serial_port.h"

#ifdef _WIN32

SERIAL_HANDLE Serial_Port_Config(uint8_t *port, uint32_t baud)
{
SERIAL_HANDLE hComm;                             // Handle to the Serial port
COMMTIMEOUTS timeouts = {0};
DCB dcbSerialParams = {0};                       // Initializing DCB structure

hComm = CreateFile(port,                         // Name of the Port to be Opened
                   GENERIC_READ | GENERIC_WRITE, // Read/Write Access
                   0,                            // No Sharing, ports cant be shared
                   NULL,                         // No Security
                   OPEN_EXISTING,                // Open existing port only
                   0,                            // Non Overlapped I/O
                   NULL);                        // Null for Comm Devices

dcbSerialParams.BaudRate = baud;                 // Setting BaudRate
dcbSerialParams.ByteSize = 8;                    // Setting ByteSize = 8
dcbSerialParams.StopBits = ONESTOPBIT;           // Setting StopBits = 1
dcbSerialParams.Parity = NOPARITY;               // Setting Parity = None
SetCommState(hComm, &dcbSerialParams);           //Configuring the port according to settings in DCB


timeouts.ReadIntervalTimeout = 100;
timeouts.ReadTotalTimeoutConstant = 100;
timeouts.ReadTotalTimeoutMultiplier = 100;
timeouts.WriteTotalTimeoutConstant = 100;
timeouts.WriteTotalTimeoutMultiplier = 100;
SetCommTimeouts(hComm, &timeouts);

return hComm;
}

uint32_t Serial_Port_Write(SERIAL_HANDLE hComm, uint8_t *str, uint32_t len)
{

    DWORD bytes_count = 0; // No of bytes written to the port

    WriteFile(hComm,        // Handle to the Serialport
              str,          // Data to be written to the port
              len,          // No of bytes to write into the port
              &bytes_count, // No of bytes written to the port
              NULL);

    return bytes_count;
}

uint32_t Serial_Port_Read(SERIAL_HANDLE hComm, uint8_t *buf, uint32_t len)
{

DWORD bytes_count = 0;                    // No of bytes read from the port

ReadFile(hComm, buf, len, &bytes_count, NULL);

return bytes_count;
}

void Serial_Port_Close(SERIAL_HANDLE hComm)
{
    CloseHandle(hComm); //Closing the Serial Port
}

void Serial_Port_Timeout(SERIAL_HANDLE hComm, uint32_t len)
{
    COMMTIMEOUTS timeouts = {0};
    GetCommTimeouts(hComm, &timeouts);

    timeouts.ReadTotalTimeoutConstant = len;
    SetCommTimeouts(hComm, &timeouts);
}
#endif

#ifdef __linux__

SERIAL_HANDLE Serial_Port_Config(uint8_t *port, uint32_t baud)
{
    SERIAL_HANDLE fd;   /* File Descriptor */
    struct termios tty; /* Create the structure */

    switch (baud)
    {
    case 9600:
        baud = B9600;
        break;
    case 19200:
        baud = B19200;
        break;
    case 38400:
        baud = B38400;
        break;
    case 57600:
        baud = B57600;
        break;
    case 115200:
        baud = B115200;
        break;
    case 230400:
        baud = B230400;
        break;
    case 500000:
        baud = B500000;
        break;

    default:
        baud = B115200;
        break;
    }

    fd = open(port, O_RDWR);

    tty.c_cflag &= ~PARENB;        // Clear parity bit, disabling parity (most common)
    tty.c_cflag &= ~CSTOPB;        // Clear stop field, only one stop bit used in communication (most common)
    tty.c_cflag |= CS8;            // 8 bits per byte (most common)
    tty.c_cflag &= ~CRTSCTS;       // Disable RTS/CTS hardware flow control (most common)
    tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;                                                        // Disable echo
    tty.c_lflag &= ~ECHOE;                                                       // Disable erasure
    tty.c_lflag &= ~ECHONL;                                                      // Disable new-line echo
    tty.c_lflag &= ~ISIG;                                                        // Disable interpretation of INTR, QUIT and SUSP
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);                                      // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); // Disable any special handling of received bytes

    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
                           // tty.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT PRESENT ON LINUX)
                           // tty.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars (0x004) in output (NOT PRESENT ON LINUX)

    tty.c_cc[VTIME] = 10; // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
    tty.c_cc[VMIN] = 1;

    if (fd != -1)
    {
        tcsetattr(fd, TCSANOW, &tty);
        tcflush(fd, TCIOFLUSH);
    }

    return fd;
}

uint32_t Serial_Port_Write(SERIAL_HANDLE fd, uint8_t *str, uint32_t len)
{
uint32_t bytes_count = 0;                   // No of bytes written to the port

bytes_count = write(fd, str, len);

return bytes_count;
}

uint32_t Serial_Port_Read(SERIAL_HANDLE fd, uint8_t *buf, uint32_t len)
{
    uint32_t bytes_count = -1; // No of bytes read from the port

    bytes_count = read(fd, buf, 1);

    return bytes_count;
}

void Serial_Port_Close(SERIAL_HANDLE fd)
{
    close(fd); //Closing the Serial Port
}

void Serial_Port_Timeout(SERIAL_HANDLE fd, uint32_t len)
{
    struct termios tty;

    tcgetattr(fd, &tty);

    tty.c_cc[VTIME] = len /100;

    tcsetattr(fd, TCSANOW, &tty);
}

#endif
