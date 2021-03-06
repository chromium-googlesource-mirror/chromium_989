// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_process_sub_thread.h"

#include "build/build_config.h"
#include "content/browser/notification_service_impl.h"

#if defined(OS_WIN)
#include <Objbase.h>
#endif

namespace content {

BrowserProcessSubThread::BrowserProcessSubThread(BrowserThread::ID identifier)
    : BrowserThreadImpl(identifier),
      notification_service_(NULL) {
}

BrowserProcessSubThread::~BrowserProcessSubThread() {
  Stop();
}

void BrowserProcessSubThread::Init() {
#if defined(OS_WIN)
  // Initializes the COM library on the current thread.
  CoInitialize(NULL);
#endif

  notification_service_ = new NotificationServiceImpl;

  BrowserThreadImpl::Init();
}

void BrowserProcessSubThread::CleanUp() {
  BrowserThreadImpl::CleanUp();

  delete notification_service_;
  notification_service_ = NULL;

#if defined(OS_WIN)
  // Closes the COM library on the current thread. CoInitialize must
  // be balanced by a corresponding call to CoUninitialize.
  CoUninitialize();
#endif
}

}  // namespace content
