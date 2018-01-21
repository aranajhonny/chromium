// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"

#include "ash/frame/caption_buttons/frame_caption_button.h"
#include "ash/test/ash_test_base.h"
#include "grit/ash_resources.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

class TestWidgetDelegate : public views::WidgetDelegateView {
 public:
  TestWidgetDelegate(bool can_maximize) : can_maximize_(can_maximize) {
  }
  virtual ~TestWidgetDelegate() {
  }

  virtual bool CanMaximize() const OVERRIDE {
    return can_maximize_;
  }

 private:
  bool can_maximize_;

  DISALLOW_COPY_AND_ASSIGN(TestWidgetDelegate);
};

}  // namespace

class FrameCaptionButtonContainerViewTest : public ash::test::AshTestBase {
 public:
  enum MaximizeAllowed {
    MAXIMIZE_ALLOWED,
    MAXIMIZE_DISALLOWED
  };

  FrameCaptionButtonContainerViewTest() {
  }

  virtual ~FrameCaptionButtonContainerViewTest() {
  }

  // Creates a widget which allows maximizing based on |maximize_allowed|.
  // The caller takes ownership of the returned widget.
  views::Widget* CreateTestWidget(
      MaximizeAllowed maximize_allowed) WARN_UNUSED_RESULT {
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params;
    params.delegate = new TestWidgetDelegate(
        maximize_allowed == MAXIMIZE_ALLOWED);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.context = CurrentContext();
    widget->Init(params);
    return widget;
  }

  // Sets |container| to use arbitrary images for the buttons. Setting the
  // images causes the buttons to have non-empty sizes.
  void SetMockImages(FrameCaptionButtonContainerView* container) {
    for (int icon = 0; icon < CAPTION_BUTTON_ICON_COUNT; ++icon) {
      container->SetButtonImages(
          static_cast<CaptionButtonIcon>(icon),
          IDR_AURA_WINDOW_CONTROL_ICON_CLOSE,
          IDR_AURA_WINDOW_CONTROL_ICON_CLOSE_I,
          IDR_AURA_WINDOW_CONTROL_BACKGROUND_H,
          IDR_AURA_WINDOW_CONTROL_BACKGROUND_P);
    }
  }

  // Tests that |leftmost| and |rightmost| are at |container|'s edges.
  bool CheckButtonsAtEdges(FrameCaptionButtonContainerView* container,
                           const ash::FrameCaptionButton& leftmost,
                           const ash::FrameCaptionButton& rightmost) {
    gfx::Rect expected(container->GetPreferredSize());

    gfx::Rect container_size(container->GetPreferredSize());
    if (leftmost.y() == rightmost.y() &&
        leftmost.height() == rightmost.height() &&
        leftmost.x() == expected.x() &&
        leftmost.y() == expected.y() &&
        leftmost.height() == expected.height() &&
        rightmost.bounds().right() == expected.right()) {
      return true;
    }

    LOG(ERROR) << "Buttons " << leftmost.bounds().ToString() << " "
               << rightmost.bounds().ToString() << " not at edges of "
               << expected.ToString();
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameCaptionButtonContainerViewTest);
};

// Test how the allowed actions affect which caption buttons are visible.
TEST_F(FrameCaptionButtonContainerViewTest, ButtonVisibility) {
  // All the buttons should be visible when minimizing and maximizing are
  // allowed.
  scoped_ptr<views::Widget> widget_can_maximize(
      CreateTestWidget(MAXIMIZE_ALLOWED));
  FrameCaptionButtonContainerView container1(widget_can_maximize.get(),
      FrameCaptionButtonContainerView::MINIMIZE_ALLOWED);
  SetMockImages(&container1);
  container1.Layout();
  FrameCaptionButtonContainerView::TestApi t1(&container1);
  EXPECT_TRUE(t1.minimize_button()->visible());
  EXPECT_TRUE(t1.size_button()->visible());
  EXPECT_TRUE(t1.close_button()->visible());
  EXPECT_TRUE(CheckButtonsAtEdges(
      &container1, *t1.minimize_button(), *t1.close_button()));

  // The minimize button should be visible when minimizing is allowed but
  // maximizing is disallowed.
  scoped_ptr<views::Widget> widget_cannot_maximize(
      CreateTestWidget(MAXIMIZE_DISALLOWED));
  FrameCaptionButtonContainerView container2(widget_cannot_maximize.get(),
      FrameCaptionButtonContainerView::MINIMIZE_ALLOWED);
  SetMockImages(&container2);
  container2.Layout();
  FrameCaptionButtonContainerView::TestApi t2(&container2);
  EXPECT_TRUE(t2.minimize_button()->visible());
  EXPECT_FALSE(t2.size_button()->visible());
  EXPECT_TRUE(t2.close_button()->visible());
  EXPECT_TRUE(CheckButtonsAtEdges(
      &container2, *t2.minimize_button(), *t2.close_button()));

  // Neither the minimize button nor the size button should be visible when
  // neither minimizing nor maximizing are allowed.
  FrameCaptionButtonContainerView container3(widget_cannot_maximize.get(),
      FrameCaptionButtonContainerView::MINIMIZE_DISALLOWED);
  SetMockImages(&container3);
  container3.Layout();
  FrameCaptionButtonContainerView::TestApi t3(&container3);
  EXPECT_FALSE(t3.minimize_button()->visible());
  EXPECT_FALSE(t3.size_button()->visible());
  EXPECT_TRUE(t3.close_button()->visible());
  EXPECT_TRUE(CheckButtonsAtEdges(
      &container3, *t3.close_button(), *t3.close_button()));
}

}  // namespace ash
