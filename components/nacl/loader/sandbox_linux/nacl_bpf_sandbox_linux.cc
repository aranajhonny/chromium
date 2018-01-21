// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/sandbox_linux/nacl_bpf_sandbox_linux.h"

#include "build/build_config.h"

#if defined(USE_SECCOMP_BPF)

#include <errno.h>
#include <signal.h>
#include <sys/ptrace.h>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"

#include "content/public/common/sandbox_init.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_policy.h"
#include "sandbox/linux/services/linux_syscalls.h"

#endif  // defined(USE_SECCOMP_BPF)

namespace nacl {

#if defined(USE_SECCOMP_BPF)

namespace {

class NaClBPFSandboxPolicy : public sandbox::SandboxBPFPolicy {
 public:
  NaClBPFSandboxPolicy()
      : baseline_policy_(content::GetBPFSandboxBaselinePolicy()) {}
  virtual ~NaClBPFSandboxPolicy() {}

  virtual sandbox::ErrorCode EvaluateSyscall(
      sandbox::SandboxBPF* sandbox_compiler,
      int system_call_number) const OVERRIDE;

 private:
  scoped_ptr<sandbox::SandboxBPFPolicy> baseline_policy_;
  DISALLOW_COPY_AND_ASSIGN(NaClBPFSandboxPolicy);
};

sandbox::ErrorCode NaClBPFSandboxPolicy::EvaluateSyscall(
    sandbox::SandboxBPF* sb, int sysno) const {
  DCHECK(baseline_policy_);
  switch (sysno) {
    // TODO(jln): NaCl's GDB debug stub uses the following socket system calls,
    // see if it can be restricted a bit.
#if defined(__x86_64__) || defined(__arm__)
    // transport_common.cc needs this.
    case __NR_accept:
    case __NR_setsockopt:
#elif defined(__i386__)
    case __NR_socketcall:
#endif
    // trusted/service_runtime/linux/thread_suspension.c needs sigwait() and is
    // used by NaCl's GDB debug stub.
    case __NR_rt_sigtimedwait:
#if defined(__i386__)
    // Needed on i386 to set-up the custom segments.
    case __NR_modify_ldt:
#endif
    // NaClAddrSpaceBeforeAlloc needs prlimit64.
    case __NR_prlimit64:
    // NaCl uses custom signal stacks.
    case __NR_sigaltstack:
    // Below is fairly similar to the policy for a Chromium renderer.
#if defined(__i386__) || defined(__x86_64__)
    case __NR_getrlimit:
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR_ugetrlimit:
#endif
    // NaCl runtime exposes clock_getres to untrusted code.
    case __NR_clock_getres:
    // NaCl runtime uses flock to simulate POSIX behavior for pwrite.
    case __NR_flock:
    case __NR_pread64:
    case __NR_pwrite64:
    case __NR_sched_get_priority_max:
    case __NR_sched_get_priority_min:
    case __NR_sched_getaffinity:
    case __NR_sched_getparam:
    case __NR_sched_getscheduler:
    case __NR_sched_setscheduler:
    case __NR_setpriority:
    case __NR_sysinfo:
    // __NR_times needed as clock() is called by CommandBufferHelper, which is
    // used by NaCl applications that use Pepper's 3D interfaces.
    // See crbug.com/264856 for details.
    case __NR_times:
    case __NR_uname:
      return sandbox::ErrorCode(sandbox::ErrorCode::ERR_ALLOWED);
    case __NR_ioctl:
    case __NR_ptrace:
      return sandbox::ErrorCode(EPERM);
    default:
      return baseline_policy_->EvaluateSyscall(sb, sysno);
  }
  NOTREACHED();
  // GCC wants this.
  return sandbox::ErrorCode(EPERM);
}

void RunSandboxSanityChecks() {
  errno = 0;
  // Make a ptrace request with an invalid PID.
  long ptrace_ret = ptrace(PTRACE_PEEKUSER, -1 /* pid */, NULL, NULL);
  CHECK_EQ(-1, ptrace_ret);
  // Without the sandbox on, this ptrace call would ESRCH instead.
  CHECK_EQ(EPERM, errno);
}

}  // namespace

#else

#if !defined(ARCH_CPU_MIPS_FAMILY)
#error "Seccomp-bpf disabled on supported architecture!"
#endif  // !defined(ARCH_CPU_MIPS_FAMILY)

#endif  // defined(USE_SECCOMP_BPF)

bool InitializeBPFSandbox() {
#if defined(USE_SECCOMP_BPF)
  bool sandbox_is_initialized = content::InitializeSandbox(
      scoped_ptr<sandbox::SandboxBPFPolicy>(new NaClBPFSandboxPolicy));
  if (sandbox_is_initialized) {
    RunSandboxSanityChecks();
    return true;
  }
#endif  // defined(USE_SECCOMP_BPF)
  return false;
}

}  // namespace nacl