package com.sec.chromium.content.browser.pipette.suctionanimator;

//a minimal vector class of 3 floats and overloaded math operators
public class Vec3 {

    public float[] f = null;

    Vec3(float x, float y, float z) {
        if (f == null) {
            f = new float[3];
        }
        f[0] = x;
        f[1] = y;
        f[2] = z;
    }

    Vec3() {
        if (f == null) {
            f = new float[3];
        }
    }

    Vec3(Vec3 v) {
        if (f == null) {
            f = new float[3];
        }
        f[0] = v.f[0];
        f[1] = v.f[1];
        f[2] = v.f[2];
    }

    public void setValues(float x, float y, float z) {
        if (f == null) {
            f = new float[3];
        }
        f[0] = x;
        f[1] = y;
        f[2] = z;
    }
    
    public void setValues(Vec3 v) {
        if (f == null) {
            f = new float[3];
        }
        f[0] = v.f[0];
        f[1] = v.f[1];
        f[2] = v.f[2];
    }

    public void resetValues() {
        f[0] = 0;
        f[1] = 0;
        f[2] = 0;
    }

    public float length() {
        return (float) Math.sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    }

    public float squareLength() {
        return f[0] * f[0] + f[1] * f[1] + f[2] * f[2];
    }

    public Vec3 normalized() {
        float l = length();
        return new Vec3(f[0] / l, f[1] / l, f[2] / l);
    }

    void add(Vec3 v) {
        f[0] += v.f[0];
        f[1] += v.f[1];
        f[2] += v.f[2];
    }

    public Vec3 divide(float a) {
        return new Vec3(f[0] / a, f[1] / a, f[2] / a);
    }

    public Vec3 sub(Vec3 v) {
        return new Vec3(f[0] - v.f[0], f[1] - v.f[1], f[2] - v.f[2]);
    }

    public Vec3 addNew(Vec3 v) {
        return new Vec3(f[0] + v.f[0], f[1] + v.f[1], f[2] + v.f[2]);
    }

    Vec3 multiply(float a) {
        return new Vec3(f[0] * a, f[1] * a, f[2] * a);
    }

    Vec3 negate() {
        return new Vec3(-f[0], -f[1], -f[2]);
    }

    public Vec3 multiplySelf(float a) {
        f[0] *= a;
        f[1] *= a;
        f[2] *= a;
        return this;
    }

    public Vec3 divideSelf(float a) {
        f[0] /= a;
        f[1] /= a;
        f[2] /= a;
        return this;
    }

    public Vec3 subFromSelf(Vec3 v) {
        f[0] -= v.f[0];
        f[1] -= v.f[1];
        f[2] -= v.f[2];
        return this;
    }

    public Vec3 negateSelf() {
        f[0] = -f[0];
        f[1] = -f[1];
        f[2] = -f[2];
        return this;
    }

    Vec3 cross(Vec3 v) {
        return new Vec3(f[1] * v.f[2] - f[2] * v.f[1], f[2] * v.f[0] - f[0]
                * v.f[2], f[0] * v.f[1] - f[1] * v.f[0]);
    }

    float dot(Vec3 v) {
        return f[0] * v.f[0] + f[1] * v.f[1] + f[2] * v.f[2];
    }

    @Override
    public String toString() {
        return f[0] + ", " + f[1] + ", " + f[2];
    }
}