// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Main entry point called once the page has loaded.
 */
function onLoad() {
  NetExportView.getInstance();
}

document.addEventListener('DOMContentLoaded', onLoad);

/**
 * This class handles the presentation of our profiler view. Used as a
 * singleton.
 */
var NetExportView = (function() {
  'use strict';

  /**
   * Delay in milliseconds between updates of certain browser information.
   */
  /** @const */ var POLL_INTERVAL_MS = 5000;

  // --------------------------------------------------------------------------
  // Important IDs in the HTML document
  // --------------------------------------------------------------------------

  /** @const */ var STOP_DATA_BUTTON_ID = 'export-view-stop-data';
  /** @const */ var SEND_DATA_BUTTON_ID = 'export-view-send-data';
  /** @const */ var FILE_PATH_TEXT_ID = 'export-view-file-path-text';

  /** @const */ var START_DATA_BUTTON_ID = 'export-view-start-data';
  /** @const */ var SELECT_BOX_ID = 'export-view-select-log-level';

  // --------------------------------------------------------------------------

  /**
   * @constructor
   */
  function NetExportView() {
    $(STOP_DATA_BUTTON_ID).onclick = this.onStopData_.bind(this);
    $(SEND_DATA_BUTTON_ID).onclick = this.onSendData_.bind(this);
    
    $(START_DATA_BUTTON_ID).onclick = this.onStartDataLogging.bind(this);

    window.setInterval(function() { chrome.send('getExportNetLogInfo'); },
                       POLL_INTERVAL_MS);

    chrome.send('getExportNetLogInfo');
  }

  cr.addSingletonGetter(NetExportView);

  NetExportView.prototype = {
    /**
     * Stops saving NetLog data to a file.
     */
    onStopData_: function() {
      chrome.send('stopNetLog');
    },

    /**
     * Sends NetLog data via email from browser.
     */
    onSendData_: function() {
      chrome.send('sendNetLog');
    },

    onStartDataLogging: function() {
        var selectedValue = document.getElementById("export-view-select-log-level").selectedIndex;
        if(selectedValue == 0) {
            chrome.send('startNetLog');
        } else if(selectedValue == 1) {
            chrome.send('startNetLogAllButBytes');
        } else if(selectedValue == 2) {
            chrome.send('startNetLogBasic');
        }
    },

    /**
     * Enable or disable START_DATA_BUTTON_ID, STOP_DATA_BUTTON_ID and
     * SEND_DATA_BUTTON_ID buttons. Displays the path name of the file where
     * NetLog data is collected.
     */
    onExportNetLogInfoChanged: function(exportNetLogInfo) {
      if (exportNetLogInfo.file) {
        var message = '';
        if (exportNetLogInfo.state == 'ALLOW_STOP')
          message = 'NetLog data is collected in: ';
        else if (exportNetLogInfo.state == 'ALLOW_START_SEND')
          message = 'NetLog data to send is in: ';
        $(FILE_PATH_TEXT_ID).textContent = message + exportNetLogInfo.file;
      } else {
        $(FILE_PATH_TEXT_ID).textContent = '';
      }

      $(START_DATA_BUTTON_ID).disabled = true;
      $(SELECT_BOX_ID).disabled = true;
      $(STOP_DATA_BUTTON_ID).disabled = true;
      $(SEND_DATA_BUTTON_ID).disabled = true;
      if (exportNetLogInfo.state == 'ALLOW_START') {
        $(START_DATA_BUTTON_ID).disabled = false;
        $(SELECT_BOX_ID).disabled = false;
      } else if (exportNetLogInfo.state == 'ALLOW_STOP') {
        $(STOP_DATA_BUTTON_ID).disabled = false;
      } else if (exportNetLogInfo.state == 'ALLOW_START_SEND') {
        $(START_DATA_BUTTON_ID).disabled = false;
        $(SELECT_BOX_ID).disabled = false;
        $(SEND_DATA_BUTTON_ID).disabled = false;
      } else if (exportNetLogInfo.state == 'UNINITIALIZED') {
        $(FILE_PATH_TEXT_ID).textContent =
            'Unable to initialize NetLog data file.';
      }
    }
  };

  return NetExportView;
})();
