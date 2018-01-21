# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import os
import re

from telemetry import decorators
from telemetry.core.platform import power_monitor

CPU_PATH = '/sys/devices/system/cpu/'

class SysfsPowerMonitor(power_monitor.PowerMonitor):
  """PowerMonitor that relies on sysfs to monitor CPU statistics on several
  different platforms.
  """
  def __init__(self, platform):
    """Constructor.

    Args:
        platform: A SysfsPlatform object.

    Attributes:
        _browser: The browser to monitor.
        _cpus: A list of the CPUs on the target device.
        _end_time: The time the test stopped monitoring power.
        _final_cstate: The c-state residency times after the test.
        _final_freq: The CPU frequency times after the test.
        _initial_cstate: The c-state residency times before the test.
        _initial_freq: The CPU frequency times before the test.
        _platform: A SysfsPlatform object associated with the target platform.
        _start_time: The time the test started monitoring power.
    """
    super(SysfsPowerMonitor, self).__init__()
    self._browser = None
    self._cpus = filter(lambda x: re.match(r'^cpu[0-9]+', x),
                        platform.RunShellCommand('ls %s' % CPU_PATH).split())
    self._end_time = None
    self._final_cstate = None
    self._final_freq = None
    self._initial_cstate = None
    self._initial_freq = None
    self._platform = platform
    self._start_time = None

  @decorators.Cache
  def CanMonitorPower(self):
    return bool(self._platform.RunShellCommand(
        'if [ -e %s ]; then echo true; fi' % CPU_PATH))

  def StartMonitoringPower(self, browser):
    assert not self._browser, 'Must call StopMonitoringPower().'
    self._browser = browser
    self._start_time = int(self._platform.RunShellCommand('date +%s'))
    if self.CanMonitorPower():
      self._initial_freq = self.GetCpuFreq()
      self._initial_cstate = self.GetCpuState()

  def StopMonitoringPower(self):
    assert self._browser, 'StartMonitoringPower() not called.'
    try:
      self._end_time = int(self._platform.RunShellCommand('date +%s'))
      out = {}
      if self.CanMonitorPower():
        self._final_freq = self.GetCpuFreq()
        self._final_cstate = self.GetCpuState()
        frequencies = SysfsPowerMonitor.ComputeCpuStats(
            SysfsPowerMonitor.ParseFreqSample(self._initial_freq),
            SysfsPowerMonitor.ParseFreqSample(self._final_freq))
        start_cstate = self._platform.ParseStateSample(
            self._initial_cstate, self._start_time)
        end_cstate = self._platform.ParseStateSample(
            self._final_cstate, self._end_time)
        cstates = SysfsPowerMonitor.ComputeCpuStats(start_cstate, end_cstate)
        for cpu in frequencies:
          out[cpu] = {'frequency_percent': frequencies[cpu]}
          out[cpu]['cstate_residency_percent'] = cstates[cpu]
      return out
    finally:
      self._browser = None

  def GetCpuState(self):
    """Retrieve CPU c-state residency times from the device.

    Returns:
        Dictionary containing c-state residency times for each CPU.
    """
    stats = {}
    for cpu in self._cpus:
      cpu_state_path = os.path.join(CPU_PATH, cpu, 'cpuidle/state*')
      stats[cpu] = self._platform.RunShellCommand(
          'cat %s %s %s' % (os.path.join(cpu_state_path, 'name'),
          os.path.join(cpu_state_path, 'time'),
          os.path.join(cpu_state_path, 'latency')))
    return stats

  def GetCpuFreq(self):
    """Retrieve CPU frequency times from the device.

    Returns:
        Dictionary containing frequency times for each CPU.
    """
    stats = {}
    for cpu in self._cpus:
      cpu_freq_path = os.path.join(
          CPU_PATH, cpu, 'cpufreq/stats/time_in_state')
      stats[cpu] = self._platform.RunShellCommand('cat %s' % cpu_freq_path)
    return stats

  @staticmethod
  def ParseFreqSample(sample):
    """Parse a single frequency sample.

    Args:
        sample: The single sample of frequency data to be parsed.

    Returns:
        A dictionary associating a frequency with a time.
    """
    sample_stats = {}
    for cpu in sample:
      frequencies = {}
      for line in sample[cpu].splitlines():
        pair = line.split()
        freq = int(pair[0]) * 10 ** 3
        timeunits = int(pair[1])
        if freq in frequencies:
          frequencies[freq] += timeunits
        else:
          frequencies[freq] = timeunits
      sample_stats[cpu] = frequencies
    return sample_stats

  @staticmethod
  def ComputeCpuStats(initial, final):
    """Parse the CPU c-state and frequency values saved during monitoring.

    Args:
        initial: The parsed dictionary of initial statistics to be converted
        into percentages.
        final: The parsed dictionary of final statistics to be converted
        into percentages.

    Returns:
        Dictionary containing percentages for each CPU as well as an average
        across all CPUs.
    """
    cpu_stats = {}
    # Each core might have different states or frequencies, so keep track of
    # the total time in a state or frequency and how many cores report a time.
    cumulative_times = collections.defaultdict(lambda: (0, 0))
    for cpu in initial:
      current_cpu = {}
      total = 0
      for state in initial[cpu]:
        current_cpu[state] = final[cpu][state] - initial[cpu][state]
        total += current_cpu[state]
      for state in current_cpu:
        current_cpu[state] /= (float(total) / 100.0)
        # Calculate the average c-state residency across all CPUs.
        time, count = cumulative_times[state]
        cumulative_times[state] = (time + current_cpu[state], count + 1)
      cpu_stats[cpu] = current_cpu
    average = {}
    for state in cumulative_times:
      time, count = cumulative_times[state]
      average[state] = time / float(count)
    cpu_stats['whole_package'] = average
    return cpu_stats
