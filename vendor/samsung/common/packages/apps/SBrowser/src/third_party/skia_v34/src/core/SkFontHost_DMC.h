#ifndef SK_FONT_HOST_DMC
#define SK_FONT_HOST_DMC

// Black gamma luminance cut-off
#define DEFAULT_TEXT_BLACK_GAMMA_THRESHOLD 64
// White gamma luminance cut-off
#define DEFAULT_TEXT_WHITE_GAMMA_THRESHOLD 192

typedef enum SF_Gamma_Mode_{
    SF_GAMMA_NONE = 0,         /*  No gamma  */
    SF_GAMMA_BLACK,              /*  Black gamma mode */
    SF_GAMMA_WHITE,             /*  White gamma mode */
    SF_GAMMA_TABLE_GRAY,  /*  Gray gamma mode */
} SF_Gamma_Mode;

/** \class SkFontHost
       This class is for Calculate gamma features for samsung font engine extension
*/
class SkFontHostDMC {
public:
    /** Calculates color luminance value from given color */
    static int getLuminance( int color );
    
  /** Returns gamma flags from given color value */ 
    static int getFontGammaFlag( int color );
};
#endif
