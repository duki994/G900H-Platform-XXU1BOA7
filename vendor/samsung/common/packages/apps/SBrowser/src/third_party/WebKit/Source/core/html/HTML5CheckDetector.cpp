/*
 * Copyright 2014 Samsung. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "HTML5CheckDetector.h"

#ifdef S_HTML5CHECKDETECTOR

#include "config.h"

#include <wtf/CurrentTime.h>
#include "content/public/renderer/render_thread.h"
#include "content/common/view_messages.h"

int HTML5CheckDetector::_canvasCount=0;
int HTML5CheckDetector::_counter=0;
double HTML5CheckDetector::_prevCanvasTime=0;
double HTML5CheckDetector::_prevTime=0;
const int _threshold=10;
const double _timeDeltaThreshold = 0.1;
const int _canvasCountThreshold = 4;

#if defined(OS_ANDROID)
static void SSRMMode() {
    content::RenderThread* thread = content::RenderThread::Get();
    if (thread) {
      int routing_id = thread->GetLastViewId();
      thread->Send(new ViewHostMsg_OnSSRMModeCallback(routing_id, 2, 0)); //2 is CANVAS
    }
}
#endif

void HTML5CheckDetector::checkCanvasCount()
{
    double currentTime = WTF::currentTime();
    double timedelta = currentTime - _prevCanvasTime;

    if((_prevCanvasTime != 0) && (timedelta < _timeDeltaThreshold)) {
		_canvasCount++;
		_prevCanvasTime=currentTime;
		
         if(_canvasCount >= _canvasCountThreshold) 
         {
        	_canvasCount = 0;        	
#if defined(OS_ANDROID)
        	SSRMMode();
         }
#endif
    } else {
        _canvasCount=0;
        _prevCanvasTime=currentTime;
    }
}

void HTML5CheckDetector::checkCallerCount()
{
    double currentTime = WTF::currentTime();
    double timedelta = currentTime - _prevTime;

    if ((_prevTime != 0) && (timedelta < _timeDeltaThreshold)) {
        _counter++;
        _prevTime=currentTime;

        if (_counter >= _threshold) {
            _counter = 0;
#if defined(OS_ANDROID)
            SSRMMode();
#endif
        }
    } else {
        _counter=0;
        _prevTime=currentTime;
    }
}
#endif
