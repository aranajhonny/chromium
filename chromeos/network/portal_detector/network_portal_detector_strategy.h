// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_STRATEGY_H_
#define CHROMEOS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_STRATEGY_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "chromeos/chromeos_export.h"
#include "net/base/backoff_entry.h"

namespace chromeos {

class CHROMEOS_EXPORT PortalDetectorStrategy : protected net::BackoffEntry {
 public:
  enum StrategyId {
    STRATEGY_ID_LOGIN_SCREEN,
    STRATEGY_ID_ERROR_SCREEN,
    STRATEGY_ID_SESSION
  };

  class Delegate {
   public:
    virtual ~Delegate() {}

    // Returns number of attempts in a row with NO RESPONSE result.
    // If last detection attempt has different result, returns 0.
    virtual int NoResponseResultCount() = 0;

    // Returns time when current attempt was started.
    virtual base::TimeTicks AttemptStartTime() = 0;

    // Returns current TimeTicks.
    virtual base::TimeTicks GetCurrentTimeTicks() = 0;
  };

  virtual ~PortalDetectorStrategy();

  static scoped_ptr<PortalDetectorStrategy> CreateById(StrategyId id);

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // Returns delay before next detection attempt. This delay is needed
  // to separate detection attempts in time.
  base::TimeDelta GetDelayTillNextAttempt();

  // Returns timeout for the next detection attempt.
  base::TimeDelta GetNextAttemptTimeout();

  virtual StrategyId Id() const = 0;

  // Resets strategy to the initial state.
  void Reset();

  // Should be called when portal detection is completed and timeout before next
  // attempt should be adjusted.
  void OnDetectionCompleted();

 protected:
  PortalDetectorStrategy();

  // net::BackoffEntry overrides:
  virtual base::TimeTicks ImplGetTimeNow() const OVERRIDE;

  // Interface for subclasses:
  virtual base::TimeDelta GetNextAttemptTimeoutImpl();

  Delegate* delegate_;
  net::BackoffEntry::Policy policy_;

 private:
  friend class NetworkPortalDetectorImplTest;
  friend class NetworkPortalDetectorImplBrowserTest;

  static void set_delay_till_next_attempt_for_testing(
      const base::TimeDelta& timeout) {
    delay_till_next_attempt_for_testing_ = timeout;
    delay_till_next_attempt_for_testing_initialized_ = true;
  }

  static void set_next_attempt_timeout_for_testing(
      const base::TimeDelta& timeout) {
    next_attempt_timeout_for_testing_ = timeout;
    next_attempt_timeout_for_testing_initialized_ = true;
  }

  static void reset_fields_for_testing() {
    delay_till_next_attempt_for_testing_initialized_ = false;
    next_attempt_timeout_for_testing_initialized_ = false;
  }

  // Test delay before detection attempt, used by unit tests.
  static base::TimeDelta delay_till_next_attempt_for_testing_;

  // True when |min_time_between_attempts_for_testing_| is initialized.
  static bool delay_till_next_attempt_for_testing_initialized_;

  // Test timeout for a detection attempt, used by unit tests.
  static base::TimeDelta next_attempt_timeout_for_testing_;

  // True when |next_attempt_timeout_for_testing_| is initialized.
  static bool next_attempt_timeout_for_testing_initialized_;

  DISALLOW_COPY_AND_ASSIGN(PortalDetectorStrategy);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_STRATEGY_H_
