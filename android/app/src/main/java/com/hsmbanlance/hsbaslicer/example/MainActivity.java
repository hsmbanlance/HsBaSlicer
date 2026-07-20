package com.hsmbanlance.hsbaslicer.example;

import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;

/**
 * HsBaSlicer Android 示例入口。
 *
 * 加载本地库 HsBaSlicer（包含 FDM/SLA/SLS 流水线示例），
 * 通过 JNI 调用 runPipelineExamples() 运行全部工艺示例。
 *
 * 注意：runPipelineExamples() 为非生产入口，仅供示例和测试使用。
 */
public class MainActivity extends Activity {

    static {
        // 加载 HsBaSlicer 本地共享库（已包含 DllHsBaSlicer 依赖）
        System.loadLibrary("HsBaSlicer");
    }

    /**
     * JNI 入口：运行 FDM / SLA / SLS 三种流水线示例。
     * 对应 native 函数：
     *   Java_com_hsmbanlance_hsbaslicer_example_MainActivity_runPipelineExamples
     */
    private native void runPipelineExamples();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        TextView tv = new TextView(this);
        tv.setText("HsBaSlicer native libs loaded\nRunning pipeline examples...");
        setContentView(tv);

        // 在 UI 线程上直接调用示例（仅供测试，生产环境应在后台线程执行）
        try {
            runPipelineExamples();
            tv.setText("HsBaSlicer pipeline examples completed successfully.");
        } catch (Exception e) {
            tv.setText("HsBaSlicer pipeline examples failed: " + e.getMessage());
        }
    }
}
