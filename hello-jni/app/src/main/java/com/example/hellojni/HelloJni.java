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
    private int mAutoPlayCount = 0;
    private final int DELAY_TIME = 250;

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

        jniCreate(sampleRate, bufferSizeInFrames);

        String[] files = new String[10];
        for (int i = 0; i < files.length; ++i) {
            files[i] = String.format("%02d.mp3", i);
            Log.d(TAG, "load file: " + files[i]);
        }
//        files[10] = "/sdcard/doorOpen.ogg";

//        String files[] = new String[] {"doorOpen.ogg"};
        jniLoadSamples(getAssets(), files);

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

        mHandler = new Handler();
        mHandler.postDelayed(mRunnable, DELAY_TIME);
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

    private native boolean jniCreate(int sampleRate, int bufferSizeInFrames);
    private native boolean jniShutdown();
    private native void jniOnPause();
    private native void jniOnResume();
    private native boolean jniLoadSamples(AssetManager manager, String[] files);
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
        mHandler.postDelayed(mRunnable, DELAY_TIME);
        jniOnResume();
    }
}
