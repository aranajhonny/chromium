# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import sys

from telemetry.unittest import gtest_output_formatter
from telemetry.unittest import simple_mock


class DummyException(Exception):
  pass
try:
  raise DummyException('Dummy exception')
except DummyException:
  DUMMY_EXCEPTION = sys.exc_info()


class TestFoo(unittest.TestCase):
  # Test method doesn't have test- prefix intentionally. This is so that
  # run_test script won't run this test.
  def runTezt(self):
    pass


class TestOutputStream(object):
  def __init__(self):
    self._output_data = []

  @property
  def output_data(self):
    return ''.join(self._output_data)

  def write(self, data):
    self._output_data.append(data)

  def flush(self):
    pass


class TestResultWithSuccesses(unittest.TestResult):
  def __init__(self):
    super(TestResultWithSuccesses, self).__init__()
    self.successes = []

  def addSuccess(self, test):
    super(TestResultWithSuccesses, self).addSuccess(test)
    self.successes.append(test)


class GTestOutputFormatterTest(unittest.TestCase):
  def setUp(self):
    super(GTestOutputFormatterTest, self).setUp()
    self._stream = TestOutputStream()
    self._formatter = gtest_output_formatter.GTestOutputFormatter(self._stream)

    self._mock_timer = simple_mock.MockTimer()
    self._real_time_time = gtest_output_formatter.time.time
    gtest_output_formatter.time.time = self._mock_timer.GetTime

  def tearDown(self):
    gtest_output_formatter.time.time = self._real_time_time

  def testTestSuiteWithWrapperSuite(self):
    suite = unittest.TestSuite()
    suite.addTest(unittest.TestSuite())
    self._formatter.StartTestSuite(suite)
    self._formatter.StopTestSuite(suite)

    self.assertEqual(self._stream.output_data, '')

  def testTestSuiteWithTestCase(self):
    suite = unittest.TestSuite()
    suite.addTest(TestFoo(methodName='runTezt'))
    self._formatter.StartTestSuite(suite)
    self._mock_timer.SetTime(0.042)
    self._formatter.StopTestSuite(suite)

    expected = ('[----------] 1 test\n'
                '[----------] 1 test (42 ms total)\n\n')
    self.assertEqual(self._stream.output_data, expected)

  def testCaseFailure(self):
    test = TestFoo(methodName='runTezt')
    self._formatter.StartTest(test)
    self._mock_timer.SetTime(0.042)
    self._formatter.Failure(test, DUMMY_EXCEPTION)

    expected = ('[ RUN      ] gtest_output_formatter_unittest.TestFoo.runTezt\n'
                '[  FAILED  ] gtest_output_formatter_unittest.TestFoo.runTezt '
                '(42 ms)\n')
    self.assertEqual(self._stream.output_data, expected)

  def testCaseSuccess(self):
    test = TestFoo(methodName='runTezt')
    self._formatter.StartTest(test)
    self._mock_timer.SetTime(0.042)
    self._formatter.Success(test)

    expected = ('[ RUN      ] gtest_output_formatter_unittest.TestFoo.runTezt\n'
                '[       OK ] gtest_output_formatter_unittest.TestFoo.runTezt '
                '(42 ms)\n')
    self.assertEqual(self._stream.output_data, expected)

  def testStopTestRun(self):
    result = TestResultWithSuccesses()
    self._formatter.StopTestRun(result)

    expected = '[  PASSED  ] 0 tests.\n\n'
    self.assertEqual(self._stream.output_data, expected)

  def testStopTestRunWithFailureAndSuccess(self):
    test = TestFoo(methodName='runTezt')
    result = TestResultWithSuccesses()
    result.addSuccess(test)
    result.addFailure(test, DUMMY_EXCEPTION)
    self._formatter.StopTestRun(result)

    expected = (
        '[  PASSED  ] 1 test.\n'
        '[  FAILED  ] 1 test, listed below:\n'
        '[  FAILED  ]  gtest_output_formatter_unittest.TestFoo.runTezt\n\n'
        '1 FAILED TEST\n\n')
    self.assertEqual(self._stream.output_data, expected)
