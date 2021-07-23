/* Copyright (C) 2021 The uOFW team
See the file COPYING for copying permission.
*/

#include <common/types.h>

#include <adhocctl/pspnet_adhoc_auth.h>
#include <adhocctl/pspnet_adhocctl.h>
#include <common_imp.h>
#include <net/psp_net_error.h>
#include <pspnet/pspnet.h>
#include <registry/registry.h>
#include <sysmem_kdebug.h>
#include <threadman_kernel.h>
#include <usersystemlib_kernel.h>

/**
 * uofw/src/net/pspnet_adhocctl.c
 *
 * PSP Adhoc control - PSP Adhoc control networking libraries
 *
 * I honestly don't know what this does yet.
 * TODO: Figure that out
 */

struct unk_struct {
    SceUID eventFlags; // 0x0
    SceUID kernelFlags; // 0x4
    s32 someActionInThreadFunc; // 0x8
    char unk[12]; // 0xC
    s32 unk2; // 0x18, could contain 0x80410b83
    s32 unk3; // 0x1C
    s32 unk4; // 0x20, flag (param_1->field_0x20 & 2)
    char unk5[1972]; // 0x24
    struct ProductStruct product;
};

int g_Init = 0; // 0x0
char g_SSIDPrefix[4]; // 0x4

struct unk_struct g_Unk; // 0x8

// (g_Unk2 + 0xb8) & 2) != 0) {
char g_Unk2[188];
// TODO: What's in 0x8A2 - 0x8A8 ?
char g_Unk3[64]; // 0x8A8
char g_Unk4[64]; // 0x8E0

s32 g_MutexInited; // 0x1948
s32* g_WorkAreaPtr; // 0x194C

const char g_WifiAdapter[] = "wifi";
const char g_DefSSIDPrefix[] = "PSP";
const char g_AdhocRegString[] = "/CONFIG/NETWORK/ADHOC";
const char g_SSIDPrefixRegKey[] = "ssid_prefix";

SCE_MODULE_INFO(
    "sceNetAdhocctl_Library",
    SCE_MODULE_ATTR_EXCLUSIVE_LOAD | SCE_MODULE_ATTR_EXCLUSIVE_START,
    1, 6);

SCE_SDK_VERSION(SDK_VERSION);

s32 CreateLwMutex();
s32 GetSSIDPrefix(char *ssidPrefix);
s32 StartAuthAndThread(s32 stackSize, s32 priority, struct ProductStruct* product);
s32 ThreadFunc(SceSize args, void *argp);

s32 sceNetAdhocctlInit(s32 stackSize, s32 priority, struct ProductStruct* product) {
    u32 i = 0;
    s32 errorCode = 0;

    // We need atleast 3040 bytes
    if (sceKernelCheckThreadStack() < 3040) {
        return SCE_ERROR_NET_NOT_ENOUGH_STACK_SPACE;
    }

    if (g_Init > 0) {
        // Why does it reset the init flag in the disassembly ??!
        g_Init = 0;
        return SCE_ERROR_NET_ADHOCCTL_ALREADY_INITIALIZED;
    }

    // TODO: figure out why this needs to be bigger than the available stack size.
    if (stackSize < 3072) {
        g_Init = 0;
        return SCE_ERROR_NET_ADHOCCTL_ARG_INVALID_STACK_SIZE;
    }

    if (((stackSize == 0) | priority >> 31) || product == NULL || product->adhocId >= 3)
    {
        g_Init = 0;
        return SCE_ERROR_NET_ADHOCCTL_INVALID_ARG;
    }

    for (i = 0; i < (sizeof(product->product)); i++) {
        if (product->product[i] == '\0') {
            g_Init = 0;
            break;
        }
        if ((i >= 9) && (product->adhocId == SCE_NET_ADHOCCTL_ADHOCTYPE_RESERVED)) {
            sceKernelPrintf("WARNING: SCE_NET_ADHOCCTL_ADHOCTYPE_RESERVED as adhoc_id is for debugging purposes only");
        }

        GetSSIDPrefix(g_SSIDPrefix);

        // TODO Init everything else
        // errorCode = StartAuthAndThread(stackSize, priority);
        if (errorCode < 0) {
            g_Init = 0;
        } else {
            g_Init = 1;
            return errorCode;
        }
    }
    return SCE_ERROR_NET_ADHOCCTL_INVALID_ARG;
}

s32 StartAuthAndThread(s32 stackSize, s32 priority, struct ProductStruct* product)
{
    // Seems to do something with buffers
    s32* tmp[4];
    s32 ret;

    tmp[0] = (int *) &g_Unk;

    sceKernelMemset(&g_Unk, 0, sizeof(struct unk_struct));
    sceKernelMemset(g_Unk3, 0, 64);
    sceKernelMemset(g_Unk4, 0, 64);

    ret = sceNetAdhocAuthInit();
    if (ret < 0) {
        return ret;
    } else {
        sceKernelMemcpy((void*) &g_Unk.product, product, (sizeof(struct ProductStruct)));
        ret = CreateLwMutex();
        if (ret >= 0) {
            ret = sceKernelCreateEventFlag("SceNetAdhocctl",0,0,0);
            if(ret >= 0) {
                g_Unk.eventFlags = ret;
                g_Unk.kernelFlags = sceKernelCreateThread("SceNetAdhocctl", ThreadFunc, priority, stackSize);
            }
        }
    }
    sceNetAdhocAuthTerm();


}

s32 CreateLwMutex()
{
    s32 ret;

    ret = sceKernelCreateLwMutex(g_WorkAreaPtr, "SceNetAdhocctl", 512, 0, 0);
    if ( ret >= 0 )
    {
        g_MutexInited = 1;
        ret = 0;
    }
    return ret;
}

s32 GetSSIDPrefix(char *ssidPrefix)
{
    u32 prefixSize;
    struct RegParam regParams;
    u32 regHandle;
    u32 keyHandle;
    enum RegKeyTypes regKeyType;

    sceKernelMemset(&regParams, 0, (sizeof(struct RegParam)));
    regParams.unk2 = 1;
    regParams.unk3 = 1;
    regParams.regtype = 1;

    if (sceRegOpenRegistry(&regParams, 2, &regHandle) >= 0) {
        if (sceRegOpenCategory(regHandle, g_AdhocRegString, 2, &keyHandle) >= 0) {

            // Key would usually be "PSP" so size 4 with null terminator and type string
            if ((sceRegGetKeyInfo(regHandle, g_SSIDPrefixRegKey, &regHandle, &regKeyType, &prefixSize) >= 0)
                    && (prefixSize == (sizeof(g_DefSSIDPrefix)))
                    && (regKeyType == REG_TYPE_STR)
                    && (sceRegGetKeyValue(keyHandle, regHandle, ssidPrefix, (sizeof(g_DefSSIDPrefix))) >= 0))
            {
                sceRegCloseCategory(keyHandle);
                return sceRegCloseRegistry(regHandle);
            }
            sceRegCloseCategory(keyHandle);
        }
        sceRegCloseRegistry(regHandle);
    }

    prefixSize = sceNetStrlen(g_DefSSIDPrefix);
    return (s32) sceKernelMemcpy(ssidPrefix, g_DefSSIDPrefix, prefixSize);
}