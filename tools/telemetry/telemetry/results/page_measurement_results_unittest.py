# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from telemetry import perf_tests_helper
from telemetry.page import page_set
from telemetry.results import page_measurement_results
from telemetry.value import histogram
from telemetry.value import scalar


def _MakePageSet():
  ps = page_set.PageSet(file_path=os.path.dirname(__file__))
  ps.AddPageWithDefaultRunNavigate("http://www.bar.com/")
  ps.AddPageWithDefaultRunNavigate("http://www.baz.com/")
  ps.AddPageWithDefaultRunNavigate("http://www.foo.com/")
  return ps

class NonPrintingPageMeasurementResults(
    page_measurement_results.PageMeasurementResults):
  def __init__(self):
    super(NonPrintingPageMeasurementResults, self).__init__()

  def _PrintPerfResult(self, *args):
    pass

class SummarySavingPageMeasurementResults(
    page_measurement_results.PageMeasurementResults):
  def __init__(self):
    super(SummarySavingPageMeasurementResults, self).__init__()
    self.results = []

  def _PrintPerfResult(self, *args):
    res = perf_tests_helper.PrintPerfResult(*args, print_to_stdout=False)
    self.results.append(res)

class PageMeasurementResultsTest(unittest.TestCase):
  def setUp(self):
    self._page_set = _MakePageSet()

  @property
  def pages(self):
    return self._page_set.pages

  def test_basic(self):
    results = NonPrintingPageMeasurementResults()
    results.WillMeasurePage(self.pages[0])
    results.AddValue(scalar.ScalarValue(self.pages[0], 'a', 'seconds', 3))
    results.DidMeasurePage()

    results.WillMeasurePage(self.pages[1])
    results.AddValue(scalar.ScalarValue(self.pages[1], 'a', 'seconds', 3))
    results.DidMeasurePage()

    results.PrintSummary()

    values = results.FindPageSpecificValuesForPage(self.pages[0], 'a')
    self.assertEquals(1, len(values))
    v = values[0]
    self.assertEquals(v.name, 'a')
    self.assertEquals(v.page, self.pages[0])

    values = results.FindAllPageSpecificValuesNamed('a')
    assert len(values) == 2

  def test_url_is_invalid_value(self):
    results = NonPrintingPageMeasurementResults()
    results.WillMeasurePage(self.pages[0])
    self.assertRaises(
      AssertionError,
      lambda: results.AddValue(scalar.ScalarValue(
          self.pages[0], 'url', 'string', 'foo')))

  def test_add_summary_value_with_page_specified(self):
    results = NonPrintingPageMeasurementResults()
    results.WillMeasurePage(self.pages[0])
    self.assertRaises(
      AssertionError,
      lambda: results.AddSummaryValue(scalar.ScalarValue(self.pages[0],
                                                         'a', 'units', 3)))

  def test_unit_change(self):
    results = NonPrintingPageMeasurementResults()
    results.WillMeasurePage(self.pages[0])
    results.AddValue(scalar.ScalarValue(self.pages[0], 'a', 'seconds', 3))
    results.DidMeasurePage()

    results.WillMeasurePage(self.pages[1])
    self.assertRaises(
      AssertionError,
      lambda: results.AddValue(scalar.ScalarValue(
          self.pages[1], 'a', 'foobgrobbers', 3)))

  def test_type_change(self):
    results = NonPrintingPageMeasurementResults()
    results.WillMeasurePage(self.pages[0])
    results.AddValue(scalar.ScalarValue(self.pages[0], 'a', 'seconds', 3))
    results.DidMeasurePage()

    results.WillMeasurePage(self.pages[1])
    self.assertRaises(
      AssertionError,
      lambda: results.AddValue(histogram.HistogramValue(
          self.pages[1], 'a', 'seconds',
          raw_value_json='{"buckets": [{"low": 1, "high": 2, "count": 1}]}')))

  def test_basic_summary_all_pages_fail(self):
    """If all pages fail, no summary is printed."""
    results = SummarySavingPageMeasurementResults()
    results.WillMeasurePage(self.pages[0])
    results.AddValue(scalar.ScalarValue(self.pages[0], 'a', 'seconds', 3))
    results.DidMeasurePage()
    results.AddFailureMessage(self.pages[0], 'message')

    results.WillMeasurePage(self.pages[1])
    results.AddValue(scalar.ScalarValue(self.pages[1], 'a', 'seconds', 7))
    results.DidMeasurePage()
    results.AddFailureMessage(self.pages[1], 'message')

    results.PrintSummary()
    self.assertEquals(results.results, [])

  def test_get_successful_page_values_merged_no_failures(self):
    results = SummarySavingPageMeasurementResults()
    results.WillMeasurePage(self.pages[0])
    results.AddValue(scalar.ScalarValue(self.pages[0], 'a', 'seconds', 3))
    self.assertEquals(1, len(results.page_specific_values_for_current_page))
    results.DidMeasurePage()
    self.assertRaises(
        AssertionError,
        lambda: results.page_specific_values_for_current_page)

  def test_get_all_values_for_successful_pages(self):
    results = SummarySavingPageMeasurementResults()
    results.WillMeasurePage(self.pages[0])
    results.AddValue(scalar.ScalarValue(self.pages[0], 'a', 'seconds', 3))
    results.DidMeasurePage()

    results.WillMeasurePage(self.pages[1])
    results.AddValue(scalar.ScalarValue(self.pages[1], 'a', 'seconds', 3))
    results.DidMeasurePage()

    results.WillMeasurePage(self.pages[2])
    results.AddValue(scalar.ScalarValue(self.pages[2], 'a', 'seconds', 3))
    results.DidMeasurePage()

    values = results.all_page_specific_values
    self.assertEquals(3, len(values))
    self.assertEquals([self.pages[0], self.pages[1], self.pages[2]],
                      [v.page for v in values])

  def test_get_all_values_for_successful_pages_one_page_fails(self):
    results = SummarySavingPageMeasurementResults()
    results.WillMeasurePage(self.pages[0])
    results.AddValue(scalar.ScalarValue(self.pages[0], 'a', 'seconds', 3))
    results.DidMeasurePage()

    results.WillMeasurePage(self.pages[1])
    results.AddFailureMessage(self.pages[1], "Failure")
    results.DidMeasurePage()

    results.WillMeasurePage(self.pages[2])
    results.AddValue(scalar.ScalarValue(self.pages[2], 'a', 'seconds', 3))
    results.DidMeasurePage()

    values = results.all_page_specific_values
    self.assertEquals(2, len(values))
    self.assertEquals([self.pages[0], self.pages[2]],
                      [v.page for v in values])
