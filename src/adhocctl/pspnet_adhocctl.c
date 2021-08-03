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

typedef void(*unk_struct3_func)(s32 unk, s32 unk2, s32 unk3);

struct unk_struct3 {
    s32 unk;
    unk_struct3_func func;
    s32 func_argument;
    s32 unk2;
};

// TODO: What's in 0x8A2 - 0x8A8 ?
// Vars at: [((fptr) 0x4), (0xC)] * (i * 0x10)
struct unk_struct3 *g_Unk4[4]; // 0x8A8

struct unk_struct3 *g_Unk5[4]; // 0x8E0

// TODO: What's in 0x920 - 0x944
s32 g_Unk6; // 0x944

s32 g_MutexInited; // 0x1948
SceLwMutex *g_Mutex; // 0x194C

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

s32 GetChannelAndSSID(struct unk_struct *unpackedArgs, char *ssid, u32 *channel);

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
s32 BuildSSID(struct unk_struct *unpackedArgs, char *ssid, char adhocSubType, char *ssidSuffix);

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
    s32 ret;

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
                g_Unk.kernelFlags = sceKernelCreateThread("SceNetAdhocctl", ThreadFunc, priority, stackSize, 0, 0);
            }
        }
    }
    sceNetAdhocAuthTerm();

    return ret;
}

s32 SomethingWithFunctionPointers(s32 unk, s32 unk2) {
    s32 lockRes;

    struct unk_struct3 *funcPtr = g_Unk5[0];
    u32 i = 4;

    lockRes = sceKernelLockLwMutex(g_Mutex, 1);

    while (i > (sizeof(g_Unk5))) {
        if (funcPtr->func != NULL) {
            funcPtr->func(unk, unk2, funcPtr->func_argument);
        }
        i--;
        funcPtr++;
    }

    if (lockRes == 0) {
        sceKernelUnlockLwMutex(g_Mutex, 1);
    }
    return 0;
}

s32 FUN_00003bc0(struct unk_struct *unpackedArgs) {
    s32 ret;
    s32 eventAddr;
    s32 unk;
    u32 outBits;

    ret = sceNetConfigGetIfEvent(g_WifiAdapter, &eventAddr, &unk);
    while (ret != (s32) SCE_NET_ID_CORE_NO_EVENT) {
        if (ret < 0) {
            return ret;
        }
        if (eventAddr == 8) {
            if (unpackedArgs->connectionState == 5) {
                unpackedArgs->unk5 |= 0x10;
            }
            return 0;
        }

        if (eventAddr == 9) {
            return 0;
        }

        ret = sceNetConfigGetIfEvent(g_WifiAdapter, &eventAddr, &unk);
    }

    while (1) {
        ret = sceKernelWaitEventFlag(unpackedArgs->eventFlags, 0x8, 1, &outBits, 0);
        if (ret < 0) {
            return ret;
        }

        sceKernelClearEventFlag(unpackedArgs->eventFlags, ~0x8);

        ret = sceNetConfigGetIfEvent(g_WifiAdapter, &eventAddr, &unk);
        while (ret != (s32) SCE_NET_ID_CORE_NO_EVENT) {
            if (ret < 0) {
                return ret;
            }
            if (eventAddr == 8) {
                if (unpackedArgs->connectionState == 5) {
                    unpackedArgs->unk5 |= 0x10;
                }
                return 0;
            }
            if (eventAddr == 9) {
                return 0;
            }
            ret = sceNetConfigGetIfEvent(g_WifiAdapter, &eventAddr, &unk);
        }
        ret = unpackedArgs->eventFlags;
    }
}

int FUN_00003d80(struct unk_struct *unpackedArgs, s32 *eventAddr, s32 *unk) {
    s32 ret = 0;
    s32 event;
    s32 tmp;

    while (1) {
        tmp = sceNetConfigGetIfEvent(g_WifiAdapter, eventAddr, unk);

        if (tmp == (s32) SCE_NET_ID_CORE_NO_EVENT) {
            break;
        }
        if (tmp < 0) {
            ret = tmp;
            break;
        }

        event = *eventAddr;
        if (event == 4) {
            if (unpackedArgs->connectionState == 1) {
                // TODO: What does this mean?
                unpackedArgs->unk3 = 0x80410B83;
            }
            ret = SCE_ERROR_NET_ADHOCCTL_WLAN_SWITCH_DISABLED;
        } else if (event == 5) {
            if ((unpackedArgs->unk5 & 0x02) == 0) {
                ret = SCE_ERROR_NET_ADHOCCTL_BEACON_LOST;
                unpackedArgs->unk5 |= 0x02;
            }
        } else if (event == 7) {
            if (unpackedArgs->connectionState == 1) {
                sceNetAdhocAuth_lib_0x2E6AA271();
                // TODO: What does this mean?
                unpackedArgs->unk3 = 0X80410B84;
            }
            SomethingWithFunctionPointers(5, 0);
            ret = FUN_00003bc0(unpackedArgs);
            if ((ret >= 0) && (unpackedArgs->connectionState != 5)) {
                ret = SCE_ERROR_NET_ADHOCCTL_UNKOWN_ERR1;
            }
        }
    }
    if (sceKernelPollEventFlag(unpackedArgs->eventFlags, 0x42, SCE_KERNEL_EW_OR, NULL) == 0) {
        ret = 0;
    }
    return ret;
}

int FUN_00003cf8(struct unk_struct *unpackedArgs) {
    s32 ret;
    u32 outBits;
    s32 eventAddr;
    s32 unk;

    ret = sceKernelPollEventFlag(unpackedArgs->eventFlags, 0x8, SCE_KERNEL_EW_OR, &outBits);

    if (ret != (s32) SCE_ERROR_KERNEL_EVENT_FLAG_POLL_FAILED) {
        if (ret >= 0) {
            if ((outBits & 0x8) != 0) {
                sceKernelClearEventFlag(unpackedArgs->eventFlags, ~0x8);
                ret = FUN_00003d80(unpackedArgs, &eventAddr, &unk);
            }
        }
    } else {
        ret = 0;
    }
    return ret;
}

u32 InitAdhoc(struct unk_struct *unpackedArgs) {
    s32 err = 0;
    s32 ret;
    s32 i;
    char ssid[33];
    s32 unk[5];
    char nickname[128];
    u32 channel;
    s32 unk2[3];

    i = 0;
    if (unpackedArgs->connectionState != 0) {
        return SCE_ERROR_NET_ADHOCCTL_ALREADY_CONNECTED;
    }

    if (sceWlanGetSwitchState() == 0) {
        return SCE_ERROR_NET_ADHOCCTL_WLAN_SWITCH_DISABLED;
    }

    // Try to attach wlan device
    while (1) {
        // Used for timeout
        i++;

        ret = sceWlanDevAttach();

        if (ret == 0 || ((u32) ret == SCE_ERROR_NET_WLAN_ALREADY_ATTACHED)) break;

        // Return on negative error code or retry if not ready yet
        if ((ret >> 0x1f & ((u32) ret != SCE_ERROR_NET_WLAN_DEVICE_NOT_READY)) != 0) {
            return ret;
        }

        if (unpackedArgs->timeout <= i) {
            return SCE_ERROR_NET_ADHOCCTL_TIMEOUT;
        }

        sceKernelDelayThread(1000000);
    }

    unpackedArgs->unk5 = unpackedArgs->unk5 & 0xc0000001;
    if ((sceNetConfigUpInterface(g_WifiAdapter) >= 0) &&
        (sceNetConfigSetIfEventFlag(g_WifiAdapter, unpackedArgs->eventFlags, 0x8) >=
         0)) /* Event Flag 8, adapter inited? */ {

        sceKernelMemset(ssid, 0, (sizeof(ssid)));
        ret = GetChannelAndSSID(unpackedArgs, ssid, &channel);
        if (ret >= 0) {
            if (sceWlanGetSwitchState() == 0) {
                ret = SCE_ERROR_NET_ADHOCCTL_WLAN_SWITCH_DISABLED;
                err = 1;
            } else {
                // This function needs to be disassembled
                ret = sceNet_lib_0xD5B64E37(g_SSIDPrefix, ssid, ret, channel);

                if ((((FUN_00003cf8(unpackedArgs) << 4) >> 0x14) !=
                     0x41) // Check for SCE_ERROR_FACILITY_NETWORK (bits 27-16)
                    || (ret < 0)) { // if sceNet_lib_0xD5B64E37 failed
                    err = 1;
                } else {
                    ret = sceUtilityGetSystemParamString(1, nickname, sizeof(nickname));
                    err = 1;
                }

                if(!err) {
                    // TODO: What does this mean?
                    unk[0] = 1000000;
                    unk[1] = 500000;
                    unk[2] = 5;
                    unk[3] = 30000000;
                    unk[4] = 300000000;

                    ret = sceNetAdhocAuth_lib_0x89F2A732(g_SSIDPrefix, 0x30, 0x2000, unk);
                    if (ret < 0) {
                        err = 1;
                    } else {
                        unpackedArgs->connectionState = 1;
                        unpackedArgs->channel = channel;
                        return ret;
                    }
                }
            }

            if (err) {
                sceNetAdhocAuth_lib_0x2E6AA271();
                sceNetConfigSetIfEventFlag(g_SSIDPrefix, 0, 0);
                unk2[0] = 0;
                sceNet_lib_0xDA02F383(g_SSIDPrefix, unk2);
                sceNetConfigDownInterface(g_SSIDPrefix);
                sceWlanDevDetach();
                return ret;
            }
        }
    }
    return ret;
}

s32 GetChannelAndSSID(struct unk_struct *unpackedArgs, char *ssid, u32 *channel) {
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


s32 BuildSSID(struct unk_struct *unpackedArgs, char *ssid, char adhocSubType, char *ssidSuffix) {
    struct ProductStruct *product;
    int i;
    char adhocTypeStr;
    char *ssidSuffixPtr;
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
    sceKernelMemset(ssid, 0, 33);

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
    // Bypass compiler warning
    (void)(args);

    int iVar1;
    int connectionState;
    //u32 usedInFuncBufThing;
    u32 tmp;
    u32 outBits;

    struct unk_struct *unpackedArgs = (struct unk_struct *) argp;

    do {
        while (1) {
            sceKernelWaitEventFlag(unpackedArgs->eventFlags, 0xFFF, 1, &outBits, 0);
            if ((outBits & 0x200) != 0) {
                sceKernelClearEventFlag(unpackedArgs->eventFlags, ~0x200);
                if (unpackedArgs->connectionState == 1) {
                    // FUN_000022b8(unpackedArgs);
                } else {
                    if (unpackedArgs->connectionState == 3) {
                        // FUN_0000554c(unpackedArgs, g_Unk2);
                    }
                }
                return 0;
            }
            if ((outBits & 8) == 0) break;
            sceKernelClearEventFlag(unpackedArgs->eventFlags, ~0x8);
            connectionState = unpackedArgs->connectionState;
            if ((connectionState == 1 || connectionState == 4) || (connectionState == 5)) {
                // iVar2 = FUN_00003cd8(unpackedArgs);
                if (connectionState >= 0) {
                    if (unpackedArgs->connectionState == 4) {
                        // FUN_0000233c(unpackedArgs);
                        //usedInFuncBufThing = 6;
                        //FUN_00002600(uVar3, 0);
                        tmp = unpackedArgs->unk5;
                    } else {
                        if (unpackedArgs->connectionState == 5) {
                            // FUN_0000233c(unpackedArgs);
                            //usedInFuncBufThing = 7;
                            if ((unpackedArgs->unk5 & 0x10) != 0) {
                                //usedInFuncBufThing = 8;
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
                if ((connectionState != 3) /* || (iVar2 = FUN_00003cd8(param_2) && connectionState >= 0 )*/) {
                    break;
                }
                //FUN_0000554c(unpackedArgs, g_Unk2);
            }
            LAB_000038bc:
            //FUN_00002600(0, actInThread);
            unpackedArgs->unk5 = unpackedArgs->unk5 & 0xe0000000;
        }
        if ((outBits & SCE_NET_EVENT_ADHOCCTL_CONNECT) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xfffffffe);
            connectionState = InitAdhoc(unpackedArgs);
            if (connectionState >= 0) {
                //FUN_00002600(1, 0);
                goto LAB_00003800;
            }
            //iVar1 = FUN_00003cf8(unpackedArgs);
            if (iVar1 < 0) {
                connectionState = iVar1;
            }
            goto LAB_000038bc;
        }
        LAB_00003800:
        if ((outBits & SCE_NET_EVENT_ADHOCCTL_DISCONNECT) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xfffffffd);
            //FUN_000022b8(unpackedArgs);
            //FUN_00002600(2, 0);
        }
        if ((outBits & 4) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xfffffffb);
            //connectionState = FUN_00001534(unpackedArgs);
            //if (connectionState < 0) goto LAB_000038a8;
            //FUN_00002600(3, 0);
        }
        if ((outBits & 0x10) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xffffffef);
            //connectionState = FUN_00003f00(unpackedArgs, g_Unk2);
            //if (connectionState < 0) goto LAB_000038a8;
            //FUN_00002600(4, 0);
        }
        if ((outBits & 0x20) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xffffffdf);
            //connectionState = FUN_000046ac(param_2, 0x7e0);
            //if (connectionState < 0) goto LAB_000038a8;
            //FUN_00002600(4, 0);
        }
        if ((outBits & 0x40) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xffffffbf);
            //FUN_0000554c(param_2, 0x7e0);
            //FUN_00002600(2, 0);
        }
        if ((outBits & 0x80) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xffffff7f);
            //connectionState = FUN_000018a8(param_2);
            //if (connectionState < 0) goto LAB_000038a8;
            //FUN_00002600(1, 0);
        }
        if ((outBits & 0x100) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xfffffeff);
            //connectionState = FUN_00001b9c(param_2);
            //if (connectionState < 0) goto LAB_000038a8;
            //FUN_00002600(1, 0);
        }
        if ((outBits & 0x400) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xfffffbff);
            //connectionState = FUN_00001ec8(param_2);
            //if (connectionState < 0) goto LAB_000038a8;
            //FUN_00002d40(5, 1);
        }
        if ((outBits & 0x800) == 0) {
            tmp = g_Unk.connectionState;
        } else {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, 0xfffff7ff);
            //connectionState = FUN_000020ac(param_2);
            //if (connectionState < 0) goto LAB_000038a8;
            //FUN_00002d40(5, 1);
            tmp = g_Unk.connectionState;
        }
        g_Unk.connectionState = (s32)(tmp & 0xe0000000);
    } while (1);
}

s32 CreateLwMutex() {
    s32 ret;

    ret = sceKernelCreateLwMutex(g_Mutex, "SceNetAdhocctl", 512, 0, 0);
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