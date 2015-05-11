// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/sync_promo_ui.h"

#include "chrome/browser/profiles/profile.h"
#if defined(ENABLE_SIGNIN)
#include "chrome/browser/signin/signin_promo.h"
#endif
bool SyncPromoUI::ShouldShowSyncPromo(Profile* profile) {
  // Don't show sync promo if the sign in promo should not be shown.
#if defined(ENABLE_SIGNIN)
  if (!signin::ShouldShowPromo(profile)) {
    return false;
  }
#endif
  // Don't show if sync is disabled by policy.
  if (!profile->IsSyncAccessible())
    return false;

  return true;
}
