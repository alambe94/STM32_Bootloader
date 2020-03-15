import serial
import os
import sys
import time


USER_APP_ADDRESS = 0x08008000
FLASH_SIZE = 480000 #+512000 #uncomment for 407VG
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


millis = lambda: int(round(time.time() * 1000))

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

        # no char to receive from stm32
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

        # send no chars in bl_packet
        Serial_Port.write(int_to_bytes(9))  # total bytes in bl_packet

        # send bl_packet
        Serial_Port.write(bl_packet)

        if(stm32_read_ack()):

            rcvd_packet = Serial_Port.read(read_block_size)

            crc_recvd = ord(Serial_Port.read(1))

            crc_calc = CRC8(rcvd_packet, read_block_size)

            if(crc_recvd == crc_calc):

                rcvd_file += rcvd_packet
                print("flash read succsess at " + hex(stm32_app_address))
                remaining_bytes -= read_block_size
                stm32_app_address += read_block_size
                print("remaining bytes:{}".format(remaining_bytes))

            else:
                print("crc mismatch")

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

        # no char to flash to stm32
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

        # send no char in bl_packet
        Serial_Port.write(int_to_bytes(write_block_size + 9))

        # send bl_packet
        Serial_Port.write(bl_packet)

        if(stm32_read_ack()):
            print("flash write success at " + hex(stm32_app_address))
        else:
            print("flash write error at " + hex(stm32_app_address))
            break

        remaining_bytes -= write_block_size
        stm32_app_address += write_block_size
        print("remaining bytes:{}".format(remaining_bytes))

        if(remaining_bytes == 0):
            print("flash write successfull, jolly good!!!!")
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

        # no char to flash to stm32
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

        # send no char in bl_packet
        Serial_Port.write(int_to_bytes(write_block_size + 9))

        # send bl_packet
        Serial_Port.write(bl_packet)

        if(stm32_read_ack()):
            print("verify write success at " + hex(stm32_app_address))
        else:
            print("verify write error at " + hex(stm32_app_address))
            break

        remaining_bytes -= write_block_size
        stm32_app_address += write_block_size
        print("remaining bytes:{}".format(remaining_bytes))

        if(remaining_bytes == 0):
            print("verify write successfull, jolly good!!!!")
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
        cmd  = sys.argv[3]

        try:
            Serial_Port = serial.Serial(port, baud, timeout=1)
            ser_open = True
        except:
            print("Not valid port")
    else:
        print("please enter port, baud, cmd and optional input file")

    if ser_open:
        print("Port open success")

        if(cmd == "write"):
            if len(sys.argv) >= 5:
                bin_file = sys.argv[4]
                stm32_write(bin_file)
                #stm32_jump()
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

