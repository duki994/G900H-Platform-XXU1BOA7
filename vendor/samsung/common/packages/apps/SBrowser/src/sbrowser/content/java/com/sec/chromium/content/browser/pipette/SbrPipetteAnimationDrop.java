package com.sec.chromium.content.browser.pipette;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.ShortBuffer;

import javax.microedition.khronos.opengles.GL10;

import android.graphics.Bitmap;
import android.opengl.GLES10;
import android.opengl.GLUtils;
import android.os.SystemClock;
import android.view.animation.AccelerateDecelerateInterpolator;
import android.view.animation.AccelerateInterpolator;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.Interpolator;

public class SbrPipetteAnimationDrop {
    
//    private static final String TAG = SbrPipetteAnimationDrop.class
//            .getSimpleName();
    
    public static final int GRID = 25;
    private FloatBuffer mVertexBuffer;
    private FloatBuffer mTextureBuffer;
    private ShortBuffer mIndexBuffer;

    private int[] mTextures = new int[1];
    private float mVertices[] = new float[(GRID + 1) * (GRID + 1) * 3];
    private float mTexture[] = new float[(GRID + 1) * (GRID + 1) * 2];
    private short mIndices[] = new short[GRID * GRID * 6];

    public float mYGapTop = 0.0f;
    public float mYGapBottom = 0.0f;
    public float mXGapStart = 0.0f;
    public float mXGapEnd = 0.0f;

    public float mStartZPosition = 0.8f;

    public float mStartXPosition = 0.0f;
    public float mXLeftDestPosition = 0.0f;
    public float mXRightDestPosition = 0.0f;
    public float mXLeftTravelDistance = mStartXPosition - mXLeftDestPosition;
    public float mXRightTravelDistance = mStartXPosition - mXRightDestPosition;
    public float mXGapStartToDestCenter = 0.0f;
            
    public float mStartYPosition = 0.0f;
    public float mYTopDestPosition = 0.0f;
    public float mYBottomDestPosition = 0.0f;
    public float mYTopTravelDistance = mStartYPosition - mYTopDestPosition;
    public float mYBottomTravelDistance = mStartYPosition
            - mYBottomDestPosition;

    Interpolator mDecelInterpolator;
    Interpolator mAccelDecelInterpolator;
    Interpolator mAccelInterpolator;

    int mDomePeakRow = (GRID + 1) / 2;

    float mMultiplier = 2;

    float mDomeAnimationInterpolation[] = new float[GRID + 1];
    double mDomeAnimationDuration[] = new double[GRID + 1];
    double mDomeAnimationStartTime[] = new double[GRID + 1];

    double mTextureStartTime = 0.0f;
    double mTextureDuration = 200.0f;
    float mTextureInterpolation = 1.0f;

    double mStartTime = 0.0f;
    double mDuration = 200 * mMultiplier;
    float mTotalInterpolation = 0.0f;
    double mXTranslationDuration[] = new double[GRID + 1];
    float mXTranslationInterpolation[] = new float[GRID + 1];

    double mYTopTranslationDuration[] = new double[GRID + 1];
    float mYTopTranslationInterpolation[] = new float[GRID + 1];

    double mYBottomTranslationDuration[] = new double[GRID + 1];
    float mYBottomTranslationInterpolation[] = new float[GRID + 1];

    float mZLeftControlPoint1XStart = 0.45f;
    float mZLeftControlPoint1XEnd = 0.20f;
    float mZLeftControlPoint1YStart = 0.00f;
    float mZLeftControlPoint1YEnd = 0.00f;
    float mZLeftControlPoint2XStart = 0.48f;
    float mZLeftControlPoint2XEnd = 0.35f;
    float mZLeftControlPoint3XStart = 0.50f;
    float mZLeftControlPoint3XEnd = 0.48f;

    float mZRightControlPoint0XStart = 0.50f;
    float mZRightControlPoint0XEnd = 0.52f;
    float mZRightControlPoint1XStart = 0.52f;
    float mZRightControlPoint1XEnd = 0.65f;
    float mZRightControlPoint2XStart = 0.70f;
    float mZRightControlPoint2XEnd = 0.80f;

    float mZRightControlPoint2YStart = 0.00f;
    float mZRightControlPoint2YEnd = 0.00f;
    
//    float mTxDurationVariationRange = (float) (4 * GRID); //100 ms running time
//    float mDomeAnimationDurationVariationRange = (50f * (float)13 /*mDomePeakRow*/); // 650 ms running time
//    float mYTranslationDurationVarRange = 13 /*mDomePeakRow*/; // 13 ms running time

    // Time calculation based on system time
    float mSystemTimeMultFactor = 3.0f; //3 -> 2.4 seconds
    float mTxDurationVariationRange = 100f * mSystemTimeMultFactor; //100 ms running time
    float mDomeAnimationDurationVariationRange = 150f * mSystemTimeMultFactor; // 150 ms running time
    float mYTranslationDurationVarRange = 13f * mSystemTimeMultFactor; // 13 ms running time

    
    public float getInterpolation() {
        return mTotalInterpolation;
    }
    
    private long getCurrentTime() {
        return SystemClock.uptimeMillis(); //Moved to user time instead of thread time
        //return SystemClock.currentThreadTimeMillis();//time in thread running time ms
    }

    public void setStartTime(double time) {
        //mStartTime = time;
        mStartTime = getCurrentTime();
        for (int row = 0; row <= GRID; row++) {
            mXTranslationDuration[row] = mDuration
                    - mAccelInterpolator.getInterpolation((float) row
                            / (float) GRID) * (mTxDurationVariationRange);

            if (row < mDomePeakRow) {
                mDomeAnimationDuration[row] = mDuration
                        + mDecelInterpolator.getInterpolation((float)row
                                / (float)mDomePeakRow) * (mDomeAnimationDurationVariationRange);
            } else {
                mDomeAnimationDuration[row] = mDuration
                        + mAccelInterpolator.getInterpolation((float)(GRID - row)
                                / (float)mDomePeakRow) * (mDomeAnimationDurationVariationRange);
            }

            if (row < mDomePeakRow) {
                mYTopTranslationDuration[row] = mDuration
                        + mAccelDecelInterpolator.getInterpolation((float) row
                                / (float) mDomePeakRow) * (mYTranslationDurationVarRange);

                mYBottomTranslationDuration[row] = mYTopTranslationDuration[row]
                        + mAccelDecelInterpolator.getInterpolation((float) row
                                / (float) mDomePeakRow) * (mYTranslationDurationVarRange);

            } else {
                mYTopTranslationDuration[row] = mDuration
                        + mAccelDecelInterpolator.getInterpolation((float) row
                                / (float) mDomePeakRow) * (mYTranslationDurationVarRange);

                mYBottomTranslationDuration[row] = mYTopTranslationDuration[row]
                        + mAccelDecelInterpolator
                                .getInterpolation((float) (GRID - row)
                                        / (float) mDomePeakRow) * mYTranslationDurationVarRange;
            }

            if (row < mDomePeakRow) {
                mDomeAnimationStartTime[row] = mStartTime
                        + mXTranslationDuration[row]
                        * mDecelInterpolator
                                .getInterpolation(((float) row / (float) mDomePeakRow) * 0.4f);
            } else {
                mDomeAnimationStartTime[row] = mStartTime
                        + mXTranslationDuration[row]
                        * mDecelInterpolator
                                .getInterpolation(((float) (GRID - row) / (float) mDomePeakRow) * 0.4f);
            }

        }

    }

    public double getStartTime() {
        return mStartTime;
    }

    public void interpolate() {
        double currentTime = getCurrentTime();
        if (currentTime < mStartTime + mDuration) {
            mTotalInterpolation = (float) ((currentTime - mStartTime) / mDuration);
        } else {
            mTotalInterpolation = 1.0f;
        }
    }

    public void interpolateDomeDuration() {
        double currentTime = getCurrentTime();
        for (int row = 0; row <= GRID; row++) {
            if (currentTime < mDomeAnimationStartTime[row]
                    + mDomeAnimationDuration[row]) {
                if (currentTime > mDomeAnimationStartTime[row]) {
                    mDomeAnimationInterpolation[row] = mAccelInterpolator
                            .getInterpolation(((float) ((currentTime - mDomeAnimationStartTime[row]) / mDomeAnimationDuration[row])));
                } else {
                    mDomeAnimationInterpolation[row] = 0;
                }
            } else {
                mDomeAnimationInterpolation[row] = 1.0f;
            }
        }
    }

    public void interpolateXTransDuration() {
        double currentTime = getCurrentTime();
        for (int row = 0; row <= GRID; row++) {
            if (currentTime < mStartTime + mXTranslationDuration[row]) {
                mXTranslationInterpolation[row] = mAccelDecelInterpolator
                        .getInterpolation((float) ((currentTime - mStartTime) / mXTranslationDuration[row]));
            } else {
                mXTranslationInterpolation[row] = 1.0f;
            }
        }
    }

    public void interpolateYTransDuration() {
        double currentTime = getCurrentTime();
        for (int col = 0; col <= GRID; col++) {
            if (currentTime < mStartTime + mYTopTranslationDuration[col]) {
                mYTopTranslationInterpolation[col] = mAccelDecelInterpolator
                        .getInterpolation((float) ((currentTime - mStartTime) / mYTopTranslationDuration[col]));
            } else {
                mYTopTranslationInterpolation[col] = 1.0f;
            }

            if (currentTime < mStartTime + mYBottomTranslationDuration[col]) {
                mYBottomTranslationInterpolation[col] = mAccelDecelInterpolator
                        .getInterpolation((float) ((currentTime - mStartTime) / mYBottomTranslationDuration[col]));
            } else {
                mYBottomTranslationInterpolation[col] = 1.0f;
            }

        }
    }

    public void calculateVerticesCoords() {
        for (int row = 0; row <= GRID; row++) {
            mXGapStart = mStartXPosition + mXTranslationInterpolation[row]
                    * mXLeftTravelDistance;
            mXGapEnd = mStartXPosition + mXTranslationInterpolation[row]
                    * mXRightTravelDistance;

            float yValue = mDecelInterpolator
                    .getInterpolation(mTotalInterpolation)
                    * ((float) (row) / (float) GRID);

            for (int col = 0; col <= GRID; col++) {
                float xValue = (float) (col) / (float) GRID;
                mYGapTop = mStartYPosition + mYTopTranslationInterpolation[col]
                        * mYTopTravelDistance;
                mYGapBottom = mStartYPosition
                        + mYBottomTranslationInterpolation[col]
                        * mYBottomTravelDistance;

                int pos = 3 * (row * (GRID + 1) + col);

                // z-coordinate
                if (col < 13) {
                    mVertices[pos + 2] = cubicBezierLeftZ((float) col / GRID,
                            row);
                } else {
                    mVertices[pos + 2] = cubicBezierRightZ((float) col / GRID,
                            row);
                }

                // x-coordinate
//                float midX = (1.0f - mAccelInterpolator
//                        .getInterpolation(mXTranslationInterpolation[row]))
//                        * mStartXPosition;
                
                //AMIT - modified for center adjustment
                float midX = mStartXPosition + mAccelInterpolator
                        .getInterpolation(mXTranslationInterpolation[row])
                        *mXGapStartToDestCenter;
                
                if (xValue <= 0.5f) {
                    mVertices[pos] = mXGapStart + (2.0f * xValue)
                            * (midX - mXGapStart);
                } else {
                    mVertices[pos] = midX + (2.0f * (xValue - 0.5f))
                            * (mXGapEnd - midX);
                }

                // y-coordinate
                mVertices[pos + 1] = mYGapBottom + yValue
                        * (mYGapTop - mYGapBottom);
                
//                Log.d(TAG, "mVertices["+row+"]["+col+"] = " + mVertices[pos] + ", "
//                        + mVertices[pos+1] + ", " + mVertices[pos+2] + ", midX =" + midX);
            }
        }
//        Log.d(TAG, "------------------------------------------");
        ByteBuffer byteBuf = ByteBuffer.allocateDirect(mVertices.length * 4);
        byteBuf.order(ByteOrder.nativeOrder());
        mVertexBuffer = byteBuf.asFloatBuffer();
        mVertexBuffer.put(mVertices);
        mVertexBuffer.position(0);
    }

    private void calculateIndices() {

        for (int row = 0; row < GRID; row++) {
            for (int col = 0; col < GRID; col++) {
                int pos = 6 * (row * GRID + col);

                mIndices[pos] = (short) (row * (GRID + 1) + col);
                mIndices[pos + 1] = (short) (row * (GRID + 1) + col + 1);
                mIndices[pos + 2] = (short) ((row + 1) * (GRID + 1) + col);

                mIndices[pos + 3] = (short) (row * (GRID + 1) + col + 1);
                mIndices[pos + 4] = (short) ((row + 1) * (GRID + 1) + col + 1);
                mIndices[pos + 5] = (short) ((row + 1) * (GRID + 1) + col);

            }
        }
    }

    private void calculateTextureCoords() {
        double currentTime = getCurrentTime();
        if (mTextureStartTime == 0) {
            mTextureStartTime = currentTime;
        } else if (currentTime - mTextureStartTime < mTextureDuration) {
            mTextureInterpolation = mDecelInterpolator
                    .getInterpolation((float) ((currentTime - mTextureStartTime) / mTextureDuration));
        } else {
            mTextureInterpolation = 1.0f;
        }

        float u, v;
        for (int row = 0; row <= GRID; row++) {
            v = row / (float) GRID;
            for (int col = 0; col <= GRID; col++) {
                int pos = 2 * (row * (GRID + 1) + col);

                u = col / (float) GRID;

                if (u < 0.5f) {
                    mTexture[pos] = u * mTextureInterpolation;
                } else {
                    mTexture[pos] = (1.0f + (u * mTextureInterpolation))
                            - mTextureInterpolation;
                }

                //if (v < 0.5f) {
                mTexture[pos + 1] = mTextureInterpolation - v
                        * mTextureInterpolation;
//                } else {
//                    mTexture[pos + 1] = mTextureInterpolation - v
//                            * mTextureInterpolation;
//                }
            }
        }
        ByteBuffer byteBuf = ByteBuffer.allocateDirect(mTexture.length * 4);
        byteBuf.order(ByteOrder.nativeOrder());
        mTextureBuffer = byteBuf.asFloatBuffer();
        mTextureBuffer.put(mTexture);
        mTextureBuffer.position(0);
    }

    public SbrPipetteAnimationDrop() {
        mDecelInterpolator = new DecelerateInterpolator();
        mAccelDecelInterpolator = new AccelerateDecelerateInterpolator();
        mAccelInterpolator = new AccelerateInterpolator();

        calculateIndices();

        ByteBuffer byteBuf = ByteBuffer.allocateDirect(mIndices.length * 4);
        byteBuf.order(ByteOrder.nativeOrder());
        mIndexBuffer = byteBuf.asShortBuffer();
        mIndexBuffer.put(mIndices);
        mIndexBuffer.position(0);
    }

    public boolean checkDomeAndXYTransInterpolation() {
        for (int row = 0; row <= GRID; row++) {
            if (mDomeAnimationInterpolation[row] < 1.0f
                    || mXTranslationInterpolation[row] < 1.0f
                    || mYTopTranslationInterpolation[row] < 1.0f
                    || mYBottomTranslationInterpolation[row] < 1.0f) {
                return true;
            }
        }
        return false;
    }

    public void draw(GL10 gl) {
        if (mStartTime == 0) {
            return;
        }
        if (mTotalInterpolation < 1.0f || checkDomeAndXYTransInterpolation()) {
            interpolate();
            interpolateDomeDuration();
            interpolateXTransDuration();
            interpolateYTransDuration();
            calculateVerticesCoords();
            calculateTextureCoords();
        } else {
            return;
        }

        GLES10.glBindTexture(GLES10.GL_TEXTURE_2D, mTextures[0]);

        GLES10.glEnableClientState(GLES10.GL_VERTEX_ARRAY);
        GLES10.glEnableClientState(GLES10.GL_TEXTURE_COORD_ARRAY);

        GLES10.glFrontFace(GLES10.GL_CCW);

        GLES10.glVertexPointer(3, GLES10.GL_FLOAT, 0, mVertexBuffer);
        GLES10.glTexCoordPointer(2, GLES10.GL_FLOAT, 0, mTextureBuffer);

        GLES10.glDrawElements(GLES10.GL_TRIANGLES, mIndices.length,
                GLES10.GL_UNSIGNED_SHORT, mIndexBuffer);

        GLES10.glDisableClientState(GLES10.GL_VERTEX_ARRAY);
        GLES10.glDisableClientState(GLES10.GL_TEXTURE_COORD_ARRAY);

    }

    public void loadGLTexture(GL10 gl, Bitmap bitmap) {
        GLES10.glGenTextures(1, mTextures, 0);
        GLES10.glBindTexture(GLES10.GL_TEXTURE_2D, mTextures[0]);

        GLES10.glTexParameterf(GLES10.GL_TEXTURE_2D,
                GLES10.GL_TEXTURE_MIN_FILTER, GLES10.GL_NEAREST);
        GLES10.glTexParameterf(GLES10.GL_TEXTURE_2D,
                GLES10.GL_TEXTURE_MAG_FILTER, GLES10.GL_LINEAR);

        GLES10.glTexParameterf(GLES10.GL_TEXTURE_2D, GLES10.GL_TEXTURE_WRAP_S,
                GLES10.GL_REPEAT);
        GLES10.glTexParameterf(GLES10.GL_TEXTURE_2D, GLES10.GL_TEXTURE_WRAP_T,
                GLES10.GL_REPEAT);

        GLUtils.texImage2D(GLES10.GL_TEXTURE_2D, 0, bitmap, 0);

        bitmap.recycle();
    }

    float cubicBezierLeftZ(float t, int row) {
        float rowFactorForX;
        if (row < mDomePeakRow) {
            rowFactorForX = cubicBezierRowFactorForX((float) row / mDomePeakRow);
        } else {
            rowFactorForX = cubicBezierRowFactorForX((float) (GRID - row)
                    / mDomePeakRow);
        }
        float x = t;

        float initialPointX = 0.0f;
        float initialPointY = 0.0f + mStartZPosition
                * (1.0f - mXTranslationInterpolation[row]);

        float controlPoint1X = mZLeftControlPoint1XStart
                + (mZLeftControlPoint1XEnd - mZLeftControlPoint1XStart)
                * rowFactorForX;

        float controlPoint1Y = mZLeftControlPoint1YStart
                + (mZLeftControlPoint1YEnd - mZLeftControlPoint1YStart)
                * rowFactorForX + +mStartZPosition
                * (1.0f - mXTranslationInterpolation[row]);

        float controlPoint2X = mZLeftControlPoint2XEnd
                - (mZLeftControlPoint2XEnd - mZLeftControlPoint2XStart)
                * rowFactorForX;
        float controlPoint2Y = mStartZPosition;

        float finalPointX = mZLeftControlPoint3XStart
                - (mZLeftControlPoint3XEnd - mZLeftControlPoint3XStart)
                * rowFactorForX;
        float finalPointY = mStartZPosition;

        if (mDomeAnimationInterpolation[row] != 0.0f) {

            finalPointY *= (1.0f - mDomeAnimationInterpolation[row]);
            controlPoint2Y *= (1.0f - mDomeAnimationInterpolation[row]);
            controlPoint1Y *= (1.0f - mDomeAnimationInterpolation[row]);
        }

        float Xcontrol1 = finalPointX - 3 * controlPoint2X + 3 * controlPoint1X
                - initialPointX;
        float Xcontrol2 = 3 * controlPoint2X - 6 * controlPoint1X + 3
                * initialPointX;
        float Xcontrol3 = 3 * controlPoint1X - 3 * initialPointX;
        float Xcontrol4 = initialPointX;

        float Ycontrol1 = finalPointY - 3 * controlPoint2Y + 3 * controlPoint1Y
                - initialPointY;
        float Ycontrol2 = 3 * controlPoint2Y - 6 * controlPoint1Y + 3
                * initialPointY;
        float Ycontrol3 = 3 * controlPoint1Y - 3 * initialPointY;
        float Ycontrol4 = initialPointY;

        float currentt = x;
        int nRefinementIterations = 5;
        for (int i = 0; i < nRefinementIterations; i++) {
            float currentx = xFromT(currentt, Xcontrol1, Xcontrol2, Xcontrol3,
                    Xcontrol4);
            float currentslope = slopeFromT(currentt, Xcontrol1, Xcontrol2,
                    Xcontrol3);
            currentt -= (currentx - x) * currentslope;
        }
        return (yFromT(currentt, Ycontrol1, Ycontrol2, Ycontrol3, Ycontrol4));
    }

    float cubicBezierRightZ(float t, int row) {
        float rowFactorForX;
        if (row < mDomePeakRow) {
            rowFactorForX = cubicBezierRowFactorForX((float) row / mDomePeakRow);
        } else {
            rowFactorForX = cubicBezierRowFactorForX((float) (GRID - row)
                    / mDomePeakRow);
        }
        float x = t;

        float initialPointX = mZRightControlPoint0XStart
                + (mZRightControlPoint0XEnd - mZRightControlPoint0XStart)
                * rowFactorForX;
        float initialPointY = mStartZPosition;

        float controlPoint1X = mZRightControlPoint1XStart
                + (mZRightControlPoint1XEnd - mZRightControlPoint1XStart)
                * rowFactorForX;
        float controlPoint1Y = mStartZPosition;

        float controlPoint2X = mZRightControlPoint2XEnd
                + (mZRightControlPoint2XEnd - mZRightControlPoint2XStart)
                * rowFactorForX;
        float controlPoint2Y = mZRightControlPoint2YStart
                + (mZRightControlPoint2YEnd - mZRightControlPoint2YStart)
                * rowFactorForX + mStartZPosition
                * (1.0f - mXTranslationInterpolation[row]);

        float finalPointX = 1.0f;
        float finalPointY = 0.0f + mStartZPosition
                * (1.0f - mXTranslationInterpolation[row]);

        if (mDomeAnimationInterpolation[row] != 0.0f) {
            initialPointY *= (1.0f - mDomeAnimationInterpolation[row]);
            controlPoint1Y *= (1.0f - mDomeAnimationInterpolation[row]);
            controlPoint2Y *= (1.0f - mDomeAnimationInterpolation[row]);
        }

        float Xcontrol1 = finalPointX - 3 * controlPoint2X + 3 * controlPoint1X
                - initialPointX;
        float Xcontrol2 = 3 * controlPoint2X - 6 * controlPoint1X + 3
                * initialPointX;
        float Xcontrol3 = 3 * controlPoint1X - 3 * initialPointX;
        float Xcontrol4 = initialPointX;

        float Ycontrol1 = finalPointY - 3 * controlPoint2Y + 3 * controlPoint1Y
                - initialPointY;
        float Ycontrol2 = 3 * controlPoint2Y - 6 * controlPoint1Y + 3
                * initialPointY;
        float Ycontrol3 = 3 * controlPoint1Y - 3 * initialPointY;
        float Ycontrol4 = initialPointY;

        float currentt = x;
        int nRefinementIterations = 5;
        for (int i = 0; i < nRefinementIterations; i++) {
            float currentx = xFromT(currentt, Xcontrol1, Xcontrol2, Xcontrol3,
                    Xcontrol4);
            float currentslope = slopeFromT(currentt, Xcontrol1, Xcontrol2,
                    Xcontrol3);
            currentt -= (currentx - x) * currentslope;
        }
        return (yFromT(currentt, Ycontrol1, Ycontrol2, Ycontrol3, Ycontrol4));
    }

    float cubicBezierRowFactorForX(float t) {

        float reductionStartsFrom = 1.0f;
        float x = t;

        float initialPointX = 0.0f;
        float initialPointY = 0.0f + reductionStartsFrom;

        float controlPoint1X = 0.5f;
        float controlPoint1Y = reductionStartsFrom;

        float controlPoint2X = 0.5f;
        float controlPoint2Y = reductionStartsFrom;

        float finalPointX = 1.0f;
        float finalPointY = 0.3f + reductionStartsFrom;

        float Xcontrol1 = finalPointX - 3 * controlPoint2X + 3 * controlPoint1X
                - initialPointX;
        float Xcontrol2 = 3 * controlPoint2X - 6 * controlPoint1X + 3
                * initialPointX;
        float Xcontrol3 = 3 * controlPoint1X - 3 * initialPointX;
        float Xcontrol4 = initialPointX;

        float Ycontrol1 = finalPointY - 3 * controlPoint2Y + 3 * controlPoint1Y
                - initialPointY;
        float Ycontrol2 = 3 * controlPoint2Y - 6 * controlPoint1Y + 3
                * initialPointY;
        float Ycontrol3 = 3 * controlPoint1Y - 3 * initialPointY;
        float Ycontrol4 = initialPointY;

        // Solve for t given x (using Newton-Raphelson), then solve for y given
        // t.
        // Assume for the first guess that t = x.
        float currentt = x;
        int nRefinementIterations = 5;
        for (int i = 0; i < nRefinementIterations; i++) {
            float currentx = xFromT(currentt, Xcontrol1, Xcontrol2, Xcontrol3,
                    Xcontrol4);
            float currentslope = slopeFromT(currentt, Xcontrol1, Xcontrol2,
                    Xcontrol3);
            currentt -= (currentx - x) * currentslope;
        }
        return (yFromT(currentt, Ycontrol1, Ycontrol2, Ycontrol3, Ycontrol4));
    }

    float slopeFromT(float t, float controlPoint1, float controlPoint2,
            float controlPoint3) {
        return (1.0f / (3.0f * controlPoint1 * t * t + 2.0f * controlPoint2 * t + controlPoint3));
    }

    float xFromT(float t, float controlPoint1, float controlPoint2,
            float controlPoint3, float controlPoint4) {
        return (controlPoint1 * (t * t * t) + controlPoint2 * (t * t)
                + controlPoint3 * t + controlPoint4);
    }

    float yFromT(float t, float controlPoint1, float controlPoint2,
            float controlPoint3, float controlPoint4) {
        return (controlPoint1 * (t * t * t) + controlPoint2 * (t * t)
                + controlPoint3 * t + controlPoint4);
    }

    public Boolean isAnimationEnd() {
        if (getInterpolation() < 1.0f || checkDomeAndXYTransInterpolation()) {
            return false;
        } else {
            return true;
        }

    }

    public void reset(float xAnimStart, float yAnimStart, float xLeftDest,
            float yBottomDest, float xRightDest, float yTopDest) {

        mStartZPosition = 0.8f;

        mStartXPosition = xAnimStart;
        mXLeftDestPosition = xLeftDest;
        mXRightDestPosition = xRightDest;
        mXLeftTravelDistance = mXLeftDestPosition - mStartXPosition;
        mXRightTravelDistance = mXRightDestPosition - mStartXPosition;

        mStartYPosition = yAnimStart;
        mYBottomDestPosition = yBottomDest;
        mYTopDestPosition = yTopDest;
        mYTopTravelDistance = mYTopDestPosition - mStartYPosition;
        mYBottomTravelDistance = mYBottomDestPosition - mStartYPosition;

        //AMIT
        mXGapStartToDestCenter = (mXLeftDestPosition + mXRightDestPosition)/2.0f - mStartXPosition;
        
        mMultiplier = 2 * mSystemTimeMultFactor;
        mStartTime = 0.0f;
        mTotalInterpolation = 0.0f;
        mDuration = 200 * mMultiplier;
        //mDomeAnimationStartOffset = 10 * mMultiplier;

        mTextureStartTime = 0.0f;
        mTextureInterpolation = 1.0f;
        mTextureDuration = 100.0f * mMultiplier;

        for (int row = 0; row <= GRID; row++) {
            mDomeAnimationInterpolation[row] = 0.0f;
            mDomeAnimationDuration[row] = 0.0f;
            mDomeAnimationStartTime[row] = 0.0f;

            mXTranslationDuration[row] = 0.0f;
            mXTranslationInterpolation[row] = 0.0f;

            mYTopTranslationDuration[row] = 0.0f;
            mYTopTranslationInterpolation[row] = 0.0f;

            mYBottomTranslationDuration[row] = 0.0f;
            mYBottomTranslationInterpolation[row] = 0.0f;
        }

        mZLeftControlPoint1XStart = 0.30f;
        mZLeftControlPoint1XEnd = 0.20f;
        mZLeftControlPoint2XStart = 0.48f;
        mZLeftControlPoint2XEnd = 0.35f;
        mZLeftControlPoint3XStart = 0.50f;
        mZLeftControlPoint3XEnd = 0.48f;

        mZRightControlPoint0XStart = 0.50f;
        mZRightControlPoint0XEnd = 0.52f;
        mZRightControlPoint1XStart = 0.52f;
        mZRightControlPoint1XEnd = 0.65f;
        mZRightControlPoint2XStart = 0.70f;
        mZRightControlPoint2XEnd = 0.80f;

//        Log.d(TAG, " mStartXPosition = " + mStartXPosition + ", mXLeftTravelDistance = "
//                + mXLeftTravelDistance + ", mXRightTravelDistance = " + mXRightTravelDistance
//                + ", mXLeftDestPosition =" + mXLeftDestPosition
//                + ", mXRightDestPosition = " + mXRightDestPosition);
    }
}