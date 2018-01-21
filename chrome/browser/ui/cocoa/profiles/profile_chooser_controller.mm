// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/profiles/profile_chooser_controller.h"

#include "base/mac/bundle_locations.h"
#include "base/prefs/pref_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_header_helper.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/hyperlink_text_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/profiles/user_manager_mac.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/browser/mutable_profile_oauth2_token_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/base/cocoa/controls/blue_label_button.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#import "ui/base/cocoa/hover_image_button.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/text_elider.h"
#include "ui/native_theme/native_theme.h"

namespace {

// Constants taken from the Windows/Views implementation at:
// chrome/browser/ui/views/profile_chooser_view.cc
const int kLargeImageSide = 88;
const int kSmallImageSide = 32;
const CGFloat kFixedMenuWidth = 250;

const CGFloat kVerticalSpacing = 16.0;
const CGFloat kSmallVerticalSpacing = 10.0;
const CGFloat kHorizontalSpacing = 16.0;
const CGFloat kTitleFontSize = 15.0;
const CGFloat kTextFontSize = 12.0;
const CGFloat kProfileButtonHeight = 30;
const int kBezelThickness = 3;  // Width of the bezel on an NSButton.
const int kImageTitleSpacing = 10;
const int kBlueButtonHeight = 30;

// Fixed size for embedded sign in pages as defined in Gaia.
const CGFloat kFixedGaiaViewWidth = 360;
const CGFloat kFixedGaiaViewHeight = 400;

// Fixed size for the account removal view.
const CGFloat kFixedAccountRemovalViewWidth = 280;
// Fixed size for the end-preview view.
const int kFixedEndPreviewViewWidth = 280;

// Maximum number of times to show the welcome tutorial in the profile avatar
// bubble.
const int kProfileAvatarTutorialShowMax = 1;

// The tag number for the primary account.
const int kPrimaryProfileTag = -1;

gfx::Image CreateProfileImage(const gfx::Image& icon, int imageSize) {
  return profiles::GetSizedAvatarIcon(
      icon, true /* image is a square */, imageSize, imageSize);
}

// Updates the window size and position.
void SetWindowSize(NSWindow* window, NSSize size) {
  NSRect frame = [window frame];
  frame.origin.x += frame.size.width - size.width;
  frame.origin.y += frame.size.height - size.height;
  frame.size = size;
  [window setFrame:frame display:YES];
}

NSString* ElideEmail(const std::string& email, CGFloat width) {
  const base::string16 elidedEmail = gfx::ElideText(
      base::UTF8ToUTF16(email), gfx::FontList(), width, gfx::ELIDE_EMAIL);
  return base::SysUTF16ToNSString(elidedEmail);
}

// Builds a label with the given |title| anchored at |frame_origin|. Sets the
// text color and background color to the given colors if not null.
NSTextField* BuildLabel(NSString* title,
                        NSPoint frame_origin,
                        NSColor* background_color,
                        NSColor* text_color) {
  base::scoped_nsobject<NSTextField> label(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [label setStringValue:title];
  [label setEditable:NO];
  [label setAlignment:NSLeftTextAlignment];
  [label setBezeled:NO];
  [label setFont:[NSFont labelFontOfSize:kTextFontSize]];
  [label setFrameOrigin:frame_origin];
  [label sizeToFit];

  if (background_color)
    [[label cell] setBackgroundColor:background_color];
  if (text_color)
    [[label cell] setTextColor:text_color];

  return label.autorelease();
}

// Builds an NSTextView that has the contents set to the specified |message|,
// with a non-underlined |link| inserted at |link_offset|. The view is anchored
// at the specified |frame_origin| and has a fixed |frame_width|.
NSTextView* BuildFixedWidthTextViewWithLink(
    id<NSTextViewDelegate> delegate,
    NSString* message,
    NSString* link,
    int link_offset,
    NSPoint frame_origin,
    CGFloat frame_width) {
  base::scoped_nsobject<HyperlinkTextView> text_view(
      [[HyperlinkTextView alloc] initWithFrame:NSZeroRect]);
  NSColor* link_color = gfx::SkColorToCalibratedNSColor(
      chrome_style::GetLinkColor());
  // Adds a padding row at the bottom, because |boundingRectWithSize| below cuts
  // off the last row sometimes.
  [text_view setMessageAndLink:[NSString stringWithFormat:@"%@\n", message]
                      withLink:link
                      atOffset:link_offset
                          font:[NSFont labelFontOfSize:kTextFontSize]
                  messageColor:[NSColor blackColor]
                     linkColor:link_color];

  // Removes the underlining from the link.
  [text_view setLinkTextAttributes:nil];
  NSTextStorage* text_storage = [text_view textStorage];
  NSRange link_range = NSMakeRange(link_offset, [link length]);
  [text_storage addAttribute:NSUnderlineStyleAttributeName
                       value:[NSNumber numberWithInt:NSUnderlineStyleNone]
                       range:link_range];

  NSRect frame = [[text_view attributedString]
      boundingRectWithSize:NSMakeSize(frame_width, 0)
                   options:NSStringDrawingUsesLineFragmentOrigin];
  frame.origin = frame_origin;
  [text_view setFrame:frame];
  [text_view setDelegate:delegate];
  return text_view.autorelease();
}

// Returns the native dialog background color.
NSColor* GetDialogBackgroundColor() {
  return gfx::SkColorToCalibratedNSColor(
      ui::NativeTheme::instance()->GetSystemColor(
          ui::NativeTheme::kColorId_DialogBackground));
}

// Builds a title card with one back button right aligned and one label center
// aligned.
NSView* BuildTitleCard(NSRect frame_rect,
                       int message_id,
                       id back_button_target,
                       SEL back_button_action) {
  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:frame_rect]);

  base::scoped_nsobject<HoverImageButton> button(
      [[HoverImageButton alloc] initWithFrame:frame_rect]);
  [button setBordered:NO];
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  [button setDefaultImage:rb->GetNativeImageNamed(IDR_BACK).ToNSImage()];
  [button setHoverImage:rb->GetNativeImageNamed(IDR_BACK_H).ToNSImage()];
  [button setPressedImage:rb->GetNativeImageNamed(IDR_BACK_P).ToNSImage()];
  [button setTarget:back_button_target];
  [button setAction:back_button_action];
  [button setFrameSize:NSMakeSize(kProfileButtonHeight, kProfileButtonHeight)];
  [button setFrameOrigin:NSMakePoint(kHorizontalSpacing, 0)];

  NSTextField* title_label =
      BuildLabel(l10n_util::GetNSString(message_id), NSZeroPoint,
                 GetDialogBackgroundColor(), nil /* text_color */);
  [title_label setAlignment:NSCenterTextAlignment];
  [title_label setFont:[NSFont labelFontOfSize:kTitleFontSize]];
  [title_label sizeToFit];
  CGFloat x_offset = (frame_rect.size.width - NSWidth([title_label frame])) / 2;
  CGFloat y_offset =
      (NSHeight([button frame]) - NSHeight([title_label frame])) / 2;
  [title_label setFrameOrigin:NSMakePoint(x_offset, y_offset)];

  [container addSubview:button];
  [container addSubview:title_label];
  CGFloat height = std::max(NSMaxY([title_label frame]),
                            NSMaxY([button frame])) + kSmallVerticalSpacing;
  [container setFrameSize:NSMakeSize(NSWidth([container frame]), height)];

  return container.autorelease();
}

bool HasAuthError(Profile* profile) {
  const SigninErrorController* error_controller =
      profiles::GetSigninErrorController(profile);
  return error_controller && error_controller->HasError();
}

}  // namespace

// Class that listens to changes to the OAuth2Tokens for the active profile,
// changes to the avatar menu model or browser close notifications.
class ActiveProfileObserverBridge : public AvatarMenuObserver,
                                    public content::NotificationObserver,
                                    public OAuth2TokenService::Observer {
 public:
  ActiveProfileObserverBridge(ProfileChooserController* controller,
                              Browser* browser)
      : controller_(controller),
        browser_(browser),
        token_observer_registered_(false) {
    registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSING,
                   content::NotificationService::AllSources());
    if (!browser_->profile()->IsGuestSession())
      AddTokenServiceObserver();
  }

  virtual ~ActiveProfileObserverBridge() {
    RemoveTokenServiceObserver();
  }

 private:
  void AddTokenServiceObserver() {
    ProfileOAuth2TokenService* oauth2_token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile());
    DCHECK(oauth2_token_service);
    oauth2_token_service->AddObserver(this);
    token_observer_registered_ = true;
  }

  void RemoveTokenServiceObserver() {
    if (!token_observer_registered_)
      return;
    DCHECK(browser_->profile());
    ProfileOAuth2TokenService* oauth2_token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile());
    DCHECK(oauth2_token_service);
    oauth2_token_service->RemoveObserver(this);
    token_observer_registered_ = false;
  }

  // OAuth2TokenService::Observer:
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE {
    // Tokens can only be added by adding an account through the inline flow,
    // which is started from the account management view. Refresh it to show the
    // update.
    profiles::BubbleViewMode viewMode = [controller_ viewMode];
    if (viewMode == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT ||
        viewMode == profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN ||
        viewMode == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
        viewMode == profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH) {
      [controller_ initMenuContentsWithView:
          profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT];
    }
  }

  virtual void OnRefreshTokenRevoked(const std::string& account_id) OVERRIDE {
    // Tokens can only be removed from the account management view. Refresh it
    // to show the update.
    if ([controller_ viewMode] == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT)
      [controller_ initMenuContentsWithView:
          profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT];
  }

  // AvatarMenuObserver:
  virtual void OnAvatarMenuChanged(AvatarMenu* avatar_menu) OVERRIDE {
    // While the bubble is open, the avatar menu can only change from the
    // profile chooser view by modifying the current profile's photo or name.
    [controller_
        initMenuContentsWithView:profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER];
  }

  // content::NotificationObserver:
  virtual void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE {
    DCHECK_EQ(chrome::NOTIFICATION_BROWSER_CLOSING, type);
    if (browser_ == content::Source<Browser>(source).ptr()) {
      RemoveTokenServiceObserver();
      // Clean up the bubble's WebContents (used by the Gaia embedded view), to
      // make sure the guest profile doesn't have any dangling host renderers.
      // This can happen if Chrome is quit using Command-Q while the bubble is
      // still open, which won't give the bubble a chance to be closed and
      // clean up the WebContents itself.
      [controller_ cleanUpEmbeddedViewContents];
    }
  }

  ProfileChooserController* controller_;  // Weak; owns this.
  Browser* browser_;  // Weak.
  content::NotificationRegistrar registrar_;

  // The observer can be removed both when closing the browser, and by just
  // closing the avatar bubble. However, in the case of closing the browser,
  // the avatar bubble will also be closed afterwards, resulting in a second
  // attempt to remove the observer. This ensures the observer is only
  // removed once.
  bool token_observer_registered_;

  DISALLOW_COPY_AND_ASSIGN(ActiveProfileObserverBridge);
};

// Custom button cell that adds a left padding before the button image, and
// a custom spacing between the button image and title.
@interface CustomPaddingImageButtonCell : NSButtonCell {
 @private
  // Padding added to the left margin of the button.
  int leftMarginSpacing_;
  // Spacing between the cell image and title.
  int imageTitleSpacing_;
}

- (id)initWithLeftMarginSpacing:(int)leftMarginSpacing
              imageTitleSpacing:(int)imageTitleSpacing;
@end

@implementation CustomPaddingImageButtonCell
- (id)initWithLeftMarginSpacing:(int)leftMarginSpacing
              imageTitleSpacing:(int)imageTitleSpacing {
  if ((self = [super init])) {
    leftMarginSpacing_ = leftMarginSpacing;
    imageTitleSpacing_ = imageTitleSpacing;
  }
  return self;
}

- (NSRect)drawTitle:(NSAttributedString*)title
          withFrame:(NSRect)frame
             inView:(NSView*)controlView {
  NSRect marginRect;
  NSDivideRect(frame, &marginRect, &frame, leftMarginSpacing_, NSMinXEdge);

  // The title frame origin isn't aware of the left margin spacing added
  // in -drawImage, so it must be added when drawing the title as well.
  if ([self imagePosition] == NSImageLeft)
    NSDivideRect(frame, &marginRect, &frame, imageTitleSpacing_, NSMinXEdge);

  return [super drawTitle:title withFrame:frame inView:controlView];
}

- (void)drawImage:(NSImage*)image
        withFrame:(NSRect)frame
           inView:(NSView*)controlView {
  if ([self imagePosition] == NSImageLeft)
    frame.origin.x = leftMarginSpacing_;
  [super drawImage:image withFrame:frame inView:controlView];
}

- (NSSize)cellSize {
  NSSize buttonSize = [super cellSize];
  buttonSize.width += leftMarginSpacing_;
  if ([self imagePosition] == NSImageLeft)
    buttonSize.width += imageTitleSpacing_;
  return buttonSize;
}

@end

// A custom button that has a transparent backround.
@interface TransparentBackgroundButton : NSButton
@end

@implementation TransparentBackgroundButton
- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    [self setBordered:NO];
    [self setFont:[NSFont labelFontOfSize:kTextFontSize]];
    [self setButtonType:NSMomentaryChangeButton];
  }
  return self;
}

- (void)drawRect:(NSRect)dirtyRect {
  NSColor* backgroundColor = [NSColor colorWithCalibratedWhite:1 alpha:0.6f];
  [backgroundColor setFill];
  NSRectFillUsingOperation(dirtyRect, NSCompositeSourceAtop);
  [super drawRect:dirtyRect];
}
@end

// A custom image control that shows a "Change" button when moused over.
@interface EditableProfilePhoto : NSImageView {
 @private
  AvatarMenu* avatarMenu_;  // Weak; Owned by ProfileChooserController.
  base::scoped_nsobject<TransparentBackgroundButton> changePhotoButton_;
  // Used to display the "Change" button on hover.
  ui::ScopedCrTrackingArea trackingArea_;
  ProfileChooserController* controller_;
}

- (id)initWithFrame:(NSRect)frameRect
         avatarMenu:(AvatarMenu*)avatarMenu
        profileIcon:(const gfx::Image&)profileIcon
     editingAllowed:(BOOL)editingAllowed
     withController:(ProfileChooserController*)controller;

// Called when the "Change" button is clicked.
- (void)editPhoto:(id)sender;

// When hovering over the profile photo, show the "Change" button.
- (void)mouseEntered:(NSEvent*)event;

// When hovering away from the profile photo, hide the "Change" button.
- (void)mouseExited:(NSEvent*)event;
@end

@interface EditableProfilePhoto (Private)
// Create the "Change" avatar photo button.
- (TransparentBackgroundButton*)changePhotoButtonWithRect:(NSRect)rect;
@end

@implementation EditableProfilePhoto
- (id)initWithFrame:(NSRect)frameRect
         avatarMenu:(AvatarMenu*)avatarMenu
        profileIcon:(const gfx::Image&)profileIcon
     editingAllowed:(BOOL)editingAllowed
     withController:(ProfileChooserController*)controller {
  if ((self = [super initWithFrame:frameRect])) {
    avatarMenu_ = avatarMenu;
    controller_ = controller;
    [self setImage:CreateProfileImage(
        profileIcon, kLargeImageSide).ToNSImage()];

    // Add a tracking area so that we can show/hide the button when hovering.
    trackingArea_.reset([[CrTrackingArea alloc]
        initWithRect:[self bounds]
             options:NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways
               owner:self
            userInfo:nil]);
    [self addTrackingArea:trackingArea_.get()];

    NSRect bounds = NSMakeRect(0, 0, kLargeImageSide, kLargeImageSide);
    if (editingAllowed) {
      changePhotoButton_.reset([self changePhotoButtonWithRect:bounds]);
      [self addSubview:changePhotoButton_];

      // Hide the button until the image is hovered over.
      [changePhotoButton_ setHidden:YES];
    }
  }
  return self;
}

- (void)drawRect:(NSRect)dirtyRect {
  NSRect bounds = [self bounds];

  // Display the profile picture as a circle.
  NSBezierPath* path = [NSBezierPath bezierPathWithOvalInRect:bounds];
  [path addClip];
  [self.image drawAtPoint:bounds.origin
                 fromRect:bounds
                operation:NSCompositeSourceOver
                 fraction:1.0];

}

- (void)editPhoto:(id)sender {
  avatarMenu_->EditProfile(avatarMenu_->GetActiveProfileIndex());
  [controller_
      postActionPerformed:ProfileMetrics::PROFILE_DESKTOP_MENU_EDIT_IMAGE];
}

- (void)mouseEntered:(NSEvent*)event {
  [changePhotoButton_ setHidden:NO];
}

- (void)mouseExited:(NSEvent*)event {
  [changePhotoButton_ setHidden:YES];
}

- (TransparentBackgroundButton*)changePhotoButtonWithRect:(NSRect)rect {
  TransparentBackgroundButton* button =
      [[TransparentBackgroundButton alloc] initWithFrame:rect];
  [button setImage:ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_ICON_PROFILES_EDIT_CAMERA).AsNSImage()];
  [button setImagePosition:NSImageOnly];
  [button setTarget:self];
  [button setAction:@selector(editPhoto:)];
  return button;
}
@end

// A custom text control that turns into a textfield for editing when clicked.
@interface EditableProfileNameButton : HoverImageButton {
 @private
  base::scoped_nsobject<NSTextField> profileNameTextField_;
  Profile* profile_;  // Weak.
  ProfileChooserController* controller_;
}

- (id)initWithFrame:(NSRect)frameRect
            profile:(Profile*)profile
        profileName:(NSString*)profileName
     editingAllowed:(BOOL)editingAllowed
     withController:(ProfileChooserController*)controller;

// Called when the button is clicked.
- (void)showEditableView:(id)sender;

// Called when enter is pressed in the text field.
- (void)saveProfileName:(id)sender;

@end

@implementation EditableProfileNameButton
- (id)initWithFrame:(NSRect)frameRect
            profile:(Profile*)profile
        profileName:(NSString*)profileName
     editingAllowed:(BOOL)editingAllowed
     withController:(ProfileChooserController*)controller {
  if ((self = [super initWithFrame:frameRect])) {
    profile_ = profile;
    controller_ = controller;

    if (editingAllowed) {
      // Show an "edit" pencil icon when hovering over. In the default state,
      // we need to create an empty placeholder of the correct size, so that
      // the text doesn't jump around when the hovered icon appears.
      ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
      NSImage* hoverImage = rb->GetNativeImageNamed(
          IDR_ICON_PROFILES_EDIT_HOVER).AsNSImage();

      // In order to center the button title, we need to add a left padding of
      // the same width as the pencil icon.
      base::scoped_nsobject<CustomPaddingImageButtonCell> cell(
          [[CustomPaddingImageButtonCell alloc]
              initWithLeftMarginSpacing:[hoverImage size].width
                      imageTitleSpacing:0]);
      [self setCell:cell.get()];

      NSImage* placeholder = [[NSImage alloc] initWithSize:[hoverImage size]];
      [self setDefaultImage:placeholder];
      [self setHoverImage:hoverImage];
      [self setAlternateImage:
          rb->GetNativeImageNamed(IDR_ICON_PROFILES_EDIT_PRESSED).AsNSImage()];
      [self setImagePosition:NSImageRight];
      [self setTarget:self];
      [self setAction:@selector(showEditableView:)];

      // We need to subtract the width of the bezel from the frame rect, so that
      // the textfield can take the exact same space as the button.
      frameRect.size.height -= 2 * kBezelThickness;
      frameRect.origin = NSMakePoint(0, kBezelThickness);
      profileNameTextField_.reset(
          [[NSTextField alloc] initWithFrame:frameRect]);
      [profileNameTextField_ setStringValue:profileName];
      [profileNameTextField_ setFont:[NSFont labelFontOfSize:kTitleFontSize]];
      [profileNameTextField_ setEditable:YES];
      [profileNameTextField_ setDrawsBackground:YES];
      [profileNameTextField_ setBezeled:YES];
      [profileNameTextField_ setAlignment:NSCenterTextAlignment];
      [[profileNameTextField_ cell] setWraps:NO];
      [[profileNameTextField_ cell] setLineBreakMode:
          NSLineBreakByTruncatingTail];
      [self addSubview:profileNameTextField_];
      [profileNameTextField_ setTarget:self];
      [profileNameTextField_ setAction:@selector(saveProfileName:)];

      // Hide the textfield until the user clicks on the button.
      [profileNameTextField_ setHidden:YES];
    }

    [self setBordered:NO];
    [self setFont:[NSFont labelFontOfSize:kTitleFontSize]];
    [self setAlignment:NSCenterTextAlignment];
    [[self cell] setLineBreakMode:NSLineBreakByTruncatingTail];
    [self setTitle:profileName];
  }
  return self;
}

- (void)saveProfileName:(id)sender {
  NSString* text = [profileNameTextField_ stringValue];
  // Empty profile names are not allowed, and are treated as a cancel.
  if ([text length] > 0) {
    profiles::UpdateProfileName(profile_, base::SysNSStringToUTF16(text));
    [controller_
        postActionPerformed:ProfileMetrics::PROFILE_DESKTOP_MENU_EDIT_NAME];
    [self setTitle:text];
  }
  [profileNameTextField_ setHidden:YES];
}

- (void)showEditableView:(id)sender {
  [profileNameTextField_ setHidden:NO];
  [[self window] makeFirstResponder:profileNameTextField_];
}

@end

// A custom button that allows for setting a background color when hovered over.
@interface BackgroundColorHoverButton : HoverImageButton {
 @private
  base::scoped_nsobject<NSColor> backgroundColor_;
  base::scoped_nsobject<NSColor> hoverColor_;
}
@end

@implementation BackgroundColorHoverButton

- (id)initWithFrame:(NSRect)frameRect
  imageTitleSpacing:(int)imageTitleSpacing
    backgroundColor:(NSColor*)backgroundColor {
  if ((self = [super initWithFrame:frameRect])) {
    backgroundColor_.reset([backgroundColor retain]);
    hoverColor_.reset([gfx::SkColorToCalibratedNSColor(
        ui::NativeTheme::instance()->GetSystemColor(
            ui::NativeTheme::kColorId_ButtonHoverBackgroundColor)) retain]);

    [self setBordered:NO];
    [self setFont:[NSFont labelFontOfSize:kTextFontSize]];
    [self setButtonType:NSMomentaryChangeButton];

    base::scoped_nsobject<CustomPaddingImageButtonCell> cell(
        [[CustomPaddingImageButtonCell alloc]
            initWithLeftMarginSpacing:kHorizontalSpacing
                    imageTitleSpacing:imageTitleSpacing]);
    [cell setLineBreakMode:NSLineBreakByTruncatingTail];
    [self setCell:cell.get()];
  }
  return self;
}

- (void)setHoverState:(HoverState)state {
  [super setHoverState:state];
  bool isHighlighted = ([self hoverState] != kHoverStateNone);

  NSColor* backgroundColor = isHighlighted ? hoverColor_ : backgroundColor_;
  [[self cell] setBackgroundColor:backgroundColor];
}

@end

// A custom view with the given background color.
@interface BackgroundColorView : NSView {
 @private
  base::scoped_nsobject<NSColor> backgroundColor_;
}
@end

@implementation BackgroundColorView
- (id)initWithFrame:(NSRect)frameRect
          withColor:(NSColor*)color {
  if ((self = [super initWithFrame:frameRect]))
    backgroundColor_.reset([color retain]);
  return self;
}

- (void)drawRect:(NSRect)dirtyRect {
  [backgroundColor_ setFill];
  NSRectFill(dirtyRect);
  [super drawRect:dirtyRect];
}
@end

@interface ProfileChooserController ()
// Builds the profile chooser view.
- (NSView*)buildProfileChooserView;

// Builds a tutorial card with a title label using |titleMessageId|, a content
// label using |contentMessageId|, and a bottom row with a right-aligned link
// using |linkMessageId|, and a left aligned button using |buttonMessageId|.
// On click, the link would execute |linkAction|, and the button would execute
// |buttonAction|. It sets |tutorialMode_| to the given |mode|.
- (NSView*)tutorialViewWithMode:(profiles::TutorialMode)mode
                   titleMessage:(int)titleMessageId
                 contentMessage:(int)contentMessageId
                    linkMessage:(int)linkMessageId
                  buttonMessage:(int)buttonMessageId
                     linkAction:(SEL)linkAction
                   buttonAction:(SEL)buttonAction;

// Builds a a tutorial card for new profile management preview if needed. if
// new profile management is not enabled yet, then it prompts the user to try
// out the feature. Otherwise, it notifies the user that the feature has been
// enabled if needed.
- (NSView*)buildPreviewTutorialIfNeeded:(const AvatarMenu::Item&)item;

// Creates the main profile card for the profile |item| at the top of
// the bubble.
- (NSView*)createCurrentProfileView:(const AvatarMenu::Item&)item;

// Creates the possible links for the main profile card with profile |item|.
- (NSView*)createCurrentProfileLinksForItem:(const AvatarMenu::Item&)item
                                       rect:(NSRect)rect;

// Creates the disclaimer text for supervised users, telling them that the
// manager can view their history etc.
- (NSView*)createSupervisedUserDisclaimerView;

// Creates a main profile card for the guest user.
- (NSView*)createGuestProfileView;

// Creates an item for the profile |itemIndex| that is used in the fast profile
// switcher in the middle of the bubble.
- (NSButton*)createOtherProfileView:(int)itemIndex;

// Creates the "Not you" and Lock option buttons.
- (NSView*)createOptionsViewWithRect:(NSRect)rect
                          enableLock:(BOOL)enableLock;

// Creates the account management view for the active profile.
- (NSView*)createCurrentProfileAccountsView:(NSRect)rect;

// Creates the list of accounts for the active profile.
- (NSView*)createAccountsListWithRect:(NSRect)rect;

// Creates the Gaia sign-in/add account view.
- (NSView*)buildGaiaEmbeddedView;

// Creates the account removal view.
- (NSView*)buildAccountRemovalView;

// Creates the end-preview view.
- (NSView*)buildEndPreviewView;

// Creates a button with |text|, an icon given by |imageResourceId| and with
// |action|. The icon |alternateImageResourceId| is displayed in the button's
// hovered and pressed states.
- (NSButton*)hoverButtonWithRect:(NSRect)rect
                            text:(NSString*)text
                 imageResourceId:(int)imageResourceId
        alternateImageResourceId:(int)alternateImageResourceId
                          action:(SEL)action;

// Creates a generic link button with |title| and an |action| positioned at
// |frameOrigin|.
- (NSButton*)linkButtonWithTitle:(NSString*)title
                     frameOrigin:(NSPoint)frameOrigin
                          action:(SEL)action;

// Creates an email account button with |title| and a remove icon. If
// |reauthRequired| is true, the button also displays a warning icon. |tag|
// indicates which account the button refers to.
- (NSButton*)accountButtonWithRect:(NSRect)rect
                             title:(const std::string&)title
                               tag:(int)tag
                    reauthRequired:(BOOL)reauthRequired;
@end

@implementation ProfileChooserController
- (profiles::BubbleViewMode) viewMode {
  return viewMode_;
}

- (IBAction)switchToProfile:(id)sender {
  // Check the event flags to see if a new window should be created.
  bool alwaysCreate = ui::WindowOpenDispositionFromNSEvent(
      [NSApp currentEvent]) == NEW_WINDOW;
  avatarMenu_->SwitchToProfile([sender tag], alwaysCreate,
                               ProfileMetrics::SWITCH_PROFILE_ICON);
}

- (IBAction)showUserManager:(id)sender {
  profiles::ShowUserManagerMaybeWithTutorial(browser_->profile());
}

- (IBAction)exitGuest:(id)sender {
  DCHECK(browser_->profile()->IsGuestSession());
  [self showUserManager:sender];
  profiles::CloseGuestProfileWindows();
}

- (IBAction)showAccountManagement:(id)sender {
  [self initMenuContentsWithView:profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT];
}

- (IBAction)hideAccountManagement:(id)sender {
  [self initMenuContentsWithView:profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER];
}

- (IBAction)lockProfile:(id)sender {
  profiles::LockProfile(browser_->profile());
  [self postActionPerformed:ProfileMetrics::PROFILE_DESKTOP_MENU_LOCK];
}

- (IBAction)showInlineSigninPage:(id)sender {
  [self initMenuContentsWithView:profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN];
}

- (IBAction)showTabbedSigninPage:(id)sender {
  chrome::ShowBrowserSignin(browser_, signin::SOURCE_MENU);
}

- (IBAction)addAccount:(id)sender {
  [self initMenuContentsWithView:profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT];
  [self postActionPerformed:ProfileMetrics::PROFILE_DESKTOP_MENU_ADD_ACCT];
}

- (IBAction)navigateBackFromSigninPage:(id)sender {
  std::string primaryAccount = SigninManagerFactory::GetForProfile(
      browser_->profile())->GetAuthenticatedUsername();
  [self initMenuContentsWithView:primaryAccount.empty() ?
      profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER :
      profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT];
}

- (IBAction)showAccountRemovalView:(id)sender {
  DCHECK(!isGuestSession_);

  // Tag is either |kPrimaryProfileTag| for the primary account, or equal to the
  // index in |currentProfileAccounts_| for a secondary account.
  int tag = [sender tag];
  if (tag == kPrimaryProfileTag) {
    accountIdToRemove_ = SigninManagerFactory::GetForProfile(
        browser_->profile())->GetAuthenticatedUsername();
  } else {
    DCHECK(ContainsKey(currentProfileAccounts_, tag));
    accountIdToRemove_ = currentProfileAccounts_[tag];
  }

  [self initMenuContentsWithView:profiles::BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL];
}

- (IBAction)showAccountReauthenticationView:(id)sender {
  DCHECK(!isGuestSession_);
  [self initMenuContentsWithView:profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH];
}

- (IBAction)removeAccount:(id)sender {
  DCHECK(!accountIdToRemove_.empty());
  ProfileOAuth2TokenServiceFactory::GetPlatformSpecificForProfile(
      browser_->profile())->RevokeCredentials(accountIdToRemove_);
  [self postActionPerformed:ProfileMetrics::PROFILE_DESKTOP_MENU_REMOVE_ACCT];
  accountIdToRemove_.clear();

  [self initMenuContentsWithView:profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT];
}

- (IBAction)openTutorialLearnMoreURL:(id)sender {
  ProfileMetrics::LogProfileUpgradeEnrollment(
      ProfileMetrics::PROFILE_ENROLLMENT_LAUNCH_LEARN_MORE);
  // TODO(guohui): update |learnMoreUrl| once it is decided.
  const GURL learnMoreUrl("https://support.google.com/chrome/?hl=en#to");
  chrome::NavigateParams params(browser_->profile(), learnMoreUrl,
                                content::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
}

- (IBAction)enableNewProfileManagementPreview:(id)sender {
  ProfileMetrics::LogProfileUpgradeEnrollment(
      ProfileMetrics::PROFILE_ENROLLMENT_ACCEPT_NEW_PROFILE_MGMT);
  profiles::EnableNewProfileManagementPreview(browser_->profile());
}

- (IBAction)dismissTutorial:(id)sender {
  ProfileMetrics::LogProfileUpgradeEnrollment(
      ProfileMetrics::PROFILE_ENROLLMENT_CLOSE_WELCOME_CARD);
  // If the user manually dismissed the tutorial, never show it again by setting
  // the number of times shown to the maximum plus 1, so that later we could
  // distinguish between the dismiss case and the case when the tutorial is
  // indeed shown for the maximum number of times.
  browser_->profile()->GetPrefs()->SetInteger(
      prefs::kProfileAvatarTutorialShown, kProfileAvatarTutorialShowMax + 1);
  [self initMenuContentsWithView:profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER];
}

- (IBAction)showSendFeedbackTutorial:(id)sender {
  ProfileMetrics::LogProfileUpgradeEnrollment(
      ProfileMetrics::PROFILE_ENROLLMENT_SEND_FEEDBACK);
  tutorialMode_ = profiles::TUTORIAL_MODE_SEND_FEEDBACK;
  [self initMenuContentsWithView:profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER];
}

- (IBAction)showEndPreviewView:(id)sender {
  [self initMenuContentsWithView:profiles::BUBBLE_VIEW_MODE_END_PREVIEW];
}

- (IBAction)sendFeedback:(id)sender {
  chrome::OpenFeedbackDialog(browser_);
}

- (IBAction)endPreviewAndRelaunch:(id)sender {
  profiles::DisableNewProfileManagementPreview(browser_->profile());
}

- (void)cleanUpEmbeddedViewContents {
  webContents_.reset();
}

- (id)initWithBrowser:(Browser*)browser
           anchoredAt:(NSPoint)point
             withMode:(profiles::BubbleViewMode)mode
      withServiceType:(signin::GAIAServiceType)serviceType {
  base::scoped_nsobject<InfoBubbleWindow> window([[InfoBubbleWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSBorderlessWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);

  if ((self = [super initWithWindow:window
                       parentWindow:browser->window()->GetNativeWindow()
                         anchoredAt:point])) {
    browser_ = browser;
    viewMode_ = mode;
    tutorialMode_ = profiles::TUTORIAL_MODE_NONE;
    observer_.reset(new ActiveProfileObserverBridge(self, browser_));
    serviceType_ = serviceType;

    avatarMenu_.reset(new AvatarMenu(
        &g_browser_process->profile_manager()->GetProfileInfoCache(),
        observer_.get(),
        browser_));
    avatarMenu_->RebuildMenu();

    // Guest profiles do not have a token service.
    isGuestSession_ = browser_->profile()->IsGuestSession();

    // If view mode is PROFILE_CHOOSER but there is an auth error, force
    // ACCOUNT_MANAGEMENT mode.
    if (viewMode_ == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER &&
        HasAuthError(browser_->profile())) {
      viewMode_ = profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT;
    }

    [[self bubble] setAlignment:info_bubble::kAlignRightEdgeToAnchorEdge];
    [[self bubble] setArrowLocation:info_bubble::kNoArrow];
    [[self bubble] setBackgroundColor:GetDialogBackgroundColor()];
    [self initMenuContentsWithView:viewMode_];
  }

  return self;
}

- (void)initMenuContentsWithView:(profiles::BubbleViewMode)viewToDisplay {
  if (browser_->profile()->IsSupervised() &&
      (viewToDisplay == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
       viewToDisplay == profiles::BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL)) {
    LOG(WARNING) << "Supervised user attempted to add/remove account";
    return;
  }
  viewMode_ = viewToDisplay;
  NSView* contentView = [[self window] contentView];
  [contentView setSubviews:[NSArray array]];
  NSView* subView;

  switch (viewMode_) {
    case profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN:
    case profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT:
    case profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH:
      subView = [self buildGaiaEmbeddedView];
      break;
    case profiles::BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL:
      subView = [self buildAccountRemovalView];
      break;
    case profiles::BUBBLE_VIEW_MODE_END_PREVIEW:
      subView = [self buildEndPreviewView];
      break;
    case profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER:
    case profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT:
      subView = [self buildProfileChooserView];
      break;
  }

  [contentView addSubview:subView];
  SetWindowSize([self window],
      NSMakeSize(NSWidth([subView frame]), NSHeight([subView frame])));
}

- (NSView*)buildProfileChooserView {
  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:NSZeroRect]);

  NSView* tutorialView = nil;
  NSView* currentProfileView = nil;
  base::scoped_nsobject<NSMutableArray> otherProfiles(
      [[NSMutableArray alloc] init]);
  // Local and guest profiles cannot lock their profile.
  bool enableLock = false;
  // Store the most recently displayed tutorial mode
  profiles::TutorialMode lastTutorialMode = tutorialMode_;

  // Loop over the profiles in reverse, so that they are sorted by their
  // y-coordinate, and separate them into active and "other" profiles.
  for (int i = avatarMenu_->GetNumberOfItems() - 1; i >= 0; --i) {
    const AvatarMenu::Item& item = avatarMenu_->GetItemAt(i);
    if (item.active) {
      if (viewMode_ == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER)
        tutorialView = [self buildPreviewTutorialIfNeeded:item];
      currentProfileView = [self createCurrentProfileView:item];
      enableLock = item.signed_in;
    } else {
      [otherProfiles addObject:[self createOtherProfileView:i]];
    }
  }
  if (!currentProfileView)  // Guest windows don't have an active profile.
    currentProfileView = [self createGuestProfileView];

  // |yOffset| is the next position at which to draw in |container|
  // coordinates. Add a pixel offset so that the bottom option buttons don't
  // overlap the bubble's rounded corners.
  CGFloat yOffset = 1;

  // Option buttons. Only available with the new profile management flag.
  if (switches::IsNewProfileManagement()) {
    NSRect rect = NSMakeRect(0, yOffset, kFixedMenuWidth, 0);
    NSView* optionsView = [self createOptionsViewWithRect:rect
                                               enableLock:enableLock];
    [container addSubview:optionsView];
    rect.origin.y = NSMaxY([optionsView frame]);

    NSBox* separator = [self horizontalSeparatorWithFrame:rect];
    [container addSubview:separator];
    yOffset = NSMaxY([separator frame]);
  }

  if (viewMode_ == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER &&
      switches::IsFastUserSwitching()) {
    // Other profiles switcher. The profiles have already been sorted
    // by their y-coordinate, so they can be added in the existing order.
    for (NSView *otherProfileView in otherProfiles.get()) {
      [otherProfileView setFrameOrigin:NSMakePoint(0, yOffset)];
      [container addSubview:otherProfileView];
      yOffset = NSMaxY([otherProfileView frame]);

      NSBox* separator = [self horizontalSeparatorWithFrame:NSMakeRect(
          0, yOffset, kFixedMenuWidth, 0)];
      [container addSubview:separator];
      yOffset = NSMaxY([separator frame]);
    }
  }

  // For supervised users, add the disclaimer text.
  if (browser_->profile()->IsSupervised()) {
    yOffset += kSmallVerticalSpacing;
    NSView* disclaimerContainer = [self createSupervisedUserDisclaimerView];
    [disclaimerContainer setFrameOrigin:NSMakePoint(0, yOffset)];
    [container addSubview:disclaimerContainer];
    yOffset = NSMaxY([disclaimerContainer frame]);
    yOffset += kSmallVerticalSpacing;

    NSBox* separator = [self horizontalSeparatorWithFrame:NSMakeRect(
        0, yOffset, kFixedMenuWidth, 0)];
    [container addSubview:separator];
    yOffset = NSMaxY([separator frame]);
  }

  if (viewMode_ == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT) {
    NSView* currentProfileAccountsView = [self createCurrentProfileAccountsView:
        NSMakeRect(0, yOffset, kFixedMenuWidth, 0)];
    [container addSubview:currentProfileAccountsView];
    yOffset = NSMaxY([currentProfileAccountsView frame]);

    NSBox* accountsSeparator = [self horizontalSeparatorWithFrame:
        NSMakeRect(0, yOffset, kFixedMenuWidth, 0)];
    [container addSubview:accountsSeparator];
    yOffset = NSMaxY([accountsSeparator frame]);
  }

  // Active profile card.
  if (currentProfileView) {
    yOffset += kVerticalSpacing;
    [currentProfileView setFrameOrigin:NSMakePoint(0, yOffset)];
    [container addSubview:currentProfileView];
    yOffset = NSMaxY([currentProfileView frame]) + kVerticalSpacing;
  }

  if (tutorialView) {
    [tutorialView setFrameOrigin:NSMakePoint(0, yOffset)];
    [container addSubview:tutorialView];
    yOffset = NSMaxY([tutorialView frame]);
    if (!switches::IsNewProfileManagement() &&
        tutorialMode_ != lastTutorialMode) {
      ProfileMetrics::LogProfileUpgradeEnrollment(
          ProfileMetrics::PROFILE_ENROLLMENT_SHOW_PREVIEW_PROMO);
    }
  } else {
    tutorialMode_ = profiles::TUTORIAL_MODE_NONE;
  }

  [container setFrameSize:NSMakeSize(kFixedMenuWidth, yOffset)];
  return container.autorelease();
}

- (NSView*)buildPreviewTutorialIfNeeded:(const AvatarMenu::Item&)item {
  if (!switches::IsNewProfileManagement()) {
    return [self tutorialViewWithMode:profiles::TUTORIAL_MODE_ENABLE_PREVIEW
                         titleMessage:IDS_PROFILES_PREVIEW_TUTORIAL_TITLE
                       contentMessage:IDS_PROFILES_PREVIEW_TUTORIAL_CONTENT_TEXT
                          linkMessage:IDS_PROFILES_PROFILE_TUTORIAL_LEARN_MORE
                        buttonMessage:IDS_PROFILES_TUTORIAL_TRY_BUTTON
                           linkAction:@selector(openTutorialLearnMoreURL:)
                         buttonAction:
        @selector(enableNewProfileManagementPreview:)];
  }

  if (!switches::IsNewProfileManagementPreviewEnabled())
    return nil;

  if (tutorialMode_ == profiles::TUTORIAL_MODE_SEND_FEEDBACK) {
    return [self tutorialViewWithMode:profiles::TUTORIAL_MODE_SEND_FEEDBACK
                         titleMessage:IDS_PROFILES_FEEDBACK_TUTORIAL_TITLE
                       contentMessage:
        IDS_PROFILES_FEEDBACK_TUTORIAL_CONTENT_TEXT
                          linkMessage:IDS_PROFILES_END_PREVIEW
                        buttonMessage:IDS_PROFILES_SEND_FEEDBACK_BUTTON
                           linkAction:@selector(showEndPreviewView:)
                         buttonAction:@selector(sendFeedback:)];
  }

  Profile* profile = browser_->profile();
  const int showCount = profile->GetPrefs()->GetInteger(
      prefs::kProfileAvatarTutorialShown);
  // Do not show the tutorial if user has dismissed it.
  if (showCount > kProfileAvatarTutorialShowMax)
    return nil;

  if (tutorialMode_ != profiles::TUTORIAL_MODE_WELCOME) {
    if (showCount == kProfileAvatarTutorialShowMax)
      return nil;
    profile->GetPrefs()->SetInteger(
        prefs::kProfileAvatarTutorialShown, showCount + 1);
  }

  return [self tutorialViewWithMode:profiles::TUTORIAL_MODE_WELCOME
                       titleMessage:IDS_PROFILES_PREVIEW_ENABLED_TUTORIAL_TITLE
                     contentMessage:
      IDS_PROFILES_PREVIEW_ENABLED_TUTORIAL_CONTENT_TEXT
                        linkMessage:IDS_PROFILES_PROFILE_TUTORIAL_LEARN_MORE
                      buttonMessage:IDS_PROFILES_TUTORIAL_OK_BUTTON
                         linkAction:@selector(openTutorialLearnMoreURL:)
                       buttonAction:@selector(dismissTutorial:)];
}

- (NSView*)tutorialViewWithMode:(profiles::TutorialMode)mode
                   titleMessage:(int)titleMessageId
                 contentMessage:(int)contentMessageId
                    linkMessage:(int)linkMessageId
                  buttonMessage:(int)buttonMessageId
                     linkAction:(SEL)linkAction
                   buttonAction:(SEL)buttonAction {
  tutorialMode_ = mode;

  NSColor* tutorialBackgroundColor =
      gfx::SkColorToSRGBNSColor(profiles::kAvatarTutorialBackgroundColor);
  base::scoped_nsobject<NSView> container([[BackgroundColorView alloc]
      initWithFrame:NSMakeRect(0, 0, kFixedMenuWidth, 0)
          withColor:tutorialBackgroundColor]);
  CGFloat availableWidth = kFixedMenuWidth - 2 * kHorizontalSpacing;
  CGFloat yOffset = kSmallVerticalSpacing;

  // Adds links and buttons at the bottom.
  base::scoped_nsobject<NSButton> tutorialOkButton([[HoverButton alloc]
      initWithFrame:NSZeroRect]);
  [tutorialOkButton setTitle:l10n_util::GetNSString(
      buttonMessageId)];
  [tutorialOkButton setBezelStyle:NSRoundedBezelStyle];
  [tutorialOkButton setTarget:self];
  [tutorialOkButton setAction:buttonAction];
  [tutorialOkButton sizeToFit];
  NSSize buttonSize = [tutorialOkButton frame].size;
  const CGFloat kTopBottomTextPadding = 6;
  const CGFloat kLeftRightTextPadding = 15;
  buttonSize.width += 2 * kLeftRightTextPadding;
  buttonSize.height += 2 * kTopBottomTextPadding;
  [tutorialOkButton setFrameSize:buttonSize];
  [tutorialOkButton setAlignment:NSCenterTextAlignment];
  [tutorialOkButton setFrameOrigin:NSMakePoint(
      kFixedMenuWidth - NSWidth([tutorialOkButton frame]) - kHorizontalSpacing,
      yOffset)];
  [container addSubview:tutorialOkButton];

  NSButton* learnMoreLink =
      [self linkButtonWithTitle:l10n_util::GetNSString(linkMessageId)
                    frameOrigin:NSZeroPoint
                         action:linkAction];
  [[learnMoreLink cell] setTextColor:[NSColor whiteColor]];
  CGFloat linkYOffset = yOffset + (NSHeight([tutorialOkButton frame]) -
                                   NSHeight([learnMoreLink frame])) / 2;
  [learnMoreLink setFrameOrigin:NSMakePoint(kHorizontalSpacing, linkYOffset)];
  [container addSubview:learnMoreLink];

  yOffset = std::max(NSMaxY([learnMoreLink frame]),
                     NSMaxY([tutorialOkButton frame])) + kVerticalSpacing;

  // Adds body content.
  NSTextField* contentLabel = BuildLabel(
      l10n_util::GetNSString(contentMessageId),
      NSMakePoint(kHorizontalSpacing, yOffset),
      tutorialBackgroundColor,
      gfx::SkColorToSRGBNSColor(profiles::kAvatarTutorialContentTextColor));
  [contentLabel setFrameSize:NSMakeSize(availableWidth, 0)];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:contentLabel];
  [container addSubview:contentLabel];
  yOffset = NSMaxY([contentLabel frame]) + kSmallVerticalSpacing;

  // Adds title.
  NSTextField* titleLabel =
      BuildLabel(l10n_util::GetNSString(titleMessageId),
                 NSMakePoint(kHorizontalSpacing, yOffset),
                 tutorialBackgroundColor,
                 [NSColor whiteColor] /* text_color */);
  [titleLabel setFont:[NSFont labelFontOfSize:kTitleFontSize]];
  [titleLabel sizeToFit];
  [titleLabel setFrameSize:
      NSMakeSize(availableWidth, NSHeight([titleLabel frame]))];
  [container addSubview:titleLabel];
  yOffset = NSMaxY([titleLabel frame]) + kVerticalSpacing;

  [container setFrameSize:NSMakeSize(kFixedMenuWidth, yOffset)];

  // Adds caret at the bottom.
  NSImage* caretImage = ui::ResourceBundle::GetSharedInstance().
      GetNativeImageNamed(IDR_ICON_PROFILES_MENU_CARET).AsNSImage();
  base::scoped_nsobject<NSImageView> caretView(
      [[NSImageView alloc] initWithFrame:NSMakeRect(
          kHorizontalSpacing, 0, caretImage.size.width,
          caretImage.size.height)]);
  [caretView setImage:caretImage];

  base::scoped_nsobject<NSView> containerWithCaret([[NSView alloc]
      initWithFrame:NSMakeRect(0, 0, kFixedMenuWidth, 0)]);
  [containerWithCaret addSubview:caretView];

  [container setFrameOrigin:NSMakePoint(0, caretImage.size.height)];
  [containerWithCaret addSubview:container];

  [containerWithCaret setFrameSize:
      NSMakeSize(kFixedMenuWidth, NSMaxY([container frame]))];
  return containerWithCaret.autorelease();
}

- (NSView*)createCurrentProfileView:(const AvatarMenu::Item&)item {
  base::scoped_nsobject<NSView> container([[NSView alloc]
      initWithFrame:NSZeroRect]);

  CGFloat xOffset = kHorizontalSpacing;
  CGFloat yOffset = 0;
  CGFloat availableTextWidth = kFixedMenuWidth - 2 * kHorizontalSpacing;

  // Profile options. This can be a link to the accounts view, the profile's
  // username for signed in users, or a "Sign in" button for local profiles.
  SigninManagerBase* signinManager =
      SigninManagerFactory::GetForProfile(
          browser_->profile()->GetOriginalProfile());
  if (!isGuestSession_ && signinManager->IsSigninAllowed()) {
    NSView* linksContainer =
        [self createCurrentProfileLinksForItem:item
                                          rect:NSMakeRect(xOffset, yOffset,
                                                          availableTextWidth,
                                                          0)];
    [container addSubview:linksContainer];
    yOffset = NSMaxY([linksContainer frame]);
  }

  // Profile name, centered.
  bool editingAllowed = !isGuestSession_ &&
                        !browser_->profile()->IsSupervised();
  base::scoped_nsobject<EditableProfileNameButton> profileName(
      [[EditableProfileNameButton alloc]
          initWithFrame:NSMakeRect(xOffset,
                                   yOffset,
                                   availableTextWidth,
                                   kProfileButtonHeight)
                profile:browser_->profile()
            profileName:base::SysUTF16ToNSString(
                            profiles::GetAvatarNameForProfile(
                                browser_->profile()->GetPath()))
         editingAllowed:editingAllowed
         withController:self]);

  [container addSubview:profileName];
  yOffset = NSMaxY([profileName frame]);

  // Profile icon, centered.
  xOffset = (kFixedMenuWidth - kLargeImageSide) / 2;
  base::scoped_nsobject<EditableProfilePhoto> iconView(
      [[EditableProfilePhoto alloc]
          initWithFrame:NSMakeRect(xOffset, yOffset,
                                   kLargeImageSide, kLargeImageSide)
             avatarMenu:avatarMenu_.get()
            profileIcon:item.icon
         editingAllowed:!isGuestSession_
         withController:self]);

  [container addSubview:iconView];
  yOffset = NSMaxY([iconView frame]);

  if (browser_->profile()->IsSupervised()) {
    base::scoped_nsobject<NSImageView> supervisedIcon(
        [[NSImageView alloc] initWithFrame:NSZeroRect]);
    ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
    [supervisedIcon setImage:rb->GetNativeImageNamed(
        IDR_ICON_PROFILES_MENU_SUPERVISED).ToNSImage()];
    NSSize size = [[supervisedIcon image] size];
    [supervisedIcon setFrameSize:size];
    NSRect parentFrame = [iconView frame];
    [supervisedIcon setFrameOrigin:NSMakePoint(NSMaxX(parentFrame) - size.width,
                                               NSMinY(parentFrame))];
    [container addSubview:supervisedIcon];
  }

  if (switches::IsNewProfileManagementPreviewEnabled()) {
    base::scoped_nsobject<HoverImageButton> questionButton(
        [[HoverImageButton alloc] initWithFrame:NSZeroRect]);
    [questionButton setBordered:NO];
    ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
    [questionButton setDefaultImage:rb->GetNativeImageNamed(
        IDR_ICON_PROFILES_MENU_QUESTION_STABLE).ToNSImage()];
    [questionButton setHoverImage:rb->GetNativeImageNamed(
        IDR_ICON_PROFILES_MENU_QUESTION_HOVER).ToNSImage()];
    [questionButton setPressedImage:rb->GetNativeImageNamed(
        IDR_ICON_PROFILES_MENU_QUESTION_SELECT).ToNSImage()];
    [questionButton setTarget:self];
    [questionButton setAction:@selector(showSendFeedbackTutorial:)];
    [questionButton sizeToFit];
    const CGFloat size = NSHeight([questionButton frame]) + 2;
    [questionButton setFrame:
        NSMakeRect(kHorizontalSpacing, yOffset - size, size, size)];
    [container addSubview:questionButton];
  }

  [container setFrameSize:NSMakeSize(kFixedMenuWidth, yOffset)];
  return container.autorelease();
}

- (NSView*)createCurrentProfileLinksForItem:(const AvatarMenu::Item&)item
                                       rect:(NSRect)rect {
  base::scoped_nsobject<NSView> container([[NSView alloc] initWithFrame:rect]);

  // Don't double-apply the left margin to the sub-views.
  rect.origin.x = 0;

  // The available links depend on the type of profile that is active.
  NSButton* link;
  if (item.signed_in) {
    if (switches::IsNewProfileManagement()) {
      NSString* linkTitle = l10n_util::GetNSString(
          viewMode_ == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER ?
              IDS_PROFILES_PROFILE_MANAGE_ACCOUNTS_BUTTON :
              IDS_PROFILES_PROFILE_HIDE_MANAGE_ACCOUNTS_BUTTON);
      SEL linkSelector =
          (viewMode_ == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER) ?
          @selector(showAccountManagement:) : @selector(hideAccountManagement:);
      link = [self linkButtonWithTitle:linkTitle
                           frameOrigin:rect.origin
                                action:linkSelector];
    } else {
      link = [self linkButtonWithTitle:base::SysUTF16ToNSString(item.sync_state)
                           frameOrigin:rect.origin
                                action:nil];
      [[link cell] setTextColor:[NSColor blackColor]];
    }
    // -linkButtonWithTitle sizeToFit's the link, so re-stretch it so that it
    // can be centered correctly in the view.
    rect.size.height = NSMaxY([link frame]);
    [link setFrame:rect];
    [link setAlignment:NSCenterTextAlignment];
  } else {
    rect.size.height = kBlueButtonHeight;
    link = [[BlueLabelButton alloc] initWithFrame:rect];

    // Manually elide the button text so that the contents fit inside the bubble
    // This is needed because the BlueLabelButton cell resets the style on
    // every call to -cellSize, which prevents setting a custom lineBreakMode.
    NSString* elidedButtonText = base::SysUTF16ToNSString(gfx::ElideText(
        l10n_util::GetStringFUTF16(
            IDS_SYNC_START_SYNC_BUTTON_LABEL,
            l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)),
        gfx::FontList(), rect.size.width, gfx::ELIDE_TAIL));

    [link setTitle:elidedButtonText];
    [link setTarget:self];
    [link setAction:switches::IsNewProfileManagement() ?
        @selector(showInlineSigninPage:) : @selector(showTabbedSigninPage:)];
  }

  [container addSubview:link];
  [container setFrameSize:rect.size];
  return container.autorelease();
}

- (NSView*)createSupervisedUserDisclaimerView {
  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:NSZeroRect]);

  int yOffset = 0;
  int availableTextWidth = kFixedMenuWidth - 2 * kHorizontalSpacing;

  NSTextField* disclaimer = BuildLabel(
      base::SysUTF16ToNSString(avatarMenu_->GetSupervisedUserInformation()),
      NSMakePoint(kHorizontalSpacing, yOffset),
      nil /* background_color */,
      nil /* text_color */);
  [disclaimer setFrameSize:NSMakeSize(availableTextWidth, 0)];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:disclaimer];
  yOffset = NSMaxY([disclaimer frame]);

  [container addSubview:disclaimer];
  [container setFrameSize:NSMakeSize(kFixedMenuWidth, yOffset)];
  return container.autorelease();
}

- (NSView*)createGuestProfileView {
  gfx::Image guestIcon =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          profiles::GetPlaceholderAvatarIconResourceID());
  AvatarMenu::Item guestItem(std::string::npos, /* menu_index, not used */
                             std::string::npos, /* profile_index, not used */
                             guestIcon);
  guestItem.active = true;
  guestItem.name = base::SysNSStringToUTF16(
      l10n_util::GetNSString(IDS_PROFILES_GUEST_PROFILE_NAME));

  return [self createCurrentProfileView:guestItem];
}

- (NSButton*)createOtherProfileView:(int)itemIndex {
  const AvatarMenu::Item& item = avatarMenu_->GetItemAt(itemIndex);

  NSRect rect = NSMakeRect(0, 0, kFixedMenuWidth,
                           kBlueButtonHeight + kSmallVerticalSpacing);
  base::scoped_nsobject<BackgroundColorHoverButton> profileButton(
      [[BackgroundColorHoverButton alloc]
          initWithFrame:rect
      imageTitleSpacing:kImageTitleSpacing
        backgroundColor:GetDialogBackgroundColor()]);
  [profileButton setTitle:base::SysUTF16ToNSString(item.name)];
  [profileButton setDefaultImage:CreateProfileImage(
      item.icon, kSmallImageSide).ToNSImage()];
  [profileButton setImagePosition:NSImageLeft];
  [profileButton setAlignment:NSLeftTextAlignment];
  [profileButton setBordered:NO];
  [profileButton setTag:itemIndex];
  [profileButton setTarget:self];
  [profileButton setAction:@selector(switchToProfile:)];

  return profileButton.autorelease();
}

- (NSView*)createOptionsViewWithRect:(NSRect)rect
                          enableLock:(BOOL)enableLock {
  int widthOfLockButton = enableLock ? 2 * kHorizontalSpacing + 14 : 0;
  NSRect viewRect = NSMakeRect(0, 0,
                               rect.size.width - widthOfLockButton,
                               kBlueButtonHeight + kVerticalSpacing);
  NSString* text = isGuestSession_ ?
      l10n_util::GetNSString(IDS_PROFILES_EXIT_GUEST) :
      l10n_util::GetNSStringF(IDS_PROFILES_NOT_YOU_BUTTON,
          profiles::GetAvatarNameForProfile(
              browser_->profile()->GetPath()));
  NSButton* notYouButton =
      [self hoverButtonWithRect:viewRect
                           text:text
                imageResourceId:IDR_ICON_PROFILES_MENU_AVATAR
       alternateImageResourceId:IDR_ICON_PROFILES_MENU_AVATAR
                         action:isGuestSession_? @selector(exitGuest:) :
                                                 @selector(showUserManager:)];

  rect.size.height = NSMaxY([notYouButton frame]);
  base::scoped_nsobject<NSView> container([[NSView alloc] initWithFrame:rect]);
  [container addSubview:notYouButton];

  if (enableLock) {
    viewRect.origin.x = NSMaxX([notYouButton frame]);
    NSBox* separator = [self verticalSeparatorWithFrame:viewRect];
    [container addSubview:separator];

    viewRect.origin.x = NSMaxX([separator frame]);
    viewRect.size.width = widthOfLockButton;
    NSButton* lockButton =
        [self hoverButtonWithRect:viewRect
                             text:@""
                  imageResourceId:IDR_ICON_PROFILES_MENU_LOCK
         alternateImageResourceId:IDR_ICON_PROFILES_MENU_LOCK
                           action:@selector(lockProfile:)];
    [container addSubview:lockButton];
  }

  return container.autorelease();
}

- (NSView*)createCurrentProfileAccountsView:(NSRect)rect {
  const CGFloat kAccountButtonHeight = 34;

  const AvatarMenu::Item& item =
      avatarMenu_->GetItemAt(avatarMenu_->GetActiveProfileIndex());
  DCHECK(item.signed_in);

  NSColor* backgroundColor = gfx::SkColorToCalibratedNSColor(
      profiles::kAvatarBubbleAccountsBackgroundColor);
  base::scoped_nsobject<NSView> container([[BackgroundColorView alloc]
      initWithFrame:rect
          withColor:backgroundColor]);

  rect.origin.y = 0;
  if (!browser_->profile()->IsSupervised()) {
    // Manually elide the button text so the contents fit inside the bubble.
    // This is needed because the BlueLabelButton cell resets the style on
    // every call to -cellSize, which prevents setting a custom lineBreakMode.
    NSString* elidedButtonText = base::SysUTF16ToNSString(gfx::ElideText(
        l10n_util::GetStringFUTF16(
            IDS_PROFILES_PROFILE_ADD_ACCOUNT_BUTTON, item.name),
        gfx::FontList(), rect.size.width, gfx::ELIDE_TAIL));

    NSButton* addAccountsButton =
        [self linkButtonWithTitle:elidedButtonText
                      frameOrigin:NSMakePoint(
            kHorizontalSpacing, kSmallVerticalSpacing)
                           action:@selector(addAccount:)];
    [container addSubview:addAccountsButton];
    rect.origin.y += kAccountButtonHeight;
  }

  NSView* accountEmails = [self createAccountsListWithRect:NSMakeRect(
      0, rect.origin.y, rect.size.width, kAccountButtonHeight)];
  [container addSubview:accountEmails];

  [container setFrameSize:NSMakeSize(rect.size.width,
                                     NSMaxY([accountEmails frame]))];
  return container.autorelease();
}

- (NSView*)createAccountsListWithRect:(NSRect)rect {
  base::scoped_nsobject<NSView> container([[NSView alloc] initWithFrame:rect]);
  currentProfileAccounts_.clear();

  Profile* profile = browser_->profile();
  std::string primaryAccount =
      SigninManagerFactory::GetForProfile(profile)->GetAuthenticatedUsername();
  DCHECK(!primaryAccount.empty());
  std::vector<std::string>accounts =
      profiles::GetSecondaryAccountsForProfile(profile, primaryAccount);

  // If there is an account with an authentication error, it needs to be
  // badged with a warning icon.
  const SigninErrorController* errorController =
      profiles::GetSigninErrorController(profile);
  std::string errorAccountId =
      errorController ? errorController->error_account_id() : std::string();

  rect.origin.y = 0;
  for (size_t i = 0; i < accounts.size(); ++i) {
    // Save the original email address, as the button text could be elided.
    currentProfileAccounts_[i] = accounts[i];
    NSButton* accountButton =
        [self accountButtonWithRect:rect
                              title:accounts[i]
                                tag:i
                     reauthRequired:errorAccountId == accounts[i]];
    [container addSubview:accountButton];
    rect.origin.y = NSMaxY([accountButton frame]);
  }

  // The primary account should always be listed first.
  NSButton* accountButton =
      [self accountButtonWithRect:rect
                            title:primaryAccount
                              tag:kPrimaryProfileTag
                   reauthRequired:errorAccountId == primaryAccount];
  [container addSubview:accountButton];
  [container setFrameSize:NSMakeSize(NSWidth([container frame]),
                                     NSMaxY([accountButton frame]))];
  return container.autorelease();
}

- (NSView*)buildGaiaEmbeddedView {
  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:NSZeroRect]);
  CGFloat yOffset = 0;

  GURL url;
  int messageId = -1;
  SigninErrorController* errorController = NULL;
  switch (viewMode_) {
    case profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN:
      url = signin::GetPromoURL(signin::SOURCE_AVATAR_BUBBLE_SIGN_IN,
                                false /* auto_close */,
                                true /* is_constrained */);
      messageId = IDS_PROFILES_GAIA_SIGNIN_TITLE;
      break;
    case profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT:
      url = signin::GetPromoURL(signin::SOURCE_AVATAR_BUBBLE_ADD_ACCOUNT,
                                false /* auto_close */,
                                true /* is_constrained */);
      messageId = IDS_PROFILES_GAIA_ADD_ACCOUNT_TITLE;
      break;
    case profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH:
      DCHECK(HasAuthError(browser_->profile()));
      errorController = profiles::GetSigninErrorController(browser_->profile());
      url = signin::GetReauthURL(
          browser_->profile(),
          errorController ? errorController->error_username() : std::string());
      messageId = IDS_PROFILES_GAIA_REAUTH_TITLE;
      break;
    default:
      NOTREACHED() << "Called with invalid mode=" << viewMode_;
      break;
  }

  webContents_.reset(content::WebContents::Create(
      content::WebContents::CreateParams(browser_->profile())));
  webContents_->GetController().LoadURL(url,
                                        content::Referrer(),
                                        content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                        std::string());
  NSView* webview = webContents_->GetNativeView();
  [webview setFrameSize:NSMakeSize(kFixedGaiaViewWidth, kFixedGaiaViewHeight)];
  [container addSubview:webview];
  yOffset = NSMaxY([webview frame]);

  // Adds the title card.
  NSBox* separator = [self horizontalSeparatorWithFrame:
      NSMakeRect(0, yOffset, kFixedGaiaViewWidth, 0)];
  [container addSubview:separator];
  yOffset = NSMaxY([separator frame]) + kSmallVerticalSpacing;

  NSView* titleView = BuildTitleCard(
      NSMakeRect(0, yOffset, kFixedGaiaViewWidth, 0),
      messageId,
      self /* backButtonTarget*/,
      @selector(navigateBackFromSigninPage:) /* backButtonAction */);
  [container addSubview:titleView];
  yOffset = NSMaxY([titleView frame]);

  [container setFrameSize:NSMakeSize(kFixedGaiaViewWidth, yOffset)];
  return container.autorelease();
}

- (NSView*)buildAccountRemovalView {
  DCHECK(!accountIdToRemove_.empty());

  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:NSZeroRect]);
  CGFloat availableWidth =
      kFixedAccountRemovalViewWidth - 2 * kHorizontalSpacing;
  CGFloat yOffset = kVerticalSpacing;

  const std::string& primaryAccount = SigninManagerFactory::GetForProfile(
      browser_->profile())->GetAuthenticatedUsername();
  bool isPrimaryAccount = primaryAccount == accountIdToRemove_;

  // Adds "remove account" button at the bottom if needed.
  if (!isPrimaryAccount) {
    base::scoped_nsobject<NSButton> removeAccountButton(
        [[BlueLabelButton alloc] initWithFrame:NSZeroRect]);
    [removeAccountButton setTitle:l10n_util::GetNSString(
        IDS_PROFILES_ACCOUNT_REMOVAL_BUTTON)];
    [removeAccountButton setTarget:self];
    [removeAccountButton setAction:@selector(removeAccount:)];
    [removeAccountButton sizeToFit];
    [removeAccountButton setAlignment:NSCenterTextAlignment];
    CGFloat xOffset = (kFixedAccountRemovalViewWidth -
        NSWidth([removeAccountButton frame])) / 2;
    [removeAccountButton setFrameOrigin:NSMakePoint(xOffset, yOffset)];
    [container addSubview:removeAccountButton];

    yOffset = NSMaxY([removeAccountButton frame]) + kVerticalSpacing;
  }

  NSView* contentView;
  NSPoint contentFrameOrigin = NSMakePoint(kHorizontalSpacing, yOffset);
  if (isPrimaryAccount) {
    std::vector<size_t> offsets;
    NSString* contentStr = l10n_util::GetNSStringF(
        IDS_PROFILES_PRIMARY_ACCOUNT_REMOVAL_TEXT,
        base::UTF8ToUTF16(accountIdToRemove_), base::string16(), &offsets);
    NSString* linkStr = l10n_util::GetNSString(IDS_PROFILES_SETTINGS_LINK);
    contentView = BuildFixedWidthTextViewWithLink(self, contentStr, linkStr,
        offsets[1], contentFrameOrigin, availableWidth);
  } else {
    NSString* contentStr =
        l10n_util::GetNSString(IDS_PROFILES_ACCOUNT_REMOVAL_TEXT);
    NSTextField* contentLabel = BuildLabel(contentStr, contentFrameOrigin,
        GetDialogBackgroundColor(), nil /* text_color */);
    [contentLabel setFrameSize:NSMakeSize(availableWidth, 0)];
    [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:contentLabel];
    contentView = contentLabel;
  }
  [container addSubview:contentView];
  yOffset = NSMaxY([contentView frame]) + kVerticalSpacing;

  // Adds the title card.
  NSBox* separator = [self horizontalSeparatorWithFrame:
      NSMakeRect(0, yOffset, kFixedAccountRemovalViewWidth, 0)];
  [container addSubview:separator];
  yOffset = NSMaxY([separator frame]) + kSmallVerticalSpacing;

  NSView* titleView = BuildTitleCard(
      NSMakeRect(0, yOffset, kFixedAccountRemovalViewWidth,0),
      IDS_PROFILES_ACCOUNT_REMOVAL_TITLE,
      self /* backButtonTarget*/,
      @selector(showAccountManagement:) /* backButtonAction */);
  [container addSubview:titleView];
  yOffset = NSMaxY([titleView frame]);

  [container setFrameSize:NSMakeSize(kFixedAccountRemovalViewWidth, yOffset)];
  return container.autorelease();
}

- (NSView*)buildEndPreviewView {
  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:NSZeroRect]);
  CGFloat availableWidth =
      kFixedEndPreviewViewWidth - 2 * kHorizontalSpacing;
  CGFloat yOffset = kVerticalSpacing;

  // Adds the "end preview and relaunch" button at the bottom.
  base::scoped_nsobject<NSButton> endPreviewAndRelaunchButton(
      [[BlueLabelButton alloc] initWithFrame:NSZeroRect]);
  [endPreviewAndRelaunchButton setTitle:l10n_util::GetNSString(
      IDS_PROFILES_END_PREVIEW_AND_RELAUNCH)];
  [endPreviewAndRelaunchButton setTarget:self];
  [endPreviewAndRelaunchButton setAction:@selector(endPreviewAndRelaunch:)];
  [endPreviewAndRelaunchButton sizeToFit];
  [endPreviewAndRelaunchButton setAlignment:NSCenterTextAlignment];
  CGFloat xOffset = (kFixedEndPreviewViewWidth -
      NSWidth([endPreviewAndRelaunchButton frame])) / 2;
  [endPreviewAndRelaunchButton setFrameOrigin:NSMakePoint(xOffset, yOffset)];
  [container addSubview:endPreviewAndRelaunchButton];
  yOffset = NSMaxY([endPreviewAndRelaunchButton frame]) + kVerticalSpacing;

  // Adds the main text label.
  NSPoint contentFrameOrigin = NSMakePoint(kHorizontalSpacing, yOffset);
  NSString* contentStr =
      l10n_util::GetNSString(IDS_PROFILES_END_PREVIEW_TEXT);
  NSTextField* contentLabel = BuildLabel(contentStr, contentFrameOrigin,
      GetDialogBackgroundColor(), nil /* text_color */);
  [contentLabel setFrameSize:NSMakeSize(availableWidth, 0)];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:contentLabel];
  [container addSubview:contentLabel];
  yOffset = NSMaxY([contentLabel frame]) + kVerticalSpacing;

  // Adds the title card.
  NSBox* separator = [self horizontalSeparatorWithFrame:
      NSMakeRect(0, yOffset, kFixedEndPreviewViewWidth, 0)];
  [container addSubview:separator];
  yOffset = NSMaxY([separator frame]) + kSmallVerticalSpacing;

  NSView* titleView = BuildTitleCard(
      NSMakeRect(0, yOffset, kFixedEndPreviewViewWidth, 0),
      IDS_PROFILES_END_PREVIEW,
      self /* backButtonTarget*/,
      @selector(showSendFeedbackTutorial:) /* backButtonAction */);
  [container addSubview:titleView];
  yOffset = NSMaxY([titleView frame]);

  [container setFrameSize:NSMakeSize(kFixedEndPreviewViewWidth, yOffset)];
  return container.autorelease();
}

// Called when clicked on the settings link.
- (BOOL)textView:(NSTextView*)textView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  chrome::ShowSettings(browser_);
  return YES;
}

- (NSButton*)hoverButtonWithRect:(NSRect)rect
                            text:(NSString*)text
                 imageResourceId:(int)imageResourceId
        alternateImageResourceId:(int)alternateImageResourceId
                          action:(SEL)action {
  base::scoped_nsobject<BackgroundColorHoverButton> button(
      [[BackgroundColorHoverButton alloc]
          initWithFrame:rect
      imageTitleSpacing:kImageTitleSpacing
        backgroundColor:GetDialogBackgroundColor()]);

  [button setTitle:text];
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  NSImage* alternateImage = rb->GetNativeImageNamed(
      alternateImageResourceId).ToNSImage();
  [button setDefaultImage:rb->GetNativeImageNamed(imageResourceId).ToNSImage()];
  [button setHoverImage:alternateImage];
  [button setPressedImage:alternateImage];
  [button setImagePosition:NSImageLeft];
  [button setAlignment:NSLeftTextAlignment];
  [button setBordered:NO];
  [button setTarget:self];
  [button setAction:action];

  return button.autorelease();
}

- (NSButton*)linkButtonWithTitle:(NSString*)title
                     frameOrigin:(NSPoint)frameOrigin
                          action:(SEL)action {
  base::scoped_nsobject<NSButton> link(
      [[HyperlinkButtonCell buttonWithString:title] retain]);

  [[link cell] setShouldUnderline:NO];
  [[link cell] setTextColor:gfx::SkColorToCalibratedNSColor(
      chrome_style::GetLinkColor())];
  [link setTitle:title];
  [link setBordered:NO];
  [link setFont:[NSFont labelFontOfSize:kTextFontSize]];
  [link setTarget:self];
  [link setAction:action];
  [link setFrameOrigin:frameOrigin];
  [link sizeToFit];

  return link.autorelease();
}

- (NSButton*)accountButtonWithRect:(NSRect)rect
                             title:(const std::string&)title
                               tag:(int)tag
                    reauthRequired:(BOOL)reauthRequired {
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  NSImage* deleteImage = rb->GetNativeImageNamed(IDR_CLOSE_1).ToNSImage();
  CGFloat deleteImageWidth = [deleteImage size].width;
  NSImage* warningImage = reauthRequired ? rb->GetNativeImageNamed(
      IDR_ICON_PROFILES_ACCOUNT_BUTTON_ERROR).ToNSImage() : nil;
  CGFloat warningImageWidth = [warningImage size].width;

  CGFloat availableTextWidth = rect.size.width - kHorizontalSpacing -
      warningImageWidth - deleteImageWidth;
  if (warningImage)
    availableTextWidth -= kHorizontalSpacing;

  NSColor* backgroundColor = gfx::SkColorToCalibratedNSColor(
      profiles::kAvatarBubbleAccountsBackgroundColor);
  base::scoped_nsobject<BackgroundColorHoverButton> button(
      [[BackgroundColorHoverButton alloc] initWithFrame:rect
                                      imageTitleSpacing:0
                                        backgroundColor:backgroundColor]);
  [button setTitle:ElideEmail(title, availableTextWidth)];
  [button setAlignment:NSLeftTextAlignment];
  [button setBordered:NO];
  if (reauthRequired) {
    [button setDefaultImage:warningImage];
    [button setImagePosition:NSImageLeft];
    [button setTarget:self];
    [button setAction:@selector(showAccountReauthenticationView:)];
    [button setTag:tag];
  }

  // Delete button.
  if (!browser_->profile()->IsSupervised()) {
    NSRect buttonRect;
    NSDivideRect(rect, &buttonRect, &rect,
        deleteImageWidth + kHorizontalSpacing, NSMaxXEdge);
    buttonRect.origin.y = 0;

    base::scoped_nsobject<HoverImageButton> deleteButton(
        [[HoverImageButton alloc] initWithFrame:buttonRect]);
    [deleteButton setBordered:NO];
    [deleteButton setDefaultImage:deleteImage];
    [deleteButton setHoverImage:rb->GetNativeImageNamed(
        IDR_CLOSE_1_H).ToNSImage()];
    [deleteButton setPressedImage:rb->GetNativeImageNamed(
        IDR_CLOSE_1_P).ToNSImage()];
    [deleteButton setTarget:self];
    [deleteButton setAction:@selector(showAccountRemovalView:)];
    [deleteButton setTag:tag];

    [button addSubview:deleteButton];
  }

  return button.autorelease();
}

- (void)postActionPerformed:(ProfileMetrics::ProfileDesktopMenu)action {
  ProfileMetrics::LogProfileDesktopMenu(action, serviceType_);
  serviceType_ = signin::GAIA_SERVICE_TYPE_NONE;
}

@end
