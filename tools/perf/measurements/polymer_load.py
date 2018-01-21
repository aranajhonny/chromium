# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page
from telemetry.page import page_measurement
from telemetry.value import scalar


class PageForPolymerLoad(page.Page):

  def __init__(self, url, page_set):
    super(PageForPolymerLoad, self).__init__(
      url=url,
      page_set=page_set)
    self.script_to_evaluate_on_commit = '''
      document.addEventListener("polymer-ready", function() {
        var unused = document.body.offsetHeight;
        window.__polymer_ready_time = performance.now();
        setTimeout(function() {
          window.__polymer_ready = true;
        }, 1000);
      })
    '''

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition('window.__polymer_ready')


class PolymerLoadMeasurement(page_measurement.PageMeasurement):
  def MeasurePage(self, _, tab, results):
    result = int(tab.EvaluateJavaScript('__polymer_ready_time'))
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'Total', 'ms', result))
