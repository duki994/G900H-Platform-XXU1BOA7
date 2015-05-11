package com.sec.chromium.content.browser.pipette.suctionanimator;

/* The particle class represents a particle of mass that can move around in 3D space*/
public class Particle {

    private boolean movable;
    // the mass of the particle (is always 1 in this case)
    private float mass;
    // the current position of the particle in 3D space
    private Vec3 mPos;
    // the position of the particle in the previous time step
    private Vec3 oldPos;
    // a vector representing the current acceleration of the particle
    private Vec3 acceleration;
    // an accumulated normal (i.e. non normalized)
    private Vec3 accumulatedNormal;

    private int mIndex;

    public Particle(Vec3 pos) {
        mPos = new Vec3(pos);
        oldPos = new Vec3(pos);
        // m_pos = new Vec3(pos);
        acceleration = new Vec3(0, 0, 0);
        mass = 1;
        movable = true;
        accumulatedNormal = new Vec3(0, 0, 0);
    }

    public Particle(Vec3 pos, int index) {
        mPos = new Vec3(pos);
        oldPos = new Vec3(pos);
        // m_pos = new Vec3(pos);
        acceleration = new Vec3(0, 0, 0);
        mass = 1;
        movable = true;
        accumulatedNormal = new Vec3(0, 0, 0);
        mIndex = index;
    }

    Particle() {
    }

    void addForce(Vec3 f) {
        acceleration.add(f.divide(mass));
    }

    /*
     * Given the equation "force = mass * acceleration" the next position is
     * found through verlet integration
     */
    void timeStep() {
        if (movable) {
            Vec3 temp = new Vec3(mPos);
            Vec3 verletIntegral1 = mPos.sub(oldPos).multiply(
                    1.0f - SimulationConstants.DAMPING);
            Vec3 verletIntegral2 = acceleration
                    .multiply(SimulationConstants.TIME_STEPSIZE2);
            mPos.add(verletIntegral1);
            mPos.add(verletIntegral2);
            oldPos = temp;
            // acceleration is reset since it HAS been translated into a change
            // in position (and implicitly into velocity)
            acceleration = new Vec3(0, 0, 0);
        }
    }

    /*
     * Optimized version of previous timeStep method
     */
    void timeStep(Vec3 temp1, Vec3 temp2) {
        if (movable) {
            temp1.setValues(mPos);
            temp2.setValues(mPos);
            Vec3 verletIntegral1 = temp2.subFromSelf(oldPos);
            verletIntegral1.multiplySelf(1.0f - SimulationConstants.DAMPING);
            Vec3 verletIntegral2 = acceleration
                    .multiplySelf(SimulationConstants.TIME_STEPSIZE2);
            mPos.add(verletIntegral1);
            mPos.add(verletIntegral2);
            oldPos.setValues(temp1);
            acceleration.resetValues();
        }
    }

    Vec3 getPos() {
        return mPos;
    }

    void setPos(Vec3 v) {
        mPos.f[0] = v.f[0];
        mPos.f[1] = v.f[1];
        mPos.f[2] = v.f[2];
    }

    Vec3 getOldPos() {
        return oldPos;
    }

    void resetAcceleration() {
        acceleration = new Vec3(0, 0, 0);
    }

    void offsetPos(Vec3 v) {
        if (movable) {
            mPos.add(v);
        }
    }

    void makeUnmovable() {
        movable = false;
    }

    void addToNormal(Vec3 normal) {
        accumulatedNormal.add(normal.normalized());
    }

    Vec3 getNormal() {
        return accumulatedNormal;
    }

    void resetNormal() {
        accumulatedNormal = new Vec3(0, 0, 0);
    }

    int getIndex() {
        return mIndex;
    }

    @Override
    public String toString() {
        return "Particle-" + mIndex + ": mPos [" + mPos + "] oldPos= ["
                + oldPos + "]";
    }

}