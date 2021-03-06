// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

function onLoad() {
  var goHome = function() {
    remoting.setMode(remoting.AppMode.HOME);
  };
  var goClient = function() {
    remoting.setMode(remoting.AppMode.CLIENT_UNCONNECTED);
  };
  /** @param {Event} event */
  var sendAccessCode = function(event) {
    remoting.tryConnect();
    event.preventDefault();
  };
  var doAuthRedirect = function() {
    remoting.oauth2.doAuthRedirect();
  }
  /** @type {Array.<{event: string, id: string, fn: function(Event):void}>} */
  var actions = [
      { event: 'click', id: 'clear-oauth', fn: remoting.clearOAuth2 },
      { event: 'click', id: 'toolbar-disconnect', fn: remoting.disconnect },
      { event: 'click', id: 'toggle-scaling', fn: remoting.toggleScaleToFit },
      { event: 'click', id: 'auth-button', fn: doAuthRedirect },
      { event: 'click', id: 'share-button', fn: remoting.tryShare },
      { event: 'click', id: 'access-mode-button', fn: goClient },
      { event: 'click', id: 'cancel-share-button', fn: remoting.cancelShare },
      { event: 'click', id: 'host-finished-button', fn: goHome },
      { event: 'click', id: 'client-cancel-button', fn: goHome },
      { event: 'click', id: 'client-finished-button', fn: goClient },
      { event: 'click', id: 'cancel-button',
        fn: remoting.cancelPendingOperation },
      { event: 'submit', id: 'access-code-form', fn: sendAccessCode }
  ];

  for (var i = 0; i < actions.length; ++i) {
    var action = actions[i];
    var element = document.getElementById(action.id);
    if (element) {
      element.addEventListener(action.event, action.fn, false);
    } else {
      console.error('Could not set ' + action.id +
                    ' event handler on element ' + action.event +
                    ': element not found.');
    }
  }
  remoting.init();
}

function onBeforeUnload() {
  return remoting.promptClose();
}

window.addEventListener('load', onLoad, false);
window.addEventListener('beforeunload', onBeforeUnload, false);
window.addEventListener('resize', remoting.onResize, false);
window.addEventListener('unload', remoting.disconnect, false);
