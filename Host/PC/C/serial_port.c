#include "serial_port.h"

#ifdef _WIN32

SERIAL_HANDLE Serial_Port_Config(char *port, int baud)
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

dcbSerialParams.BaudRate = baud;                 // Setting BaudRate = 9600
dcbSerialParams.ByteSize = 8;                    // Setting ByteSize = 8
dcbSerialParams.StopBits = ONESTOPBIT;           // Setting StopBits = 1
dcbSerialParams.Parity = NOPARITY;               // Setting Parity = None
SetCommState(hComm, &dcbSerialParams);  //Configuring the port according to settings in DCB


timeouts.ReadIntervalTimeout = 300;
timeouts.ReadTotalTimeoutConstant = 300;
timeouts.ReadTotalTimeoutMultiplier = 300;
timeouts.WriteTotalTimeoutConstant = 50;
timeouts.WriteTotalTimeoutMultiplier = 10;
SetCommTimeouts(hComm, &timeouts);

return hComm;
}

int Serial_Port_Write(SERIAL_HANDLE hComm, char *str, int len)
{

    DWORD bytes_count = 0; // No of bytes written to the port

    WriteFile(hComm,        // Handle to the Serialport
              str,          // Data to be written to the port
              len,          // No of bytes to write into the port
              &bytes_count, // No of bytes written to the port
              NULL);

    return bytes_count;
}

int Serial_Port_Read(SERIAL_HANDLE hComm, char *buf, int len)
{

DWORD bytes_count = 0;                    // No of bytes read from the port   

ReadFile(hComm, buf, len, &bytes_count, NULL);

return bytes_count;
}

void Serial_Port_Close(SERIAL_HANDLE hComm)
{
    CloseHandle(hComm); //Closing the Serial Port
}

#endif


#ifdef __linux__
SERIAL_HANDLE Serial_Port_Config(char *port, int baud)
{

    SERIAL_HANDLE fd;                              /* File Descriptor */
    struct termios SerialPortSettings;             /* Create the structure */
    

    fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY); /* ttyUSB0 is the FT232 based USB2SERIAL Converter   */
                                                   /* O_RDWR Read/Write access to serial port           */
                                                   /* O_NOCTTY - No terminal will control the process   */
                                                   /* O_NDELAY -Non Blocking Mode,Does not care about-  */
                                                   /* -the status of DCD line,Open() returns immediatly */

    cfsetispeed(&SerialPortSettings, baud); /* Set Read  Speed                        */
    cfsetospeed(&SerialPortSettings, baud); /* Set Write Speed                        */

    SerialPortSettings.c_cflag &= ~PARENB; /* Disables the Parity Enable bit(PARENB),So No Parity   */
    SerialPortSettings.c_cflag &= ~CSTOPB; /* CSTOPB = 2 Stop bits,here it is cleared so 1 Stop bit */
    SerialPortSettings.c_cflag &= ~CSIZE;  /* Clears the mask for setting the data size             */
    SerialPortSettings.c_cflag |= CS8;     /* Set the data bits = 8                                 */

    SerialPortSettings.c_cflag &= ~CRTSCTS;       /* No Hardware flow Control                         */
    SerialPortSettings.c_cflag |= CREAD | CLOCAL; /* Enable receiver,Ignore Modem Control lines       */

    SerialPortSettings.c_iflag &= ~(IXON | IXOFF | IXANY);         /* Disable XON/XOFF flow control both i/p and o/p */
    SerialPortSettings.c_iflag &= ~(ICANON | ECHO | ECHOE | ISIG); /* Non Cannonical mode                            */

    SerialPortSettings.c_oflag &= ~OPOST; /*No Output Processing*/

    if (fd != -1)
    {
        tcsetattr(fd, TCSANOW, &SerialPortSettings);
    }

    return fd;
}

int Serial_Port_Write(SERIAL_HANDLE fd, char *str, int len)
{

int bytes_count = 0;                   // No of bytes written to the port

bytes_count = write(fd, str, len);

return bytes_count;
}

int Serial_Port_Read(SERIAL_HANDLE fd, char *buf, int len)
{

int bytes_count = 0;                    // No of bytes read from the port   

bytes_count = read(fd, &buf, len);

return bytes_count;
}

void Serial_Port_Close(SERIAL_HANDLE fd)
{
    close(fd); //Closing the Serial Port
}
#endif
