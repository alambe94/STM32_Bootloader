package com.example.stm32_flasher;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;

import android.app.Activity;
import android.app.ProgressDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.Intent;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Message;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.view.View;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import java.io.BufferedWriter;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.util.UUID;

public class MainActivity extends AppCompatActivity {

    private final int STM32_FLASH_START = 0x08008000;
    private final int STM32_FLASH_SIZE = 480000;

    public static final int PICK_FILE_RESULT_CODE = 1;
    public static final int SAVE_FILE_RESULT_CODE = 11;

    final byte CMD_WRITE = 0x50;
    final byte CMD_READ = 0x51;
    final byte CMD_ERASE = 0x52;
    final byte CMD_RESET = 0x53;
    final byte CMD_JUMP = 0x54;
    final byte CMD_VERIFY = 0x55;

    final byte CMD_ACK = (byte) 0x90;
    final byte CMD_NACK = (byte) 0x91;
    final byte CMD_ERROR = (byte) 0x92;

    final byte CMD_HELP = 0x40;

    final byte SYNC_CHAR = '$';

    private static final String TAG = "STM32_Flasher";
    static final UUID myUUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");

    private ProgressDialog bt_connection_progress;
    private String bluetooth_address;
    private boolean isBtConnected = false;
    private String serial_port;

    private Uri fileUri;
    private String filePath;

    ConnectedThread readWriteThread;
    private static Handler tvLogHandler;

    TextView tvLog;
    ProgressBar pBar;


    /*Maxim APPLICATION NOTE 27 */
    int[] CRC8_Table =
            {
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
                    116, 42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107, 53};

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        tvLog = findViewById(R.id.tvLog);
        tvLog.setMovementMethod(new ScrollingMovementMethod());
        tvLog.setBackgroundColor(ContextCompat.getColor(this, R.color.lightBlue));

        pBar = findViewById(R.id.progressBar);

        pBar.setMax(100);
        pBar.setProgress(0);

        Intent intent = getIntent();
        bluetooth_address = intent.getStringExtra(ComBluetooth.EXTRA_ADDRESS);
        if (bluetooth_address != null) {
            // connect bluetooth
            new ConnectBT().execute();
        }

        serial_port = intent.getStringExtra(ComUsbSerial.EXTRA_ADDRESS);
        if (serial_port != null) {
            // open serial port
        }

        tvLogHandler = new Handler() {
            @Override
            public void handleMessage(Message msg) {
                // msg.what == 1 message  for textbox
                // msg.what == 2 message  for progressbar
                if (msg.what == 1) {
                    tvLog.append((String) msg.obj);
                } else if (msg.what == 2) {
                    pBar.setProgress(msg.arg1);
                }
            }
        };

        findViewById(R.id.btOpenFile).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
                intent.setType("*/*");
                intent.addCategory(Intent.CATEGORY_OPENABLE);

                try {
                    startActivityForResult(
                            Intent.createChooser(intent, "Select a File to Upload"),
                            PICK_FILE_RESULT_CODE);
                } catch (android.content.ActivityNotFoundException e) {
                    e.printStackTrace();
                    Toast.makeText(getApplicationContext(), "Please install a File Manager.", Toast.LENGTH_LONG).show();
                }
            }

        });

        findViewById(R.id.btSaveFile).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);

                // filter to only show openable items.
                intent.addCategory(Intent.CATEGORY_OPENABLE);

                // Create a file with the requested Mime type
                intent.setType("application/octet-stream");
                intent.putExtra(Intent.EXTRA_TITLE, "stm32ReadFile.bin");

                startActivityForResult(intent, SAVE_FILE_RESULT_CODE);
            }

        });

        findViewById(R.id.btWrite).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {

                if (fileUri != null) {
                    tvLog.append("writing stm32 flash\n");
                    new Thread(new Runnable() {
                        @Override
                        public void run() {
                            readWriteThread.stm32WriteFlash(fileUri);
                        }
                    }).start();
                }
                else
                {
                    tvLog.append("please first select file to flash\n");
                }
            }
        });

        findViewById(R.id.btReadFile).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                tvLog.append("reading stm32 flash\n");
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        readWriteThread.stm32ReadFlash();
                    }
                }).start();

            }
        });

        findViewById(R.id.btEraseMcu).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                tvLog.append("erasing stm32\n");
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        readWriteThread.stm32Erase();
                    }
                }).start();
            }
        });

        findViewById(R.id.btResetMcu).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                tvLog.append("resetting mcu\n");
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        readWriteThread.stm32Reset();
                    }
                }).start();
            }
        });

        findViewById(R.id.btJumpMcu).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                tvLog.append("enetr user app\n");
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        readWriteThread.stm32Jump();
                    }
                }).start();
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == PICK_FILE_RESULT_CODE && resultCode == Activity.RESULT_OK) {
            fileUri = data.getData();
            if (fileUri != null) {
                filePath = fileUri.getPath();
            }
            tvLog.append("selected file = " + filePath + "\n");
        } else if (requestCode == SAVE_FILE_RESULT_CODE && resultCode == Activity.RESULT_OK) {
            if (data != null
                    && data.getData() != null) {
                readWriteThread.stm32SaveFlash(data.getData());
            }

        }

    }


    private class ConnectBT extends AsyncTask<Void, Void, Void>  // UI thread
    {
        private boolean ConnectSuccess = true; //if it's here, it's almost connected
        BluetoothSocket btSocket;

        @Override
        protected void onPreExecute() {
            bt_connection_progress = ProgressDialog.show(MainActivity.this, "Connecting...", "Please wait!!!");  //show a progress dialog
        }

        @Override
        protected Void doInBackground(Void... devices) //while the progress dialog is shown, the connection is done in background
        {
            try {
                if (!isBtConnected) {
                    BluetoothDevice BT = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(bluetooth_address);//connects to the device's address and checks if it's available
                    btSocket = BT.createInsecureRfcommSocketToServiceRecord(myUUID);//create a RFCOMM (SPP) connection
                    BluetoothAdapter.getDefaultAdapter().cancelDiscovery();
                    btSocket.connect();//start connection
                }
            } catch (IOException e) {
                e.printStackTrace();
                ConnectSuccess = false;//if the try failed, you can check the exception here
            }
            return null;
        }

        @Override
        protected void onPostExecute(Void result) //after the doInBackground, it checks if everything went fine
        {
            super.onPostExecute(result);

            if (!ConnectSuccess) {
                msg("Connection Failed. Is it a SPP Bluetooth? Try again.");
                finish();
            } else {
                msg("Connected.");
                isBtConnected = true;
                readWriteThread = new ConnectedThread(btSocket);
                readWriteThread.start();
            }
            bt_connection_progress.dismiss();
        }
    }

    private class ConnectedThread extends Thread {

        private final BluetoothSocket mmSocket;
        private final InputStream mmInStream;
        private final OutputStream mmOutStream;

        private byte[]  readBinary = new byte[STM32_FLASH_SIZE];
        private boolean isReadBinary = false;

        ConnectedThread(BluetoothSocket socket) {

            mmSocket = socket;
            InputStream tmpIn = null;
            OutputStream tmpOut = null;

            // Get the input and output streams; using temp objects because
            // member streams are final.
            try {
                tmpIn = socket.getInputStream();
            } catch (IOException e) {
                e.printStackTrace();
                Log.e(TAG, "Error occurred when creating input stream", e);
            }
            try {
                tmpOut = socket.getOutputStream();
            } catch (IOException e) {
                e.printStackTrace();
                Log.e(TAG, "Error occurred when creating output stream", e);
            }

            mmInStream = tmpIn;
            mmOutStream = tmpOut;
        }

        public void run() {
            // Keep listening to the InputStream until an exception occurs.
            while (true) {

            }
        }

        // Call this from the main activity to send data to the remote device.
        void write(byte[] bytes) {
            try {
                mmOutStream.write(bytes);
            } catch (IOException e) {
                e.printStackTrace();
                Log.e(TAG, "Error occurred when sending data", e);
            }
        }

        // Call this method from the main activity to shut down the connection.
        public void cancel() {
            try {
                mmSocket.close();
            } catch (IOException e) {
                e.printStackTrace();
                Log.e(TAG, "Could not close the connect socket", e);
            }
        }

        byte CRC8(byte[] data, int len) {

            int crc = 0;
            int temp = 0;

            for (int i = 0; i < len; i++) {
                temp = crc ^ ((int) data[i]) & 0xFF;
                crc = (CRC8_Table[temp]);
            }

            return (byte) crc;
        }

        private void stm32SendCMD(byte cmd) {
            byte[] temp = new byte[1];

            temp[0] = cmd;
            byte crc = CRC8(temp, 1);

            temp[0] = SYNC_CHAR;
            readWriteThread.write(temp);

            temp[0] = 2;
            readWriteThread.write(temp);

            temp[0] = cmd;
            readWriteThread.write(temp);

            temp[0] = crc;
            readWriteThread.write(temp);
        }

        byte stm32ReadByte(int timeOut) {
            long tstamp = System.currentTimeMillis();
            byte[] rx_char = new byte[1];

            try {
                while (System.currentTimeMillis() - tstamp < timeOut && mmInStream.available() == 0) {
                }
                mmInStream.read(rx_char, 0, 1);
            } catch (IOException e) {
                e.printStackTrace();
                Log.e(TAG, "read failed", e);
            }

            return rx_char[0];
        }


        boolean stm32ReadACK(int timeOut) {
            byte reply = stm32ReadByte(timeOut);
            if (reply == CMD_ACK) {
                return true;
            }
            return false;
        }

        void stm32Erase() {

            stm32SendCMD(CMD_ERASE);

            if (stm32ReadACK(10000)) {
                tvLogHandler.obtainMessage(1, -1, -1, "MCU flash erase success\n").sendToTarget();
            } else {
                tvLogHandler.obtainMessage(1, -1, -1, "MCU flash erase failed\n").sendToTarget();
            }

        }

        void stm32Reset() {

            stm32SendCMD(CMD_RESET);

            if (stm32ReadACK(100)) {
                tvLogHandler.obtainMessage(1, -1, -1, "MCU reset success\n").sendToTarget();
            } else {
                tvLogHandler.obtainMessage(1, -1, -1, "MCU reset failed\n").sendToTarget();
            }

        }

        void stm32Jump() {
            stm32SendCMD(CMD_JUMP);

            if (stm32ReadACK(100)) {
                tvLogHandler.obtainMessage(1, -1, -1, "MCU jump success\n").sendToTarget();
            } else {
                tvLogHandler.obtainMessage(1, -1, -1, "MCU jump failed\n").sendToTarget();
            }
        }

        void stm32ReadFlash() {

            long start_time = System.currentTimeMillis();
            long flashReadLen = STM32_FLASH_SIZE;
            long stm32AapAddress = STM32_FLASH_START;
            final long totalLen = flashReadLen;

            byte[] blPacket = new byte[10];
            byte[] rxBuffer = new byte[256];
            byte[] temp = new byte[1];

            int readBlockSize = 240;
            int blPacketIndex = 0;

            int readFileIndex = 0;
            isReadBinary = false;


            while (flashReadLen > 0) {

                if (flashReadLen < readBlockSize) {
                    readBlockSize = (byte) flashReadLen;
                }

                blPacketIndex = 0;

                // assemble cmd
                blPacket[blPacketIndex++] = CMD_READ;

                // no char to receive from stm32
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
                byte crc = CRC8(blPacket, 8);

                // assemble crc
                blPacket[blPacketIndex++] = crc;

                // send sync char
                temp[0] = SYNC_CHAR;
                readWriteThread.write(temp);

                // send no chars in blPacket
                temp[0] = (byte) blPacketIndex;
                readWriteThread.write(temp);

                // send blPacket
                readWriteThread.write(blPacket);

                if (stm32ReadACK(100)) {

                    for (int i = 0; i < readBlockSize; i++) {
                        rxBuffer[i] = stm32ReadByte(100);
                    }

                    byte crc_recvd = stm32ReadByte(100);

                    byte crc_calc = CRC8(rxBuffer, readBlockSize);

                    if (crc_recvd == crc_calc) {
                        System.arraycopy(rxBuffer,0,readBinary,readFileIndex,readBlockSize);
                        readFileIndex += readBlockSize;
                        flashReadLen -= readBlockSize;
                        stm32AapAddress += readBlockSize;
                        tvLogHandler.obtainMessage(2, (int) (100 - (flashReadLen * 100) / totalLen), 1).sendToTarget();
                    } else {
                        tvLogHandler.obtainMessage(1, -1, -1, "crc mismatch\n").sendToTarget();
                        break;
                    }

                } else {
                    tvLogHandler.obtainMessage(1, -1, -1, "flash read error at " + stm32AapAddress).sendToTarget();
                    break;
                }
            }

            if (flashReadLen == 0) {
                isReadBinary = true;
                long elapsedTime = System.currentTimeMillis() - start_time;
                tvLogHandler.obtainMessage(1, -1, -1, "flash read successful, jolly good!!!!\n").sendToTarget();
                tvLogHandler.obtainMessage(1, -1, -1, "elapsed time = " + elapsedTime + "\n").sendToTarget();
                tvLogHandler.obtainMessage(1, -1, -1, "read speed = " + (totalLen / elapsedTime) + "KBS\n").sendToTarget();

            }

        }

        void stm32SaveFlash(Uri uri) {
            OutputStream outputStream;
            try {
                outputStream = getContentResolver().openOutputStream(uri);
                if (outputStream != null) {
                    if (isReadBinary) {
                        isReadBinary = false;
                        outputStream.write(readBinary);
                        outputStream.flush();
                        outputStream.close();
                        tvLogHandler.obtainMessage(1, -1, -1, "flash saved\n").sendToTarget();
                    }
                    else {
                        tvLogHandler.obtainMessage(1, -1, -1, "please read stm32 flash first\n").sendToTarget();
                    }

                }
            } catch (IOException e) {
                e.printStackTrace();
                tvLogHandler.obtainMessage(1, -1, -1, "flash save failed\n").sendToTarget();
            }
        }


        void stm32WriteFlash(Uri uri) {

            long start_time = System.currentTimeMillis();
            long stm32AapAddress = STM32_FLASH_START;
            long totalLen = 1;
            long bytesRemaining = 0;

            byte[] blPacket = new byte[256];
            byte[] rxBuffer = new byte[256];
            byte[] temp = new byte[1];

            int writeBlockSize = 240;
            int blPacketIndex = 0;

            int writeFileIndex = 0;

            byte[] flashData = new byte[STM32_FLASH_SIZE];

            InputStream inputStream = null;
            try {
                inputStream = getContentResolver().openInputStream(uri);
                if (inputStream != null) {
                    totalLen = inputStream.read(flashData);
                    bytesRemaining = totalLen;
                }
                tvLogHandler.obtainMessage(1, -1, -1, "selected file size " + totalLen + "\n").sendToTarget();
            } catch (FileNotFoundException ex) {
                ex.printStackTrace();
                tvLogHandler.obtainMessage(1, -1, -1, "file not found\n").sendToTarget();
            } catch (IOException ex) {
                ex.printStackTrace();
            }


            while (bytesRemaining > 0) {

                if (bytesRemaining < writeBlockSize) {
                    writeBlockSize = (byte) bytesRemaining;
                }

                blPacketIndex = 0;

                // assemble cmd
                blPacket[blPacketIndex++] = CMD_WRITE;

                // no char to to write to stm32
                blPacket[blPacketIndex++] = (byte) writeBlockSize;

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
                byte crc = CRC8(blPacket, 8 + writeBlockSize);

                // assemble crc
                blPacket[blPacketIndex++] = crc;

                // send sync char
                temp[0] = SYNC_CHAR;
                readWriteThread.write(temp);

                // send no chars in blPacket
                temp[0] = (byte) (blPacketIndex);
                readWriteThread.write(temp);

                // send blPacket
                readWriteThread.write(blPacket);

                if (stm32ReadACK(100)) {
                    writeFileIndex += writeBlockSize;
                    bytesRemaining -= writeBlockSize;
                    stm32AapAddress += writeBlockSize;
                    tvLogHandler.obtainMessage(2, (int) (100 - (bytesRemaining * 100) / totalLen), 1).sendToTarget();

                } else {
                    tvLogHandler.obtainMessage(1, -1, -1, "write flash error at " + stm32AapAddress).sendToTarget();
                    break;
                }
            }

            if (bytesRemaining == 0) {
                isReadBinary = true;
                long elapsedTime = System.currentTimeMillis() - start_time;
                tvLogHandler.obtainMessage(1, -1, -1, "flash write successful, jolly good!!!!\n").sendToTarget();
                tvLogHandler.obtainMessage(1, -1, -1, "elapsed time = " + elapsedTime + "\n").sendToTarget();
                tvLogHandler.obtainMessage(1, -1, -1, "read speed = " + (totalLen / elapsedTime) + "KBS\n").sendToTarget();

            }


        }








    }

    // fast way to call Toast
    private void msg(String s) {
        Toast.makeText(getApplicationContext(), s, Toast.LENGTH_LONG).show();
    }


}



