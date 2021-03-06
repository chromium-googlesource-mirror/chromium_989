// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_drag_bookmark_handler_mac.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/tab_contents/tab_contents.h"

WebDragBookmarkHandlerMac::WebDragBookmarkHandlerMac()
    : tab_(NULL) {
}

WebDragBookmarkHandlerMac::~WebDragBookmarkHandlerMac() {}

void WebDragBookmarkHandlerMac::DragInitialize(TabContents* contents) {
  DCHECK(tab_ ? (tab_->tab_contents() == contents) : true);
  if (!tab_)
    tab_ = TabContentsWrapper::GetCurrentWrapperForContents(contents);

  bookmark_drag_data_.ReadFromDragClipboard();
}

void WebDragBookmarkHandlerMac::OnDragOver() {
  if (tab_ && tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()) {
    tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()->OnDragOver(
        bookmark_drag_data_);
  }
}

void WebDragBookmarkHandlerMac::OnDragEnter() {
  if (tab_ && tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()) {
    tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()->OnDragEnter(
        bookmark_drag_data_);
  }
}

void WebDragBookmarkHandlerMac::OnDrop() {
  // This is non-null if tab_contents_ is showing an ExtensionWebUI with
  // support for (at the moment experimental) drag and drop extensions.
  if (tab_) {
    if (tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()) {
      tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()->OnDrop(
          bookmark_drag_data_);
    }

    // Focus the target browser.
    Browser* browser = Browser::GetBrowserForController(
        &tab_->tab_contents()->GetController(), NULL);
    if (browser)
      browser->window()->Show();
  }
}

void WebDragBookmarkHandlerMac::OnDragLeave() {
  if (tab_ && tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()) {
    tab_->bookmark_tab_helper()->GetBookmarkDragDelegate()->OnDragLeave(
        bookmark_drag_data_);
  }
}
