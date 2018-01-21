# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging

from operator import attrgetter
from telemetry.page import page_measurement
from telemetry.web_perf.metrics import rendering_frame

# These are LatencyInfo component names indicating the various components
# that the input event has travelled through.
# This is when the input event first reaches chrome.
UI_COMP_NAME = 'INPUT_EVENT_LATENCY_UI_COMPONENT'
# This is when the input event was originally created by OS.
ORIGINAL_COMP_NAME = 'INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT'
# This is when the input event was sent from browser to renderer.
BEGIN_COMP_NAME = 'INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT'
# This is when the input event has reached swap buffer.
END_COMP_NAME = 'INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT'


class NotEnoughFramesError(page_measurement.MeasurementFailure):
  def __init__(self, frame_count):
    super(NotEnoughFramesError, self).__init__(
      'Only %i frame timestamps were collected ' % frame_count +
      '(at least two are required).\n'
      'Issues that have caused this in the past:\n' +
      '- Browser bugs that prevents the page from redrawing\n' +
      '- Bugs in the synthetic gesture code\n' +
      '- Page and benchmark out of sync (e.g. clicked element was renamed)\n' +
      '- Pages that render extremely slow\n' +
      '- Pages that can\'t be scrolled')


def GetInputLatencyEvents(process, timeline_range):
  """Get input events' LatencyInfo from the process's trace buffer that are
     within the timeline_range.

  Input events dump their LatencyInfo into trace buffer as async trace event
  with name "InputLatency". The trace event has a memeber 'data' containing
  its latency history.

  """
  input_events = []
  if not process:
    return input_events
  for event in process.IterAllAsyncSlicesOfName('InputLatency'):
    if event.start >= timeline_range.min and event.end <= timeline_range.max:
      for ss in event.sub_slices:
        if 'data' in ss.args:
          input_events.append(ss)
  return input_events


def ComputeInputEventLatency(input_events):
  """ Compute the input event latency.

  Input event latency is the time from when the input event is created to
  when its resulted page is swap buffered.
  Input event on differnt platforms uses different LatencyInfo component to
  record its creation timestamp. We go through the following component list
  to find the creation timestamp:
  1. INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT -- when event is created in OS
  2. INPUT_EVENT_LATENCY_UI_COMPONENT -- when event reaches Chrome
  3. INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT -- when event reaches RenderWidget

  """
  input_event_latency = []
  for event in input_events:
    data = event.args['data']
    if END_COMP_NAME in data:
      end_time = data[END_COMP_NAME]['time']
      if ORIGINAL_COMP_NAME in data:
        latency = end_time - data[ORIGINAL_COMP_NAME]['time']
      elif UI_COMP_NAME in data:
        latency = end_time - data[UI_COMP_NAME]['time']
      elif BEGIN_COMP_NAME in data:
        latency = end_time - data[BEGIN_COMP_NAME]['time']
      else:
        raise ValueError, 'LatencyInfo has no begin component'
      input_event_latency.append(latency / 1000.0)
  return input_event_latency


def HasRenderingStats(process):
  """ Returns True if the process contains at least one
      BenchmarkInstrumentation::*RenderingStats event with a frame.
  """
  if not process:
    return False
  for event in process.IterAllSlicesOfName(
      'BenchmarkInstrumentation::MainThreadRenderingStats'):
    if 'data' in event.args and event.args['data']['frame_count'] == 1:
      return True
  for event in process.IterAllSlicesOfName(
      'BenchmarkInstrumentation::ImplThreadRenderingStats'):
    if 'data' in event.args and event.args['data']['frame_count'] == 1:
      return True
  return False


class RenderingStats(object):
  def __init__(self, renderer_process, browser_process, timeline_ranges):
    """
    Utility class for extracting rendering statistics from the timeline (or
    other loggin facilities), and providing them in a common format to classes
    that compute benchmark metrics from this data.

    Stats are lists of lists of numbers. The outer list stores one list per
    timeline range.

    All *_time values are measured in milliseconds.
    """
    assert(len(timeline_ranges) > 0)
    # Find the top level process with rendering stats (browser or renderer).
    if HasRenderingStats(browser_process):
      timestamp_process = browser_process
    else:
      timestamp_process  = renderer_process

    self.frame_timestamps = []
    self.frame_times = []
    self.paint_times = []
    self.painted_pixel_counts = []
    self.record_times = []
    self.recorded_pixel_counts = []
    self.rasterize_times = []
    self.rasterized_pixel_counts = []
    self.approximated_pixel_percentages = []
    # End-to-end latency for input event - from when input event is
    # generated to when the its resulted page is swap buffered.
    self.input_event_latency = []
    self.frame_queueing_durations = []

    for timeline_range in timeline_ranges:
      self.frame_timestamps.append([])
      self.frame_times.append([])
      self.paint_times.append([])
      self.painted_pixel_counts.append([])
      self.record_times.append([])
      self.recorded_pixel_counts.append([])
      self.rasterize_times.append([])
      self.rasterized_pixel_counts.append([])
      self.approximated_pixel_percentages.append([])
      self.input_event_latency.append([])

      if timeline_range.is_empty:
        continue
      self._InitFrameTimestampsFromTimeline(timestamp_process, timeline_range)
      self._InitMainThreadRenderingStatsFromTimeline(
          renderer_process, timeline_range)
      self._InitImplThreadRenderingStatsFromTimeline(
          renderer_process, timeline_range)
      self._InitInputLatencyStatsFromTimeline(
          browser_process, renderer_process, timeline_range)
      self._InitFrameQueueingDurationsFromTimeline(
          renderer_process, timeline_range)

    # Check if we have collected at least 2 frames in every range. Otherwise we
    # can't compute any meaningful metrics.
    for segment in self.frame_timestamps:
      if len(segment) < 2:
        raise NotEnoughFramesError(len(segment))

  def _InitInputLatencyStatsFromTimeline(
      self, browser_process, renderer_process, timeline_range):
    latency_events = GetInputLatencyEvents(browser_process, timeline_range)
    # Plugin input event's latency slice is generated in renderer process.
    latency_events.extend(GetInputLatencyEvents(renderer_process,
                                                timeline_range))
    self.input_event_latency[-1] = ComputeInputEventLatency(latency_events)

  def _GatherEvents(self, event_name, process, timeline_range):
    events = []
    for event in process.IterAllSlicesOfName(event_name):
      if event.start >= timeline_range.min and event.end <= timeline_range.max:
        if 'data' not in event.args:
          continue
        events.append(event)
    events.sort(key=attrgetter('start'))
    return events

  def _AddFrameTimestamp(self, event):
    frame_count = event.args['data']['frame_count']
    if frame_count > 1:
      raise ValueError('trace contains multi-frame render stats')
    if frame_count == 1:
      self.frame_timestamps[-1].append(
          event.start)
      if len(self.frame_timestamps[-1]) >= 2:
        self.frame_times[-1].append(round(self.frame_timestamps[-1][-1] -
                                          self.frame_timestamps[-1][-2], 2))

  def _InitFrameTimestampsFromTimeline(self, process, timeline_range):
    event_name = 'BenchmarkInstrumentation::MainThreadRenderingStats'
    for event in self._GatherEvents(event_name, process, timeline_range):
      self._AddFrameTimestamp(event)

    event_name = 'BenchmarkInstrumentation::ImplThreadRenderingStats'
    for event in self._GatherEvents(event_name, process, timeline_range):
      self._AddFrameTimestamp(event)

  def _InitMainThreadRenderingStatsFromTimeline(self, process, timeline_range):
    event_name = 'BenchmarkInstrumentation::MainThreadRenderingStats'
    for event in self._GatherEvents(event_name, process, timeline_range):
      data = event.args['data']
      self.paint_times[-1].append(1000.0 * data['paint_time'])
      self.painted_pixel_counts[-1].append(data['painted_pixel_count'])
      self.record_times[-1].append(1000.0 * data['record_time'])
      self.recorded_pixel_counts[-1].append(data['recorded_pixel_count'])

  def _InitImplThreadRenderingStatsFromTimeline(self, process, timeline_range):
    event_name = 'BenchmarkInstrumentation::ImplThreadRenderingStats'
    for event in self._GatherEvents(event_name, process, timeline_range):
      data = event.args['data']
      self.rasterize_times[-1].append(1000.0 * data['rasterize_time'])
      self.rasterized_pixel_counts[-1].append(data['rasterized_pixel_count'])
      if data.get('visible_content_area', 0):
        self.approximated_pixel_percentages[-1].append(
            round(float(data['approximated_visible_content_area']) /
                  float(data['visible_content_area']) * 100.0, 3))
      else:
        self.approximated_pixel_percentages[-1].append(0.0)

  def _InitFrameQueueingDurationsFromTimeline(self, process, timeline_range):
    try:
      events = rendering_frame.GetFrameEventsInsideRange(process,
                                                         timeline_range)
      new_frame_queueing_durations = [e.queueing_duration for e in events]
      self.frame_queueing_durations.append(new_frame_queueing_durations)
    except rendering_frame.NoBeginFrameIdException:
      logging.warning('Current chrome version does not support the queueing '
                      'delay metric.')
