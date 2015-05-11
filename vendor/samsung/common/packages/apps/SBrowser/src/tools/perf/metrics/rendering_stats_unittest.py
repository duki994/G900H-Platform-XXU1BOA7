# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import random
import unittest

from metrics.rendering_stats import UI_COMP_NAME, BEGIN_COMP_NAME, END_COMP_NAME
from metrics.rendering_stats import GetScrollInputLatencyEvents
from metrics.rendering_stats import ComputeMouseWheelScrollLatency
from metrics.rendering_stats import ComputeTouchScrollLatency
from metrics.rendering_stats import RenderingStats
import telemetry.core.timeline.bounds as timeline_bounds
from telemetry.core.timeline import model
import telemetry.core.timeline.async_slice as tracing_async_slice


class MockTimer(object):
  """A mock timer class which can generate random durations.

  An instance of this class is used as a global timer to generate random
  durations for stats and consistent timestamps for all mock trace events.
  The unit of time is milliseconds.
  """
  def __init__(self):
    self.milliseconds = 0

  def Get(self):
    return self.milliseconds

  def Advance(self, low=0, high=1):
    delta = random.uniform(low, high)
    self.milliseconds += delta
    return delta


class ReferenceRenderingStats(object):
  """ Stores expected data for comparison with actual RenderingStats """
  def __init__(self):
    self.frame_timestamps = []
    self.frame_times = []
    self.paint_times = []
    self.painted_pixel_counts = []
    self.record_times = []
    self.recorded_pixel_counts = []
    self.rasterize_times = []
    self.rasterized_pixel_counts = []

  def AppendNewRange(self):
    self.frame_timestamps.append([])
    self.frame_times.append([])
    self.paint_times.append([])
    self.painted_pixel_counts.append([])
    self.record_times.append([])
    self.recorded_pixel_counts.append([])
    self.rasterize_times.append([])
    self.rasterized_pixel_counts.append([])

class ReferenceInputLatencyStats(object):
  """ Stores expected data for comparison with actual input latency stats """
  def __init__(self):
    self.mouse_wheel_scroll_latency = []
    self.touch_scroll_latency = []
    self.js_touch_scroll_latency = []
    self.mouse_wheel_scroll_events = []
    self.touch_scroll_events = []
    self.js_touch_scroll_events = []

def AddMainThreadRenderingStats(mock_timer, thread, first_frame,
                                ref_stats = None):
  """ Adds a random main thread rendering stats event.

  thread: The timeline model thread to which the event will be added.
  first_frame: Is this the first frame within the bounds of an action?
  ref_stats: A ReferenceRenderingStats object to record expected values.
  """
  # Create randonm data and timestap for main thread rendering stats.
  data = { 'frame_count': 0,
           'paint_time': 0.0,
           'painted_pixel_count': 0,
           'record_time': mock_timer.Advance(2, 4) / 1000.0,
           'recorded_pixel_count': 3000*3000 }
  timestamp = mock_timer.Get()

  # Add a slice with the event data to the given thread.
  thread.PushCompleteSlice(
      'benchmark', 'BenchmarkInstrumentation::MainThreadRenderingStats',
      timestamp, duration=0.0, thread_timestamp=None, thread_duration=None,
      args={'data': data})

  if not ref_stats:
    return

  # Add timestamp only if a frame was output
  if data['frame_count'] == 1:
    if not first_frame:
      # Add frame_time if this is not the first frame in within the bounds of an
      # action.
      prev_timestamp = ref_stats.frame_timestamps[-1][-1]
      ref_stats.frame_times[-1].append(round(timestamp - prev_timestamp, 2))
    ref_stats.frame_timestamps[-1].append(timestamp)

  ref_stats.paint_times[-1].append(data['paint_time'] * 1000.0)
  ref_stats.painted_pixel_counts[-1].append(data['painted_pixel_count'])
  ref_stats.record_times[-1].append(data['record_time'] * 1000.0)
  ref_stats.recorded_pixel_counts[-1].append(data['recorded_pixel_count'])


def AddImplThreadRenderingStats(mock_timer, thread, first_frame,
                                ref_stats = None):
  """ Adds a random impl thread rendering stats event.

  thread: The timeline model thread to which the event will be added.
  first_frame: Is this the first frame within the bounds of an action?
  ref_stats: A ReferenceRenderingStats object to record expected values.
  """
  # Create randonm data and timestap for impl thread rendering stats.
  data = { 'frame_count': 1,
           'rasterize_time': mock_timer.Advance(5, 10) / 1000.0,
           'rasterized_pixel_count': 1280*720 }
  timestamp = mock_timer.Get()

  # Add a slice with the event data to the given thread.
  thread.PushCompleteSlice(
      'benchmark', 'BenchmarkInstrumentation::ImplThreadRenderingStats',
      timestamp, duration=0.0, thread_timestamp=None, thread_duration=None,
      args={'data': data})

  if not ref_stats:
    return

  # Add timestamp only if a frame was output
  if data['frame_count'] == 1:
    if not first_frame:
      # Add frame_time if this is not the first frame in within the bounds of an
      # action.
      prev_timestamp = ref_stats.frame_timestamps[-1][-1]
      ref_stats.frame_times[-1].append(round(timestamp - prev_timestamp, 2))
    ref_stats.frame_timestamps[-1].append(timestamp)

  ref_stats.rasterize_times[-1].append(data['rasterize_time'] * 1000.0)
  ref_stats.rasterized_pixel_counts[-1].append(data['rasterized_pixel_count'])


def AddInputLatencyStats(mock_timer, input_type, start_thread, end_thread,
                         ref_latency_stats = None):
  """ Adds a random input latency stats event.

  input_type: The input type for which the latency slice is generated.
  start_thread: The start thread on which the async slice is added.
  end_thread: The end thread on which the async slice is ended.
  ref_latency_stats: A ReferenceInputLatencyStats object for expected values.
  """

  mock_timer.Advance()
  ui_comp_time = mock_timer.Get() * 1000.0
  mock_timer.Advance()
  begin_comp_time = mock_timer.Get() * 1000.0
  mock_timer.Advance(10, 20)
  end_comp_time = mock_timer.Get() * 1000.0

  data = { UI_COMP_NAME: {'time': ui_comp_time},
           BEGIN_COMP_NAME: {'time': begin_comp_time},
           END_COMP_NAME: {'time': end_comp_time} }

  timestamp = mock_timer.Get()

  async_slice = tracing_async_slice.AsyncSlice(
      'benchmark', 'InputLatency', timestamp)

  async_sub_slice = tracing_async_slice.AsyncSlice(
      'benchmark', 'InputLatency', timestamp)
  async_sub_slice.args = {'data': data, 'step': input_type}
  async_sub_slice.parent_slice = async_slice
  async_sub_slice.start_thread = start_thread
  async_sub_slice.end_thread = end_thread

  async_slice.sub_slices.append(async_sub_slice)
  async_slice.start_thread = start_thread
  async_slice.end_thread = end_thread
  start_thread.AddAsyncSlice(async_slice)

  if not ref_latency_stats:
    return

  if input_type == 'MouseWheel':
    ref_latency_stats.mouse_wheel_scroll_events.append(async_sub_slice)
    ref_latency_stats.mouse_wheel_scroll_latency.append(
        (data[END_COMP_NAME]['time'] - data[BEGIN_COMP_NAME]['time']) / 1000.0)

  if input_type == 'GestureScrollUpdate':
    ref_latency_stats.touch_scroll_events.append(async_sub_slice)
    ref_latency_stats.touch_scroll_latency.append(
        (data[END_COMP_NAME]['time'] - data[UI_COMP_NAME]['time']) / 1000.0)

  if input_type == 'TouchMove':
    ref_latency_stats.js_touch_scroll_events.append(async_sub_slice)
    ref_latency_stats.js_touch_scroll_latency.append(
        (data[END_COMP_NAME]['time'] - data[UI_COMP_NAME]['time']) / 1000.0)

class RenderingStatsUnitTest(unittest.TestCase):
  def testFromTimeline(self):
    timeline = model.TimelineModel()

    # Create a browser process and a renderer process, and a main thread and
    # impl thread for each.
    browser = timeline.GetOrCreateProcess(pid = 1)
    browser_main = browser.GetOrCreateThread(tid = 11)
    browser_compositor = browser.GetOrCreateThread(tid = 12)
    renderer = timeline.GetOrCreateProcess(pid = 2)
    renderer_main = renderer.GetOrCreateThread(tid = 21)
    renderer_compositor = renderer.GetOrCreateThread(tid = 22)

    timer = MockTimer()
    ref_stats = ReferenceRenderingStats()

    # Create 10 main and impl rendering stats events for Action A.
    timer.Advance()
    renderer_main.BeginSlice('webkit.console', 'ActionA', timer.Get(), '')
    ref_stats.AppendNewRange()
    for i in xrange(0, 10):
      first = (i == 0)
      AddMainThreadRenderingStats(timer, renderer_main, first, ref_stats)
      AddImplThreadRenderingStats(timer, renderer_compositor, first, ref_stats)
      AddMainThreadRenderingStats(timer, browser_main, first, None)
      AddImplThreadRenderingStats(timer, browser_compositor, first, None)
    renderer_main.EndSlice(timer.Get())

    # Create 5 main and impl rendering stats events not within any action.
    for i in xrange(0, 5):
      first = (i == 0)
      AddMainThreadRenderingStats(timer, renderer_main, first, None)
      AddImplThreadRenderingStats(timer, renderer_compositor, first, None)
      AddMainThreadRenderingStats(timer, browser_main, first, None)
      AddImplThreadRenderingStats(timer, browser_compositor, first, None)

    # Create 10 main and impl rendering stats events for Action B.
    timer.Advance()
    renderer_main.BeginSlice('webkit.console', 'ActionB', timer.Get(), '')
    ref_stats.AppendNewRange()
    for i in xrange(0, 10):
      first = (i == 0)
      AddMainThreadRenderingStats(timer, renderer_main, first, ref_stats)
      AddImplThreadRenderingStats(timer, renderer_compositor, first, ref_stats)
      AddMainThreadRenderingStats(timer, browser_main, first, None)
      AddImplThreadRenderingStats(timer, browser_compositor, first, None)
    renderer_main.EndSlice(timer.Get())

    # Create 10 main and impl rendering stats events for Action A.
    timer.Advance()
    renderer_main.BeginSlice('webkit.console', 'ActionA', timer.Get(), '')
    ref_stats.AppendNewRange()
    for i in xrange(0, 10):
      first = (i == 0)
      AddMainThreadRenderingStats(timer, renderer_main, first, ref_stats)
      AddImplThreadRenderingStats(timer, renderer_compositor, first, ref_stats)
      AddMainThreadRenderingStats(timer, browser_main, first, None)
      AddImplThreadRenderingStats(timer, browser_compositor, first, None)
    renderer_main.EndSlice(timer.Get())

    renderer_main.FinalizeImport()
    renderer_compositor.FinalizeImport()

    timeline_markers = timeline.FindTimelineMarkers(
        ['ActionA', 'ActionB', 'ActionA'])
    timeline_ranges = [ timeline_bounds.Bounds.CreateFromEvent(marker)
                        for marker in timeline_markers ]
    stats = RenderingStats(renderer, browser, timeline_ranges)

    # Compare rendering stats to reference.
    self.assertEquals(stats.frame_timestamps, ref_stats.frame_timestamps)
    self.assertEquals(stats.frame_times, ref_stats.frame_times)
    self.assertEquals(stats.rasterize_times, ref_stats.rasterize_times)
    self.assertEquals(stats.rasterized_pixel_counts,
                      ref_stats.rasterized_pixel_counts)
    self.assertEquals(stats.paint_times, ref_stats.paint_times)
    self.assertEquals(stats.painted_pixel_counts,
                      ref_stats.painted_pixel_counts)
    self.assertEquals(stats.record_times, ref_stats.record_times)
    self.assertEquals(stats.recorded_pixel_counts,
                      ref_stats.recorded_pixel_counts)

  def testScrollLatencyFromTimeline(self):
    timeline = model.TimelineModel()

    # Create a browser process and a renderer process.
    browser = timeline.GetOrCreateProcess(pid = 1)
    browser_main = browser.GetOrCreateThread(tid = 11)
    renderer = timeline.GetOrCreateProcess(pid = 2)
    renderer_main = renderer.GetOrCreateThread(tid = 21)

    timer = MockTimer()
    ref_latency_stats = ReferenceInputLatencyStats()

    # Create 10 input latency stats events for Action A.
    timer.Advance()
    renderer_main.BeginSlice('webkit.console', 'ActionA', timer.Get(), '')
    for _ in xrange(0, 10):
      AddInputLatencyStats(timer, 'MouseWheel', browser_main,
                           renderer_main, ref_latency_stats)
      AddInputLatencyStats(timer, 'GestureScrollUpdate', browser_main,
                           renderer_main, ref_latency_stats)
      AddInputLatencyStats(timer, 'TouchMove', browser_main,
                           renderer_main, ref_latency_stats)
    renderer_main.EndSlice(timer.Get())

    # Create 5 input latency stats events not within any action.
    for _ in xrange(0, 5):
      AddInputLatencyStats(timer, 'MouseWheel', browser_main,
                           renderer_main, None)
      AddInputLatencyStats(timer, 'GestureScrollUpdate', browser_main,
                           renderer_main, None)
      AddInputLatencyStats(timer, 'TouchMove', browser_main,
                           renderer_main, None)

    # Create 10 input latency stats events for Action B.
    timer.Advance()
    renderer_main.BeginSlice('webkit.console', 'ActionB', timer.Get(), '')
    for _ in xrange(0, 10):
      AddInputLatencyStats(timer, 'MouseWheel', browser_main,
                           renderer_main, ref_latency_stats)
      AddInputLatencyStats(timer, 'GestureScrollUpdate', browser_main,
                           renderer_main, ref_latency_stats)
      AddInputLatencyStats(timer, 'TouchMove', browser_main,
                           renderer_main, ref_latency_stats)
    renderer_main.EndSlice(timer.Get())

    # Create 10 input latency stats events for Action A.
    timer.Advance()
    renderer_main.BeginSlice('webkit.console', 'ActionA', timer.Get(), '')
    for _ in xrange(0, 10):
      AddInputLatencyStats(timer, 'MouseWheel', browser_main,
                                  renderer_main, ref_latency_stats)
      AddInputLatencyStats(timer, 'GestureScrollUpdate', browser_main,
                                  renderer_main, ref_latency_stats)
      AddInputLatencyStats(timer, 'TouchMove', browser_main,
                                  renderer_main, ref_latency_stats)
    renderer_main.EndSlice(timer.Get())

    browser_main.FinalizeImport()
    renderer_main.FinalizeImport()

    mouse_wheel_scroll_events = []
    touch_scroll_events = []
    js_touch_scroll_events = []

    timeline_markers = timeline.FindTimelineMarkers(
        ['ActionA', 'ActionB', 'ActionA'])
    for timeline_range in [ timeline_bounds.Bounds.CreateFromEvent(marker)
                            for marker in timeline_markers ]:
      if timeline_range.is_empty:
        continue
      tmp_mouse_events = GetScrollInputLatencyEvents(
          'MouseWheel', browser, timeline_range)
      tmp_touch_scroll_events = GetScrollInputLatencyEvents(
          'GestureScrollUpdate', browser, timeline_range)
      tmp_js_touch_scroll_events = GetScrollInputLatencyEvents(
          'TouchMove', browser, timeline_range)
      mouse_wheel_scroll_events.extend(tmp_mouse_events)
      touch_scroll_events.extend(tmp_touch_scroll_events)
      js_touch_scroll_events.extend(tmp_js_touch_scroll_events)

    self.assertEquals(mouse_wheel_scroll_events,
                      ref_latency_stats.mouse_wheel_scroll_events)
    self.assertEquals(touch_scroll_events,
                      ref_latency_stats.touch_scroll_events)
    self.assertEquals(js_touch_scroll_events,
                      ref_latency_stats.js_touch_scroll_events)
    self.assertEquals(ComputeMouseWheelScrollLatency(mouse_wheel_scroll_events),
                      ref_latency_stats.mouse_wheel_scroll_latency)
    self.assertEquals(ComputeTouchScrollLatency(touch_scroll_events),
                      ref_latency_stats.touch_scroll_latency)
    self.assertEquals(ComputeTouchScrollLatency(js_touch_scroll_events),
                      ref_latency_stats.js_touch_scroll_latency)
