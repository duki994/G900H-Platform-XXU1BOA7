

#include "base/android/sbr/sbr_feature.h"

#include "base/android/jni_android.h"
#include "base/android/jni_helper.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"

#if defined(S_NATIVE_SUPPORT) && !defined(S_UNITTEST_SUPPORT)
#include "out_jni/Feature_jni.h"
#endif
#include "base/strings/string_util.h"
#include <string>
#include "base/logging.h"




using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;


namespace base {
    namespace android {
        namespace sbr{
            
            bool RegisterSbrFeature(JNIEnv* env) {
                #if defined(S_NATIVE_SUPPORT) && !defined(S_UNITTEST_SUPPORT)
                bool result = RegisterNativesImpl(env);
                if(result)
                    Java_Feature_InitCSCFeature(env);
                return result;
                #else 
                    return false; 
                #endif 
            }

            bool getEnableStatus( std::string tag )
            {
                #if defined(S_NATIVE_SUPPORT) && !defined(S_UNITTEST_SUPPORT)
                JNIEnv* env = AttachCurrentThread();                
                ScopedJavaLocalRef<jstring> jTag =  ConvertUTF8ToJavaString(env, tag.c_str());
                return Java_Feature_getEnableStatus( env, jTag.obj() );
                #else 
                    return false; 
                #endif 
            }
            std::string getString( std::string tag) 
            {
                #if defined(S_NATIVE_SUPPORT) && !defined(S_UNITTEST_SUPPORT)
                JNIEnv* env = AttachCurrentThread();                
                ScopedJavaLocalRef<jstring> jTag =  ConvertUTF8ToJavaString(env, tag.c_str());
                ScopedJavaLocalRef<jstring> returnString =Java_Feature_getString( env, jTag.obj());
                return ConvertJavaStringToUTF8(returnString);
                #else 
                    return std::string(""); 
                #endif 
             }
        }
    }
}
