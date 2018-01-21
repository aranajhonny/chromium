// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_L10N_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_L10N_UTIL_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"

namespace base {
class ListValue;
}

namespace chromeos {

// GetUILanguageList() returns a concatenated list of the most relevant
// languages followed by all others. An entry with its "code" attribute set to
// this value is inserted in between.
extern const char kMostRelevantLanguagesDivider[];

// Utility methods for retrieving lists of supported locales and input methods /
// keyboard layouts during OOBE and on the login screen.

// Return a list of languages in which the UI can be shown. Each list entry is a
// dictionary that contains data such as the language's locale code and a
// display name. The list will consist of the |most_relevant_language_codes|,
// followed by a divider and all other supported languages after that. If
// |most_relevant_language_codes| is NULL, the most relevant languages are read
// from initial_locale in VPD. If |selected| matches the locale code of any
// entry in the resulting list, that entry will be marked as selected.
scoped_ptr<base::ListValue> GetUILanguageList(
    const std::vector<std::string>* most_relevant_language_codes,
    const std::string& selected);

// Return a list of supported accept languages. The listed languages can be used
// in the Accept-Language header. The return value will look like:
// [{'code': 'fi', 'displayName': 'Finnish', 'nativeDisplayName': 'suomi'}, ...]
// The most relevant languages, read from initial_locale in VPD, will be first
// in the list.
scoped_ptr<base::ListValue> GetAcceptLanguageList();

// Return a list of keyboard layouts that can be used for |locale| on the login
// screen. Each list entry is a dictionary that contains data such as an ID and
// a display name. The list will consist of the device's hardware layouts,
// followed by a divider and locale-specific keyboard layouts, if any. The list
// will also always contain the US keyboard layout. If |selected| matches the ID
// of any entry in the resulting list, that entry will be marked as selected.
// In addition to returning the list of keyboard layouts, this function also
// activates them so that they can be selected by the user (e.g. by cycling
// through keyboard layouts via keyboard shortcuts).
scoped_ptr<base::ListValue> GetLoginKeyboardLayouts(
    const std::string& locale,
    const std::string& selected);

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_L10N_UTIL_H_
