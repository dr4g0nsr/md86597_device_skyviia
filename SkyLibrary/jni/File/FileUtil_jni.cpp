#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <utils/Log.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>

//#include "JNIHelp.h"
#include "android_runtime/AndroidRuntime.h"

static const char *classPathName = "com/skyviia/util/FileUtil";

//#define _DEBUG_FILEUTIL_
#define LOG_TAG "FileUtil_jni"

#ifdef _DEBUG_FILEUTIL_
#include <time.h>
#endif

extern jstring charArrayToString(JNIEnv *env, char* charArray);

static void jniThrowException(JNIEnv* env, const char* exc, const char* msg = NULL)
{
    jclass excClazz = env->FindClass(exc);
    LOG_ASSERT(excClazz, "Unable to find class %s", exc);

    env->ThrowNew(excClazz, msg);
}

static bool m_sbStopListing = false;
static void
native_stop(JNIEnv *env, jobject thiz)
{
    m_sbStopListing = true;
}

static void
native_listFiles(JNIEnv *env, jobject thiz, jstring jstrPath, jobject folderList, jobject fileList, jobject matcher)
{
    m_sbStopListing = false;

    if(NULL == jstrPath ||
       NULL == folderList ||
       NULL == fileList)
    {
        jniThrowException(env, "java/lang/IllegalArgumentException");
        return;
    }

    jmethodID method_ExtensionMatcher_isMatch = NULL;
    if(NULL != matcher)
    {
        jclass class_ExtensionMatcher = env->FindClass("com/skyviia/util/FileUtil$ExtensionMatcher");
        method_ExtensionMatcher_isMatch = env->GetMethodID(class_ExtensionMatcher, "isMatch", "(Ljava/lang/String;)Z");
    }

    jclass class_ArrayList = env->FindClass("java/util/ArrayList");
    jmethodID method_ArrayList_Add = env->GetMethodID(class_ArrayList, "add", "(Ljava/lang/Object;)Z");

    static char strFullPath[PATH_MAX];
    const char *DIRECTORY_PATH = env->GetStringUTFChars(jstrPath, NULL);
    if (NULL == DIRECTORY_PATH)    // Out of memory
    {
        jniThrowException(env, "java/lang/RuntimeException", "Out of memory");
        return;
    }

    int nPathLength = strlen(DIRECTORY_PATH);
    char *strPath = NULL;
    if('/' != DIRECTORY_PATH[nPathLength - 1])
    {
        ++nPathLength;
        strPath = new char[nPathLength+1];
        strcpy(strPath, DIRECTORY_PATH);
        strPath[nPathLength-1] = '/';
        strPath[nPathLength] = 0;
    }
    else
    {
        strPath = new char[nPathLength+1];
        strcpy(strPath, DIRECTORY_PATH);
    }
    
    DIR* dir = opendir(strPath);
    if(!dir)
    {
        LOGE("opendir %s failed, errno: %d", strPath, errno);
        switch(errno)
        {
            case EACCES:
                jniThrowException(env, "java/lang/SecurityException", "Cannot open directory");
                break;
            case ENOENT:
                jniThrowException(env, "java/io/FileNotFoundException");
                break;
        }

        return;
    }
    
    struct dirent* entry;
    int nType;
    struct stat statbuf;
    jstring jstrFullPath;
            
    while ((entry = readdir(dir)) &&
           !m_sbStopListing)
    {
        const char* strEntryName = entry->d_name;

        // ignore ".",  ".." and hidden files
        if (strEntryName[0] == '.') {
            continue;
        }

        if(nPathLength + strlen(strEntryName) >= PATH_MAX)
        {
            LOGW("Path is too long");
            continue;
        }

        memset(strFullPath, PATH_MAX, 0);
        strcpy(strFullPath, strPath);
        strcat(strFullPath, strEntryName);

        nType = entry->d_type;
        if (DT_UNKNOWN == nType)
        {
            // If the type is unknown, stat() the file instead.
            // This is sometimes necessary when accessing NFS mounted filesystems, but
            // could be needed in other cases well.
            if (0 == stat(strFullPath, &statbuf))
            {
                if (S_ISREG(statbuf.st_mode))
                {
                    nType = DT_REG;
                }
                else if (S_ISDIR(statbuf.st_mode)) 
                {
                    nType = DT_DIR;
                }
            } else {
                LOGD("stat() failed for %s: %s", strPath, strerror(errno) );
            }
        }

        if (DT_DIR == nType || DT_REG == nType)
        {
            jstrFullPath = env->NewStringUTF(strFullPath);
            if(NULL == jstrFullPath)
            {
                jniThrowException(env, "java/lang/RuntimeException", "Out of memory");
                return;
            }

            if (DT_DIR == nType)    //directories
            {
                env->CallBooleanMethod(folderList, method_ArrayList_Add, jstrFullPath);
            }
            else                    //regular files
            {
                if(NULL != method_ExtensionMatcher_isMatch &&
                   JNI_FALSE == env->CallBooleanMethod(matcher, method_ExtensionMatcher_isMatch, jstrFullPath))
                {
                    env->DeleteLocalRef(jstrFullPath);
                    continue;
                }

                env->CallBooleanMethod(fileList, method_ArrayList_Add, jstrFullPath);
            }

            env->DeleteLocalRef(jstrFullPath);
        }
    }

    closedir(dir);
}

//======================================================================================================
//  JNI Methods
//======================================================================================================
static JNINativeMethod methods[] = {
    {"native_listFiles", "(Ljava/lang/String;Ljava/util/ArrayList;Ljava/util/ArrayList;Lcom/skyviia/util/FileUtil$ExtensionMatcher;)V", (void*)native_listFiles },
    {"native_stop",      "()V",  (void*)native_stop },
};

/*
 * Register several native methods for one class.
 */
static int registerNativeMethods(JNIEnv* env, const char* className,
    JNINativeMethod* gMethods, int numMethods)
{
    jclass clazz;

    clazz = env->FindClass(className);
    if (clazz == NULL)
    {
        fprintf(stderr, "Native registration unable to find class '%s'", className);
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0)
    {
        fprintf(stderr, "RegisterNatives failed for '%s'", className);
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

/*
 * Register native methods for all classes we know about.
 */
static int registerNatives(JNIEnv* env)
{
    if (!registerNativeMethods(env, classPathName,
                               methods, sizeof(methods) / sizeof(methods[0])))
    {
        return JNI_FALSE;
    }

  return JNI_TRUE;
}

/*
 * Set some test stuff up.
 *
 * Returns the JNI version on success, -1 on failure.
 */

typedef union
{
    JNIEnv* env;
    void* venv;
} UnionJNIEnvToVoid;

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    UnionJNIEnvToVoid uenv;
    uenv.venv = NULL;
    jint result = -1;
    JNIEnv* env = NULL;
    
    printf("JNI_OnLoad");

    if (vm->GetEnv(&uenv.venv, JNI_VERSION_1_4) != JNI_OK)
    {
        fprintf(stderr, "GetEnv failed");
        return result;
    }
    env = uenv.env;

    if (!registerNatives(env))
    {
        fprintf(stderr, "registerNatives failed");
    }
    
    result = JNI_VERSION_1_4;
    
    return result;
}

void JNI_OnUnload(JavaVM* vm, void* reserved)
{
}
