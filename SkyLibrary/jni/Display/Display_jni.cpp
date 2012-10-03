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

#include "Disp2Ctrl_api.h"

static const char *classPathName = "com/skyviia/util/DisplayUtil";

//#define _DEBUG_DISPLAY_
#ifdef _DEBUG_DISPLAY_
#define LOG_TAG "DisplayUtil_jni"
#include <time.h>
#endif

//======================================================================================================
//  Display buffer control APIs
//======================================================================================================
static jboolean
native_lockDisplay(JNIEnv *env, jobject thiz)
{
#ifdef _DEBUG_DISPLAY_
    LOGD("Lock Display...\n");
#endif

    int fbfd = 0;
    if((fbfd = open("/dev/graphics/fb0", O_RDWR)) == -1)
    {
        LOGE("Open frame buffer device failed\n");
        return JNI_FALSE;
    }

    if(ioctl(fbfd, SKYFB_LOCK_DISPLAY) == -1)
    {
        LOGE("SKYFB_LOCK_DISPLAY ioctl failed\n");
        close(fbfd);
        return JNI_FALSE;
    }
    close(fbfd);
    return JNI_TRUE;
}

static jboolean
native_turnDisplayOff(JNIEnv *env, jobject thiz)
{
#ifdef _DEBUG_DISPLAY_
    LOGD("Turn Display 1 Off...\n");
#endif
    int fbfd = 0;
    struct skyfb_api_display_status display_status;
    if((fbfd = open("/dev/graphics/fb0", O_RDWR)) == -1)
    {
        LOGE("Open frame buffer device failed\n");
        return JNI_FALSE;
    }
    display_status.display = SKYFB_DISP1;
	display_status.status = SKYFB_OFF;
    if(ioctl(fbfd, SKYFB_SET_DISPLAY_STATUS, &display_status) == -1)
    {
        LOGE("turnDisplayOff SKYFB_SET_DISPLAY_STATUS ioctl failed\n");
        close(fbfd);
        return JNI_FALSE;
    }
    close(fbfd);
    return JNI_TRUE;
}

static jboolean
native_restoreDisplay(JNIEnv *env, jobject thiz)
{
#ifdef _DEBUG_DISPLAY_
    LOGD("Restore display: \n");
#endif
    int fbfd = 0;
    if((fbfd = open("/dev/graphics/fb0", O_RDWR)) == -1)
    {
        LOGE("Open frame buffer device failed\n");
        return JNI_FALSE;
    }

    if(ioctl(fbfd, SKYFB_RECOVER_DISPLAY) == -1)
    {
        LOGE("SKYFB_RECOVER_DISPLAY ioctl failed\n");
        close(fbfd);
        return JNI_FALSE;
    }

    close(fbfd);
    return JNI_TRUE;
}

static jboolean
native_isDisplayLocked(JNIEnv *env, jobject thiz)
{
    int fbfd = 0;
    if((fbfd = open("/dev/graphics/fb0", O_RDWR)) == -1)
    {
        LOGE("Open frame buffer device failed\n");
        return JNI_FALSE;
    }
    
    uint32_t nLockStatus = 0;
    if(ioctl(fbfd, SKYFB_GET_LOCK_STATUS, &nLockStatus) == -1)
    {
        LOGE("SKYFB_GET_LOCK_STATUS ioctl failed\n");
        close(fbfd);
        return JNI_FALSE;
    }

    close(fbfd);
    return (0 == nLockStatus)?JNI_FALSE:JNI_TRUE;
}

//======================================================================================================
//  Display 2 Control APIs
//======================================================================================================

static disp2_api g_sDisp2Ctrl;

static void
native_setDisplay2Params(JNIEnv *env, jobject thiz, int nWidth, int nHeight, int nPosX, int nPosY)
{
#ifdef _DEBUG_DISPLAY_
    LOGD("Set display2 params: %d x %d @(%d, %d)...\n", nWidth, nHeight, nPosX, nPosY);
#endif

    g_sDisp2Ctrl.change_display2_env_resolution(nWidth, nHeight, nPosX, nPosY);
}

static void
native_showDisplay2(JNIEnv *env, jobject thiz)
{
#ifdef _DEBUG_DISPLAY_
    LOGD("Show display2...\n");
#endif

    g_sDisp2Ctrl.set_display_2_on();
}

static void
native_hideDisplay2(JNIEnv *env, jobject thiz)
{
#ifdef _DEBUG_DISPLAY_
    LOGD("Hide display2...\n");
#endif

    g_sDisp2Ctrl.set_display_2_off();
}

static void
native_cleanDisplay2(JNIEnv *env, jobject thiz)
{
#ifdef _DEBUG_DISPLAY_
    LOGD("Clean display2...\n");
#endif

    g_sDisp2Ctrl.Clean_whole_disp2_drawing_board();
}

static void
native_copyPixelsToDisplay2(JNIEnv *env, jobject thiz, jintArray intArray, int nWidth, int nHeight, int nPosX, int nPosY)
{
    if(NULL == intArray)
    {
        return;
    }

    jint len = env->GetArrayLength(intArray);
    if(len <= 0)
    {
        return;
    }

    int *pixels = env->GetIntArrayElements(intArray, 0);
    if(NULL == pixels)
    {
        return;
    }
    g_sDisp2Ctrl.draw_image_directly((uint8_t*)pixels, nWidth, nHeight, nPosX, nPosY);
    env->ReleaseIntArrayElements(intArray, pixels, 0);
}

static void
native_resetDisplay2(JNIEnv *env, jobject thiz)
{
#ifdef _DEBUG_DISPLAY_
    LOGD("Reset display2...\n");
#endif

    g_sDisp2Ctrl.reset_display2_env_par();
}

static int
native_getDisplayOutputResolution(JNIEnv *env, jobject thiz)
{
#ifdef _DEBUG_DISPLAY_
    LOGD("Get Display Output Resolution...\n");
#endif

    int fbfd = 0;
    if((fbfd = open("/dev/graphics/fb0", O_RDWR)) == -1)
    {
        LOGE("Open frame buffer device failed\n");
        return 0;
    }

    int res = 0;
    if(ioctl(fbfd, SKYFB_GET_REAL_DISPLAY, &res) == -1)
    {
        LOGE("SKYFB_GET_REAL_DISPLAY ioctl failed\n");
        close(fbfd);
        return 0;
    }

    close(fbfd);
    return res;
}

/*=====================================
//           OSD Controls
//===================================*/
static void
native_cleanOSD(JNIEnv *env, jobject thiz, int nOSDBlock)
{
#ifdef _DEBUG_DISPLAY_
    LOGD("Clean OSD %d\n", nOSDBlock);
#endif
    skyfb_api_osd osdData;
    int fbfd = 0;
    if((fbfd = open("/dev/graphics/fb0", O_RDWR)) == -1)
    {
        LOGE("Open frame buffer device failed\n");
        return;
    }

    osdData.block = nOSDBlock;
    if(ioctl(fbfd, SKYFB_OSD_ERASE, &osdData) == -1)
    {
        LOGE("SKYFB_OSD_ERASE ioctl failed\n");
        close(fbfd);
        return;
    }
    close(fbfd);
}

/*
 * For drawing ARGB to OSD(RGB232)
 */
struct ColorMapping_S
{
    unsigned char RGB_888_Color;
    unsigned char RGB_232_Color;

    ColorMapping_S(unsigned char rgb888, unsigned char rgb232)
    {
        RGB_888_Color = rgb888;
        RGB_232_Color = rgb232;
    }
};

static const ColorMapping_S RED[4]   = {ColorMapping_S(  0, 0<<5),
                                        ColorMapping_S(112, 1<<5),
                                        ColorMapping_S(196, 2<<5),
                                        ColorMapping_S(255, 3<<5)};
static const ColorMapping_S GREEN[8] = {ColorMapping_S(  0, 0<<2),
                                        ColorMapping_S( 40, 1<<2),
                                        ColorMapping_S( 85, 2<<2),
                                        ColorMapping_S(112, 3<<2),
                                        ColorMapping_S(150, 4<<2),
                                        ColorMapping_S(170, 5<<2),
                                        ColorMapping_S(196, 6<<2),
                                        ColorMapping_S(255, 7<<2)};
static const ColorMapping_S BLUE[4] =  {ColorMapping_S(  0, 0),
                                        ColorMapping_S(112, 1),
                                        ColorMapping_S(196, 2),
                                        ColorMapping_S(255, 3)};

static unsigned char _matchColor(const ColorMapping_S* const MATCH_TABLE,
                                 const int                  &TABLE_SIZE,
                                 const unsigned char        &RGB888_color,
                                 const bool                 &bIgnoreUndefinedColor)
{
    unsigned char ucMatchColor = 0;
    bool bMatched = false;

    //search space is small, we don't need to use binary search
    if(bIgnoreUndefinedColor)
    {
        for(int nMatchIndex = 0; nMatchIndex < TABLE_SIZE; nMatchIndex++)
        {
            if(RGB888_color == MATCH_TABLE[nMatchIndex].RGB_888_Color)
            {
                ucMatchColor = MATCH_TABLE[nMatchIndex].RGB_232_Color;
                bMatched = true;
                break;
            }
        }
    }
    else
    {
        bMatched = true;    //Always find a match
        if(RGB888_color < MATCH_TABLE[0].RGB_888_Color)
        {
            ucMatchColor = MATCH_TABLE[0].RGB_232_Color;
        }
        else if(RGB888_color > MATCH_TABLE[TABLE_SIZE-1].RGB_888_Color)
        {
            ucMatchColor = MATCH_TABLE[TABLE_SIZE-1].RGB_232_Color;
        }
        else
        {
            for(int nMatchIndex = 0; nMatchIndex < TABLE_SIZE-1; nMatchIndex++)
            {
                if(RGB888_color == MATCH_TABLE[nMatchIndex].RGB_888_Color)
                {
                    ucMatchColor = MATCH_TABLE[nMatchIndex].RGB_232_Color;
                    break;
                }
                else if(RGB888_color <= MATCH_TABLE[nMatchIndex+1].RGB_888_Color)
                {
                    if(RGB888_color - MATCH_TABLE[nMatchIndex].RGB_888_Color >
                       MATCH_TABLE[nMatchIndex+1].RGB_888_Color - RGB888_color)
                    {
                        ucMatchColor = MATCH_TABLE[nMatchIndex+1].RGB_232_Color;
                    }
                    else
                    {
                        ucMatchColor = MATCH_TABLE[nMatchIndex].RGB_232_Color;
                    }
                    break;
                }
            }
        }
    }
    
    if(bMatched)
    {
        ucMatchColor |= 0x80;
    }
    
    return ucMatchColor;
}

static void
_fillOSD(int nOSDBlock, int nAlpha, int nPosX, int nPosY, int nWidth, int nHeight, unsigned char* osd_pixels)
{
#ifdef _DEBUG_DISPLAY_
    LOGD("Fill OSD in block %d with alpha %d @(%d, %d), %d x %d...\n", nOSDBlock, nAlpha, nPosX, nPosY, nWidth, nHeight);
#endif
    skyfb_api_osd osdData;
    int fbfd = 0;
    if((fbfd = open("/dev/graphics/fb0", O_RDWR)) == -1)
    {
        LOGE("Open frame buffer device failed\n");
        return;
    }

    osdData.x = nPosX;
    osdData.y = nPosY;
    osdData.width = nWidth;
    osdData.height = nHeight;
    osdData.data_addr = (uint32_t)osd_pixels;
    osdData.block = nOSDBlock;
    osdData.alpha = nAlpha;

    if(ioctl(fbfd, SKYFB_OSD_FILL, &osdData) == -1)
    {
        LOGE("SKYFB_OSD_FILL ioctl failed\n");
        close(fbfd);
        return;
    }
    close(fbfd);
}

static void
native_drawARGB_To_OSD(JNIEnv *env, jobject thiz, int nOSDBlock, int nAlpha, jintArray ARGB_pixels, int nWidth, int nHeight, int nPosX, int nPosY)
{
    jbyteArray ret = NULL;
    if(NULL == ARGB_pixels)
    {
        return;
    }
    
    jint len = env->GetArrayLength(ARGB_pixels);
    if(0 == len)
    {
        return;
    }

    int* ARGB_data = (int*)env->GetIntArrayElements(ARGB_pixels, 0);
    if(NULL == ARGB_data)
    {
        return;
    }

    unsigned char r, g, b;
    unsigned char RGB232_color = 0;
    unsigned char* RGB232_pixels = new unsigned char[len];
#ifdef _DEBUG_DISPLAY_
    clock_t duration = clock();
#endif

    for(int nPixelIndex = 0; nPixelIndex < len; nPixelIndex++)
    {
        RGB232_color = 0;
        if((ARGB_data[nPixelIndex] & 0xFF000000) != 0)
        {
            r = (unsigned char)(ARGB_data[nPixelIndex]>>16);
            g = (unsigned char)(ARGB_data[nPixelIndex]>>8);
            b = (unsigned char)(ARGB_data[nPixelIndex]);

            RGB232_color = _matchColor(RED,   4, r, false) |
                           _matchColor(GREEN, 8, g, false) |
                           _matchColor(BLUE,  4, b, false);
        }
        RGB232_pixels[nPixelIndex] = RGB232_color;
    }
#ifdef _DEBUG_DISPLAY_
    duration = clock()-duration;
    LOGD("    === Transform %dx%d(%d) pixels ARGB to RGB232 in %d ms ===\n", nWidth, nHeight, len, (int)((double)(duration)/(double)CLOCKS_PER_SEC*1000.0f));
#endif

    _fillOSD(nOSDBlock, nAlpha, nPosX, nPosY, nWidth, nHeight, RGB232_pixels);
    env->ReleaseIntArrayElements(ARGB_pixels, ARGB_data, 0);
}

static void
native_drawRGB232_To_OSD(JNIEnv *env, jobject thiz, int nOSDBlock, int nAlpha, jbyteArray data, int nWidth, int nHeight, int nPosX, int nPosY)
{
    jbyteArray ret = NULL;
    if(NULL == data)
    {
        return;
    }
    
    jint len = env->GetArrayLength(data);
    if(0 == len)
    {
        return;
    }

    unsigned char* RGB232_pixels = (unsigned char*)env->GetByteArrayElements(data, 0);
    if(NULL == RGB232_pixels)
    {
        return;
    }

#ifdef _DEBUG_DISPLAY_
    LOGD("native_drawRGB232_To_OSD: %d x %d @(%d, %d)\n", nWidth, nHeight, nPosX, nPosY);
#endif
    _fillOSD(nOSDBlock, nAlpha, nPosX, nPosY, nWidth, nHeight, RGB232_pixels);
    env->ReleaseByteArrayElements(data, (jbyte*)RGB232_pixels, 0);
}

//======================================================================================================
//  JNI Methods
//======================================================================================================
static JNINativeMethod methods[] = {
    { "native_lockDisplay",                "()Z",         (void*)native_lockDisplay },
    { "native_turnDisplayOff",             "()Z",         (void*)native_turnDisplayOff },    
    { "native_restoreDisplay",             "()Z",         (void*)native_restoreDisplay },
    { "native_isDisplayLocked",            "()Z",         (void*)native_isDisplayLocked },

    { "native_copyPixelsToDisplay2",       "([IIIII)V",   (void*)native_copyPixelsToDisplay2 },
    { "native_setDisplay2Params",          "(IIII)V",     (void*)native_setDisplay2Params },
    { "native_showDisplay2",               "()V",         (void*)native_showDisplay2 },
    { "native_hideDisplay2",               "()V",         (void*)native_hideDisplay2 },
    { "native_cleanDisplay2",              "()V",         (void*)native_cleanDisplay2 },
    { "native_resetDisplay2",              "()V",         (void*)native_resetDisplay2 },
    { "native_getDisplayOutputResolution", "()I",         (void*)native_getDisplayOutputResolution },

    { "native_cleanOSD",                   "(I)V",        (void*)native_cleanOSD },
    { "native_drawARGB_To_OSD",            "(II[IIIII)V", (void*)native_drawARGB_To_OSD },
    { "native_drawRGB232_To_OSD",          "(II[BIIII)V", (void*)native_drawRGB232_To_OSD },
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
