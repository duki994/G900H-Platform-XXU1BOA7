package com.sec.chromium.content.browser.pipette.suctionanimator;

public class Constraint {

    //enable if square root approximation required for faster execution
    //public static final boolean ENABLE_SQUARE_ROOT_APPROXIMATION = false; 
    private float restDistance;
    //private float restDistanceSquare;
    public Particle p1, p2;

    Constraint(Particle p1, Particle p2) {
        this.p1 = p1;
        this.p2 = p2;
        Vec3 pos1 = p1.getPos();
        Vec3 pos2 = p2.getPos();
        Vec3 p1_to_p2 = pos1.sub(pos2); // vector from p1 to p2
        restDistance = p1_to_p2.length();
        //restDistanceSquare = restDistance * restDistance;
    }

    /*
     * Single constraint between two particles p1 and p2 is solved by this
     * method. This method is called many times per frame
     */
    void satisfyConstraint() {
        Vec3 pos1 = p1.getPos();
        Vec3 pos2 = p2.getPos();
        Vec3 p1_to_p2 = pos2.sub(pos1); // vector from p1 to p2
        Vec3 correctionVectorHalf = null;
//        if (!ENABLE_SQUARE_ROOT_APPROXIMATION) {
        float currentDistance = p1_to_p2.length();
        Vec3 correctionVector = p1_to_p2.multiply(1 - restDistance
                / currentDistance);
        correctionVectorHalf = correctionVector.multiply(0.5f);
//        } else {
//            float sqlen = p1_to_p2.squareLength();
//            float multiplier = (float) (0.5 - (restDistanceSquare)
//                    / (sqlen + restDistanceSquare));
//            correctionVectorHalf = p1_to_p2.multiply(multiplier);
//        }
        p1.offsetPos(correctionVectorHalf);
        p2.offsetPos(correctionVectorHalf.negate());
    }

    /* Optimized version of satisfyConstraint method */
    void satisfyConstraint(Vec3 distanceVector) {
        Vec3 pos1 = p1.getPos();
        Vec3 pos2 = p2.getPos();
        distanceVector.setValues(pos2);
        // vector from p1 to p2
        Vec3 p1_to_p2 = distanceVector.subFromSelf(pos1);

        Vec3 correctionVectorHalf = null;
//        if (ENABLE_SQUARE_ROOT_APPROXIMATION) {
//            float sqlen = p1_to_p2.squareLength();
//            float multiplier = (float) (0.5 - (restDistanceSquare)
//                    / (sqlen + restDistanceSquare));
//            correctionVectorHalf = p1_to_p2.multiplySelf(multiplier);
//        } else {
        float currentDistance = p1_to_p2.length();

        Vec3 correctionVector = p1_to_p2.multiplySelf(1 - restDistance
                / currentDistance);
        correctionVectorHalf = correctionVector.multiplySelf(0.5f);

//        }
        p1.offsetPos(correctionVectorHalf);
        p2.offsetPos(correctionVectorHalf.negateSelf());
    }

}
