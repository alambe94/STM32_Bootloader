import serial
import os
import sys
import time


USER_APP_ADDRESS = 0x08008000  # 0x08004000->8K, 0x08004000 -> 16k, 0x08008000->32k
FLASH_SIZE = 496000 + 512000  # uncomment for 407VG
Serial_Port = ""

"""
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
"""

CMD_WRITE = 0x50
CMD_READ = 0x51
CMD_ERASE = 0x52
CMD_RESET = 0x53
CMD_JUMP = 0x54
CMD_VERIFY = 0x55

CMD_ACK = 0x90
CMD_NACK = 0x91
CMD_ERROR = 0x92

CMD_HELP = 0x40

CMD_CONNECT = 0x7F

SYNC_CHAR = ord('$')

# Maxim APPLICATION NOTE 27

CRC8_Table = [
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
    0x74, 0x2A, 0xC8, 0x96, 0x15, 0x4B, 0xA9, 0xF7, 0xB6, 0xE8, 0x0A, 0x54, 0xD7, 0x89, 0x6B, 0x35]


def millis(): return int(round(time.time() * 1000))


def CRC8(data, count):
    crc = 0
    for i in range(count):
        crc = CRC8_Table[crc ^ data[i]]
    return crc


def int_to_bytes(data):
    return bytes([data])


def stm32_bl_send_cmd(cmd):

    crc = CRC8([cmd], 1)
    Serial_Port.write(int_to_bytes(SYNC_CHAR))
    Serial_Port.write(int_to_bytes(2))

    Serial_Port.write(int_to_bytes(cmd))
    Serial_Port.write(int_to_bytes(crc))


def stm32_send_ack():
    Serial_Port.write(int_to_bytes(CMD_ACK))


def stm32_read_line():

    rx_string = Serial_Port.readline().decode('utf-8')
    return rx_string


def stm32_read_ack():
    rx_char = Serial_Port.read(1)
    if(rx_char == b''):
        return False
    else:
        return (ord(rx_char) == CMD_ACK)


def stm32_erase():

    Serial_Port.timeout = 10

    stm32_bl_send_cmd(CMD_ERASE)

    print("erasing", end='')

    for i in range(10):
        print(".", end='')
        time.sleep(0.01)

    if(stm32_read_ack()):
        print("flash erase success")
    else:
        print("flash erase error")

    Serial_Port.timeout = 1


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
    if(stm32_read_ack()):
        print("mcu reset")
    else:
        print("mcu reset failed")


def stm32_jump():
    stm32_bl_send_cmd(CMD_JUMP)
    if(stm32_read_ack()):
        print("jumping to user apllication")
    else:
        print("jump to user apllication failed")


def stm32_read_flash():

    start = millis()
    read_block_size = 240
    remaining_bytes = FLASH_SIZE
    stm32_app_address = USER_APP_ADDRESS
    rcvd_file = bytes()

    while(remaining_bytes > 0):

        if(remaining_bytes < read_block_size):
            read_block_size = remaining_bytes

        bl_packet = bytes()
        rcvd_packet = bytes()

        # assemble cmd
        bl_packet += int_to_bytes(CMD_READ)

        # no of char to receive from stm32
        bl_packet += int_to_bytes(read_block_size)

        # 2 bytes padding for stm32 word alignment
        bl_packet += int_to_bytes(0x00)
        bl_packet += int_to_bytes(0x00)

        # assemble address
        bl_packet += int_to_bytes(stm32_app_address >> 24 & 0xFF)
        bl_packet += int_to_bytes(stm32_app_address >> 16 & 0xFF)
        bl_packet += int_to_bytes(stm32_app_address >> 8 & 0xFF)
        bl_packet += int_to_bytes(stm32_app_address >> 0 & 0xFF)

        # calculate crc
        crc = CRC8(bl_packet, 8)

        # assemble crc
        bl_packet += int_to_bytes(crc)

        # send sync char
        Serial_Port.write(int_to_bytes(SYNC_CHAR))

        # send no of chars in bl_packet
        Serial_Port.write(int_to_bytes(9))  # total bytes in bl_packet

        # send bl_packet
        Serial_Port.write(bl_packet)

        if(stm32_read_ack()):

            rcvd_packet = Serial_Port.read(read_block_size)

            crc_recvd = ord(Serial_Port.read(1))

            crc_calc = CRC8(rcvd_packet, read_block_size)

            if(crc_recvd == crc_calc):

                rcvd_file += rcvd_packet
                #print("read write success at " + hex(stm32_app_address))
                remaining_bytes -= read_block_size
                stm32_app_address += read_block_size
                print("\rremaining bytes:{}".format(remaining_bytes), end='')

            else:
                print("crc mismatch")
                break

        else:
            print("flash read error at " + hex(stm32_app_address))
            break

        if(remaining_bytes == 0):
            file_size = 0
            print("flash read successfull, jolly good!!!!")
            try:
                read_file_data = open("read_file.bin", "wb")
                read_file_data.write(rcvd_file)
                read_file_data.close()

                file_size = os.path.getsize("read_file.bin")
                print("file size " + str(file_size))

            except(OSError):
                print("can not open " + "read_file.bin")

            elapsed_time = millis() - start
            print("elapsed time = {}ms".format(int(elapsed_time)))
            print("read speed = {}kB/S".format(int(file_size/elapsed_time)))


def stm32_write(bin_file):

    start = millis()

    remaining_bytes = 0
    f_file_exist = False
    f_file_size = 0
    write_block_size = 240
    stm32_app_address = USER_APP_ADDRESS

    print("opening file...")

    try:
        remaining_bytes = os.path.getsize(bin_file)
        f_file_size = remaining_bytes
        bin_file_data = open(bin_file, "rb")
        print("file size " + str(remaining_bytes))
        f_file_exist = True
    except(OSError):
        print("can not open " + bin_file)

    while(remaining_bytes > 0):

        if(remaining_bytes < write_block_size):
            write_block_size = remaining_bytes

        bl_packet = bytes()

        # assemble cmd
        bl_packet += int_to_bytes(CMD_WRITE)

        # no of char to flash to stm32
        bl_packet += int_to_bytes(write_block_size)

        # 2 bytes padding for stm32 word alignment
        bl_packet += int_to_bytes(0x00)
        bl_packet += int_to_bytes(0x00)

        # assemble address
        bl_packet += int_to_bytes(stm32_app_address >> 24 & 0xFF)
        bl_packet += int_to_bytes(stm32_app_address >> 16 & 0xFF)
        bl_packet += int_to_bytes(stm32_app_address >> 8 & 0xFF)
        bl_packet += int_to_bytes(stm32_app_address >> 0 & 0xFF)

        # assemble payload
        for x in range(write_block_size):
            bl_packet += bin_file_data.read(1)

        # calculate crc
        crc = CRC8(bl_packet, (write_block_size + 8))

        # assemble crc
        bl_packet += int_to_bytes(crc)

        # send sync char
        Serial_Port.write(int_to_bytes(SYNC_CHAR))

        # send no of char in bl_packet
        Serial_Port.write(int_to_bytes(write_block_size + 9))

        # send bl_packet
        Serial_Port.write(bl_packet)

        if(stm32_read_ack()):
            #print("flash write success at " + hex(stm32_app_address))
            pass
        else:
            print("flash write error at " + hex(stm32_app_address))
            break

        remaining_bytes -= write_block_size
        stm32_app_address += write_block_size
        print("\rremaining bytes:{}".format(remaining_bytes), end='')

        if(remaining_bytes == 0):
            print("\nflash write successfull, jolly good!!!!")
            elapsed_time = millis() - start
            print("elapsed time = {}ms".format(int(elapsed_time)))
            print("write speed = {}kB/S".format(int(f_file_size/elapsed_time)))

    if(f_file_exist):
        bin_file_data.close()
        print("closing file")


def stm32_verify(bin_file):

    start = millis()

    remaining_bytes = 0
    f_file_exist = False
    f_file_size = 0
    write_block_size = 240
    stm32_app_address = USER_APP_ADDRESS

    print("opening file...")

    try:
        remaining_bytes = os.path.getsize(bin_file)
        f_file_size = remaining_bytes
        bin_file_data = open(bin_file, "rb")
        print("file size " + str(remaining_bytes))
        f_file_exist = True
    except(OSError):
        print("can not open " + bin_file)

    while(remaining_bytes > 0):

        if(remaining_bytes < write_block_size):
            write_block_size = remaining_bytes

        bl_packet = bytes()

        # assemble cmd
        bl_packet += int_to_bytes(CMD_VERIFY)

        # no of char to flash to stm32
        bl_packet += int_to_bytes(write_block_size)

        # 2 bytes padding for stm32 word alignment
        bl_packet += int_to_bytes(0x00)
        bl_packet += int_to_bytes(0x00)

        # assemble address
        bl_packet += int_to_bytes(stm32_app_address >> 24 & 0xFF)
        bl_packet += int_to_bytes(stm32_app_address >> 16 & 0xFF)
        bl_packet += int_to_bytes(stm32_app_address >> 8 & 0xFF)
        bl_packet += int_to_bytes(stm32_app_address >> 0 & 0xFF)

        # assemble payload
        for x in range(write_block_size):
            bl_packet += bin_file_data.read(1)

        # calculate crc
        crc = CRC8(bl_packet, (write_block_size + 8))

        # assemble crc
        bl_packet += int_to_bytes(crc)

        # send sync char
        Serial_Port.write(int_to_bytes(SYNC_CHAR))

        # send no of char in bl_packet
        Serial_Port.write(int_to_bytes(write_block_size + 9))

        # send bl_packet
        Serial_Port.write(bl_packet)

        if(stm32_read_ack()):
            #print("verify write success at " + hex(stm32_app_address))
            pass
        else:
            print("verify write error at " + hex(stm32_app_address))
            break

        remaining_bytes -= write_block_size
        stm32_app_address += write_block_size
        print("\rremaining bytes:{}".format(remaining_bytes), end='')

        if(remaining_bytes == 0):
            print("\nverify successfull, jolly good!!!!")
            elapsed_time = millis() - start
            print("elapsed time = {}ms".format(int(elapsed_time)))
            print("verify speed = {}kB/S".format(int(f_file_size/elapsed_time)))

    if(f_file_exist):
        bin_file_data.close()
        print("closing file")


def main():

    ser_open = False
    global Serial_Port

    print(sys.argv)

    if len(sys.argv) >= 4:

        port = sys.argv[1]
        baud = int(sys.argv[2])
        cmd = sys.argv[3]

        try:
            Serial_Port = serial.Serial(port, baud, timeout=1)
            ser_open = True
        except:
            print("Not valid port")
    else:
        print("please enter port, baud, cmd and optional input file")

    if ser_open:
        print("Port open success")

        # CMD_CONNECT used for auto baud detection on stm32

        retry = 10
        connected = False
        while(retry):
            retry -= 1
            Serial_Port.write(int_to_bytes(CMD_CONNECT))
            if(stm32_read_ack() == False):
                time.sleep(100)
            else:
                connected = True
                break

        if(connected == False):
            print("stm32 device connection failed")
        else:
            print("connected to stm32 device")
            if(cmd == "write"):
                if len(sys.argv) >= 5:
                    bin_file = sys.argv[4]
                    stm32_write(bin_file)
                    # stm32_jump()
                else:
                    print("please enter input file")
            elif(cmd == "erase"):
                stm32_erase()
            elif(cmd == "reset"):
                stm32_reset()
            elif(cmd == "jump"):
                stm32_jump()
            elif(cmd == "help"):
                stm32_get_help()
            elif(cmd == "read"):
                stm32_read_flash()
            elif(cmd == "verify"):
                if len(sys.argv) >= 5:
                    bin_file = sys.argv[4]
                    stm32_verify(bin_file)
                else:
                    print("please enter input file to verfy")
            else:
                print("invalid cmd")

        Serial_Port.close()

    else:
        print("Port open failed")


if __name__ == "__main__":
    main()
