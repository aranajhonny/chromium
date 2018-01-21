// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/public/home_card.h"

#include <limits>

#include "athena/home/app_list_view_delegate.h"
#include "athena/home/bottom_home_view.h"
#include "athena/home/minimized_home.h"
#include "athena/home/public/app_model_builder.h"
#include "athena/input/public/accelerator_manager.h"
#include "athena/screen/public/screen_manager.h"
#include "athena/wm/public/window_manager.h"
#include "athena/wm/public/window_manager_observer.h"
#include "base/bind.h"
#include "ui/app_list/search_provider.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_animations.h"

namespace athena {
namespace {

HomeCard* instance = NULL;

// Makes sure the homecard is center-aligned horizontally and bottom-aligned
// vertically.
class HomeCardLayoutManager : public aura::LayoutManager {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual int GetHomeCardHeight() const = 0;

    virtual int GetHorizontalMargin() const = 0;

    virtual aura::Window* GetNativeWindow() = 0;
  };

  explicit HomeCardLayoutManager(Delegate* delegate)
      : delegate_(delegate) {}

  virtual ~HomeCardLayoutManager() {}

  void Layout() {
    aura::Window* home_card = delegate_->GetNativeWindow();
    // |home_card| could be detached from the root window (e.g. when it is being
    // destroyed).
    if (!home_card || !home_card->GetRootWindow())
      return;

    int height = delegate_->GetHomeCardHeight();
    int horiz_margin = delegate_->GetHorizontalMargin();
    gfx::Rect screen_bounds = home_card->GetRootWindow()->bounds();
    height = std::min(height, screen_bounds.height());
    gfx::Rect card_bounds = screen_bounds;
    card_bounds.Inset(horiz_margin, screen_bounds.height() - height,
                      horiz_margin, 0);

    SetChildBoundsDirect(home_card, card_bounds);
  }

 private:
  // aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE { Layout(); }
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE { Layout(); }
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE {}
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE {
    Layout();
  }
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE {
    Layout();
  }
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE {
    SetChildBoundsDirect(child, gfx::Rect(requested_bounds.size()));
  }

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardLayoutManager);
};

// The container view of home card contents of each state.
class HomeCardView : public views::WidgetDelegateView {
 public:
  HomeCardView(app_list::AppListViewDelegate* view_delegate,
               aura::Window* container,
               MinimizedHomeDragDelegate* minimized_delegate) {
    set_background(views::Background::CreateSolidBackground(SK_ColorWHITE));

    bottom_view_ = new BottomHomeView(view_delegate);
    AddChildView(bottom_view_);

    main_view_ = new app_list::AppListMainView(
        view_delegate, 0 /* initial_apps_page */, container);
    AddChildView(main_view_);

    minimized_view_ = CreateMinimizedHome(minimized_delegate);
    AddChildView(minimized_view_);
  }

  void SetState(HomeCard::State state) {
    bottom_view_->SetVisible(state == HomeCard::VISIBLE_BOTTOM);
    main_view_->SetVisible(state == HomeCard::VISIBLE_CENTERED);
    minimized_view_->SetVisible(state == HomeCard::VISIBLE_MINIMIZED);
    if (state == HomeCard::VISIBLE_CENTERED) {
      app_list::ContentsView* contents_view = main_view_->contents_view();
      contents_view->SetActivePage(contents_view->GetPageIndexForNamedPage(
          app_list::ContentsView::NAMED_PAGE_START));
    }
  }

  // views::View:
  virtual void Layout() OVERRIDE {
    for (int i = 0; i < child_count(); ++i) {
      views::View* child = child_at(i);
      if (child->visible()) {
        child->SetBoundsRect(bounds());
        return;
      }
    }

    // One of the child views has to be visible.
    NOTREACHED();
  }

 private:
  // views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }

  app_list::AppListMainView* main_view_;
  BottomHomeView* bottom_view_;
  views::View* minimized_view_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardView);
};

class HomeCardImpl : public HomeCard,
                     public AcceleratorHandler,
                     public HomeCardLayoutManager::Delegate,
                     public MinimizedHomeDragDelegate,
                     public WindowManagerObserver {
 public:
  explicit HomeCardImpl(AppModelBuilder* model_builder);
  virtual ~HomeCardImpl();

  void Init();

 private:
  enum Command {
    COMMAND_SHOW_HOME_CARD,
  };
  void InstallAccelerators();

  // Overridden from HomeCard:
  virtual void SetState(State state) OVERRIDE;
  virtual void RegisterSearchProvider(
      app_list::SearchProvider* search_provider) OVERRIDE;
  virtual void UpdateVirtualKeyboardBounds(
      const gfx::Rect& bounds) OVERRIDE;

  // AcceleratorHandler:
  virtual bool IsCommandEnabled(int command_id) const OVERRIDE { return true; }
  virtual bool OnAcceleratorFired(int command_id,
                                  const ui::Accelerator& accelerator) OVERRIDE {
    DCHECK_EQ(COMMAND_SHOW_HOME_CARD, command_id);
    if (state_ == HIDDEN)
      SetState(VISIBLE_CENTERED);
    else
      SetState(HIDDEN);
    return true;
  }

  // HomeCardLayoutManager::Delegate:
  virtual int GetHomeCardHeight() const OVERRIDE {
    const int kHomeCardHeight = 150;
    const int kHomeCardMinimizedHeight = 8;

    switch (state_) {
      case VISIBLE_CENTERED:
        // Span the screen fully.
        return std::numeric_limits<int>::max();
      case VISIBLE_BOTTOM:
        return kHomeCardHeight;
      case VISIBLE_MINIMIZED:
        return kHomeCardMinimizedHeight;
      case HIDDEN:
        break;
    }
    NOTREACHED();
    return -1;
  }

  virtual int GetHorizontalMargin() const OVERRIDE {
    CHECK_NE(HIDDEN, state_);
    const int kHomeCardHorizontalMargin = 50;
    return state_ == VISIBLE_BOTTOM ? kHomeCardHorizontalMargin : 0;
  }

  virtual aura::Window* GetNativeWindow() OVERRIDE {
    if (state_ == HIDDEN)
      return NULL;

    return home_card_widget_ ? home_card_widget_->GetNativeWindow() : NULL;
  }

  // MinimizedHomeDragDelegate:
  virtual void OnDragUpCompleted() OVERRIDE {
    WindowManager::GetInstance()->ToggleOverview();
  }

  // WindowManagerObserver:
  virtual void OnOverviewModeEnter() OVERRIDE {
    SetState(VISIBLE_BOTTOM);
  }

  virtual void OnOverviewModeExit() OVERRIDE {
    SetState(VISIBLE_MINIMIZED);
  }

  scoped_ptr<AppModelBuilder> model_builder_;

  HomeCard::State state_;

  // original_state_ is the state which the home card should go back to after
  // the virtual keyboard is hidden.
  HomeCard::State original_state_;

  views::Widget* home_card_widget_;
  HomeCardView* home_card_view_;
  scoped_ptr<AppListViewDelegate> view_delegate_;
  HomeCardLayoutManager* layout_manager_;

  // Right now HomeCard allows only one search provider.
  // TODO(mukai): port app-list's SearchController and Mixer.
  scoped_ptr<app_list::SearchProvider> search_provider_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardImpl);
};

HomeCardImpl::HomeCardImpl(AppModelBuilder* model_builder)
    : model_builder_(model_builder),
      state_(VISIBLE_MINIMIZED),
      original_state_(VISIBLE_MINIMIZED),
      home_card_widget_(NULL),
      home_card_view_(NULL),
      layout_manager_(NULL) {
  DCHECK(!instance);
  instance = this;
  WindowManager::GetInstance()->AddObserver(this);
}

HomeCardImpl::~HomeCardImpl() {
  DCHECK(instance);
  WindowManager::GetInstance()->RemoveObserver(this);
  home_card_widget_->CloseNow();
  instance = NULL;
}

void HomeCardImpl::SetState(HomeCard::State state) {
  // Update |state_| before changing the visibility of the widgets, so that
  // LayoutManager callbacks get the correct state.
  state_ = state;
  original_state_ = state;
  if (state_ == HIDDEN) {
    home_card_widget_->Hide();
  } else {
    home_card_widget_->Show();
    home_card_view_->SetState(state);
    layout_manager_->Layout();
  }
}

void HomeCardImpl::RegisterSearchProvider(
    app_list::SearchProvider* search_provider) {
  DCHECK(!search_provider_);
  search_provider_.reset(search_provider);
  view_delegate_->RegisterSearchProvider(search_provider_.get());
}

void HomeCardImpl::UpdateVirtualKeyboardBounds(
    const gfx::Rect& bounds) {
  if (state_ == VISIBLE_MINIMIZED && !bounds.IsEmpty()) {
    SetState(HIDDEN);
    original_state_ = VISIBLE_MINIMIZED;
  } else if (state_ == VISIBLE_BOTTOM && !bounds.IsEmpty()) {
    SetState(VISIBLE_CENTERED);
    original_state_ = VISIBLE_BOTTOM;
  } else if (state_ != original_state_ && bounds.IsEmpty()) {
    SetState(original_state_);
  }
}

void HomeCardImpl::Init() {
  InstallAccelerators();
  ScreenManager::ContainerParams params("HomeCardContainer");
  params.can_activate_children = true;
  aura::Window* container = ScreenManager::Get()->CreateContainer(params);
  layout_manager_ = new HomeCardLayoutManager(this);

  container->SetLayoutManager(layout_manager_);
  wm::SetChildWindowVisibilityChangesAnimated(container);

  view_delegate_.reset(new AppListViewDelegate(model_builder_.get()));
  if (search_provider_)
    view_delegate_->RegisterSearchProvider(search_provider_.get());

  home_card_view_ = new HomeCardView(view_delegate_.get(), container, this);
  home_card_widget_ = new views::Widget();
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget_params.parent = container;
  widget_params.delegate = home_card_view_;
  widget_params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  home_card_widget_->Init(widget_params);

  SetState(VISIBLE_MINIMIZED);
  home_card_view_->Layout();
}

void HomeCardImpl::InstallAccelerators() {
  const AcceleratorData accelerator_data[] = {
      {TRIGGER_ON_PRESS, ui::VKEY_L, ui::EF_CONTROL_DOWN,
       COMMAND_SHOW_HOME_CARD, AF_NONE},
  };
  AcceleratorManager::Get()->RegisterAccelerators(
      accelerator_data, arraysize(accelerator_data), this);
}

}  // namespace

// static
HomeCard* HomeCard::Create(AppModelBuilder* model_builder) {
  (new HomeCardImpl(model_builder))->Init();
  DCHECK(instance);
  return instance;
}

// static
void HomeCard::Shutdown() {
  DCHECK(instance);
  delete instance;
  instance = NULL;
}

// static
HomeCard* HomeCard::Get() {
  DCHECK(instance);
  return instance;
}

}  // namespace athena
