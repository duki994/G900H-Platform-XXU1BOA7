package com.sec.chromium.content.browser.pipette.suctionanimator;

import java.util.Vector;

class SimulationObject {

    private int numParticlesWidth; // number of particles in "width" direction
    private int numParticlesHeight; // number of particles in "height" direction

    private Vector<Constraint> constraints; // all constraints between particles
                                            // as part of this simulation obj

    private Particle[] mParticles;

    private Particle getParticle(int x, int y) {
        return mParticles[y * numParticlesWidth + x];
    }

    private void makeConstraint(Particle p1, Particle p2) {
        constraints.add(new Constraint(p1, p2));
    }

    public SimulationObject(float width, float height, int num_particles_width,
            int num_particles_height, float x_offset, float y_offset,
            float z_depth) {
        // y_offset = bottom
        // x_offset = left
        this.numParticlesWidth = num_particles_width;
        this.numParticlesHeight = num_particles_height;

        // particles = new Vector<Particle>();
        constraints = new Vector<Constraint>();

        mParticles = new Particle[num_particles_width * num_particles_height];

        mInitialZDepth = z_depth;
        mInitialXLeft = x_offset;
        mInitialYLeft = y_offset;
        mInitialXRight = x_offset + width;
        mInitialYRight = y_offset + height;


        Vec3 pos = new Vec3();
        
        for (int x = 0; x < num_particles_width; x++) {
            for (int y = 0; y < num_particles_height; y++) {
                
                pos.setValues(x_offset + width
                        * (x / (float) (num_particles_width - 1)), y_offset
                        + height * (y / (float) (num_particles_height - 1)),
                        z_depth);

                mParticles[y * num_particles_width + x] = new Particle(pos, y
                        * num_particles_width + x);

                // particles.add(y*num_particles_width+x, new Particle(pos)); //
                // insert particle in column x at y'th row
            }
        }

        // Connecting immediate neighbor particles with constraints (distance 1
        // and sqrt(2) in the grid)
        for (int x = 0; x < num_particles_width; x++) {
            for (int y = 0; y < num_particles_height; y++) {
                if (x < num_particles_width - 1) {
                    makeConstraint(getParticle(x, y), getParticle(x + 1, y));
                }
                if (y < num_particles_height - 1) {
                    makeConstraint(getParticle(x, y), getParticle(x, y + 1));
                    // if (x<num_particles_width-1 && y<num_particles_height-1)
                    // makeConstraint(getParticle(x,y),getParticle(x+1,y+1));
                    // if (x<num_particles_width-1 && y<num_particles_height-1)
                    // makeConstraint(getParticle(x+1,y),getParticle(x,y+1));
                }
            }
        }

    }

    public void reset() {

        constraints = new Vector<Constraint>();

        // creating particles in a grid of particles from (0,0,0) to
        // (width,-height,0)
        Vec3 pos = new Vec3();
        
        for (int x = 0; x < numParticlesWidth; x++) {
            for (int y = 0; y < numParticlesHeight; y++) {
                pos.setValues(mInitialXLeft
                        + (mInitialXRight - mInitialXLeft)
                        * (x / (float) (numParticlesWidth - 1)), mInitialYLeft
                        + (mInitialYRight - mInitialYLeft)
                        * (y / (float) (numParticlesHeight - 1)),
                        mInitialZDepth);

                mParticles[y * numParticlesWidth + x] = new Particle(pos, y
                        * numParticlesWidth + x);

                // insert particle in column x at y'th row
                // particles.add(y*num_particles_width+x, new Particle(pos));

            }
        }

        // Connecting immediate neighbor particles with constraints (distance 1
        // and sqrt(2) in the grid)
        for (int x = 0; x < numParticlesWidth; x++) {
            for (int y = 0; y < numParticlesHeight; y++) {
                if (x < numParticlesWidth - 1) {
                    makeConstraint(getParticle(x, y), getParticle(x + 1, y));
                }
                if (y < numParticlesHeight - 1) {
                    makeConstraint(getParticle(x, y), getParticle(x, y + 1));
                    // if (x<num_particles_width-1 && y<num_particles_height-1)
                    // makeConstraint(getParticle(x,y),getParticle(x+1,y+1));
                    // if (x<num_particles_width-1 && y<num_particles_height-1)
                    // makeConstraint(getParticle(x+1,y),getParticle(x,y+1));
                }
            }
        }

    }

    public float[] getPos(int particle_index_x, int particle_index_y) {
        return getParticle(particle_index_x, particle_index_y).getPos().f;
    }

    // This includes calling satisfyConstraint() for every constraint, and
    // calling timeStep() for all particles
    public void satisfyConstraints() {

        Vec3 tempDistanceVector = new Vec3();

        for (int i = 0; i < SimulationConstants.CONSTRAINT_ITERATIONS; i++) {
            // applyBoundConstraints();
            for (Constraint constraint : constraints) {

                if (constraint != null) {
                    (constraint).satisfyConstraint(tempDistanceVector);
                }
            }

            applyAdditionalConstraints();
        }
    }

    public void timeStep() {
        // original order
        satisfyConstraints();

        for (Particle particle : mParticles) {
            if (particle != null) {
                // calculate the position of each particle at the next time
                // step.
                (particle).timeStep();
            }
        }
    }

    public void solve() {

        // long start_time = SystemClock.currentThreadTimeMillis();
        // new order
        Vec3 temp1 = new Vec3();
        Vec3 temp2 = new Vec3();
        for (Particle particle : mParticles) {
            if (particle != null) {
                // calculate the position of each particle at the next time
                // step.
                (particle).timeStep(temp1, temp2);
            }
        }
        // long current_time = SystemClock.currentThreadTimeMillis();

        satisfyConstraints();
        // long current_time_afer_constraints =
        // SystemClock.currentThreadTimeMillis();
        // Log.d(TAG, " timestep time = " + (current_time - start_time) +
        // ", constraint time = " + (current_time_afer_constraints -
        // current_time));
    }

    /* used to add gravity (or any other arbitrary vector) to all particles */
    public void addForce(Vec3 direction) {

        for (Particle particle : mParticles) {
            if (particle != null) {
                (particle).addForce(direction);
            }
        }

    }

    void addConditionalRepulsionForce(Vec3 direction) {
        for (Particle particle : mParticles) {
            if ((particle).getPos().f[2] <= mInitialZDepth) {
                (particle).addForce(direction);
            }
        }

    }

    public void makeStatic(int x, int y) {
        getParticle(x, y).makeUnmovable();
    }

    public void pickAPoint(int x, int y) {

        mPickPointX = x;
        mPickPointY = y;
        getParticle(x, y).makeUnmovable();
        // Log.d(TAG, "pickAPoint: mPickPointX ="+mPickPointX +
        // ", mPickPointY = "+mPickPointY);

    }

    void applyOffsetAtPoint(Vec3 direction, int x, int y) {
        Particle particle = getParticle(x, y);
        Vec3 pos = (particle).getPos();
        if (mPickPointPos == null) {
            mPickPointPos = new Vec3(pos);
        }
        mPickPointPos.add(direction);
        // (particle).makeUnmovable();

    }

    void applyOffsetAtPickedPoint(Vec3 direction) {
        Particle particle = getParticle(mPickPointX, mPickPointY);
        Vec3 pos = (particle).getPos();
        if (mPickPointPos == null) {
            mPickPointPos = new Vec3(pos);
        }
        mPickPointPos.add(direction);
        // (particle).makeUnmovable();

    }

    void setPickPointPosition(float[] pos) {
        if (mPickPointPos == null) {
            mPickPointPos = new Vec3();
        }

        mPickPointPos.f[0] = pos[0];
        mPickPointPos.f[1] = pos[1];
        mPickPointPos.f[2] = pos[2];
        // (particle).makeUnmovable();
        // Log.d(TAG, "setPickPointPosition: pos ="+pos[0] + ", "+ pos[1] + ", "
        // + pos[2]);

    }

    public void applyAdditionalConstraints() {
        // center adjustment
        // Log.d(TAG, "applyAdditionalConstraints: mPickPointX ="+mPickPointX +
        // ", mPickPointY = "+mPickPointY);
        Particle particle = getParticle(mPickPointX, mPickPointY);
        if (particle == null) {
            // Log.e(TAG, "particle = null at mPickPointX ="+mPickPointX +
            // ", mPickPointY = "+mPickPointY);
            return;
        }
        (particle).setPos(mPickPointPos);
    }

    public void applyBoundConstraints() {

        for (Particle p : mParticles) {
            if (p.getPos().f[2] < mInitialZDepth) {
                p.getPos().f[2] = mInitialZDepth;
            } else if (p.getPos().f[2] > mPickPointPos.f[2]) {
                p.getPos().f[2] = mPickPointPos.f[2] - .001f;
            }

            if (p.getPos().f[0] < mVMin.f[0]) {
                p.getPos().f[0] = mVMin.f[0];
            } else if (p.getPos().f[0] > mVMax.f[0]) {
                p.getPos().f[0] = mVMax.f[0];
            }

            if (p.getPos().f[1] < mVMin.f[1]) {
                p.getPos().f[1] = mVMin.f[1];
            } else if (p.getPos().f[1] > mVMax.f[1]) {
                p.getPos().f[1] = mVMax.f[1];
            }

        }

    }

    public void setBounds(Vec3 vMin, Vec3 vMax) {
        mVMin = vMin;
        mVMax = vMax;
    }

    private int mPickPointX = 0;
    private int mPickPointY = 0;
    private Vec3 mPickPointPos = null;
    private float mInitialZDepth = 0;
    private float mInitialXLeft = 0;
    private float mInitialYLeft = 0;
    private float mInitialXRight = 0;
    private float mInitialYRight = 0;
    private Vec3 mVMin = null;
    private Vec3 mVMax = null;
}
