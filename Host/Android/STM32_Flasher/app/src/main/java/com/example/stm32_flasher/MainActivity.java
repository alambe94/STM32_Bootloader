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

    private static final String TAG = "STM32_Flasher";
    static final UUID myUUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");

    private ProgressDialog btConnectionProgress;
    private String bluetoothAddress;
    private boolean isBtConnected = false;
    private String serial_port;

    private boolean isSetFlash = false;
    private boolean isGetFlash = false;


    BootLoader bootLoaderThread;
    private static Handler tvLogHandler;

    TextView tvLog;
    ProgressBar pBar;

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
        bluetoothAddress = intent.getStringExtra(ComBluetooth.EXTRA_ADDRESS);
        if (bluetoothAddress != null) {
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
                if (msg.what == BootLoaderConstants.MESSAGE_LOG) {
                    tvLog.append((String) msg.obj);
                } else if (msg.what == BootLoaderConstants.MESSAGE_PROGRESS_BAR) {
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
                            BootLoaderConstants.PICK_FILE_RESULT_CODE);
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

                startActivityForResult(intent, BootLoaderConstants.SAVE_FILE_RESULT_CODE);
            }

        });

        findViewById(R.id.btWrite).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                if (isSetFlash) {
                    tvLog.append("writing stm32 flash\n");
                    bootLoaderThread.cmdRun(BootLoaderConstants.CMD_WRITE);
                } else {
                    tvLog.append("please first select the file to flash\n");
                }
            }
        });

        findViewById(R.id.btReadFile).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                tvLog.append("reading stm32 flash\n");
                bootLoaderThread.cmdRun(BootLoaderConstants.CMD_READ);
            }
        });

        findViewById(R.id.btEraseMcu).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                tvLog.append("erasing stm32\n");
                bootLoaderThread.cmdRun(BootLoaderConstants.CMD_ERASE);
            }
        });

        findViewById(R.id.btResetMcu).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                tvLog.append("resetting mcu\n");
                bootLoaderThread.cmdRun(BootLoaderConstants.CMD_RESET);
            }
        });

        findViewById(R.id.btJumpMcu).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                tvLog.append("entering user app\n");
                bootLoaderThread.cmdRun(BootLoaderConstants.CMD_JUMP);
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == BootLoaderConstants.PICK_FILE_RESULT_CODE && resultCode == Activity.RESULT_OK) {
            if (data.getData() != null && data.getData().getPath() != null) {
                try {
                    InputStream ins = getContentResolver().openInputStream(data.getData());
                    if (ins != null) {
                        byte[] writBytes = new byte[ins.available()];
                        tvLog.append("selected file and size " + data.getData().getPath() + writBytes.length +"\n");
                        bootLoaderThread.stm32SetFlashData(writBytes);
                    }

                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        } else if (requestCode == BootLoaderConstants.SAVE_FILE_RESULT_CODE && resultCode == Activity.RESULT_OK) {
            if (data.getData() != null && data.getData().getPath() != null) {
                try {
                    OutputStream ots = getContentResolver().openOutputStream(data.getData());
                    if (ots != null) {
                        ots.write(bootLoaderThread.stm32GetFlashData());
                        ots.close();
                    }
                } catch (IOException e) {
                    e.printStackTrace();
                }

            }

        }

    }


    private class ConnectBT extends AsyncTask<Void, Void, Void>  // UI thread
    {
        private boolean ConnectSuccess = true; //if it's here, it's almost connected
        BluetoothSocket btSocket;

        @Override
        protected void onPreExecute() {
            btConnectionProgress = ProgressDialog.show(MainActivity.this, "Connecting...", "Please wait!!!");  //show a progress dialog
        }

        @Override
        protected Void doInBackground(Void... devices) //while the progress dialog is shown, the connection is done in background
        {
            try {
                if (!isBtConnected) {
                    BluetoothDevice BT = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(bluetoothAddress);//connects to the device's address and checks if it's available
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

                InputStream tmpIn = null;
                OutputStream tmpOut = null;

                // member streams are final.
                try {
                    tmpIn = btSocket.getInputStream();
                } catch (IOException e) {
                    e.printStackTrace();
                    Log.e(TAG, "Error occurred when creating input stream", e);
                }
                try {
                    tmpOut = btSocket.getOutputStream();
                } catch (IOException e) {
                    e.printStackTrace();
                    Log.e(TAG, "Error occurred when creating output stream", e);
                }

                bootLoaderThread = new BootLoader(tmpIn,tmpOut,tvLogHandler);
                bootLoaderThread.start();

            }
            btConnectionProgress.dismiss();
        }
    }

    // fast way to call Toast
    private void msg(String s) {
        Toast.makeText(getApplicationContext(), s, Toast.LENGTH_LONG).show();
    }


}



