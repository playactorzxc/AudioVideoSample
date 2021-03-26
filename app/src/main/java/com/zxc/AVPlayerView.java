package com.zxc;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.os.Environment;
import android.util.AttributeSet;
import android.util.Log;
import android.view.SurfaceHolder;

import java.io.File;
import java.util.concurrent.Executors;

public class AVPlayerView extends GLSurfaceView implements Runnable, SurfaceHolder.Callback {
    private static final String TAG = "AVPlayerView";

    public AVPlayerView(Context context) {
        super(context);
    }

    public AVPlayerView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }


    @Override
    public void surfaceCreated(SurfaceHolder surfaceHolder) {
        Executors.newCachedThreadPool().execute(this);
    }

    @Override
    public void surfaceChanged(SurfaceHolder surfaceHolder, int i, int i1, int i2) {

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder surfaceHolder) {

    }

    @Override
    public void run() {
        String path = Environment.getExternalStorageDirectory().getAbsolutePath().concat(File.separator).concat("1080.mp4");
        File file = new File(path);
        boolean canRead = file.canRead();
        Log.d(TAG, String.format("path: %s canRead: %b" , path,canRead));
        AVPlayer.open(path,getHolder().getSurface());
    }
}
