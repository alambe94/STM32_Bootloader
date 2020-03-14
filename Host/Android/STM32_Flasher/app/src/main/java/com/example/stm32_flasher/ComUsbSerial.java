package com.example.stm32_flasher;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;

public class ComUsbSerial extends AppCompatActivity {


    public static String EXTRA_ADDRESS = "serial_port";



    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_com_usb_serial);
    }
}
