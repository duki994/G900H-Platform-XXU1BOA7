// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EVENT_QUEUE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EVENT_QUEUE_H_

#include <deque>
#include <map>

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/port/browser/event_with_latency_info.h"
#include "content/port/common/input_event_ack_state.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/geometry/point_f.h"

namespace content {

class CoalescedWebTouchEvent;

// Interface with which TouchEventQueue can forward touch events, and dispatch
// touch event responses.
class CONTENT_EXPORT TouchEventQueueClient {
 public:
  virtual ~TouchEventQueueClient() {}

  virtual void SendTouchEventImmediately(
      const TouchEventWithLatencyInfo& event) = 0;

  virtual void OnTouchEventAck(
      const TouchEventWithLatencyInfo& event,
      InputEventAckState ack_result) = 0;
};

// A queue for throttling and coalescing touch-events.
class CONTENT_EXPORT TouchEventQueue {
 public:
  // Different ways of dealing with touch events during scrolling.
  // TODO(rbyers): Remove (or otherwise update) this once results of
  // experiments are complete.  http://crbug.com/328503
  enum TouchScrollingMode {
    // Send a touchcancel on scroll start and no further touch events for the
    // duration of the scroll.  Chrome Android's traditional behavior.
    TOUCH_SCROLLING_MODE_TOUCHCANCEL,
    // Send touchmove events throughout a scroll, blocking on each ACK and
    // using the disposition to determine whether a scroll update should be
    // sent.  Mobile Safari's default overflow scroll behavior.
    TOUCH_SCROLLING_MODE_SYNC_TOUCHMOVE,
    // Like sync, except that consumed scroll events cause subsequent touchmove
    // events to be suppressed.  Unconsumed scroll events return touchmove
    // events to being dispatched synchronously (so scrolling may be hijacked
    // when a scroll limit is reached, and later resumed).
    TOUCH_SCROLLING_MODE_ABSORB_TOUCHMOVE,
    TOUCH_SCROLLING_MODE_DEFAULT = TOUCH_SCROLLING_MODE_TOUCHCANCEL
  };

  // The |client| must outlive the TouchEventQueue. If
  // |touchmove_suppression_length_dips| <= 0, touch move suppression is
  // disabled.
  TouchEventQueue(TouchEventQueueClient* client,
                  TouchScrollingMode mode,
                  double touchmove_suppression_length_dips);
  ~TouchEventQueue();

  // Adds an event to the queue. The event may be coalesced with previously
  // queued events (e.g. consecutive touch-move events can be coalesced into a
  // single touch-move event). The event may also be immediately forwarded to
  // the renderer (e.g. when there are no other queued touch event).
  void QueueEvent(const TouchEventWithLatencyInfo& event);

  // Notifies the queue that a touch-event has been processed by the renderer.
  // At this point, the queue may send one or more gesture events and/or
  // additional queued touch-events to the renderer.
  void ProcessTouchAck(InputEventAckState ack_result,
                       const ui::LatencyInfo& latency_info);

  // When GestureScrollBegin is received, we send a touch cancel to renderer,
  // route all the following touch events directly to client, and ignore the
  // ack for the touch cancel. When Gesture{ScrollEnd,FlingStart} is received,
  // resume the normal flow of sending touch events to the renderer.
  void OnGestureScrollEvent(const GestureEventWithLatencyInfo& gesture_event);

  void OnGestureEventAck(
      const GestureEventWithLatencyInfo& event,
      InputEventAckState ack_result);

  // Notifies the queue whether the renderer has at least one touch handler.
  void OnHasTouchEventHandlers(bool has_handlers);

  // Returns whether the currently pending touch event (waiting ACK) is for
  // a touch start event.
  bool IsPendingAckTouchStart() const;

  // Sets whether a delayed touch ack will cancel and flush the current
  // touch sequence.
  void SetAckTimeoutEnabled(bool enabled, size_t ack_timeout_delay_ms);

  bool empty() const WARN_UNUSED_RESULT {
    return touch_queue_.empty();
  }

  size_t size() const {
    return touch_queue_.size();
  }

  bool ack_timeout_enabled() const {
    return ack_timeout_enabled_;
  }

  bool has_handlers() const {
    return touch_filtering_state_ != DROP_ALL_TOUCHES;
  }

 private:
  class TouchTimeoutHandler;
  class TouchMoveSlopSuppressor;
  friend class TouchTimeoutHandler;
  friend class TouchEventQueueTest;

  bool HasTimeoutEvent() const;
  bool IsTimeoutRunningForTesting() const;
  const TouchEventWithLatencyInfo& GetLatestEventForTesting() const;

  // Empties the queue of touch events. This may result in any number of gesture
  // events being sent to the renderer.
  void FlushQueue();

  // Walks the queue, checking each event for |ShouldForwardToRenderer()|.
  // If true, forwards the touch event and stops processing further events.
  // If false, acks the event with |INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS|.
  void TryForwardNextEventToRenderer();

  // Pops the touch-event from the top of the queue and sends it to the
  // TouchEventQueueClient. This reduces the size of the queue by one.
  void PopTouchEventToClient(InputEventAckState ack_result,
                             const ui::LatencyInfo& renderer_latency_info);

  enum PreFilterResult {
    ACK_WITH_NO_CONSUMER_EXISTS,
    ACK_WITH_NOT_CONSUMED,
    FORWARD_TO_RENDERER,
  };
  // Filter touches prior to forwarding to the renderer, e.g., if the renderer
  // has no touch handler.
  PreFilterResult FilterBeforeForwarding(const blink::WebTouchEvent& event);
  void ForwardToRenderer(const TouchEventWithLatencyInfo& event);
  void UpdateTouchAckStates(const blink::WebTouchEvent& event,
                            InputEventAckState ack_result);


  // Handles touch event forwarding and ack'ed event dispatch.
  TouchEventQueueClient* client_;

  typedef std::deque<CoalescedWebTouchEvent*> TouchQueue;
  TouchQueue touch_queue_;

  // Maintain the ACK status for each touch point.
  typedef std::map<int, InputEventAckState> TouchPointAckStates;
  TouchPointAckStates touch_ack_states_;

  // Used to defer touch forwarding when ack dispatch triggers |QueueEvent()|.
  // If not NULL, |dispatching_touch_ack_| is the touch event of which the ack
  // is being dispatched.
  CoalescedWebTouchEvent* dispatching_touch_ack_;

  // Used to prevent touch timeout scheduling if we receive a synchronous
  // ack after forwarding a touch event to the client.
  bool dispatching_touch_;

  enum TouchFilteringState {
    FORWARD_ALL_TOUCHES,           // Don't filter at all - the default.
    FORWARD_TOUCHES_UNTIL_TIMEOUT, // Don't filter unless we get an ACK timeout.
    DROP_TOUCHES_IN_SEQUENCE,      // Filter all events until a new touch
                                   // sequence is received.
    DROP_ALL_TOUCHES,              // Filter all events, e.g., no touch handler.
    TOUCH_FILTERING_STATE_DEFAULT = FORWARD_ALL_TOUCHES,
  };
  TouchFilteringState touch_filtering_state_;

  // Optional handler for timed-out touch event acks, disabled by default.
  bool ack_timeout_enabled_;
  scoped_ptr<TouchTimeoutHandler> timeout_handler_;

  // Suppression of TouchMove's within a slop region when a sequence has not yet
  // been preventDefaulted.
  scoped_ptr<TouchMoveSlopSuppressor> touchmove_slop_suppressor_;

  // Whether touchmove events should be dropped due to the
  // TOUCH_SCROLLING_MODE_ABSORB_TOUCHMOVE mode.  Note that we can't use
  // touch_filtering_state_ for this (without adding a few new states and
  // complicating the code significantly) because it can occur with and without
  // timeout, and shouldn't cause touchend to be dropped.
  bool absorbing_touch_moves_;

  // How touch events are handled during scrolling.  For now this is a global
  // setting for experimentation, but we may evolve it into an app-controlled
  // mode.
  const TouchScrollingMode touch_scrolling_mode_;

  DISALLOW_COPY_AND_ASSIGN(TouchEventQueue);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EVENT_QUEUE_H_
