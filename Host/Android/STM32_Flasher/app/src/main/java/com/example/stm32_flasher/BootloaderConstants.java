package com.example.stm32_flasher;

import java.util.UUID;

public final class BootloaderConstants {

    public static final int STM32_FLASH_START = 0x08004000;
    public static final int STM32_FLASH_SIZE = 496000;

    public static final int MESSAGE_LOG = 11;
    public static final int MESSAGE_PROGRESS_BAR = 12;

    public static final byte CMD_WRITE = 0x50;
    public static final byte CMD_READ = 0x51;
    public static final byte CMD_ERASE = 0x52;
    public static final byte CMD_RESET = 0x53;
    public static final byte CMD_JUMP = 0x54;
    public static final byte CMD_VERIFY = 0x55;

    public static final byte CMD_ACK = (byte) 0x90;
    public static final byte CMD_NACK = (byte) 0x91;
    public static final byte CMD_ERROR = (byte) 0x92;

    public static final byte CMD_HELP = 0x40;

    public static final byte CMD_CONNECT = 0x7F;

    public static final byte SYNC_CHAR = '$';


    public static final int PICK_FILE_RESULT_CODE = 15;
    public static final int SAVE_FILE_RESULT_CODE = 16;
    public static final UUID myUUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");

}
