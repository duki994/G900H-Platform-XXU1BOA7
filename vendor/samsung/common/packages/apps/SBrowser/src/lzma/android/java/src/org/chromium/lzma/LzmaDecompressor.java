// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.lzma;

public class LzmaDecompressor
{
  protected boolean objMemOwn;
  private long objectPtr;


  public LzmaDecompressor(int paramInt)
  {
    this(nativenew_LzmaDecompressor(paramInt), true);
  }

  protected LzmaDecompressor(long paramLong, boolean paramBoolean)
  {
    this.objMemOwn = paramBoolean;
    this.objectPtr = paramLong;
  }

  protected static long getCPtr(LzmaDecompressor paramLzmaDecompressor)
  {
    if (paramLzmaDecompressor == null);
    for (long l = 0L; ; l = paramLzmaDecompressor.objectPtr)
      return l;
  }

  public int DecompressChunk(byte[] paramArrayOfByte, int paramInt)
  {
    return nativeLzmaDecompressor_DecompressChunk(this.objectPtr, this, paramArrayOfByte, paramInt);
  }

  public void Deinitialize()
  {
    nativeLzmaDecompressor_Deinitialize(this.objectPtr, this);
  }

  public boolean Initialize(String paramString)
  {
    return nativeLzmaDecompressor_Initialize(this.objectPtr, this, paramString);
  }

  public void delete()
  {
    try
    {
      if (this.objectPtr != 0L)
      {
        if (this.objMemOwn)
        {
          this.objMemOwn = false;
          nativedelete_LzmaDecompressor(this.objectPtr);
        }
        this.objectPtr = 0L;
      }
      return;
    }
    finally
    {
 
    }
  }

  protected void finalize()
  {
    delete();
  }

  public static native int nativeLzmaDecompressor_DecompressChunk(long paramLong, LzmaDecompressor paramLzmaDecompressor, byte[] paramArrayOfByte, int paramInt);

  public static native void nativeLzmaDecompressor_Deinitialize(long paramLong, LzmaDecompressor paramLzmaDecompressor);

  public static native boolean nativeLzmaDecompressor_Initialize(long paramLong, LzmaDecompressor paramLzmaDecompressor, String paramString);

  public static native void nativedelete_LzmaDecompressor(long paramLong);

  public static native long nativenew_LzmaDecompressor(int paramInt);
}

