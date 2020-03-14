package com.example.stm32_flasher;

import androidx.appcompat.app.AppCompatActivity;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;

public class SelectComInterface extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_select_com_interface);

        findViewById(R.id.bBluetooth).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Intent i = new Intent(SelectComInterface.this, ComBluetooth.class);
                startActivity(i);
            }
        });

        findViewById(R.id.bUsbSerial).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Intent i = new Intent(SelectComInterface.this, ComUsbSerial.class);
                startActivity(i);
            }
        });
    }

}
