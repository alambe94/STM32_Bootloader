package com.example.stm32_flasher;

import android.os.Handler;
import android.util.Log;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import static android.content.ContentValues.TAG;

public class BootLoader extends Thread {

    private final InputStream mInStream;
    private final OutputStream mOutStream;
    private final Handler mHandler;

    private byte[] readBinary = new byte[BootLoaderConstants.STM32_FLASH_SIZE];
    private boolean isReadBinary = false;

    private byte[] writeBinary = new byte[BootLoaderConstants.STM32_FLASH_SIZE];
    private boolean isWriteBinary = false;


    private boolean isCmdRunning = false;
    private byte cmdToRun;


    BootLoader(InputStream inputStream, OutputStream outputStream, Handler mHandler) {
        this.mInStream = inputStream;
        this.mOutStream = outputStream;
        this.mHandler = mHandler;
    }

    public void run() {
        // Keep listening to the InputStream until an exception occurs.
        while (true) {

            if (isCmdRunning) {

                switch (cmdToRun) {
                    case BootLoaderConstants.CMD_RESET:
                        stm32Reset();
                        break;
                    case BootLoaderConstants.CMD_ERASE:
                        stm32Erase();
                        break;
                    case BootLoaderConstants.CMD_JUMP:
                        stm32Jump();
                        break;
                    case BootLoaderConstants.CMD_READ:
                        stm32ReadFlash();
                        break;
                    case BootLoaderConstants.CMD_WRITE:
                        stm32WriteFlash();
                        break;
                    case BootLoaderConstants.CMD_VERIFY:
                        break;
                    default:
                        break;
                }

                isCmdRunning = false;
            }
        }
    }

    private void write(byte[] bytes) {
        try {
            mOutStream.write(bytes);
        } catch (IOException e) {
            e.printStackTrace();
            Log.e(TAG, "Error occurred when sending data", e);
        }
    }

    public void cancel() {
        try {
            mOutStream.close();
        } catch (IOException e) {
            e.printStackTrace();
            Log.e(TAG, "Could not close the connect socket", e);
        }
    }

    private void stm32SendCMD(byte cmd) {
        byte[] temp = new byte[1];

        temp[0] = cmd;
        byte crc = CRC8.getCRC8(temp, 1);

        temp[0] = BootLoaderConstants.SYNC_CHAR;
        write(temp);

        temp[0] = 2;
        write(temp);

        temp[0] = cmd;
        write(temp);

        temp[0] = crc;
        write(temp);
    }

    private byte stm32ReadByte(int timeOut) {
        long tstamp = System.currentTimeMillis();
        byte[] rx_char = new byte[1];

        try {
            while (System.currentTimeMillis() - tstamp < timeOut && mInStream.available() == 0) {
            }
            mInStream.read(rx_char, 0, 1);
        } catch (IOException e) {
            e.printStackTrace();
            Log.e(TAG, "read failed", e);
        }

        return rx_char[0];
    }

    boolean stm32ReadACK(int timeOut) {
        byte reply = stm32ReadByte(timeOut);
        return reply == BootLoaderConstants.CMD_ACK;
    }

    void stm32Erase() {

        stm32SendCMD(BootLoaderConstants.CMD_ERASE);

        if (stm32ReadACK(10000)) {
            mHandler.obtainMessage(BootLoaderConstants.MESSAGE_LOG,"MCU flash erase success\n").sendToTarget();
        } else {
            mHandler.obtainMessage(BootLoaderConstants.MESSAGE_LOG, "MCU flash erase failed\n").sendToTarget();
        }

    }

    void stm32Reset() {

        stm32SendCMD(BootLoaderConstants.CMD_RESET);

        if (stm32ReadACK(100)) {
            mHandler.obtainMessage(BootLoaderConstants.MESSAGE_LOG, "MCU reset success\n").sendToTarget();
        } else {
            mHandler.obtainMessage(BootLoaderConstants.MESSAGE_LOG,"MCU reset failed\n").sendToTarget();
        }

    }

    void stm32Jump() {
        stm32SendCMD(BootLoaderConstants.CMD_JUMP);

        if (stm32ReadACK(100)) {
            mHandler.obtainMessage(BootLoaderConstants.MESSAGE_LOG,"MCU jump success\n").sendToTarget();
        } else {
            mHandler.obtainMessage(BootLoaderConstants.MESSAGE_LOG,"MCU jump failed\n").sendToTarget();
        }
    }

    void stm32ReadFlash() {

        long start_time = System.currentTimeMillis();
        int flashReadLen = BootLoaderConstants.STM32_FLASH_SIZE;
        int stm32AapAddress = BootLoaderConstants.STM32_FLASH_START;
        final int totalLen = flashReadLen;

        byte[] blPacket = new byte[10];
        byte[] rxBuffer = new byte[256];
        byte[] temp = new byte[1];

        byte readBlockSize = (byte) 240;
        int blPacketIndex;

        int readFileIndex = 0;
        isReadBinary = false;

        while (flashReadLen > 0) {

            if (flashReadLen < readBlockSize) {
                readBlockSize = (byte) flashReadLen;
            }

            blPacketIndex = 0;

            // assemble cmd
            blPacket[blPacketIndex++] = BootLoaderConstants.CMD_READ;

            // no char to receive from stm32
            blPacket[blPacketIndex++] = readBlockSize;

            // 2 bytes padding for stm32 word alignment
            blPacket[blPacketIndex++] = 0x00;
            blPacket[blPacketIndex++] = 0x00;

            // assemble address
            blPacket[blPacketIndex++] = (byte) (stm32AapAddress >> 24 & 0xFF);
            blPacket[blPacketIndex++] = (byte) (stm32AapAddress >> 16 & 0xFF);
            blPacket[blPacketIndex++] = (byte) (stm32AapAddress >> 8 & 0xFF);
            blPacket[blPacketIndex++] = (byte) (stm32AapAddress & 0xFF);

            // calculate crc
            byte crc = CRC8.getCRC8(blPacket, blPacketIndex);

            // assemble crc
            blPacket[blPacketIndex++] = crc;

            // send sync char
            temp[0] = BootLoaderConstants.SYNC_CHAR;
            write(temp);

            // send no chars in blPacket
            temp[0] = (byte) blPacketIndex;
            write(temp);

            // send blPacket
            write(blPacket);

            if (stm32ReadACK(100)) {

                for (int i = 0; i < readBlockSize; i++) {
                    rxBuffer[i] = stm32ReadByte(100);
                }

                byte crc_recvd = stm32ReadByte(100);

                byte crc_calc = CRC8.getCRC8(rxBuffer, readBlockSize);

                if (crc_recvd == crc_calc) {
                    System.arraycopy(rxBuffer, 0, readBinary, readFileIndex, readBlockSize);
                    readFileIndex += readBlockSize;
                    flashReadLen -= readBlockSize;
                    stm32AapAddress += readBlockSize;
                    mHandler.obtainMessage(BootLoaderConstants.MESSAGE_PROGRESS_BAR, 100 - (flashReadLen * 100) / totalLen, 1).sendToTarget();
                } else {
                    mHandler.obtainMessage(BootLoaderConstants.MESSAGE_LOG,"crc mismatch\n").sendToTarget();
                    break;
                }

            } else {
                mHandler.obtainMessage(BootLoaderConstants.MESSAGE_LOG,"flash read error at " + stm32AapAddress).sendToTarget();
                break;
            }
        }

        if (flashReadLen == 0) {
            isReadBinary = true;
            long elapsedTime = System.currentTimeMillis() - start_time;
            mHandler.obtainMessage(BootLoaderConstants.MESSAGE_LOG,"flash read successful, jolly good!!!!\n").sendToTarget();
            mHandler.obtainMessage(BootLoaderConstants.MESSAGE_LOG,"elapsed time = " + elapsedTime + "\n").sendToTarget();
            mHandler.obtainMessage(BootLoaderConstants.MESSAGE_LOG,"read speed = " + (totalLen / elapsedTime) + "KBS\n").sendToTarget();

        }

    }

    void stm32WriteFlash() {

        long start_time = System.currentTimeMillis();
        int stm32AapAddress = BootLoaderConstants.STM32_FLASH_START;
        int totalLen = 1;
        int bytesRemaining = 0;

        byte[] blPacket = new byte[256];
        byte[] temp = new byte[1];

        byte writeBlockSize = (byte) 240;
        int blPacketIndex;

        int writeFileIndex = 0;

        byte[] flashData = new byte[BootLoaderConstants.STM32_FLASH_SIZE];

        if(isWriteBinary)
        {
            mHandler.obtainMessage(BootLoaderConstants.MESSAGE_LOG,"selected file size " + writeBinary.length + "\n").sendToTarget();
            totalLen = writeBinary.length;
            bytesRemaining = totalLen;
        }
        else {
            mHandler.obtainMessage(BootLoaderConstants.MESSAGE_LOG,"please select file first\n").sendToTarget();
        }

        while (bytesRemaining > 0) {

            if (bytesRemaining < writeBlockSize) {
                writeBlockSize = (byte) bytesRemaining;
            }

            blPacketIndex = 0;

            // assemble cmd
            blPacket[blPacketIndex++] = BootLoaderConstants.CMD_WRITE;

            // no char to to write to stm32
            blPacket[blPacketIndex++] = writeBlockSize;

            // 2 bytes padding for stm32 word alignment
            blPacket[blPacketIndex++] = 0x00;
            blPacket[blPacketIndex++] = 0x00;

            // assemble address
            blPacket[blPacketIndex++] = (byte) (stm32AapAddress >> 24 & 0xFF);
            blPacket[blPacketIndex++] = (byte) (stm32AapAddress >> 16 & 0xFF);
            blPacket[blPacketIndex++] = (byte) (stm32AapAddress >> 8 & 0xFF);
            blPacket[blPacketIndex++] = (byte) (stm32AapAddress & 0xFF);

            System.arraycopy(flashData, writeFileIndex, blPacket, blPacketIndex, writeBlockSize);
            blPacketIndex += writeBlockSize;

            // calculate crc
            byte crc = CRC8.getCRC8(blPacket, blPacketIndex);

            // assemble crc
            blPacket[blPacketIndex++] = crc;

            // send sync char
            temp[0] = BootLoaderConstants.SYNC_CHAR;
            write(temp);

            // send no chars in blPacket
            temp[0] = (byte) (blPacketIndex);
            write(temp);

            // send blPacket
            write(blPacket);

            if (stm32ReadACK(100)) {
                writeFileIndex += writeBlockSize;
                bytesRemaining -= writeBlockSize;
                stm32AapAddress += writeBlockSize;
                mHandler.obtainMessage(BootLoaderConstants.MESSAGE_PROGRESS_BAR, 100 - (bytesRemaining * 100) / totalLen, -1).sendToTarget();
            } else {
                mHandler.obtainMessage(BootLoaderConstants.MESSAGE_LOG,"write flash error at " + stm32AapAddress).sendToTarget();
                break;
            }
        }

        if (bytesRemaining == 0) {
            isReadBinary = true;
            long elapsedTime = System.currentTimeMillis() - start_time;
            mHandler.obtainMessage(BootLoaderConstants.MESSAGE_LOG, "flash write successful, jolly good!!!!\n").sendToTarget();
            mHandler.obtainMessage(BootLoaderConstants.MESSAGE_LOG, "elapsed time = " + elapsedTime + "\n").sendToTarget();
            mHandler.obtainMessage(BootLoaderConstants.MESSAGE_LOG, "read speed = " + (totalLen / elapsedTime) + "KBS\n").sendToTarget();

        }
    }

    public byte[] stm32GetFlashData() {
        if (isReadBinary)
        {
            byte arr[] = new byte[readBinary.length];
            System.arraycopy(readBinary,0,arr,0,readBinary.length);
            return readBinary;
        }
        return null;
    }

    public void stm32SetFlashData(byte[] bytes) {

        System.arraycopy(bytes, 0, writeBinary, 0, bytes.length);
        isWriteBinary = true;

    }

    public boolean cmdRun(byte cmd) {
        if (!isCmdRunning) {
            isCmdRunning = true;
            cmdToRun = cmd;
            return true;
        }
        return false;
    }
}
