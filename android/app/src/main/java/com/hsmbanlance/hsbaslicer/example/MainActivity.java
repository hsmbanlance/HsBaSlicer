package com.hsmbanlance.hsbaslicer.example;

import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;

public class MainActivity extends Activity {
    static {
        System.loadLibrary("HsBaSlicer");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        TextView tv = new TextView(this);
        tv.setText("HsBaSlicer native libs loaded");
        setContentView(tv);
    }
}
