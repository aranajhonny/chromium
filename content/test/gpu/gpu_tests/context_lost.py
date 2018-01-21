# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from telemetry import benchmark as benchmark_module
from telemetry.core import exceptions
from telemetry.core import util
from telemetry.page import page
from telemetry.page import page_set
from telemetry.page import page_test

data_path = os.path.join(
    util.GetChromiumSrcDir(), 'content', 'test', 'data', 'gpu')

wait_timeout = 20  # seconds

harness_script = r"""
  var domAutomationController = {};

  domAutomationController._loaded = false;
  domAutomationController._succeeded = false;
  domAutomationController._finished = false;

  domAutomationController.setAutomationId = function(id) {}

  domAutomationController.send = function(msg) {
    msg = msg.toLowerCase()
    if (msg == "loaded") {
      domAutomationController._loaded = true;
    } else if (msg == "success") {
      domAutomationController._succeeded = true;
      domAutomationController._finished = true;
    } else {
      domAutomationController._succeeded = false;
      domAutomationController._finished = true;
    }
  }

  window.domAutomationController = domAutomationController;
  console.log("Harness injected.");
"""

class _ContextLostValidator(page_test.PageTest):
  def __init__(self):
    # Strictly speaking this test doesn't yet need a browser restart
    # after each run, but if more tests are added which crash the GPU
    # process, then it will.
    super(_ContextLostValidator, self).__init__(
      needs_browser_restart_after_each_page=True)

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(
        '--disable-domain-blocking-for-3d-apis')
    options.AppendExtraBrowserArgs(
        '--disable-gpu-process-crash-limit')
    # Required for about:gpucrash handling from Telemetry.
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')

  def ValidatePage(self, page, tab, results):
    if page.kill_gpu_process:
      # Doing the GPU process kill operation cooperatively -- in the
      # same page's context -- is much more stressful than restarting
      # the browser every time.
      for x in range(page.number_of_gpu_process_kills):
        if not tab.browser.supports_tab_control:
          raise page_test.Failure('Browser must support tab control')
        # Reset the test's state.
        tab.EvaluateJavaScript(
          'window.domAutomationController._succeeded = false');
        tab.EvaluateJavaScript(
          'window.domAutomationController._finished = false');
        # Crash the GPU process.
        new_tab = tab.browser.tabs.New()
        # To access these debug URLs from Telemetry, they have to be
        # written using the chrome:// scheme.
        # The try/except is a workaround for crbug.com/368107.
        try:
          new_tab.Navigate('chrome://gpucrash')
        except (exceptions.TabCrashException, Exception):
          print 'Tab crashed while navigating to chrome://gpucrash'
        # Activate the original tab and wait for completion.
        tab.Activate()
        completed = False
        try:
          util.WaitFor(lambda: tab.EvaluateJavaScript(
              'window.domAutomationController._finished'), wait_timeout)
          completed = True
        except util.TimeoutException:
          pass
        # The try/except is a workaround for crbug.com/368107.
        try:
          new_tab.Close()
        except (exceptions.TabCrashException, Exception):
          print 'Tab crashed while closing chrome://gpucrash'
        if not completed:
          raise page_test.Failure(
              'Test didn\'t complete (no context lost event?)')
        if not tab.EvaluateJavaScript(
          'window.domAutomationController._succeeded'):
          raise page_test.Failure(
            'Test failed (context not restored properly?)')
    elif page.force_garbage_collection:
      # Try to corce GC to clean up any contexts not attached to the page.
      # This method seem unreliable, so the page will also attempt to force
      # GC through excessive allocations.
      tab.CollectGarbage()
      completed = False
      try:
        print "Waiting for page to finish."
        util.WaitFor(lambda: tab.EvaluateJavaScript(
            'window.domAutomationController._finished'), wait_timeout)
        completed = True
      except util.TimeoutException:
        pass

      if not completed:
        raise page_test.Failure(
            'Test didn\'t complete (no context restored event?)')
      if not tab.EvaluateJavaScript(
        'window.domAutomationController._succeeded'):
        raise page_test.Failure(
          'Test failed (context not restored properly?)')
    else:
      completed = False
      try:
        print "Waiting for page to finish."
        util.WaitFor(lambda: tab.EvaluateJavaScript(
            'window.domAutomationController._finished'), wait_timeout)
        completed = True
      except util.TimeoutException:
        pass

      if not completed:
        raise page_test.Failure('Test didn\'t complete')
      if not tab.EvaluateJavaScript(
        'window.domAutomationController._succeeded'):
        raise page_test.Failure('Test failed')

class WebGLContextLostFromGPUProcessExitPage(page.Page):
  def __init__(self, page_set, base_dir):
    super(WebGLContextLostFromGPUProcessExitPage, self).__init__(
      url='file://webgl.html?query=kill_after_notification',
      page_set=page_set,
      base_dir=base_dir,
      name='ContextLost.WebGLContextLostFromGPUProcessExit')
    self.script_to_evaluate_on_commit = harness_script
    self.kill_gpu_process = True
    self.number_of_gpu_process_kills = 1
    self.force_garbage_collection = False

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'window.domAutomationController._loaded')


class WebGLContextLostFromLoseContextExtensionPage(page.Page):
  def __init__(self, page_set, base_dir):
    super(WebGLContextLostFromLoseContextExtensionPage, self).__init__(
      url='file://webgl.html?query=WEBGL_lose_context',
      page_set=page_set,
      base_dir=base_dir,
      name='ContextLost.WebGLContextLostFromLoseContextExtension')
    self.script_to_evaluate_on_commit = harness_script
    self.kill_gpu_process = False
    self.force_garbage_collection = False

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'window.domAutomationController._finished')

class WebGLContextLostFromQuantityPage(page.Page):
  def __init__(self, page_set, base_dir):
    super(WebGLContextLostFromQuantityPage, self).__init__(
      url='file://webgl.html?query=forced_quantity_loss',
      page_set=page_set,
      base_dir=base_dir,
      name='ContextLost.WebGLContextLostFromQuantity')
    self.script_to_evaluate_on_commit = harness_script
    self.kill_gpu_process = False
    self.force_garbage_collection = True

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'window.domAutomationController._loaded')

class WebGLContextLostFromSelectElementPage(page.Page):
  def __init__(self, page_set, base_dir):
    super(WebGLContextLostFromSelectElementPage, self).__init__(
      url='file://webgl_with_select_element.html',
      page_set=page_set,
      base_dir=base_dir,
      name='ContextLost.WebGLContextLostFromSelectElement')
    self.script_to_evaluate_on_commit = harness_script
    self.kill_gpu_process = False
    self.force_garbage_collection = False

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'window.domAutomationController._loaded')

class ContextLost(benchmark_module.Benchmark):
  enabled = True
  test = _ContextLostValidator
  # For the record, this would have been another way to get the pages
  # to repeat. pageset_repeat would be another option.
  # options = {'page_repeat': 5}
  def CreatePageSet(self, options):
    ps = page_set.PageSet(
      file_path=data_path,
      user_agent_type='desktop',
      serving_dirs=set(['']))
    ps.AddPage(WebGLContextLostFromGPUProcessExitPage(ps, ps.base_dir))
    ps.AddPage(WebGLContextLostFromLoseContextExtensionPage(ps, ps.base_dir))
    ps.AddPage(WebGLContextLostFromQuantityPage(ps, ps.base_dir))
    ps.AddPage(WebGLContextLostFromSelectElementPage(ps, ps.base_dir))
    return ps
