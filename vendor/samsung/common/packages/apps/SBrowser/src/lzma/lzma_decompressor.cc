// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lzma_decompressor.h"

#include <string.h>
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"

#include "jni/LzmaDecompressor_jni.h"
#include "base/logging.h" 

/**********************************************************************************************************************************/
/* JNI HOOKS fo native calles by JAVA */

void GetJStringContent(JNIEnv *AEnv, jstring AStr, std::string &ARes) {
  if (!AStr) {
    ARes.clear();
    return;
  }

  const char *s = AEnv->GetStringUTFChars(AStr,NULL);
  ARes=s;
  AEnv->ReleaseStringUTFChars(AStr,s);
}


jint LzmaDecompressor_DecompressChunk(JNIEnv* env, jclass jcaller,
    jlong paramLong,
    jobject paramLzmaDecompressor,
    jbyteArray paramArrayOfByte,
    jint paramInt){
  
  LzmaDecompressor* decompressor = (LzmaDecompressor*) paramLong;
  jint res = -1;

  if (jbyte* bytes = env->GetByteArrayElements(paramArrayOfByte, NULL)) {
      res = decompressor->DecompressChunk(reinterpret_cast<char*>(bytes), paramInt);
      env->ReleaseByteArrayElements(paramArrayOfByte, bytes, JNI_ABORT);
  }
  return res;
}

void LzmaDecompressor_Deinitialize(JNIEnv* env, jclass jcaller,
    jlong paramLong,
    jobject paramLzmaDecompressor){
  
    LzmaDecompressor* decompressor = (LzmaDecompressor*) paramLong;
    decompressor->Deinitialize();
}

jboolean LzmaDecompressor_Initialize(JNIEnv* env, jclass jcaller,
    jlong paramLong,
    jobject paramLzmaDecompressor,
    jstring paramString) {
 

  std::string filePath;
  if (paramString) {
    // may be a NULL
    GetJStringContent(env,paramString, filePath) ;
  }

  LzmaDecompressor* decompressor = (LzmaDecompressor*) paramLong;

  return decompressor->Initialize(filePath.c_str());
}


void delete_LzmaDecompressor(JNIEnv* env, jclass jcaller,
    jlong paramLong){

    LzmaDecompressor* decompressor = (LzmaDecompressor*) paramLong;
    delete decompressor;
}

jlong new_LzmaDecompressor(JNIEnv* env, jclass jcaller,
    jint paramInt){
  
  LzmaDecompressor* decompressor = new LzmaDecompressor(paramInt);
  return (long)decompressor;
}

JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();

  if(!LzmaDecompressor::RegisterLzmaDecompressorAndroidJni(env))
	 return -1;

  return JNI_VERSION_1_4;
}

/* JNI HOOKS fo native calles by JAVA */
/**********************************************************************************************************************************/

bool LzmaDecompressor::RegisterLzmaDecompressorAndroidJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
LzmaDecompressor::LzmaDecompressor(int out_buffer_size) :
    out_buffer_size_(out_buffer_size), first_chunk_(true), out_file_(NULL),
    out_data_(NULL) {
}

LzmaDecompressor::~LzmaDecompressor() {
}

void* Alloc(void* p, size_t size) {
  return operator new(size);
}

void Free(void* p, void* address) {
  if (address)
    operator delete(address);
}

bool LzmaDecompressor::Initialize(const char* out_path_name) {
  memset(&dec_, 0, sizeof(CLzmaDec));

  alloc_.Alloc = &Alloc;
  alloc_.Free  = &Free;

  out_file_ = fopen(out_path_name, "wb");
  if (!out_file_)
    return false;

  out_data_ = new Byte[out_buffer_size_];
  if (!out_data_) {
    fclose(out_file_);
    return false;
  }

  return true;
}

void LzmaDecompressor::Deinitialize() {
  fclose(out_file_);
  LzmaDec_Free(&dec_, &alloc_);
  delete [] out_data_;
}

int LzmaDecompressor::DecompressChunk(char* data, int length) {
  static const int LZMA_FILE_SIZE_HEADER = 8;

  SizeT out_size = out_buffer_size_;
  const Byte* in_data = reinterpret_cast<Byte*>(data);
  SizeT in_size = static_cast<SizeT>(length);

  if (!out_data_)
    return -1;

  // The first chunk contains LZMA header data
  if (first_chunk_) {
    if (length < LZMA_PROPS_SIZE + LZMA_FILE_SIZE_HEADER) {
      return 0;
    }

    LzmaDec_Construct(&dec_);
    LzmaDec_Allocate(&dec_, in_data, LZMA_PROPS_SIZE, &alloc_);
    LzmaDec_Init(&dec_);

    in_data += LZMA_PROPS_SIZE + LZMA_FILE_SIZE_HEADER;
    in_size -= LZMA_PROPS_SIZE + LZMA_FILE_SIZE_HEADER;

    first_chunk_ = false;
  }

  ELzmaStatus status = LZMA_STATUS_NOT_SPECIFIED;

  while (in_size > 0 && status != LZMA_STATUS_NEEDS_MORE_INPUT) {
    SizeT current_in_size = in_size;
    SRes result = LzmaDec_DecodeToBuf(&dec_, out_data_, &out_size, in_data,
                                      &current_in_size, LZMA_FINISH_ANY,
                                      &status);

    if (result != SZ_OK)
      return -1;

    if (fwrite(out_data_, 1, out_size, out_file_) != out_size)
      return -1;

    out_size = out_buffer_size_;
    in_data += current_in_size;
    in_size -= current_in_size;
  }

  return in_data - reinterpret_cast<Byte*>(data);
}
