package com.example.stm32_flasher;

import androidx.appcompat.app.AppCompatActivity;

import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.Set;

public class ComBluetooth extends AppCompatActivity {

    public static final int BL_EN_REQUEST_CODE = 10;
    ListView btList;

    private BluetoothAdapter myBluetooth;
    private Set<BluetoothDevice> pairedDevices;
    ArrayList list = new ArrayList();
    public static String EXTRA_ADDRESS = "bluetooth_address";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_com_bluetooth);

        btList      =  findViewById(R.id.btList);
        myBluetooth =  BluetoothAdapter.getDefaultAdapter();

        findViewById(R.id.btOff).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                myBluetooth.disable();
                Toast.makeText(getApplicationContext(), "Turned off" ,Toast.LENGTH_LONG).show();
            }
        });

        findViewById(R.id.btOn).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Intent getVisible = new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
                startActivityForResult(getVisible, BL_EN_REQUEST_CODE);
            }
        });

    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == BL_EN_REQUEST_CODE) {
            pairedDevices = myBluetooth.getBondedDevices();
            if (pairedDevices.size() > 0) {
                for (BluetoothDevice bt : pairedDevices) {
                    list.add(bt.getName() + "\n" + bt.getAddress()); //Get the device's name and the address
                }
            } else {
                Toast.makeText(getApplicationContext(), "No Paired Bluetooth Devices Found.", Toast.LENGTH_LONG).show();
            }
            final ArrayAdapter adapter = new ArrayAdapter(this, android.R.layout.simple_list_item_1, list);
            btList.setAdapter(adapter);
            btList.setOnItemClickListener(myListClickListener); //Method called when the device from the list is clicked
        }
    }

    private AdapterView.OnItemClickListener myListClickListener = new AdapterView.OnItemClickListener()
    {
        public void onItemClick (AdapterView<?> av, View v, int arg2, long arg3)
        {
            // Get the device MAC address, the last 17 chars in the View
            String info = ((TextView) v).getText().toString();
            String address = info.substring(info.length() - 17);

            // Make an intent to start next activity.
            Intent i = new Intent(ComBluetooth.this, MainActivity.class);

            //Change the activity.
            i.putExtra(EXTRA_ADDRESS, address);
            startActivity(i);
        }
    };
}
