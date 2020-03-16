package com.example.stm32_flasher;

import java.util.UUID;

final class BootloaderConstants {

    static final int STM32_FLASH_START = 0x08008000;
    static final int STM32_FLASH_SIZE = 480000;

    static final int MESSAGE_LOG = 11;
    static final int MESSAGE_PROGRESS_BAR = 12;

    static final byte CMD_WRITE = 0x50;
    static final byte CMD_READ = 0x51;
    static final byte CMD_ERASE = 0x52;
    static final byte CMD_RESET = 0x53;
    static final byte CMD_JUMP = 0x54;
    static final byte CMD_VERIFY = 0x55;

    static final byte CMD_ACK = (byte) 0x90;
    static final byte CMD_NACK = (byte) 0x91;
    static final byte CMD_ERROR = (byte) 0x92;

    static final byte CMD_HELP = 0x40;

    static final byte SYNC_CHAR = '$';


    static final int PICK_FILE_RESULT_CODE = 15;
    static final int SAVE_FILE_RESULT_CODE = 16;
    static final UUID myUUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");

}
