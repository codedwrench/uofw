/* Copyright (C) 2021 The uOFW team
See the file COPYING for copying permission.
*/

#include <common/types.h>

#include <adhocctl/pspnet_adhoc_auth.h>
#include <adhocctl/pspnet_adhocctl.h>
#include <common_imp.h>
#include <net/psp_net_error.h>
#include <pspnet/pspnet.h>
#include <pspwlan.h>
#include <registry/registry.h>
#include <sysmem_kdebug.h>
#include <threadman_kernel.h>
#include <usersystemlib_kernel.h>
#include <utility/utility_sysparam.h>

/**
 * uofw/src/net/pspnet_adhocctl.c
 *
 * PSP Adhoc control - PSP Adhoc control networking libraries
 *
 * I honestly don't know what this does yet.
 * TODO: Figure that out
 */

int g_Init = 0; // 0x0
char g_SSIDPrefix[4]; // 0x4

struct unk_struct g_Unk; // 0x8

struct unk_struct2 g_Unk2; // 0x7E0

// TODO: What's in 0x8A2 - 0x8A8 ?
// Vars at: [((fptr) 0x4), (0xC)] * (i * 0x10)
char g_Unk4[4][16]; // 0x8A8

char g_Unk5[4][16]; // 0x8E0

// TODO: What's in 0x920 - 0x944
s32 g_Unk6; // 0x944

s32 g_MutexInited; // 0x1948
s32 *g_WorkAreaPtr; // 0x194C

const char g_WifiAdapter[] = "wifi";
const char g_DefSSIDPrefix[] = "PSP";
const char g_AdhocRegString[] = "/CONFIG/NETWORK/ADHOC";
const char g_SSIDPrefixRegKey[] = "ssid_prefix";
const char g_SSIDSeparator = '_'; // 0x653c

SCE_MODULE_INFO(
        "sceNetAdhocctl_Library",
        SCE_MODULE_ATTR_EXCLUSIVE_LOAD | SCE_MODULE_ATTR_EXCLUSIVE_START,
        1, 6);

SCE_SDK_VERSION(SDK_VERSION);

s32 CreateLwMutex();

s32 GetChannelAndSSID(struct unk_struct *unpackedArgs, char *ssid, int *channel);

/**
 * Builds the SSID to be used to make the connection.
 *
 * @param unpackedArgs Pointer to struct with all the information.
 * @param ssid The resulting SSID.
 * @param adhocSubType Whether it's normal adhoc or gamemode (G/L).
 * @param ssidSuffix What to put behind the last separator.
 *
 * @return Length of the SSID.
 */
s32 BuildSSID(struct unk_struct *unpackedArgs, char* ssid, char adhocSubType, char *ssidSuffix);

/**
 * Gets the prefix of the SSID (PSP)
 * @param ssidPrefix Where to put the string.
 *
 * @return 0 if successful.
 */
s32 GetSSIDPrefix(char *ssidPrefix);

s32 StartAuthAndThread(s32 stackSize, s32 priority, struct ProductStruct *product);

s32 ThreadFunc(SceSize args, void *argp);

s32 sceNetAdhocctlInit(s32 stackSize, s32 priority, struct ProductStruct *product) {
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

    if (((stackSize == 0) | priority >> 31) || product == NULL || product->adhocId >= 3) {
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

s32 StartAuthAndThread(s32 stackSize, s32 priority, struct ProductStruct *product) {
    // Seems to do something with buffers
    s32 *tmp[4];
    s32 ret;

    tmp[0] = (int *) &g_Unk;

    sceKernelMemset(&g_Unk, 0, sizeof(struct unk_struct));
    sceKernelMemset(g_Unk4, 0, 64);
    sceKernelMemset(g_Unk5, 0, 64);

    ret = sceNetAdhocAuthInit();
    if (ret < 0) {
        return ret;
    } else {
        sceKernelMemcpy((void *) &g_Unk.product, product, (sizeof(struct ProductStruct)));
        ret = CreateLwMutex();
        if (ret >= 0) {
            ret = sceKernelCreateEventFlag("SceNetAdhocctl", 0, 0, 0);
            if (ret >= 0) {
                g_Unk.eventFlags = ret;
                g_Unk.kernelFlags = sceKernelCreateThread("SceNetAdhocctl", ThreadFunc, priority, stackSize);
            }
        }
    }
    sceNetAdhocAuthTerm();


}

u32 InitAdhoc(struct unk_struct *unpackedArgs) {
    int iVar1;
    s32 tmp;
    uint uVar3;
    int i;
    char unk[33];
    undefined4 local_d0;
    undefined4 local_cc;
    undefined4 local_c8;
    undefined4 local_c4;
    undefined4 local_c0;
    undefined auStack176[128];
    u32 channel;
    undefined4 local_2c[3];

    i = 0;
    if (unpackedArgs->connectionState != 0) {
        return SCE_ERROR_NET_ADHOCCTL_ALREADY_CONNECTED;
    }

    if (sceWlanGetSwitchState() == 0) {
        return SCE_ERROR_NET_ADHOCCTL_WLAN_SWITCH_DISABLED;
    }
    while (1) {
        // Used for timeout
        i++;

        tmp = sceWlanDevAttach();
        if (tmp == 0 || ((u32) tmp == SCE_ERROR_NET_WLAN_ALREADY_ATTACHED)) break;
        // Return on negative error code or retry if not ready yet
        if ((tmp >> 0x1f & ((u32) tmp != SCE_ERROR_NET_WLAN_DEVICE_NOT_READY)) != 0) {
            return tmp;
        }
        if (unpackedArgs->timeout <= i) {
            return SCE_ERROR_NET_ADHOCCTL_TIMEOUT;
        }

        sceKernelDelayThread(1000000);
    }
    unpackedArgs->unk5 = unpackedArgs->unk5 & 0xc0000001;
    if ((sceNetConfigUpInterface(g_WifiAdapter) >= 0) &&
        // Event Flag 8, adhoc inited?
        (sceNetConfigSetIfEventFlag(g_WifiAdapter, unpackedArgs->eventFlags, 0b1000) >= 0)) {

        sceKernelMemset(unk, 0, 33);
        tmp = GetChannelAndSSID(unpackedArgs, unk, &channel);
        if (-1 < (int) tmp) {
            i = sceWlanGetSwitchState();
            if (i == 0) {
                tmp = 0x80410b03;
            } else {
                tmp = sceNet_lib_0xD5B64E37(&DAT_00006534, auStack256, tmp, channel);
                uVar3 = FUN_00003cf8(param_1);
                if ((uVar3 << 4) >> 0x14 != 0x41) {
                    if ((((int) tmp < 0) ||
                         (tmp = sceUtilityGetSystemParamString(1, auStack176, 0x80), (int) tmp < 0)) ||
                        (tmp = FUN_00003cf8(param_1), (int) tmp < 0))
                        goto LAB_00001418;
                    local_d0 = 1000000;
                    local_cc = 500000;
                    local_c8 = 5;
                    local_c4 = 30000000;
                    local_c0 = 300000000;
                    tmp = sceNetAdhocAuth_lib_0x89F2A732(&DAT_00006534, 0x30, 0x2000, &local_d0);
                    if ((int) tmp < 0) goto LAB_00001418;
                    param_1->connectionState = 1;
                    param_1->field_0xc = channel;
                    uVar3 = tmp;
                }
                tmp = uVar3;
                if (-1 < (int) tmp) {
                    return tmp;
                }
            }
        }
    }
    LAB_00001418:
    sceNetAdhocAuth_lib_0x2E6AA271();
    sceNetConfigSetIfEventFlag(&DAT_00006534, 0, 0);
    local_2c[0] = 0;
    sceNet_lib_0xDA02F383(&DAT_00006534, local_2c);
    sceNetConfigDownInterface(&DAT_00006534);
    sceWlanDevDetach();
    return tmp;
}

s32 GetChannelAndSSID(struct unk_struct *unpackedArgs, char *ssid, int *channel) {
    s32 adhocChannel;
    s32 ret;

    // TODO: Why is this being memsetted here again?
    sceKernelMemset(ssid, 0, 33);
    ret = sceUtilityGetSystemParamInt(SYSTEMPARAM_INT_ADHOC_CHANNEL, &adhocChannel);
    if (ret >= 0) {
        if ((adhocChannel != 6) && (adhocChannel != 1)) {
            adhocChannel = 11;
        }

        *channel = adhocChannel;

        ret = SCE_ERROR_NET_ADHOCCTL_CANNOT_OBTAIN_CHANNEL;
        if (*channel < 12) {
            ret = BuildSSID(unpackedArgs, ssid, ADHOC_SUBTYPE_NORMALMODE, unpackedArgs->ssidSuffix);
        }
    }
    return ret;
}


s32 BuildSSID(struct unk_struct *unpackedArgs, char* ssid, char adhocSubType, char *ssidSuffix) {
    struct ProductStruct* product;
    int i;
    char adhocTypeStr;
    char* ssidSuffixPtr;
    int ret;

    product = &unpackedArgs->product;
    adhocTypeStr = 'A';
    if (product->adhocId != SCE_NET_ADHOCCTL_ADHOCTYPE_NORMAL) {
        adhocTypeStr = 'X';
        if (product->adhocId != SCE_NET_ADHOCCTL_ADHOCTYPE_RESERVED) {
            if (product->adhocId != SCE_NET_ADHOCCTL_ADHOCTYPE_GAMESHARE) {
                return SCE_ERROR_NET_ADHOCCTL_INVALID_ARG;
            }
            adhocTypeStr = 'S';
        }
    }

    // Building SSID String
    sceKernelMemset(ssid, 0, 0x21);

    sceKernelMemcpy(ssid, g_SSIDPrefix, 3); // PSP
    sceKernelMemcpy(ssid + 3, &g_SSIDSeparator, 1); // _
    sceKernelMemcpy(ssid + 4, &adhocTypeStr, 1); // A or S
    sceKernelMemcpy(ssid + 5, product->product, 9); // UCUS98612
    sceKernelMemcpy(ssid + 14, &g_SSIDPrefix, 1); // _
    sceKernelMemcpy(ssid + 15, &adhocSubType, 1); // L or G
    sceKernelMemcpy(ssid + 16, &g_SSIDSeparator, 1); // _

    ret = 17;

    if (ssidSuffix) {
        ssidSuffixPtr = ssidSuffix;

        for (i = 0; (i < 8) && (ssidSuffixPtr != NULL); i++) {
            sceKernelMemcpy(ssid + 17 + i, ssidSuffixPtr, 1);
            ssidSuffixPtr++;
        }
    }

    return ret;
}

s32 ThreadFunc(SceSize args, void *argp) {
    int iVar1;
    int connectionState;
    u32 usedInFuncBufThing;
    u32 tmp;
    u32 event;
    int ret;

    struct unk_struct *unpackedArgs = (struct unk_struct *) argp;

    do {
        while (1) {
            sceKernelWaitEventFlag(unpackedArgs->eventFlags, 0xfff, 1, &event, 0);
            if ((event & 0x200) != 0) {
                sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xfffffdff);
                if (unpackedArgs->connectionState == 1) {
                    // FUN_000022b8(unpackedArgs);
                } else {
                    if (unpackedArgs->connectionState == 3) {
                        // FUN_0000554c(unpackedArgs, g_Unk2);
                    }
                }
                return 0;
            }
            if ((event & 8) == 0) break;
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xfffffff7);
            connectionState = unpackedArgs->connectionState;
            if ((connectionState == 1 || connectionState == 4) || (connectionState == 5)) {
                // iVar2 = FUN_00003cd8(unpackedArgs);
                if (connectionState >= 0) {
                    if (unpackedArgs->connectionState == 4) {
                        // FUN_0000233c(unpackedArgs);
                        usedInFuncBufThing = 6;
                        LAB_00003b28:
                        //FUN_00002600(uVar3, 0);
                        tmp = unpackedArgs->unk5;
                    } else {
                        if (unpackedArgs->connectionState == 5) {
                            // FUN_0000233c(unpackedArgs);
                            usedInFuncBufThing = 7;
                            if ((unpackedArgs->unk5 & 0x10) != 0) {
                                usedInFuncBufThing = 8;
                                unpackedArgs->unk5 = unpackedArgs->unk5 & 0xf;
                            }
                            //FUN_00002600(uVar3, 0);
                        }
                        tmp = unpackedArgs->unk5;
                    }
                    if ((tmp & 0x10) != 0) {
                        unpackedArgs->unk5 = tmp & 0xf;
                    }
                    break;
                }
                //FUN_000022b8(unpackedArgs);
            } else {
                if ((connectionState != 3) || /* (iVar2 = FUN_00003cd8(param_2) && */ connectionState >= 0) {
                    break;
                }
                //FUN_0000554c(unpackedArgs, g_Unk2);
            }
            LAB_000038bc:
            //FUN_00002600(0, actInThread);
            unpackedArgs->unk5 = unpackedArgs->unk5 & 0xe0000000;
        }
        if ((event & SCE_NET_EVENT_ADHOCCTL_CONNECT) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xfffffffe);
            connectionState = InitAdhoc(unpackedArgs);
            if (connectionState >= 0) {
                //FUN_00002600(1, 0);
                goto LAB_00003800;
            }
            LAB_000038a8:
            //iVar1 = FUN_00003cf8(unpackedArgs);
            if (iVar1 < 0) {
                connectionState = iVar1;
            }
            goto LAB_000038bc;
        }
        LAB_00003800:
        if ((event & SCE_NET_EVENT_ADHOCCTL_DISCONNECT) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xfffffffd);
            //FUN_000022b8(unpackedArgs);
            //FUN_00002600(2, 0);
        }
        if ((event & 4) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xfffffffb);
            //connectionState = FUN_00001534(unpackedArgs);
            //if (connectionState < 0) goto LAB_000038a8;
            //FUN_00002600(3, 0);
        }
        if ((event & 0x10) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xffffffef);
            //connectionState = FUN_00003f00(unpackedArgs, g_Unk2);
            //if (connectionState < 0) goto LAB_000038a8;
            //FUN_00002600(4, 0);
        }
        if ((event & 0x20) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xffffffdf);
            //connectionState = FUN_000046ac(param_2, 0x7e0);
            //if (connectionState < 0) goto LAB_000038a8;
            //FUN_00002600(4, 0);
        }
        if ((event & 0x40) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xffffffbf);
            //FUN_0000554c(param_2, 0x7e0);
            //FUN_00002600(2, 0);
        }
        if ((event & 0x80) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xffffff7f);
            //connectionState = FUN_000018a8(param_2);
            //if (connectionState < 0) goto LAB_000038a8;
            //FUN_00002600(1, 0);
        }
        if ((event & 0x100) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xfffffeff);
            //connectionState = FUN_00001b9c(param_2);
            //if (connectionState < 0) goto LAB_000038a8;
            //FUN_00002600(1, 0);
        }
        if ((event & 0x400) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xfffffbff);
            //connectionState = FUN_00001ec8(param_2);
            //if (connectionState < 0) goto LAB_000038a8;
            //FUN_00002d40(5, 1);
        }
        if ((event & 0x800) == 0) {
            tmp = g_Unk.connectionState;
        } else {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xfffff7ff);
            //connectionState = FUN_000020ac(param_2);
            //if (connectionState < 0) goto LAB_000038a8;
            //FUN_00002d40(5, 1);
            tmp = g_Unk.connectionState;
        }
        g_Unk.connectionState = (s32) (tmp & 0xe0000000);
    } while (1);
}

s32 CreateLwMutex() {
    s32 ret;

    ret = sceKernelCreateLwMutex(g_WorkAreaPtr, "SceNetAdhocctl", 512, 0, 0);
    if (ret >= 0) {
        g_MutexInited = 1;
        ret = 0;
    }
    return ret;
}

s32 GetSSIDPrefix(char *ssidPrefix) {
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
                && (sceRegGetKeyValue(keyHandle, regHandle, ssidPrefix, (sizeof(g_DefSSIDPrefix))) >= 0)) {
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