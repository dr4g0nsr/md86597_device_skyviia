#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/soundcard.h>
#include <utils/Log.h>
#include <time.h>

static const char *classPathName = "com/skyviia/util/AudioUtil";

#define LOG_TAG "AudioUtil_jni"


//#define PATH_DEV_MIXER "/dev/mixer"
#define PATH_DEV_MIXER "/dev/dsp"
#define MIXER_DEVICE        "/dev/mixer"
#define SOUND_MIXER_PCM     4



static const char *oss_mixer_device = PATH_DEV_MIXER;
static int oss_mixer_channel = SOUND_MIXER_PCM;

static jint
native_getVolume(JNIEnv *env, jobject thiz)
{
    LOGD("Set audio vol...\n");
    int fd, v, devs;

    if ((fd = open(oss_mixer_device, O_RDONLY)) > 0) {
        ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devs);
        if (devs & (1 << oss_mixer_channel)) {
            ioctl(fd, MIXER_READ(oss_mixer_channel), &v);
            // vol->right = (v & 0xFF00) >> 8;
            // vol->left = v & 0x00FF;
            LOGD("get volume is OK!");
        } else {
            close(fd);
            return -1; // CONTROL_ERROR;
        }
        close(fd);
        return v; // CONTROL_OK;
    } else {
        LOGD("open mixer device \"%s\" failed!", oss_mixer_device);
    }
    return -1;
}

static void
native_setLRVolume(JNIEnv *env, jobject thiz, int nLeftVol, int nRightVol)
{
    LOGD("Set audio vol...\n");
    int fd, v, devs;

    if ((fd = open(oss_mixer_device, O_RDONLY)) > 0) {
        ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devs);
        if (devs & (1 << oss_mixer_channel)) {
            v = ((int)nRightVol << 8) | (int)nLeftVol;
            ioctl(fd, MIXER_WRITE(oss_mixer_channel), &v);
            LOGD("set volume is OK!");
        } else {
            close(fd);
            return; // CONTROL_ERROR;
        }
        close(fd);
        return; // CONTROL_OK;
    } else {
        LOGD("open mixer device \"%s\" failed!", oss_mixer_device);
    }
}

static void
native_setVolume(JNIEnv *env, jobject thiz, int nVolume)
{
    /* change to sound level */
    int nVolLevel = (((unsigned short)nVolume) << 8) | nVolume;

    /* Open Mixer device */
    int mixer_fd = -1;
    if((mixer_fd = open(MIXER_DEVICE, O_RDONLY, 0)) != -1)
    {
        ioctl(mixer_fd, MIXER_WRITE(SOUND_MIXER_PCM), &nVolLevel);
        close(mixer_fd);
    }
    else
    {
        LOGE("== open %s failed ==\n", MIXER_DEVICE);
    }
}

static void
native_setSpeakerLRType(JNIEnv *env, jobject thiz, int nSpeakerLRType)
{
    LOGD("Set speaker LR type to %d\n", nSpeakerLRType);
}

static int
native_getSpeakerLRType(JNIEnv *env, jobject thiz)
{
    LOGD("Get speaker LR type: %d\n", 0);
    return 0;
}

//======================================================================================================
//  JNI Methods
//======================================================================================================
static JNINativeMethod methods[] = {
    {"native_getVolume",           "()I",   (void*)native_getVolume },
    {"native_setLRVolume",         "(II)V", (void*)native_setLRVolume },
    {"native_setVolume",           "(I)V",  (void*)native_setVolume },
    {"native_setSpeakerLRType",    "(I)V",  (void*)native_setSpeakerLRType },
    {"native_getSpeakerLRType",    "()I",   (void*)native_getSpeakerLRType },
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
