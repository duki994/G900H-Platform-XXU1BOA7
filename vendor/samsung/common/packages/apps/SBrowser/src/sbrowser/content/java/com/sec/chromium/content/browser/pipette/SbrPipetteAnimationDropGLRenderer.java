package com.sec.chromium.content.browser.pipette;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import android.content.Context;
import android.graphics.Bitmap;
import android.opengl.GLES10;
import android.opengl.GLSurfaceView.Renderer;
import android.opengl.GLU;
import android.opengl.Matrix;
import android.os.SystemClock;
import android.util.Log;

public class SbrPipetteAnimationDropGLRenderer implements Renderer {

    private static final String TAG = SbrPipetteAnimationDropGLRenderer.class
            .getSimpleName();
    
    public SbrPipetteAnimationDrop mDropAnimation;
    float mWidth = 100.0f;
    float mHeight = 200.0f;
    Context mContext;
    int[] mViewport;
    SbrPipetteAnimationData mAnimData;
    private boolean mIsFinishingActivityInProgress = false;

    @Override
    public void onDrawFrame(GL10 gl) {
        
        if(mIsFinishingActivityInProgress) {
            return;
        }
        GLES10.glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        GLES10.glClear(GLES10.GL_DEPTH_BUFFER_BIT | GLES10.GL_COLOR_BUFFER_BIT);

        if (mDropAnimation.isAnimationEnd()) {
            // long curThreadTime = SystemClock.currentThreadTimeMillis();
            // long curTime = SystemClock.uptimeMillis();
            // Log.d("AMIT", " animation time = " + (curThreadTime -
            // mDropAnimation.getStartTime()));
            // Log.d("AMIT", " animation time = " + (curTime - startTime));

            if (mContext instanceof SbrPipetteAnimationGLActivity) {
                SbrPipetteAnimationGLActivity activity = (SbrPipetteAnimationGLActivity) mContext;
                activity.exit(true);
                mIsFinishingActivityInProgress = true;
                //Log.d(TAG, " animation end time: " + System.currentTimeMillis());
            }
            return;
        }

        GLES10.glMatrixMode(GLES10.GL_MODELVIEW);
        GLES10.glLoadIdentity();
        GLU.gluLookAt(gl, 0.0f, 0.0f, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
        if (mDropAnimation.getStartTime() == 0) {
            restart();
        }
        if (mDropAnimation.getStartTime() != 0) {
            mDropAnimation.draw(gl);
        }
        fpsCounter.logFrame();
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        if (height == 0) {
            height = 1;
        }
        if (width == 0) {
            width = 1;
        }

        mWidth = width;
        mHeight = height;

        gl.glViewport(0, 0, width, height);
        mViewport = new int[4];
        mViewport[0] = mViewport[1] = 0;
        mViewport[2] = width;
        mViewport[3] = height;

        GLES10.glMatrixMode(GLES10.GL_PROJECTION);
        GLES10.glLoadIdentity();
        GLU.gluPerspective(gl, 45.0f, (float) width / (float) height, 0.1f,
                100.0f);
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {

        if(SbrPipetteAnimationGLActivity.mBitmap == null
                || SbrPipetteAnimationGLActivity.mBitmap.isRecycled()) {
            //Bitmap is null or recycled, hence finish the activity with failure status
            ((SbrPipetteAnimationGLActivity)mContext).exit(false);
            mIsFinishingActivityInProgress = true;
            return;
        }
        GLES10.glEnable(GLES10.GL_TEXTURE_2D);
        GLES10.glShadeModel(GLES10.GL_SMOOTH);
        GLES10.glClearColor(0.0f, 0.0f, 0.0f, 1f);
        GLES10.glClearDepthf(1.0f);
        GLES10.glEnable(GLES10.GL_DEPTH_TEST);
        GLES10.glDepthFunc(GLES10.GL_LEQUAL);

        GLES10.glHint(GLES10.GL_PERSPECTIVE_CORRECTION_HINT, GLES10.GL_NICEST);

        mAnimData.mBmp = SbrPipetteAnimationGLActivity.mBitmap;
        Bitmap bitmap = mAnimData.mBmp;
        mDropAnimation.loadGLTexture(gl, bitmap);
        bitmap.recycle();
    }

    public SbrPipetteAnimationDropGLRenderer(Context context,
            boolean useTranslucentBackground, SbrPipetteAnimationData data) {
        mContext = context;
        mAnimData = data;
        mDropAnimation = new SbrPipetteAnimationDrop();
        fpsCounter = new FPSCounter();
        Log.d(TAG, " animation start time: " + System.currentTimeMillis());
    }

    public void restart() {
        if(mIsFinishingActivityInProgress) {
            return;
        }

        Log.d(TAG, " bmp h= "+mAnimData.mBmp.getHeight() + ", w=" + mAnimData.mBmp.getWidth()
                + ", actual w= " + mAnimData.mImageWidth + ", h = " + mAnimData.mImageHeight);
        float xStart = mAnimData.mTouchX;
        float yStart = mAnimData.mTouchY;
        float xLeftDest = mAnimData.mTopLeftX;
        float yBottomDest = mAnimData.mTopLeftY + mAnimData.mBmp.getHeight();
        float xRightDest = mAnimData.mTopLeftX + mAnimData.mBmp.getWidth();
        float yTopDest = mAnimData.mTopLeftY;

        point startPos = GetOGLPos(xStart, yStart, 0.8f);
        point destLeftBottomPos = GetOGLPos(xLeftDest, yBottomDest, 0.0f);
        point destRightTopPos = GetOGLPos(xRightDest, yTopDest, 0.0f);

        mDropAnimation.reset(startPos.mX, startPos.mY, destLeftBottomPos.mX,
                destLeftBottomPos.mY, destRightTopPos.mX, destRightTopPos.mY);
        startTime = SystemClock.uptimeMillis();
        mDropAnimation.setStartTime(SystemClock.currentThreadTimeMillis());
    }

    long startTime = 0;

    public SbrPipetteAnimationDropGLRenderer(Context context) {
        mContext = context;
    }

    static class point {
        public float mX, mY;

        public point(float x, float y) {
            mX = x;
            mY = y;
        }
    }

    point GetOGLPos(float x, float y, float zPos) {

        float[] modelview = new float[16];
        float[] projection = new float[16];
        float winX, winY;
        float[] posNear = new float[4];
        float[] pos = new float[4];
        float[] posFar = new float[4];

        Matrix.perspectiveM(projection, 0, 45.0f, mWidth / mHeight, 0.1f, 100f);
        Matrix.setLookAtM(modelview, 0, 0.0f, 0.0f, 3.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f);

        winX = x;
        winY = mViewport[3] - y;

        GLU.gluUnProject(winX, winY, 0.0f, modelview, 0, projection, 0,
                mViewport, 0, posNear, 0);
        GLU.gluUnProject(winX, winY, 1.0f, modelview, 0, projection, 0,
                mViewport, 0, posFar, 0);

        for (int index = 0; index < 4; index++) {
            posNear[index] = posNear[index] / posNear[3];
            posFar[index] = posFar[index] / posFar[3];
        }

        float multiplicationFactor = Math.abs(posNear[2] - zPos)
                / Math.abs(posFar[2] - posNear[2]);

        pos[0] = multiplicationFactor * (posFar[0] - posNear[0]);
        pos[1] = multiplicationFactor * (posFar[1] - posNear[1]);

        return new point(pos[0], pos[1]);
    }

    static class FPSCounter {
        long startTime = System.nanoTime();
        int frames = 0;

        public void logFrame() {
            frames++;
            if (System.nanoTime() - startTime >= 1000000000) {
                Log.d(TAG, "FPSCounter - fps: " + frames);
                frames = 0;
                startTime = System.nanoTime();
            }
        }
    }

    FPSCounter fpsCounter = null;
}
