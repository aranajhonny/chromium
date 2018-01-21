// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_PAGINATION_MODEL_H_
#define UI_APP_LIST_PAGINATION_MODEL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/app_list/app_list_export.h"
#include "ui/gfx/animation/animation_delegate.h"

namespace gfx {
class SlideAnimation;
}

namespace app_list {

class PaginationModelObserver;

// A simple pagination model that consists of two numbers: the total pages and
// the currently selected page. The model is a single selection model that at
// the most one page can become selected at any time.
class APP_LIST_EXPORT PaginationModel : public gfx::AnimationDelegate {
 public:
  // Holds info for transition animation and touch scroll.
  struct Transition {
    Transition(int target_page, double progress)
        : target_page(target_page),
          progress(progress) {
    }

    bool Equals(const Transition& rhs) const {
      return target_page == rhs.target_page && progress == rhs.progress;
    }

    // Target page for the transition or -1 if there is no target page. For
    // page switcher, this is the target selected page. For touch scroll,
    // this is usually the previous or next page (or -1 when there is no
    // previous or next page).
    int target_page;

    // A [0, 1] progress indicates how much of the current page is being
    // transitioned.
    double progress;
  };

  PaginationModel();
  virtual ~PaginationModel();

  void SetTotalPages(int total_pages);

  // Selects a page. |animate| is true if the transition should be animated.
  void SelectPage(int page, bool animate);

  // Selects a page by relative |delta|.
  void SelectPageRelative(int delta, bool animate);

  // Immediately completes all queued animations, jumping directly to the final
  // target page.
  void FinishAnimation();

  void SetTransition(const Transition& transition);
  void SetTransitionDurations(int duration_ms, int overscroll_duration_ms);

  // Starts a scroll transition. If there is a running transition animation,
  // cancels it but keeps the transition info.
  void StartScroll();

  // Updates transition progress from |delta|. |delta| > 0 means transit to
  // previous page (moving pages to the right). |delta| < 0 means transit
  // to next page (moving pages to the left).
  void UpdateScroll(double delta);

  // Finishes the current scroll transition if |cancel| is false. Otherwise,
  // reverses it.
  void EndScroll(bool cancel);

  // Returns true if current transition is being reverted.
  bool IsRevertingCurrentTransition() const;

  void AddObserver(PaginationModelObserver* observer);
  void RemoveObserver(PaginationModelObserver* observer);

  int total_pages() const { return total_pages_; }
  int selected_page() const { return selected_page_; }
  const Transition& transition() const { return transition_; }

  bool is_valid_page(int page) const {
    return page >= 0 && page < total_pages_;
  }

  bool has_transition() const {
    return transition_.target_page != -1 || transition_.progress != 0;
  }

  // Gets the page that the animation will eventually land on. If there is no
  // active animation, just returns selected_page().
  int SelectedTargetPage() const;

 private:
  void NotifySelectedPageChanged(int old_selected, int new_selected);
  void NotifyTransitionStarted();
  void NotifyTransitionChanged();

  void clear_transition() {
    SetTransition(Transition(-1, 0));
  }

  // Calculates a target page number by combining current page and |delta|.
  // When there is no transition, current page is the currently selected page.
  // If there is a transition, current page is the transition target page or the
  // pending transition target page. When current page + |delta| goes beyond
  // valid range and |selected_page_| is at the range ends, invalid page number
  // -1 or |total_pages_| is returned to indicate the situation.
  int CalculateTargetPage(int delta) const;

  void StartTransitionAnimation(const Transition& transition);
  void ResetTransitionAnimation();

  // gfx::AnimationDelegate overrides:
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE;

  int total_pages_;
  int selected_page_;

  Transition transition_;

  // Pending selected page when SelectedPage is called during a transition. If
  // multiple SelectPage is called while a transition is in progress, only the
  // last target page is remembered here.
  int pending_selected_page_;

  scoped_ptr<gfx::SlideAnimation> transition_animation_;
  int transition_duration_ms_;  // Transition duration in millisecond.
  int overscroll_transition_duration_ms_;

  int last_overscroll_target_page_;
  base::TimeTicks last_overscroll_animation_start_time_;

  ObserverList<PaginationModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(PaginationModel);
};

}  // namespace app_list

#endif  // UI_APP_LIST_PAGINATION_MODEL_H_
