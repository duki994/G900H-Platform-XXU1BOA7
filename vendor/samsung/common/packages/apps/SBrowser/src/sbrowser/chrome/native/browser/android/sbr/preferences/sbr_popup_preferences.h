

#include "base/android/jni_android.h"

jobjectArray SbrGetPopupExceptions(JNIEnv* env, jobject obj);
void SbrRemovePopupException(JNIEnv* env, jobject obj, jstring pattern);
void SbrSetPopupException(JNIEnv* env, jobject obj, jstring pattern, jboolean allow);
