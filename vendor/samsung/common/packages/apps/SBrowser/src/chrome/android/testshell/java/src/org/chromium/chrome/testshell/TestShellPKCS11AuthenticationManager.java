// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

import android.content.Context;

import org.chromium.chrome.browser.PKCS11AuthenticationManager;
import org.chromium.net.AndroidPrivateKey;

import java.security.cert.X509Certificate;

/**
 * Chromium TestShell stub implementation of PKCS11AuthenticationManager.
 */
public class TestShellPKCS11AuthenticationManager implements PKCS11AuthenticationManager {
    @Override
    public boolean isPKCS11AuthEnabled() {
        return false;
    }

    @Override
    public String getClientCertificateAlias(String hostName, int port) {
        return null;
    }

    @Override
    public void initialize(Context context) {
    }


    @Override
    public X509Certificate[] getCertificateChain(String alias) {
        return null;
    }

    @Override
    public AndroidPrivateKey getPrivateKey(String alias) {
        return null;
    }
}
