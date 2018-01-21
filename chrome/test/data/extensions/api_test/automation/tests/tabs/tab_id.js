// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function createBackgroundTab(url, callback) {
  chrome.tabs.query({ active: true }, function(tabs) {
    chrome.test.assertEq(1, tabs.length);
    var originalActiveTab = tabs[0];
    createTab(url, function(tab) {
      chrome.tabs.update(originalActiveTab.id, { active: true }, function() {
        callback(tab);
      });
    })
  });
}

var allTests = [
  function testGetTabById() {
    getUrlFromConfig(function(url) {
      // Keep the NTP as the active tab so that we know we're requesting the
      // tab by ID rather than just getting the active tab still.
      createBackgroundTab(url, function(tab) {
        chrome.automation.getTree(tab.id, function(rootNode) {
          rootNode.addEventListener('loadComplete', function() {
            var title = rootNode.attributes['docTitle'];
            chrome.test.assertEq('Automation Tests', title);
            chrome.test.succeed();
          });
        })
      });
    });
  }
];

chrome.test.runTests(allTests);
