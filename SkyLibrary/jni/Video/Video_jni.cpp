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

#include "sky_api.h"
#include "sky_vdec.h"

static const char *classPathName = "com/skyviia/util/VideoUtil";

//#define _DEBUG_VIDEO_

#ifdef _DEBUG_VIDEO_
#define LOG_TAG "VideoUtil_jni"
#include <time.h>
#endif

static void
native_playThumbnail(JNIEnv *env, jobject thiz, jboolean bEnable, int nPosX, int nPosY, int nWidth, int nHeight)
{
#ifdef _DEBUG_VIDEO_
    LOGD("Play video thumbnail...\n");
#endif

    static struct skyfb_api_video_thumbnail param;
    param.flag = (bEnable == JNI_FALSE)? SKYFB_OFF : SKYFB_ON;
    param.start_x = nPosX;
    param.start_y = nPosY;
    param.width = nWidth;
    param.height = nHeight;
    param.mode = RATIO_ORIGINAL;

    int fd = 0;
    if((fd = open("/dev/graphics/fb0", O_RDWR)) == -1)
    {
        LOGE("Open frame buffer device failed\n");
        return;
    }

    if (ioctl(fd, SKYFB_VIDEO_THUMBNAIL, &param) == -1) 
    {
        LOGE("SKYFB_VIDEO_THUMBNAIL ioctl failed\n");
        close(fd);
        return;
    }
    close(fd);
}

static jboolean
native_zoomOrPan(JNIEnv *env, jobject thiz, int nCommand)
{
#ifdef _DEBUG_VIDEO_
    LOGD("Zoom video with command: %d\n", nCommand);
#endif

    int fd = 0;
    if((fd = open("/dev/graphics/fb0", O_RDWR)) == -1)
    {
        LOGE("Open frame buffer device failed\n");
        return JNI_FALSE;
    }

    uint32_t command = (uint32_t)nCommand;
    int nResult = -1;
    if((nResult = ioctl(fd, SKYFB_SET_VIDEO_ZOOM_CMD, &command)) == -1)
    {
        LOGE("SKYFB_SET_ZOOM_CMD ioctl failed\n");
        close(fd);
        return JNI_FALSE;
    }
    close(fd);

    return (nResult != 0xff);
}

//======================================================================================================
//  JNI Methods
//======================================================================================================
static JNINativeMethod methods[] = {
    {"native_playThumbnail",           "(ZIIII)V",  (void*)native_playThumbnail },
    {"native_zoomOrPan",               "(I)Z",      (void*)native_zoomOrPan },
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
