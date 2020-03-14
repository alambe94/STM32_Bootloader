package com.example.stm32_flasher;

public final class BootLoaderConstants {

    public static final int STM32_FLASH_START = 0x08008000;
    public static final int STM32_FLASH_SIZE = 480000;

    public static final int MESSAGE_LOG = 11;
    public static final int MESSAGE_PROGRESS_BAR = 11;

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

    public static final byte SYNC_CHAR = '$';


    public static final int PICK_FILE_RESULT_CODE = 15;
    public static final int SAVE_FILE_RESULT_CODE = 16;
}
