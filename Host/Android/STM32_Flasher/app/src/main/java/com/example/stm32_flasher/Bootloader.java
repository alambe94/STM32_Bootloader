package com.example.stm32_flasher;

import android.os.Handler;
import android.util.Log;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import static android.content.ContentValues.TAG;

public class Bootloader extends Thread {

    private final InputStream mInStream;
    private final OutputStream mOutStream;
    private final Handler mHandler;

    private byte[] readBinary = new byte[BootloaderConstants.STM32_FLASH_SIZE];
    private boolean isGetFlashData = false;

    private byte[] writeBinary;
    private boolean isSetFlashData = false;


    private boolean isCmdRunning = false;
    private byte cmdToRun;


    Bootloader(InputStream inputStream, OutputStream outputStream, Handler mHandler) {
        this.mInStream = inputStream;
        this.mOutStream = outputStream;
        this.mHandler = mHandler;
    }

    @Override
    public void run() {

        while (true) {

            try {
                sleep(100);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }

            if (isCmdRunning) {

                switch (cmdToRun) {
                    case BootloaderConstants.CMD_RESET:
                        stm32Reset();
                        break;
                    case BootloaderConstants.CMD_ERASE:
                        stm32Erase();
                        break;
                    case BootloaderConstants.CMD_JUMP:
                        stm32Jump();
                        break;
                    case BootloaderConstants.CMD_READ:
                        stm32ReadFlash();
                        break;
                    case BootloaderConstants.CMD_WRITE:
                        stm32WriteFlash();
                        break;
                    case BootloaderConstants.CMD_VERIFY:
                        stm32VerifyFlash();
                        break;
                    default:
                        break;
                }

                isCmdRunning = false;
                cmdToRun = 0x00;
            }
        }
    }

    private void write(byte[] bytes, int len) {
        try {
            mOutStream.write(bytes, 0, len);
        } catch (IOException e) {
            e.printStackTrace();
            Log.e(TAG, "Error occurred when sending data", e);
        }
    }

    private void stm32SendCMD(byte cmd) {
        byte[] temp = new byte[1];

        temp[0] = cmd;
        byte crc = Crc8.getCrc8(temp, 1);

        temp[0] = BootloaderConstants.SYNC_CHAR;
        write(temp, 1);

        temp[0] = 2;
        write(temp, 1);

        temp[0] = cmd;
        write(temp, 1);

        temp[0] = crc;
        write(temp, 1);
    }

    private byte stm32ReadByte(int timeOut) {
        long tstamp = System.currentTimeMillis();
        byte[] rx_char = new byte[1];

        try {
            while (System.currentTimeMillis() - tstamp < timeOut) {
                if (mInStream.available() > 0) {
                    mInStream.read(rx_char, 0, 1);
                    break;
                }
            }
        } catch (IOException e) {
            e.printStackTrace();
            Log.e(TAG, "read failed", e);
        }

        return rx_char[0];
    }

    private boolean stm32ReadACK(int timeOut) {
        byte reply = stm32ReadByte(timeOut);
        return reply == BootloaderConstants.CMD_ACK;

    }

    private void stm32Erase() {

        stm32SendCMD(BootloaderConstants.CMD_ERASE);

        if (stm32ReadACK(10000)) {
            mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "MCU flash erase success\n").sendToTarget();
        } else {
            mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "MCU flash erase failed\n").sendToTarget();
        }

    }

    private void stm32Reset() {

        stm32SendCMD(BootloaderConstants.CMD_RESET);

        if (stm32ReadACK(500)) {
            mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "MCU reset success\n").sendToTarget();
        } else {
            mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "MCU reset failed\n").sendToTarget();
        }

    }

    private void stm32Jump() {
        stm32SendCMD(BootloaderConstants.CMD_JUMP);

        if (stm32ReadACK(500)) {
            mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "MCU jump success\n").sendToTarget();
        } else {
            mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "MCU jump failed\n").sendToTarget();
        }
    }

    private void stm32ReadFlash() {

        long start_time = System.currentTimeMillis();
        int remainingBytes = BootloaderConstants.STM32_FLASH_SIZE;
        int stm32AapAddress = BootloaderConstants.STM32_FLASH_START;
        final int totalLen = remainingBytes;

        byte[] blPacket = new byte[10];
        byte[] rxBuffer = new byte[256];
        byte[] temp = new byte[1];

        int readBlockSize = 240;
        int blPacketIndex;

        int readFileIndex = 0;

        while (remainingBytes > 0) {

            if (remainingBytes < readBlockSize) {
                readBlockSize = (byte) remainingBytes;
            }

            blPacketIndex = 0;

            // assemble cmd
            blPacket[blPacketIndex++] = BootloaderConstants.CMD_READ;

            // no of char to receive from stm32
            blPacket[blPacketIndex++] = (byte) readBlockSize;

            // 2 bytes padding for stm32 word alignment
            blPacket[blPacketIndex++] = 0x00;
            blPacket[blPacketIndex++] = 0x00;

            // assemble address
            blPacket[blPacketIndex++] = (byte) (stm32AapAddress >> 24 & 0xFF);
            blPacket[blPacketIndex++] = (byte) (stm32AapAddress >> 16 & 0xFF);
            blPacket[blPacketIndex++] = (byte) (stm32AapAddress >> 8 & 0xFF);
            blPacket[blPacketIndex++] = (byte) (stm32AapAddress & 0xFF);

            // calculate crc
            byte crc = Crc8.getCrc8(blPacket, blPacketIndex);

            // assemble crc
            blPacket[blPacketIndex++] = crc;

            // send sync char
            temp[0] = BootloaderConstants.SYNC_CHAR;
            write(temp, 1);

            // send no of chars in blPacket
            temp[0] = (byte) blPacketIndex;
            write(temp, 1);

            // send blPacket
            write(blPacket, blPacketIndex);

            if (stm32ReadACK(500)) {

                for (int i = 0; i < readBlockSize; i++) {
                    rxBuffer[i] = stm32ReadByte(100);
                }

                byte crc_recvd = stm32ReadByte(100);

                byte crc_calc = Crc8.getCrc8(rxBuffer, readBlockSize);

                if (crc_recvd == crc_calc) {
                    System.arraycopy(rxBuffer, 0, readBinary, readFileIndex, readBlockSize);
                    readFileIndex += readBlockSize;
                    remainingBytes -= readBlockSize;
                    stm32AapAddress += readBlockSize;
                    mHandler.obtainMessage(BootloaderConstants.MESSAGE_PROGRESS_BAR, 100 - (remainingBytes * 100) / totalLen, 1).sendToTarget();
                } else {
                    mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "crc mismatch\n").sendToTarget();
                    break;
                }

            } else {
                mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "flash read error at 0x" + Integer.toHexString(stm32AapAddress) + "\n").sendToTarget();
                break;
            }

        }

        if (remainingBytes == 0) {
            isGetFlashData = true;
            long elapsedTime = System.currentTimeMillis() - start_time;
            mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "flash read successful, jolly good!!!!\n").sendToTarget();
            mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "elapsed time = " + elapsedTime + "\n").sendToTarget();
            mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "read speed = " + (totalLen / elapsedTime) + "KBS\n").sendToTarget();

        }

    }

    private void stm32WriteFlash() {

        long start_time = System.currentTimeMillis();
        int stm32AapAddress = BootloaderConstants.STM32_FLASH_START;
        int totalLen = 1;
        int remainingBytes = BootloaderConstants.STM32_FLASH_SIZE;

        byte[] blPacket = new byte[256];
        byte[] temp = new byte[1];

        int writeBlockSize = 240;
        int blPacketIndex;

        int writeFileIndex = 0;

        if (isSetFlashData) {
            mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "selected file size " + writeBinary.length + "\n").sendToTarget();
            totalLen = writeBinary.length;
            remainingBytes = totalLen;
        } else {
            mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "please select file first\n").sendToTarget();
        }

        while (remainingBytes > 0) {

            if (remainingBytes < writeBlockSize) {
                writeBlockSize = (byte) remainingBytes;
            }

            blPacketIndex = 0;

            // assemble cmd
            blPacket[blPacketIndex++] = BootloaderConstants.CMD_WRITE;

            // no of char to to write to stm32
            blPacket[blPacketIndex++] = (byte) writeBlockSize;

            // 2 bytes padding for stm32 word alignment
            blPacket[blPacketIndex++] = 0x00;
            blPacket[blPacketIndex++] = 0x00;

            // assemble address
            blPacket[blPacketIndex++] = (byte) (stm32AapAddress >> 24 & 0xFF);
            blPacket[blPacketIndex++] = (byte) (stm32AapAddress >> 16 & 0xFF);
            blPacket[blPacketIndex++] = (byte) (stm32AapAddress >> 8 & 0xFF);
            blPacket[blPacketIndex++] = (byte) (stm32AapAddress & 0xFF);

            System.arraycopy(writeBinary, writeFileIndex, blPacket, blPacketIndex, writeBlockSize);
            blPacketIndex += writeBlockSize;

            // calculate crc
            byte crc = Crc8.getCrc8(blPacket, blPacketIndex);

            // assemble crc
            blPacket[blPacketIndex++] = crc;

            // send sync char
            temp[0] = BootloaderConstants.SYNC_CHAR;
            write(temp, 1);

            // send no of chars in blPacket
            temp[0] = (byte) (blPacketIndex);
            write(temp, 1);

            // send blPacket
            write(blPacket, blPacketIndex);


            if (stm32ReadACK(500)) {
                writeFileIndex += writeBlockSize;
                remainingBytes -= writeBlockSize;
                stm32AapAddress += writeBlockSize;
                mHandler.obtainMessage(BootloaderConstants.MESSAGE_PROGRESS_BAR, 100 - (remainingBytes * 100) / totalLen, -1).sendToTarget();
            } else {
                mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "write flash error at 0x" + Integer.toHexString(stm32AapAddress) + "\n").sendToTarget();
                break;
            }

        }

        if (remainingBytes == 0) {
            long elapsedTime = System.currentTimeMillis() - start_time;
            mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "flash write successful, jolly good!!!!\n").sendToTarget();
            mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "elapsed time = " + elapsedTime + "\n").sendToTarget();
            mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "write speed = " + (totalLen / elapsedTime) + "KBS\n").sendToTarget();

        }
    }

    private void stm32VerifyFlash() {

        long start_time = System.currentTimeMillis();
        int stm32AapAddress = BootloaderConstants.STM32_FLASH_START;
        int totalLen = 1;
        int remainingBytes = BootloaderConstants.STM32_FLASH_SIZE;

        byte[] blPacket = new byte[256];
        byte[] temp = new byte[1];

        int writeBlockSize = 240;
        int blPacketIndex;

        int writeFileIndex = 0;

        if (isSetFlashData) {
            mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "selected file size " + writeBinary.length + "\n").sendToTarget();
            totalLen = writeBinary.length;
            remainingBytes = totalLen;
        } else {
            mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "please select file first\n").sendToTarget();
        }

        while (remainingBytes > 0) {

            if (remainingBytes < writeBlockSize) {
                writeBlockSize = (byte) remainingBytes;
            }

            blPacketIndex = 0;

            // assemble cmd
            blPacket[blPacketIndex++] = BootloaderConstants.CMD_VERIFY;

            // no of char to to write to stm32
            blPacket[blPacketIndex++] = (byte) writeBlockSize;

            // 2 bytes padding for stm32 word alignment
            blPacket[blPacketIndex++] = 0x00;
            blPacket[blPacketIndex++] = 0x00;

            // assemble address
            blPacket[blPacketIndex++] = (byte) (stm32AapAddress >> 24 & 0xFF);
            blPacket[blPacketIndex++] = (byte) (stm32AapAddress >> 16 & 0xFF);
            blPacket[blPacketIndex++] = (byte) (stm32AapAddress >> 8 & 0xFF);
            blPacket[blPacketIndex++] = (byte) (stm32AapAddress & 0xFF);

            System.arraycopy(writeBinary, writeFileIndex, blPacket, blPacketIndex, writeBlockSize);
            blPacketIndex += writeBlockSize;

            // calculate crc
            byte crc = Crc8.getCrc8(blPacket, blPacketIndex);

            // assemble crc
            blPacket[blPacketIndex++] = crc;

            // send sync char
            temp[0] = BootloaderConstants.SYNC_CHAR;
            write(temp, 1);

            // send no of chars in blPacket
            temp[0] = (byte) (blPacketIndex);
            write(temp, 1);

            // send blPacket
            write(blPacket, blPacketIndex);

            if (stm32ReadACK(500)) {
                writeFileIndex += writeBlockSize;
                remainingBytes -= writeBlockSize;
                stm32AapAddress += writeBlockSize;
                mHandler.obtainMessage(BootloaderConstants.MESSAGE_PROGRESS_BAR, 100 - (remainingBytes * 100) / totalLen, -1).sendToTarget();
            } else {
                mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "verify flash error at 0x" + Integer.toHexString(stm32AapAddress) + "\n").sendToTarget();
                break;
            }

        }

        if (remainingBytes == 0) {
            long elapsedTime = System.currentTimeMillis() - start_time;
            mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "flash verify successful, jolly good!!!!\n").sendToTarget();
            mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "elapsed time = " + elapsedTime + "\n").sendToTarget();
            mHandler.obtainMessage(BootloaderConstants.MESSAGE_LOG, "read speed = " + (totalLen / elapsedTime) + "KBS\n").sendToTarget();

        }
    }

    public boolean stm32GetFlashData(byte[] readBin) {

        if (isGetFlashData) {
            System.arraycopy(readBinary, 0, readBin, 0, readBinary.length);
            return true;
        }
        return false;
    }

    public void stm32SetFlashData(byte[] bytes, int len) {

        writeBinary = new byte[len];
        System.arraycopy(bytes, 0, writeBinary, 0, len);
        isSetFlashData = true;

    }

    public boolean cmdRun(byte cmd) {
        if (!isCmdRunning) {
            isCmdRunning = true;
            cmdToRun = cmd;
            return true;
        }
        return false;
    }

    public void close() {
        try {
            mOutStream.close();
            mInStream.close();
        } catch (IOException e) {
            e.printStackTrace();
            Log.e(TAG, "Could not close the streamt", e);
        }
    }

}
