/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.example.hellojni;

import android.Manifest;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.AssetFileDescriptor;
import android.content.res.AssetManager;
import android.media.AudioManager;
import android.media.MediaCodec;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.os.Build;
import android.os.Environment;
import android.os.Handler;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.os.Bundle;

import java.io.IOException;
import java.nio.ByteBuffer;


public class HelloJni extends Activity
{
    private static final String TAG = "cjh";
    private static final int MY_PERMISSIONS_REQUEST_WRITE_EXTERNAL = 200;
    private Handler mHandler = null;
    private Handler mCallerThreadHandler = null;
    private int mAutoPlayCount = 0;
    private final int DELAY_TIME = 100;

    private Runnable mRunnable = new Runnable() {
        @Override
        public void run() {

            playEffect();
            mAutoPlayCount++;
//                if (mAutoPlayCount > 100)
//                {
//                    return;
//                }

            mHandler.postDelayed(this, DELAY_TIME);
        }
    };

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        Log.d(TAG, "onCreate");
        super.onCreate(savedInstanceState);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            doRequestPermission();
        } else {
            init();
        }
    }

    private void doRequestPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            // Here, thisActivity is the current activity
            try {
                if (checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {

                    // Should we show an explanation?
                    if (shouldShowRequestPermissionRationale(Manifest.permission.WRITE_EXTERNAL_STORAGE)) {

                        // Show an expanation to the user *asynchronously* -- don't block
                        // this thread waiting for the user's response! After the user
                        // sees the explanation, try again to request the permission.

                    } else {

                        // No explanation needed, we can request the permission.

                        requestPermissions(new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                                MY_PERMISSIONS_REQUEST_WRITE_EXTERNAL);

                        // MY_PERMISSIONS_REQUEST_READ_CONTACTS is an
                        // app-defined int constant. The callback method gets the
                        // result of the request.
                    }
                } else {
                    init();
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    private void init() {
        PackageManager pm = getPackageManager();
        boolean isSupportLowLatency = pm.hasSystemFeature(PackageManager.FEATURE_AUDIO_LOW_LATENCY);

        Log.d(TAG, "isSupportLowLatency:" + isSupportLowLatency);

        int sampleRate = 44100;
        int bufferSizeInFrames = 192;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            AudioManager am = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
            String strSampleRate = am.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
            String strBufferSizeInFrames = am.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);

            sampleRate = Integer.parseInt(strSampleRate);
            bufferSizeInFrames = Integer.parseInt(strBufferSizeInFrames);

            Log.d(TAG, "sampleRate: " + sampleRate + ", framesPerBuffer: " + bufferSizeInFrames);
        } else {
            Log.d(TAG, "android version is lower than " + Build.VERSION_CODES.JELLY_BEAN_MR1);
        }

        jniCreate(getAssets(), sampleRate, bufferSizeInFrames);

        String[] files = new String[10];
        for (int i = 0; i < files.length; ++i) {
            files[i] = String.format("%02d.mp3", 1);
            Log.d(TAG, "load file: " + files[i]);
        }

//        jniLoadSamples(files);

        // UI
        ViewGroup.LayoutParams layoutParams = new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
        );

        LinearLayout layout = new LinearLayout(this);
        layout.setLayoutParams(layoutParams);
        layout.setOrientation(LinearLayout.VERTICAL);
        setContentView(layout);

        Button playBtn = createButton("Play", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                playEffect();
            }
        });

        layout.addView(playBtn);

        Button stopAllBtn = createButton("StopAll", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mHandler.removeCallbacksAndMessages(null);
            }
        });

        layout.addView(stopAllBtn);

        Button preloadMany = createButton("PreloadMany", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                String[] files = {
                        "test/A1-Guitar1-1.mp3",
                        "test/A1-Guitar1-2.mp3",
                        "test/A1-Guitar1-3.mp3",
                        "test/A1-Guitar1-4.mp3",
                        "test/A1-Piano1-1.mp3",
                        "test/A1-Piano1-2.mp3",
                        "test/A1-Piano1-3.mp3",
                        "test/A1-Piano1-4.mp3",
                        "test/A1-Piano2-1.mp3",
                        "test/A1-Piano2-2.mp3",
                        "test/A1-Piano2-3.mp3",
                        "test/A1-Piano2-4.mp3",
                        "test/A1-Piano3-1.mp3",
                        "test/A1-Piano3-2.mp3",
                        "test/A1-Piano3-3.mp3",
                        "test/A1-Piano3-4.mp3",
                        "test/A1-Piano4-1.mp3",
                        "test/A1-Piano4-2.mp3",
                        "test/A1-Piano4-3.mp3",
                        "test/A1-Piano4-4.mp3",
                        "test/A1-Rise1.mp3",
                        "test/A1-Rise2.mp3",
                        "test/A1-String1-1.mp3",
                        "test/A1-String1-2.mp3",
                        "test/A1-String1-3.mp3",
                        "test/A1-String1-4.mp3",
                        "test/A1.mp3",
                        "test/A10-Guitar1.mp3",
                        "test/A10-Guitar2.mp3",
                        "test/A10-Guitar3.mp3",
                        "test/A10-Guitar4.mp3",
                        "test/A10-begin.mp3",
                        "test/A10.mp3",
                        "test/A11-Bass1.mp3",
                        "test/A11-Bass2.mp3",
                        "test/A11-Bass3.mp3",
                        "test/A11-Bass4.mp3",
                        "test/A11-Guitar1.mp3",
                        "test/A11-Guitar2.mp3",
                        "test/A11-Guitar3.mp3",
                        "test/A11-Guitar4.mp3",
                        "test/A11-taccato1.mp3",
                        "test/A11-taccato2.mp3",
                        "test/A11-taccato3.mp3",
                        "test/A11-taccato4.mp3",
                        "test/A11.mp3",
                        "test/A12-taccato1.mp3",
                        "test/A12-taccato2.mp3",
                        "test/A12-taccato3.mp3",
                        "test/A12-taccato4.mp3",
                        "test/A12.mp3",
                        "test/A13-Strings1.mp3",
                        "test/A13-Strings2.mp3",
                        "test/A13-taccato1.mp3",
                        "test/A13-taccato2.mp3",
                        "test/A13.mp3",
                        "test/A14-Guitar1.mp3",
                        "test/A14-Guitar2.mp3",
                        "test/A14-Guitar3.mp3",
                        "test/A14-Guitar4.mp3",
                        "test/A14.mp3",
                        "test/A15-Begin.mp3",
                        "test/A15-Guitar1.mp3",
                        "test/A15-Guitar2.mp3",
                        "test/A15-Guitar3.mp3",
                        "test/A15-Guitar4.mp3",
                        "test/A15-Organ1.mp3",
                        "test/A15-Organ2.mp3",
                        "test/A15-Organ3.mp3",
                        "test/A15-Organ4.mp3",
                        "test/A15.mp3",
                        "test/A2.mp3",
                        "test/A3.mp3",
                        "test/A4-begin.mp3",
                        "test/A4.mp3",
                        "test/A5-Piano1.mp3",
                        "test/A5-Piano2.mp3",
                        "test/A5-Piano3.mp3",
                        "test/A5-Piano4.mp3",
                        "test/A5.mp3",
                        "test/A6-Bass1-1.mp3",
                        "test/A6-Bass1-2.mp3",
                        "test/A6-Bass1-3.mp3",
                        "test/A6-Bass1-4.mp3",
                        "test/A6-Piano1-1.mp3",
                        "test/A6-Piano1-2.mp3",
                        "test/A6-Piano1-3.mp3",
                        "test/A6-Piano1-4.mp3",
                        "test/A6-Piano2-1.mp3",
                        "test/A6-Piano2-2.mp3",
                        "test/A6-Piano2-3.mp3",
                        "test/A6-Piano2-4.mp3",
                        "test/A6.mp3",
                        "test/A7-Piano-1.mp3",
                        "test/A7-Piano-2.mp3",
                        "test/A7-String-1.mp3",
                        "test/A7-String-2.mp3",
                        "test/A7.mp3",
                        "test/A8-Piano-1.mp3",
                        "test/A8-Piano-2.mp3",
                        "test/A8-Piano-3.mp3",
                        "test/A8-Piano-4.mp3",
                        "test/A8-Piano-5.mp3",
                        "test/A8-Piano-6.mp3",
                        "test/A8-Piano-7.mp3",
                        "test/A8-Piano-8.mp3",
                        "test/A8.mp3",
                        "test/A9-Guitar1-1.mp3",
                        "test/A9-Guitar1-2.mp3",
                        "test/A9-Guitar1-3.mp3",
                        "test/A9-Guitar1-4.mp3",
                        "test/A9-Guitar2-1.mp3",
                        "test/A9-Guitar2-2.mp3",
                        "test/A9-Guitar2-3.mp3",
                        "test/A9-Guitar2-4.mp3",
                        "test/A9-Guitar3-1.mp3",
                        "test/A9-Guitar3-2.mp3",
                        "test/A9-Guitar3-3.mp3",
                        "test/A9-Guitar3-4.mp3",
                        "test/A9.mp3",
                        "test/AA-1.mp3",
                        "test/AB-1.mp3",
                        "test/B1-Piano-1.mp3",
                        "test/B1-Piano-2.mp3",
                        "test/B1-Piano-3.mp3",
                        "test/B1-Piano-4.mp3",
                        "test/B1-Piano-5.mp3",
                        "test/B1-Piano-6.mp3",
                        "test/B1-Piano-7.mp3",
                        "test/B1.mp3",
                        "test/B10.mp3",
                        "test/B11-Guitar1-1.mp3",
                        "test/B11-Guitar1-2.mp3",
                        "test/B11-Guitar1-3.mp3",
                        "test/B11-Guitar1-4.mp3",
                        "test/B11-Guitar1-5.mp3",
                        "test/B11-Guitar1-6.mp3",
                        "test/B11-Guitar1-7.mp3",
                        "test/B11-Guitar2-1.mp3",
                        "test/B11-Guitar2-2.mp3",
                        "test/B11-Guitar2-3.mp3",
                        "test/B11-Guitar2-4.mp3",
                        "test/B11-Guitar2-5.mp3",
                        "test/B11-Guitar2-6.mp3",
                        "test/B11-Guitar2-7.mp3",
                        "test/B11.mp3",
                        "test/B2-Guitar-1.mp3",
                        "test/B2-Guitar-10.mp3",
                        "test/B2-Guitar-11.mp3",
                        "test/B2-Guitar-12.mp3",
                        "test/B2-Guitar-13.mp3",
                        "test/B2-Guitar-14.mp3",
                        "test/B2-Guitar-15.mp3",
                        "test/B2-Guitar-16.mp3",
                        "test/B2-Guitar-17.mp3",
                        "test/B2-Guitar-18.mp3",
                        "test/B2-Guitar-19.mp3",
                        "test/B2-Guitar-2.mp3",
                        "test/B2-Guitar-20.mp3",
                        "test/B2-Guitar-21.mp3",
                        "test/B2-Guitar-22.mp3",
                        "test/B2-Guitar-23.mp3",
                        "test/B2-Guitar-24.mp3",
                        "test/B2-Guitar-25.mp3",
                        "test/B2-Guitar-3.mp3",
                        "test/B2-Guitar-4.mp3",
                        "test/B2-Guitar-5.mp3",
                        "test/B2-Guitar-6.mp3",
                        "test/B2-Guitar-7.mp3",
                        "test/B2-Guitar-8.mp3",
                        "test/B2-Guitar-9.mp3",
                        "test/B2-Piano-1.mp3",
                        "test/B2-Piano-2.mp3",
                        "test/B2-Piano-3.mp3",
                        "test/B2-Piano-4.mp3",
                        "test/B2-Piano-5.mp3",
                        "test/B2-Piano-6.mp3",
                        "test/B2-Piano-7.mp3",
                        "test/B2.mp3",
                        "test/B3-Piano1-1.mp3",
                        "test/B3-Piano1-10.mp3",
                        "test/B3-Piano1-11.mp3",
                        "test/B3-Piano1-12.mp3",
                        "test/B3-Piano1-13.mp3",
                        "test/B3-Piano1-14.mp3",
                        "test/B3-Piano1-15.mp3",
                        "test/B3-Piano1-16.mp3",
                        "test/B3-Piano1-17.mp3",
                        "test/B3-Piano1-18.mp3",
                        "test/B3-Piano1-19.mp3",
                        "test/B3-Piano1-2.mp3",
                        "test/B3-Piano1-20.mp3",
                        "test/B3-Piano1-21.mp3",
                        "test/B3-Piano1-22.mp3",
                        "test/B3-Piano1-23.mp3",
                        "test/B3-Piano1-24.mp3",
                        "test/B3-Piano1-25.mp3",
                        "test/B3-Piano1-3.mp3",
                        "test/B3-Piano1-4.mp3",
                        "test/B3-Piano1-5.mp3",
                        "test/B3-Piano1-6.mp3",
                        "test/B3-Piano1-7.mp3",
                        "test/B3-Piano1-8.mp3",
                        "test/B3-Piano1-9.mp3",
                        "test/B3-Piano2-1.mp3",
                        "test/B3-Piano2-10.mp3",
                        "test/B3-Piano2-2.mp3",
                        "test/B3-Piano2-3.mp3",
                        "test/B3-Piano2-4.mp3",
                        "test/B3-Piano2-5.mp3",
                        "test/B3-Piano2-6.mp3",
                        "test/B3-Piano2-7.mp3",
                        "test/B3-Piano2-8.mp3",
                        "test/B3-Piano2-9.mp3",
                        "test/B3.mp3",
                        "test/B4-Piano1-1.mp3",
                        "test/B4-Piano1-10.mp3",
                        "test/B4-Piano1-12.mp3",
                        "test/B4-Piano1-13.mp3",
                        "test/B4-Piano1-14.mp3",
                        "test/B4-Piano1-15.mp3",
                        "test/B4-Piano1-16.mp3",
                        "test/B4-Piano1-17.mp3",
                        "test/B4-Piano1-19.mp3",
                        "test/B4-Piano1-2.mp3",
                        "test/B4-Piano1-20.mp3",
                        "test/B4-Piano1-3.mp3",
                        "test/B4-Piano1-4.mp3",
                        "test/B4-Piano1-5.mp3",
                        "test/B4-Piano1-6.mp3",
                        "test/B4-Piano1-7.mp3",
                        "test/B4-Piano1-8.mp3",
                        "test/B4-Piano1-9.mp3",
                        "test/B4-Piano2-1.mp3",
                        "test/B4-Piano2-2.mp3",
                        "test/B4-Piano2-3.mp3",
                        "test/B4-Piano2-4.mp3",
                        "test/B4-Piano2-5.mp3",
                        "test/B4-Piano2-8.mp3",
                        "test/B4.mp3",
                        "test/B5-Cellos-1.mp3",
                        "test/B5-Cellos-2.mp3",
                        "test/B5-Cellos-3.mp3",
                        "test/B5-Cellos-4.mp3",
                        "test/B5-Cellos-5.mp3",
                        "test/B5-Cellos-6.mp3",
                        "test/B5-Cellos-7.mp3",
                        "test/B5.mp3",
                        "test/B6-Piano-1.mp3",
                        "test/B6-Piano-10.mp3",
                        "test/B6-Piano-11.mp3",
                        "test/B6-Piano-2.mp3",
                        "test/B6-Piano-3.mp3",
                        "test/B6-Piano-4.mp3",
                        "test/B6-Piano-5.mp3",
                        "test/B6-Piano-6.mp3",
                        "test/B6-Piano-7.mp3",
                        "test/B6-Piano-8.mp3",
                        "test/B6-Piano-9.mp3",
                        "test/B6-Synth-1.mp3",
                        "test/B6-Synth-2.mp3",
                        "test/B6-Synth-3.mp3",
                        "test/B6-Synth-4.mp3",
                        "test/B6-Synth-5.mp3",
                        "test/B6-Synth-6.mp3",
                        "test/B6.mp3",
                        "test/B7-piano2-1.mp3",
                        "test/B7-piano2-2.mp3",
                        "test/B7-piano2-3.mp3",
                        "test/B7-piano2-4.mp3",
                        "test/B7-piano2-5.mp3",
                        "test/B7-piano2-6.mp3",
                        "test/B7-piano2-7.mp3",
                        "test/B7-piano2-8.mp3",
                        "test/B7.mp3",
                        "test/B8-Synth-1.mp3",
                        "test/B8-Synth-2.mp3",
                        "test/B8-Synth-3.mp3",
                        "test/B8-Synth-4.mp3",
                        "test/B8-Synth-5.mp3",
                        "test/B8-Synth-6.mp3",
                        "test/B8-Synth-7.mp3",
                        "test/B8-Synth-8.mp3",
                        "test/B8.mp3",
                        "test/B9-Synth-1.mp3",
                        "test/B9-Synth-2.mp3",
                        "test/B9-Synth-3.mp3",
                        "test/B9-Synth-4.mp3",
                        "test/B9-Synth-5.mp3",
                        "test/B9-Synth-6.mp3",
                        "test/B9-Synth-7.mp3",
                        "test/B9-Synth-8.mp3",
                        "test/B9-Vilolins-1.mp3",
                        "test/B9-Vilolins-2.mp3",
                        "test/B9-Vilolins-3.mp3",
                        "test/B9-Vilolins-4.mp3",
                        "test/B9-Vilolins-5.mp3",
                        "test/B9-Vilolins-6.mp3",
                        "test/B9-Vilolins-7.mp3",
                        "test/B9-Vilolins-8.mp3",
                        "test/B9.mp3",
                        "test/BA-piano2-7.mp3",
                        "test/BB-piano2-7.mp3",
                        "test/L2.mp3",
                        "test/LevelBackground1.mp3",
                        "test/LevelBackground2.mp3",
                        "test/LevelBackground3.mp3",
                        "test/Spec-1.mp3",
                        "test/die.mp3",
                        "test/keytouch.mp3",
                        "test/slied.mp3",
                        "test/start.mp3",
                        "test/teach.mp3",
                };

                int size = files.length;
                String[] duplicatedFiles = new String[size];
                for (int i = 0; i < size; ++i) {
                    duplicatedFiles[i] = files[i % files.length];
                }

                jniLoadSamples(duplicatedFiles);

//                for (int i = 0; i < 100000; ++i)
//                {
//                    jniLoadSamples(getAssets(), files);
//                }
//                for (int i = 0; i< 100; ++i) {
//                    try {
//                        AssetManager manager = getAssets();
//
//                        AssetFileDescriptor descriptor = manager.openFd("test/A1-Guitar1-1.mp3");
//                        Log.d(TAG, "openFd return: " + descriptor.getLength());
//                        descriptor.close();
//
//                    } catch (IOException e) {
//                        e.printStackTrace();
//                    }
//                }
            }
        });

        layout.addView(preloadMany);


        mHandler = new Handler();
//        mHandler.postDelayed(mRunnable, DELAY_TIME);

        mCallerThreadHandler = new Handler();
        mCallerThreadHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                jniOnUpdate();
                mCallerThreadHandler.postDelayed(this, 16);
            }
        }, 16);
    }

    private Button createButton(String text, View.OnClickListener listener) {
        Button button = new Button(this);

        button.setOnClickListener(listener);

        ViewGroup.LayoutParams btnParams = new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
        );

        button.setText(text);
        button.setLayoutParams(btnParams);
        return button;
    }

    private void playEffect() {
        jniPlaySample(0, true);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           String permissions[], int[] grantResults) {
        switch (requestCode) {
            case MY_PERMISSIONS_REQUEST_WRITE_EXTERNAL: {
                // If request is cancelled, the result arrays are empty.
                if (grantResults.length > 0
                        && grantResults[0] == PackageManager.PERMISSION_GRANTED) {

                    // permission was granted, yay! Do the
                    // contacts-related task you need to do.
                    init();

                } else {

                    // permission denied, boo! Disable the
                    // functionality that depends on this permission.
                }
                return;
            }

            // other 'case' lines to check for other
            // permissions this app might request
        }
    }

    @Override
    protected void onDestroy() {
        Log.d(TAG, "onDestroy ...");
        super.onDestroy();
        mHandler.removeCallbacksAndMessages(null);
        jniShutdown();
    }

    private native boolean jniCreate(AssetManager manager, int sampleRate, int bufferSizeInFrames);
    private native boolean jniShutdown();
    private native void jniOnPause();
    private native void jniOnResume();
    private native void jniOnUpdate();
    private native boolean jniLoadSamples(String[] files);
    private native boolean jniPlaySample(int playIndex, boolean playState);

    /* this is used to load the 'hello-jni' library on application
     * startup. The library has already been unpacked into
     * /data/data/com.example.hellojni/lib/libhello-jni.so at
     * installation time by the package manager.
     */
    static {
        System.loadLibrary("hello-jni");
    }

    private static final String SAMPLE = Environment.getExternalStorageDirectory() + "/01.mp3";

    private class PlayerThread extends Thread {
        private MediaExtractor extractor;
        private MediaCodec decoder;

        public PlayerThread() {
        }

        @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
        @Override
        public void run() {
            try {
                extractor = new MediaExtractor();
                extractor.setDataSource(SAMPLE);

                MediaFormat format = null;
                String mime = null;
                int sampleRate = 0, channels = 0, bitrate = 0;
                long presentationTimeUs = 0, duration = 0;


                    for (int i = 0; i < extractor.getTrackCount(); i++) {//遍历媒体轨道 此处我们传入的是音频文件，所以也就只有一条轨道
                        format = extractor.getTrackFormat(i);
                        mime = format.getString(MediaFormat.KEY_MIME);
                        sampleRate = format.getInteger(MediaFormat.KEY_SAMPLE_RATE);
                        channels = format.getInteger(MediaFormat.KEY_CHANNEL_COUNT);
                        // if duration is 0, we are probably playing a live stream
                        duration = format.getLong(MediaFormat.KEY_DURATION);

                        if (mime.startsWith("audio")) {//获取音频轨道
//                    format.setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, 200 * 1024);
                            extractor.selectTrack(i);//选择此音频轨道
                            decoder = MediaCodec.createDecoderByType(mime);//创建Decode解码器
                            decoder.configure(format, null, null, 0);
                            break;
                        }
                    }

                if (decoder == null) {
                    Log.e("DecodeActivity", "Can't find video info!");
                    return;
                }

                decoder.start();

                ByteBuffer[] inputBuffers = decoder.getInputBuffers();
                ByteBuffer[] outputBuffers = decoder.getOutputBuffers();
                MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
                boolean isEOS = false;
                long startMs = System.currentTimeMillis();

                while (!Thread.interrupted()) {
                    if (!isEOS) {
                        int inIndex = decoder.dequeueInputBuffer(10000);
                        if (inIndex >= 0) {
                            ByteBuffer buffer = inputBuffers[inIndex];
                            int sampleSize = extractor.readSampleData(buffer, 0);
                            if (sampleSize < 0) {
                                // We shouldn't stop the playback at this point, just pass the EOS
                                // flag to decoder, we will get it again from the
                                // dequeueOutputBuffer
                                Log.d("DecodeActivity", "InputBuffer BUFFER_FLAG_END_OF_STREAM");
                                decoder.queueInputBuffer(inIndex, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                                isEOS = true;
                            } else {
                                decoder.queueInputBuffer(inIndex, 0, sampleSize, extractor.getSampleTime(), 0);
                                extractor.advance();
                            }
                        }
                    }

                    int outIndex = decoder.dequeueOutputBuffer(info, 10000);
                    switch (outIndex) {
                        case MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED:
                            Log.d("DecodeActivity", "INFO_OUTPUT_BUFFERS_CHANGED");
                            outputBuffers = decoder.getOutputBuffers();
                            break;
                        case MediaCodec.INFO_OUTPUT_FORMAT_CHANGED:
                            Log.d("DecodeActivity", "New format " + decoder.getOutputFormat());
                            break;
                        case MediaCodec.INFO_TRY_AGAIN_LATER:
                            Log.d("DecodeActivity", "dequeueOutputBuffer timed out!");
                            break;
                        default:
                            ByteBuffer buffer = outputBuffers[outIndex];
                            Log.v("DecodeActivity", "We can't use this buffer but render it due to the API limit, " + buffer);

                            // We use a very simple clock to keep the video FPS, or the video
                            // playback will be too fast
                            while (info.presentationTimeUs / 1000 > System.currentTimeMillis() - startMs) {
                                try {
                                    sleep(10);
                                } catch (InterruptedException e) {
                                    e.printStackTrace();
                                    break;
                                }
                            }
                            decoder.releaseOutputBuffer(outIndex, true);
                            break;
                    }

                    // All decoded frames have been rendered, we can stop playing now
                    if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                        Log.d("DecodeActivity", "OutputBuffer BUFFER_FLAG_END_OF_STREAM");
                        break;
                    }
                }

                decoder.stop();
                decoder.release();
                extractor.release();
            } catch (IOException e) {
                e.printStackTrace();
                return;
            }
        }
    }

    @Override
    protected void onPause() {
        Log.d(TAG, "onPause ...");
        super.onPause();
        mHandler.removeCallbacksAndMessages(null);
        jniOnPause();
    }

    @Override
    protected void onResume() {
        Log.d(TAG, "onResume ...");
        super.onResume();
//        mHandler.postDelayed(mRunnable, DELAY_TIME);
        jniOnResume();
    }
}
