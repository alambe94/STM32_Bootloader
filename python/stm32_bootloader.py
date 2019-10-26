import serial
import struct
import os
import sys
import time


USER_APP_ADDRESS = 0x08008000
SERIAL_PORT = 'ACM0'
BAUDRATE = 250000
FLASH_SIZE = 480000


CMD_WRITE = 0x50
CMD_READ = 0x51
CMD_ERASE = 0x52
CMD_RESET = 0x53
CMD_JUMP = 0x54
CMD_VERIFY = 0x55
CMD_FAST_READ = 0x56

CMD_ACK = 0x90
CMD_NACK = 0x91
CMD_ERROR = 0x92

CMD_HELP = 0x40

SYNC_CHAR = ord('$')


# Maxim APPLICATION NOTE 27

CRC8_Table = [
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
]


def CRC8(data, len):
    crc = 0
    for i in range(len):
        crc = CRC8_Table[crc ^ data[i]]

    return crc


def int_to_byte(data):
    return bytes([data])


def stm32_bl_send_cmd(cmd):

    crc = CRC8([cmd], 1)
    ser.write(int_to_byte(SYNC_CHAR))
    ser.write(int_to_byte(2))

    ser.write(int_to_byte(cmd))
    ser.write(int_to_byte(crc))


def stm32_send_ack():
    ser.write(int_to_byte(CMD_ACK))


def stm32_read_line():
    rx_string = ser.readline().decode('utf-8')
    return rx_string


def stm32_read_ack():
    rx_char = ser.read(1)
    if(rx_char == b''):
        return CMD_NACK
    else:
        return ord(rx_char)


def stm32_erase():
    stm32_bl_send_cmd(CMD_ERASE)
    reply = stm32_read_ack()
    if(reply == CMD_ACK):
        print('flash erase success')
    elif (reply == CMD_NACK):
        print('flash erase error')


def stm32_enter_application():
    stm32_bl_send_cmd(CMD_JUMP)
    reply = stm32_read_ack()
    if(reply == CMD_ACK):
        print('entering user application')
    elif (reply == CMD_NACK):
        print('user application failed')


def stm32_get_help():
    print("""
       supported commands
       write  -> write application to mcu.
       erase  -> erase mcu flash.
       reset  -> reset mcu.
       jump   -> jump to user application.
       read   -> read flash from mcu.
       verify -> verify mcu content.
       """)


def stm32_reset():
    stm32_bl_send_cmd(CMD_RESET)
    reply = stm32_read_ack()
    if(reply == CMD_ACK):
        print('mcu reset')
    elif (reply == CMD_NACK):
        print('mcu reset failed')


def stm32_jump():
    stm32_bl_send_cmd(CMD_JUMP)
    reply = stm32_read_ack()
    if(reply == CMD_ACK):
        print('jumping to user apllication')
    elif (reply == CMD_NACK):
        print('jump to user apllication failed')


def stm32_read_flash():
    data_block = 248
    read_len = FLASH_SIZE
    stm32_app_address = USER_APP_ADDRESS
    rcvd_file = bytes()

    while(read_len > 0):

        if(read_len < data_block):
            data_block = read_len

        packet = bytes()
        rcvd_packet = bytes()
        # payload structure 1-byte cmd + 4-byte addes + 1-byte data size + payload + 1-byte CRC

        # assemble cmd
        packet += int_to_byte(CMD_READ)

        # assemble address
        packet += int_to_byte(stm32_app_address >> 24 & 0xFF)
        packet += int_to_byte(stm32_app_address >> 16 & 0xFF)
        packet += int_to_byte(stm32_app_address >> 8 & 0xFF)
        packet += int_to_byte(stm32_app_address >> 0 & 0xFF)

        # assemble no of bytes to read
        packet += int_to_byte(data_block)

        # assemble crc
        crc = CRC8(packet, 6)
        packet += int_to_byte(crc)

        ser.write(int_to_byte(SYNC_CHAR))
        ser.write(int_to_byte(7))  # total bytes in packet
        ser.write(packet)

        reply = stm32_read_ack()

        if(reply == CMD_ACK):
            rcvd_packet = ser.read(data_block)
            crc_recvd = ord(ser.read(1))
            crc_calc = CRC8(rcvd_packet, data_block)
            if(crc_recvd == crc_calc):
                rcvd_file += rcvd_packet
                print('flash read succsess at ' + hex(stm32_app_address))
                read_len -= data_block
                stm32_app_address += data_block
                print("remaining bytes:{}".format(read_len))
            else:
                print("crc mismatch")

        elif (reply == CMD_NACK):
            print('flash read error at ' + hex(stm32_app_address))
            break

        if(read_len == 0):
            print("flash read successfull, jolly good!!!!")
            try:
                read_file_data = open("read_file.bin", 'wb')
                read_file_data.write(rcvd_file)
                read_file_data.close()

                len = os.path.getsize("read_file.bin")
                print("file size " + str(len))

            except(OSError):
                print("can not open " + bin_file)


def stm32_fast_read_flash():

    read_len = FLASH_SIZE
    stm32_app_address = USER_APP_ADDRESS

    rcvd_file = bytes()

    packet = bytes()
    # payload structure 1-byte cmd + 4-byte addes + 4-byte data size + 1-byte CRC

    # assemble cmd
    packet += int_to_byte(CMD_FAST_READ)

    # assemble address
    packet += int_to_byte(stm32_app_address >> 24 & 0xFF)
    packet += int_to_byte(stm32_app_address >> 16 & 0xFF)
    packet += int_to_byte(stm32_app_address >> 8 & 0xFF)
    packet += int_to_byte(stm32_app_address >> 0 & 0xFF)

    # assemble no of bytes to read
    packet += int_to_byte((read_len >> 24) & 0xFF)
    packet += int_to_byte((read_len >> 16) & 0xFF)
    packet += int_to_byte((read_len >> 8) & 0xFF)
    packet += int_to_byte((read_len >> 0) & 0xFF)

    # assemble crc
    crc = CRC8(packet, 9)
    packet += int_to_byte(crc)

    print(packet)

    ser.write(int_to_byte(SYNC_CHAR))
    ser.write(int_to_byte(10))  # total bytes in packet
    ser.write(packet)

    reply = stm32_read_ack()
    print(reply)

    if(reply == CMD_ACK):
        print('acked')
        for i in range(100):
            rcvd_file += ser.read(int(read_len/100))
            print(str(i) + '%')


        print("flash read successfull, jolly good!!!!")

        try:
            read_file_data = open("read_file.bin", 'wb')
            read_file_data.write(rcvd_file)
            read_file_data.close()

            len = os.path.getsize("read_file.bin")
            print("file size " + str(len))

        except(OSError):
            print("can not open " + bin_file)

    elif (reply == CMD_NACK):
        print('flash read error')


def stm32_write():
    f_file_len = 0
    f_file_exist = False
    data_block = 248
    stm32_app_address = USER_APP_ADDRESS

    try:
        f_file_len = os.path.getsize(bin_file)
        print("file size " + str(f_file_len))
        bin_file_data = open(bin_file, 'rb')
        f_file_exist = True
    except(OSError):
        print("can not open " + bin_file)

    while(f_file_len > 0):

        if(f_file_len < data_block):
            data_block = f_file_len

        bl_packet = bytes()
        # payload structure 1-byte cmd + 4-byte addes + 1-byte data size + payload + 1-byte CRC

        # assemble cmd
        bl_packet += int_to_byte(CMD_WRITE)

        # assemble address
        bl_packet += int_to_byte(stm32_app_address >> 24 & 0xFF)
        bl_packet += int_to_byte(stm32_app_address >> 16 & 0xFF)
        bl_packet += int_to_byte(stm32_app_address >> 8 & 0xFF)
        bl_packet += int_to_byte(stm32_app_address >> 0 & 0xFF)

        # assemble no of bytes to write
        bl_packet += int_to_byte(data_block)

        # assemble payload
        for x in range(data_block):
            bl_packet += bin_file_data.read(1)

        # assemble crc
        crc = CRC8(bl_packet, (data_block + 6))

        bl_packet += int_to_byte(crc)

        ser.write(int_to_byte(SYNC_CHAR))
        ser.write(int_to_byte(data_block + 7))
        ser.write(bl_packet)

        reply = stm32_read_ack()
        print(reply)
        if(reply == CMD_ACK):
            print('flash write success at ' + hex(stm32_app_address))
        elif (reply == CMD_NACK):
            print('flash write error at ' + hex(stm32_app_address))
            break

        f_file_len -= data_block
        stm32_app_address += data_block
        print("remaining bytes:{}".format(f_file_len))

        if(f_file_len == 0):
            print("flash write successfull, jolly good!!!!")

    if(f_file_exist):
        bin_file_data.close()
        print("closing file")


ser_open = False
port = SERIAL_PORT
baud = BAUDRATE

if len(sys.argv) >= 2:
    cmd = sys.argv[1]

    try:
        ser = serial.Serial("/dev/tty"+port, baud, timeout=10)
        ser_open = True
    except:
        print("\n Not valid port")
else:
    print("please enter port, baud rate and cmd and optional input file")

if ser_open:
    print("Port open success")

    ack = stm32_read_ack()

    if (ack == CMD_ACK):
        print("connected to target")
        stm32_send_ack()

        if(cmd == 'write'):
            if len(sys.argv) >= 3:
                bin_file = sys.argv[2]
                stm32_write()
                stm32_jump()
            else:
                print('please enter input file')
        elif(cmd == 'erase'):
            stm32_erase()
        elif(cmd == 'reset'):
            stm32_reset()
        elif(cmd == 'jump'):
            stm32_jump()
        elif(cmd == 'help'):
            stm32_get_help()
        elif(cmd == 'read'):
            stm32_read_flash()
        elif(cmd == 'fread'):
            stm32_fast_read_flash()
        else:
            print("invalid cmd")
    else:
        print("timeout error!! could not connect to target")


else:
    print("Port open failed")
