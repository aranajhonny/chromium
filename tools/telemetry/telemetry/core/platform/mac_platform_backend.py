# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ctypes
import os
import time
try:
  import resource  # pylint: disable=F0401
except ImportError:
  resource = None  # Not available on all platforms

from telemetry import decorators
from telemetry.core.platform import platform_backend
from telemetry.core.platform import posix_platform_backend
from telemetry.core.platform.power_monitor import powermetrics_power_monitor


class MacPlatformBackend(posix_platform_backend.PosixPlatformBackend):
  def __init__(self):
    super(MacPlatformBackend, self).__init__()
    self.libproc = None
    self.power_monitor_ = powermetrics_power_monitor.PowerMetricsPowerMonitor(
        self)

  def StartRawDisplayFrameRateMeasurement(self):
    raise NotImplementedError()

  def StopRawDisplayFrameRateMeasurement(self):
    raise NotImplementedError()

  def GetRawDisplayFrameRateMeasurements(self):
    raise NotImplementedError()

  def IsThermallyThrottled(self):
    raise NotImplementedError()

  def HasBeenThermallyThrottled(self):
    raise NotImplementedError()

  def _GetIdleWakeupCount(self, pid):
    top_output = self._GetTopOutput(pid, ['idlew'])

    # Sometimes top won't return anything here, just ignore such cases -
    # crbug.com/354812 .
    if top_output[-2] != 'IDLEW':
      return 0
    # Numbers reported by top may have a '+' appended.
    wakeup_count = int(top_output[-1].strip('+ '))
    return wakeup_count

  def GetCpuStats(self, pid):
    """Return current cpu processing time of pid in seconds."""
    class ProcTaskInfo(ctypes.Structure):
      """Struct for proc_pidinfo() call."""
      _fields_ = [("pti_virtual_size", ctypes.c_uint64),
                  ("pti_resident_size", ctypes.c_uint64),
                  ("pti_total_user", ctypes.c_uint64),
                  ("pti_total_system", ctypes.c_uint64),
                  ("pti_threads_user", ctypes.c_uint64),
                  ("pti_threads_system", ctypes.c_uint64),
                  ("pti_policy", ctypes.c_int32),
                  ("pti_faults", ctypes.c_int32),
                  ("pti_pageins", ctypes.c_int32),
                  ("pti_cow_faults", ctypes.c_int32),
                  ("pti_messages_sent", ctypes.c_int32),
                  ("pti_messages_received", ctypes.c_int32),
                  ("pti_syscalls_mach", ctypes.c_int32),
                  ("pti_syscalls_unix", ctypes.c_int32),
                  ("pti_csw", ctypes.c_int32),
                  ("pti_threadnum", ctypes.c_int32),
                  ("pti_numrunning", ctypes.c_int32),
                  ("pti_priority", ctypes.c_int32)]
      PROC_PIDTASKINFO = 4
      def __init__(self):
        self.size = ctypes.sizeof(self)
        super(ProcTaskInfo, self).__init__()

    proc_info = ProcTaskInfo()
    if not self.libproc:
      self.libproc = ctypes.CDLL(ctypes.util.find_library('libproc'))
    self.libproc.proc_pidinfo(pid, proc_info.PROC_PIDTASKINFO, 0,
                              ctypes.byref(proc_info), proc_info.size)

    # Convert nanoseconds to seconds.
    cpu_time = (proc_info.pti_total_user / 1000000000.0 +
                proc_info.pti_total_system / 1000000000.0)
    results = {'CpuProcessTime': cpu_time,
               'ContextSwitches': proc_info.pti_csw}

    # top only reports idle wakeup count starting from OS X 10.9.
    if self.GetOSVersionName() >= platform_backend.MAVERICKS:
      results.update({'IdleWakeupCount': self._GetIdleWakeupCount(pid)})
    return results

  def GetCpuTimestamp(self):
    """Return current timestamp in seconds."""
    return {'TotalTime': time.time()}

  def GetSystemCommitCharge(self):
    vm_stat = self._RunCommand(['vm_stat'])
    for stat in vm_stat.splitlines():
      key, value = stat.split(':')
      if key == 'Pages active':
        pages_active = int(value.strip()[:-1])  # Strip trailing '.'
        return pages_active * resource.getpagesize() / 1024
    return 0

  @decorators.Cache
  def GetSystemTotalPhysicalMemory(self):
    return int(self._RunCommand(['sysctl', '-n', 'hw.memsize']))

  def PurgeUnpinnedMemory(self):
    # TODO(pliard): Implement this.
    pass

  def GetMemoryStats(self, pid):
    rss_vsz = self._GetPsOutput(['rss', 'vsz'], pid)
    if rss_vsz:
      rss, vsz = rss_vsz[0].split()
      return {'VM': 1024 * int(vsz),
              'WorkingSetSize': 1024 * int(rss)}
    return {}

  def GetOSName(self):
    return 'mac'

  @decorators.Cache
  def GetOSVersionName(self):
    os_version = os.uname()[2]

    if os_version.startswith('9.'):
      return platform_backend.LEOPARD
    if os_version.startswith('10.'):
      return platform_backend.SNOWLEOPARD
    if os_version.startswith('11.'):
      return platform_backend.LION
    if os_version.startswith('12.'):
      return platform_backend.MOUNTAINLION
    if os_version.startswith('13.'):
      return platform_backend.MAVERICKS

    raise NotImplementedError('Unknown mac version %s.' % os_version)

  def CanFlushIndividualFilesFromSystemCache(self):
    return False

  def FlushEntireSystemCache(self):
    mavericks_or_later = self.GetOSVersionName() >= platform_backend.MAVERICKS
    p = self.LaunchApplication('purge', elevate_privilege=mavericks_or_later)
    p.communicate()
    assert p.returncode == 0, 'Failed to flush system cache'

  def CanMonitorPower(self):
    return self.power_monitor_.CanMonitorPower()

  def CanMeasurePerApplicationPower(self):
    return self.power_monitor_.CanMeasurePerApplicationPower()

  def StartMonitoringPower(self, browser):
    self.power_monitor_.StartMonitoringPower(browser)

  def StopMonitoringPower(self):
    return self.power_monitor_.StopMonitoringPower()
