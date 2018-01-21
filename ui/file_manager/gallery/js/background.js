// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @param {Object.<string, string>} stringData String data.
 * @param {VolumeManager} volumeManager Volume manager.
 */
function BackgroundComponents(stringData, volumeManager) {
  /**
   * String data.
   * @type {Object.<string, string>}
   */
  this.stringData = stringData;

  /**
   * Volume manager.
   * @type {VolumeManager}
   */
  this.volumeManager = volumeManager;

  Object.freeze(this);
}

/**
 * Loads background component.
 * @return {Promise} Promise fulfilled with BackgroundComponents.
 */
BackgroundComponents.load = function() {
  var stringDataPromise = new Promise(function(fulfill) {
    chrome.fileBrowserPrivate.getStrings(function(stringData) {
      loadTimeData.data = stringData;
      fulfill(stringData);
    });
  });

  // VolumeManager should be obtained after stringData initialized.
  var volumeManagerPromise = stringDataPromise.then(function() {
    return new Promise(function(fulfill) {
      VolumeManager.getInstance(fulfill);
    });
  });

  return Promise.all([stringDataPromise, volumeManagerPromise]).then(
      function(args) {
        return new BackgroundComponents(args[0], args[1]);
      });
};

/**
 * Promise to be fulfilled with singleton instance of background components.
 * @type {Promise}
 */
var backgroundComponentsPromise = BackgroundComponents.load();

/**
 * Resolves file system names and obtains entries.
 * @param {Array.<FileEntry>} entries Names of isolated file system.
 * @return {Promise} Promise to be fulfilled with an entry array.
 */
function resolveEntries(entries) {
  return new Promise(function(fulfill, reject) {
    chrome.fileBrowserPrivate.resolveIsolatedEntries(entries,
                                                     function(externalEntries) {
      if (!chrome.runtime.lastError)
        fulfill(externalEntries);
      else
        reject(chrome.runtime.lastError);
    });
  });
}

/**
 * Obtains child entries.
 * @param {DirectoryEntry} entry Directory entry.
 * @return {Promise} Promise to be fulfilled with child entries.
 */
function getChildren(entry) {
  var reader = entry.createReader();
  var readEntries = function() {
    return new Promise(reader.readEntries.bind(reader)).then(function(entries) {
      if (entries.length === 0)
        return [];
      return readEntries().then(function(nextEntries) {
        return entries.concat(nextEntries);
      });
    });
  };
  return readEntries().then(function(entries) {
    return entries.sort(function(a, b) {
      return a.name.localeCompare(b.name);
    });
  });
}

/**
 * Promise to be fulfilled with single application window.
 * @type {Promise}
 */
var appWindowPromise = Promise.resolve(null);

/**
 * Promise to be fulfilled with the current window is closed.
 * @type {Promise}
 */
var closingPromise = Promise.resolve(null);

/**
 * Launches the application with entries.
 *
 * @param {Promise} selectedEntriesPromise Promise to be fulfilled with the
 *     entries that are stored in the exteranl file system (not in the isolated
 *     file system).
 */
function launch(selectedEntriesPromise) {
  // If there is the previous window, close the window.
  appWindowPromise = appWindowPromise.then(function(appWindow) {
    if (appWindow) {
      appWindow.close();
      return closingPromise;
    }
  });

  // Create a new window.
  appWindowPromise = appWindowPromise.then(function() {
    return new Promise(function(fulfill) {
      chrome.app.window.create(
          'gallery.html',
          {
            id: 'gallery',
            innerBounds: {
              minWidth: 820,
              minHeight: 300
            },
            frame: 'none'
          },
          function(appWindow) {
            appWindow.contentWindow.addEventListener(
                'load', fulfill.bind(null, appWindow));
            closingPromise = new Promise(function(fulfill) {
              appWindow.onClosed.addListener(fulfill);
            });
          });
    });
  });

  // Initialize the window document.
  appWindowPromise = Promise.all([
    appWindowPromise,
    backgroundComponentsPromise,
  ]).then(function(args) {
    args[0].contentWindow.initialize(args[1]);
    return args[0];
  });


  // If only 1 entry is selected, retrieve entries in the same directory.
  // Otherwise, just use the selectedEntries as an entry set.
  var allEntriesPromise = selectedEntriesPromise.then(function(entries) {
    if (entries.length === 1) {
      var parentPromise = new Promise(entries[0].getParent.bind(entries[0]));
      return parentPromise.then(getChildren).then(function(entries) {
        return entries.filter(FileType.isImage);
      });
    } else {
      return entries;
    }
  });

  // Open entries.
  return Promise.all([
    appWindowPromise,
    allEntriesPromise,
    selectedEntriesPromise
  ]).then(function(args) {
    args[0].contentWindow.loadEntries(args[1], args[2]);
  });
}

chrome.app.runtime.onLaunched.addListener(function(launchData) {
  // Skip if files are not selected.
  if (!launchData || !launchData.items || launchData.items.length === 0)
    return;

  // Obtains entries in non-isolated file systems.
  // The entries in launchData are stored in the isolated file system.
  // We need to map the isolated entries to the normal entries to retrieve their
  // parent directory.
  var isolatedEntries = launchData.items.map(function(item) {
    return item.entry;
  });
  var selectedEntriesPromise = backgroundComponentsPromise.then(function() {
    return resolveEntries(isolatedEntries);
  });

  launch(selectedEntriesPromise).catch(function(error) {
    console.error(error.stack || error);
  });
});

// If is is run in the browser test, wait for the test resources are installed
// as a component extension, and then load the test resources.
if (chrome.test) {
  chrome.runtime.onMessageExternal.addListener(function(message) {
    if (message.name !== 'testResourceLoaded')
      return;
    var script = document.createElement('script');
    script.src =
        'chrome-extension://ejhcmmdhhpdhhgmifplfmjobgegbibkn' +
        '/gallery/test_loader.js';
    document.documentElement.appendChild(script);
  });
}
