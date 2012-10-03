#include <jni.h>
#include <utils/Log.h>

/*
 *  jstring charArrayToString(JNIEnv *env, char* charArray, char* encode);
 *  jstring charArrayToString(JNIEnv *env, char* charArray);
 *  char* jstringToCharArray(JNIEnv* env, jstring jstr, char* encode);
 *  char* jstringToCharArray(JNIEnv* env, jstring jstr);
 */
jstring charArrayToString(JNIEnv *env, char* charArray, char* encode)
{
    if(NULL == env ||
       NULL == charArray)
    {
        return NULL;
    }
    
    char* encode_type = encode;
    if(NULL == encode)
    {
        encode_type = (char *)"utf-8";
    }

    jclass strClass = env->FindClass("java/lang/String");
    jmethodID ctorID = env->GetMethodID(strClass, "<init>", "([BLjava/lang/String;)V");
    jbyteArray bytes = env->NewByteArray(strlen(charArray));
    env->SetByteArrayRegion(bytes, 0, strlen(charArray), (jbyte*)charArray);
    jstring encoding = env->NewStringUTF(encode_type);

    return (jstring)env->NewObject(strClass, ctorID, bytes, encoding);
}
jstring charArrayToString(JNIEnv *env, char* charArray)
{
    char* encode = (char *)"utf-8";
    return charArrayToString(env, charArray, encode);
}

char* jstringToCharArray(JNIEnv* env, jstring jstr, char* encode)
{
    if(NULL == env ||
       NULL == jstr)
    {
        return NULL;
    }

    char* encode_type = encode;
    if(NULL == encode)
    {
        encode_type = (char *)"utf-8";
    }

    char* rtn = NULL;
    jclass clsstring = env->FindClass("java/lang/String");
    jstring strencode = env->NewStringUTF(encode_type);
    jmethodID mid = env->GetMethodID(clsstring, "getBytes", "(Ljava/lang/String;)[B");
    jbyteArray barr= (jbyteArray)env->CallObjectMethod(jstr, mid, strencode);
    jsize alen = env->GetArrayLength(barr);
    jbyte* ba = env->GetByteArrayElements(barr, 0);
    if(NULL == ba)
    {
        return NULL;
    }

    if (alen > 0)
    {
        rtn = (char*)malloc(alen + 1);
        memcpy(rtn, ba, alen);
        rtn[alen] = 0;
    }
    env->ReleaseByteArrayElements(barr, ba, 0);
    return rtn;
}
char* jstringToCharArray(JNIEnv* env, jstring jstr)
{
    char* encode = (char *)"utf-8";
    return jstringToCharArray(env, jstr, encode);
}
