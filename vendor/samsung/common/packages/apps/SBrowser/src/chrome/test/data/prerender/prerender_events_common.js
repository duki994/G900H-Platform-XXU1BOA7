// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common js for prerender loaders; defines the helper functions that put
// event handlers on prerenders and track the events for browser tests.

// TODO(gavinp): Put more common loader logic in here.

// Currently only errors with the ordering of Prerender events are caught.
var hadPrerenderEventErrors = false;

var receivedPrerenderStartEvents = [];
var receivedPrerenderLoadEvents = [];
var receivedPrerenderDomContentLoadedEvents = [];
var receivedPrerenderStopEvents = [];

function PrerenderStartHandler(index) {
  if (receivedPrerenderStartEvents[index] ||
      receivedPrerenderLoadEvents[index] ||
      receivedPrerenderStopEvents[index]) {
    hadPrerenderEventErrors = true;
    return;
  }
  receivedPrerenderStartEvents[index] = true;
}

function PrerenderLoadHandler(index) {
  if (!receivedPrerenderStartEvents[index] ||
      receivedPrerenderStopEvents[index]) {
    hadPrerenderEventErrors = true;
    return;
  }
  if (!receivedPrerenderLoadEvents[index])
    receivedPrerenderLoadEvents[index] = 0;
  receivedPrerenderLoadEvents[index]++;
}

function PrerenderDomContentLoadedHandler(index) {
  if (!receivedPrerenderStartEvents[index] ||
      receivedPrerenderStopEvents[index]) {
    hadPrerenderEventErrors = true;
    return;
  }
  if (!receivedPrerenderDomContentLoadedEvents[index])
    receivedPrerenderDomContentLoadedEvents[index] = 0;
  receivedPrerenderDomContentLoadedEvents[index]++;
}

function PrerenderStopHandler(index) {
  if (!receivedPrerenderStartEvents[index] ||
      receivedPrerenderStopEvents[index]) {
    hadPrerenderEventErrors = true;
    return;
  }
  receivedPrerenderStopEvents[index] = true;
}

function AddEventHandlersToLinkElement(link, index) {
  link.addEventListener('webkitprerenderstart',
                        PrerenderStartHandler.bind(null, index), false);
  link.addEventListener('webkitprerenderload',
                        PrerenderLoadHandler.bind(null, index), false);
  link.addEventListener('webkitprerenderdomcontentloaded',
                        PrerenderDomContentLoadedHandler.bind(null, index),
                        false);
  link.addEventListener('webkitprerenderstop',
                        PrerenderStopHandler.bind(null, index), false);
}

function AddPrerender(url, index) {
  var link = document.createElement('link');
  link.rel = 'prerender';
  link.href = url;
  AddEventHandlersToLinkElement(link, index);
  document.body.appendChild(link);
  return link;
}

function AddAnchor(href, target) {
  var a = document.createElement('a');
  a.href = href;
  if (target)
    a.target = target;
  document.body.appendChild(a);
  return a;
}

function Click(url) {
  AddAnchor(url).dispatchEvent(new MouseEvent('click', {
    view: window,
    bubbles: true,
    cancelable: true,
    detail: 1
  }));
}

function ClickTarget(url) {
  var eventObject = new MouseEvent('click', {
    view: window,
    bubbles: true,
    cancelable: true,
    detail: 1
  });
  AddAnchor(url, '_blank').dispatchEvent(eventObject);
}

function ClickPing(url, pingUrl) {
  var a = AddAnchor(url);
  a.ping = pingUrl;
  a.dispatchEvent(new MouseEvent('click', {
    view: window,
    bubbles: true,
    cancelable: true,
    detail: 1
  }));
}

function ShiftClick(url) {
  AddAnchor(url).dispatchEvent(new MouseEvent('click', {
    view: window,
    bubbles: true,
    cancelable: true,
    detail: 1,
    shiftKey: true
  }));
}

function CtrlClick(url) {
  AddAnchor(url).dispatchEvent(new MouseEvent('click', {
    view: window,
    bubbles: true,
    cancelable: true,
    detail: 1,
    ctrlKey: true
  }));
}

function CtrlShiftClick(url) {
  AddAnchor(url).dispatchEvent(new MouseEvent('click', {
    view: window,
    bubbles: true,
    cancelable: true,
    detail: 1,
    ctrlKey: true,
    shiftKey: true
  }));
}

function MetaClick(url) {
  AddAnchor(url).dispatchEvent(new MouseEvent('click', {
    view: window,
    bubbles: true,
    cancelable: true,
    detail: 1,
    metaKey: true
  }));
}

function MetaShiftClick(url) {
  AddAnchor(url).dispatchEvent(new MouseEvent('click', {
    view: window,
    bubbles: true,
    cancelable: true,
    detail: 1,
    metaKey: true,
    shiftKey: true
  }));
}

function WindowOpen(url) {
  window.open(url);
}
