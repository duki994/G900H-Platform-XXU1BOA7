/*
 * Copyright 2014 Samsung. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef HTML5CheckDetector_h
#define HTML5CheckDetector_h

#ifdef S_HTML5CHECKDETECTOR
class HTML5CheckDetector {
public:    
	static void checkCanvasCount();
    static void checkCallerCount();

private:
    static int _counter;
    static double _prevTime;
    static double _prevCanvasTime;
    static int _canvasCount;
};
#endif

#endif
