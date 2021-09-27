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
#include <wlan.h>

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

struct unk_struct2 g_Unk2; // 0x7E0 // 0x9E642F0

// Vars at: [((fptr) 0x4), (0xC)] * (i * 0x10)
struct AdhocHandler *g_Unk4[4]; // 0x8A0

struct AdhocHandler *g_Unk5[4]; // 0x8E0

struct unk_struct3 g_Unk6[3]; // 0x920
s32 g_Unk7; // 0x944

// 0x00 = next?
// 0x0a = channel
struct ScanData g_ScanBuffer[32]; // 0xd48 = 3072 bytes

// 0x6418 - Debug ADHOCTYPE warning
const char g_AdhocRegString[] = "/CONFIG/NETWORK/ADHOC"; // 0x650c
const char g_SSIDPrefixRegKey[] = "ssid_prefix";         // 0x6524
const char g_WifiAdapter[] = "wlan";                     // 0x6534
const char g_SSIDSeparator = '_';                        // 0x653c
const char g_ThreadName[] = "SceNetAdhocctl";            // 0x6540
const char g_DefSSIDPrefix[] = "PSP";                    // 0x6550
const char g_WifiAdapter2[] = "wlan";                    // 0x6554
const char g_WifiAdapter3[] = "wlan";                    // 0x655c
const char g_WifiAdapter4[] = "wlan";                    // 0x6564
const char g_MutexName[] = "SceNetAdhocctl";             // 0x656c
const char g_Unk9;                                       // 0x657b
const char g_AllChannels[3] = {11, 6, 1};                // 0x657c

s32 g_MutexInited;  // 0x1948
SceLwMutex g_Mutex; // 0x194C

SCE_MODULE_INFO(
        "sceNetAdhocctl_Library",
        SCE_MODULE_ATTR_EXCLUSIVE_LOAD | SCE_MODULE_ATTR_EXCLUSIVE_START,
        1, 6);

SCE_SDK_VERSION(SDK_VERSION);

s32 CreateLwMutex();

void DeleteLwMutex();

s32 GetChannelAndSSID(struct unk_struct *unpackedArgs, char *ssid, u32 *channel);

int GameModeParseBeaconFrame(struct unk_struct2 *param_1, char *channel, char *unk);

s32 FUN_000054d8(char param_1);

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

// Entry:
// 0x9E5D300
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
        errorCode = StartAuthAndThread(stackSize, priority, product);
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
            ret = sceKernelCreateEventFlag(g_MutexName, 0, 0, 0);
            if (ret >= 0) {
                g_Unk.eventFlags = ret;
                g_Unk.tid = sceKernelCreateThread(g_ThreadName, ThreadFunc, priority, stackSize, 0, 0);
                if (g_Unk.tid >= 0) {
                    ret = sceKernelStartThread(g_Unk.tid, 4, &g_Unk);
                    if (ret >= 0) {
                        g_Unk.stackSpace = stackSize - 3072;
                        g_Unk.timeout = 5;
                        return ret;
                    }
                    sceKernelTerminateThread(g_Unk.tid);
                    sceKernelDeleteThread(g_Unk.tid);
                }
                sceKernelDeleteEventFlag(g_Unk.eventFlags);
            }
        }
        DeleteLwMutex();
    }
    sceNetAdhocAuthTerm();

    return ret;
}

s32 RunAdhocctlHandlers(s32 flag, s32 err) {
    u32 i = 4;
    s32 lockRes = sceKernelLockLwMutex(&g_Mutex, 1);;
    s32 oldGp;
    s32 oldSp;
    s32 stackSpace;
    struct AdhocHandler *handlerStruct = g_Unk4[0];

    while (i > 0) {
        if (handlerStruct->handler != NULL) {
            // Sets the global pointer to the new spot
            oldGp = pspSetGp(handlerStruct->funcGlobalArea);

            stackSpace = sceKernelCheckThreadStack() - g_Unk.stackSpace;
            if (stackSpace < 1) {
                handlerStruct->handler(flag, err, handlerStruct->handlerArg);
            } else {
                stackSpace += 0xF;
                stackSpace = stackSpace & (s32) 0xFFFFFFF0; // remove last nibble
                oldSp = pspGetSp();
                pspSetSp(oldSp - stackSpace); // increase stack space

                handlerStruct->handler(flag, err, handlerStruct->handlerArg);

                pspSetSp(oldSp); // Return to old stackpointer
            }
            pspSetGp(oldGp);
        }
        i--;
        handlerStruct++;
    }

    handlerStruct = g_Unk5[0];
    i = 4;

    // Seems that there is no stack size magic involved here
    while (i > 0) {
        if (handlerStruct->handler != NULL) {
            // Sets the global pointer to the new spot
            oldGp = pspSetGp(handlerStruct->funcGlobalArea);
            handlerStruct->handler(flag, err, handlerStruct->handlerArg);
            pspSetGp(oldGp);
        }
        i--;
        handlerStruct++;
    }

    if (!lockRes) {
        sceKernelUnlockLwMutex(&g_Mutex, 1);
    }
    return 0;
}

s32 FUN_00003bc0(struct unk_struct *unpackedArgs) {
    s32 ret;
    s32 eventAddr;
    s32 unk;
    u32 outBits;

    ret = sceNetConfigGetIfEvent(g_WifiAdapter3, &eventAddr, &unk);
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

        ret = sceNetConfigGetIfEvent(g_WifiAdapter3, &eventAddr, &unk);
    }

    while (1) {
        ret = sceKernelWaitEventFlag(unpackedArgs->eventFlags, SCE_NET_ADHOCCTL_DEVICE_UP, 1, &outBits, 0);
        if (ret < 0) {
            return ret;
        }

        sceKernelClearEventFlag(unpackedArgs->eventFlags, ~SCE_NET_ADHOCCTL_DEVICE_UP);

        ret = sceNetConfigGetIfEvent(g_WifiAdapter3, &eventAddr, &unk);
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
            ret = sceNetConfigGetIfEvent(g_WifiAdapter3, &eventAddr, &unk);
        }
        ret = unpackedArgs->eventFlags;
    }
}

int StartConnection(struct unk_struct *unpackedArgs, s32 *eventAddr, s32 *unk) {
    s32 ret = 0;
    s32 event;
    s32 tmp;

    while (1) {
        tmp = sceNetConfigGetIfEvent(g_WifiAdapter3, eventAddr, unk);

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
            RunAdhocctlHandlers(5, 0);
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

int WaitEventDeviceUpAndConnect(struct unk_struct *unpackedArgs) {
    s32 ret;
    u32 outBits;
    s32 eventAddr;
    s32 unk;

    ret = sceKernelPollEventFlag(unpackedArgs->eventFlags, SCE_NET_ADHOCCTL_DEVICE_UP, SCE_KERNEL_EW_OR, &outBits);

    if (ret != (s32) SCE_ERROR_KERNEL_EVENT_FLAG_POLL_FAILED) {
        if (ret >= 0) {
            if ((outBits & SCE_NET_ADHOCCTL_DEVICE_UP) != 0) {
                sceKernelClearEventFlag(unpackedArgs->eventFlags, ~SCE_NET_ADHOCCTL_DEVICE_UP);
                ret = StartConnection(unpackedArgs, &eventAddr, &unk);
            }
        }
    } else {
        ret = 0;
    }
    return ret;
}

s32 InitAdhoc(struct unk_struct *unpackedArgs) {
    s32 err = 0;
    s32 ret;
    s32 i;
    char ssid[33];
    s32 clocks[5];
    char nickname[128];
    u32 channel;
    s32 unk2;

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
        (sceNetConfigSetIfEventFlag(g_WifiAdapter, unpackedArgs->eventFlags, SCE_NET_ADHOCCTL_DEVICE_UP) >=
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

                if ((((WaitEventDeviceUpAndConnect(unpackedArgs) << 4) >> 0x14) !=
                     SCE_ERROR_FACILITY_NETWORK)
                    || (ret < 0)) { // if sceNet_lib_0xD5B64E37 failed
                    err = 1;
                } else {
                    ret = sceUtilityGetSystemParamString(SYSTEMPARAM_STRING_NICKNAME, nickname, sizeof(nickname));
                    if (ret < 0) {
                        err = 1;
                    }
                }

                if (!err) {
                    // Used as first arg in sceKernelSetAlarm in several places
                    // As clock	- The number of micro seconds till the alarm occurs.
                    clocks[0] = 1000000;
                    clocks[1] = 500000;
                    clocks[2] = 5;
                    clocks[3] = 30000000;
                    clocks[4] = 300000000;

                    // Disassembler points to connectionState as being called from the global, so give as global for now.
                    ret = sceNetAdhocAuthCreateStartThread(g_SSIDPrefix, 0x30, 0x2000, clocks,
                                                           &g_Unk.connectionState, nickname);
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
                unk2 = 0;
                sceNet_lib_0xDA02F383(g_SSIDPrefix, &unk2);
                sceNetConfigDownInterface(g_SSIDPrefix);
                sceWlanDevDetach();
                return ret;
            }
        }
    }
    return ret;
}

int MemsetAndBuildGameModeSSID(struct unk_struct *unpackedArgs, char *ssid) {
    sceKernelMemset(ssid, 0, 33);
    return BuildSSID(unpackedArgs, ssid, 'G', unpackedArgs->ssidSuffix);
}

s32 FUN_000050c8(struct unk_struct2 *gameModeData, char* channel, char* unk14) {

}

// CreateEnterGameMode?
uint FUN_00003f00(struct unk_struct *unpackedArgs, struct unk_struct2 *gameModeData) {
    s32 ret;
    s32 tmp;
    char channel;
    s32 flag;
    char unk2;
    SceUInt64 firstSystemTime;
    SceUInt64 secondSystemTime;
    SceUInt64 timeDelta;
    char ssid[33];
    char unk3[112];
    struct unk_struct4 unk4;
    s32 unk5;
    u32 unk19;
    u32 unk19MinusTimeDelta;
    u32 uVar7;
    s32 err = 0;

    if (unpackedArgs->connectionState != 0) {
        return SCE_ERROR_NET_ADHOCCTL_ALREADY_CONNECTED;
    }

    if (sceWlanGetSwitchState() == 0) {
        return SCE_ERROR_NET_ADHOCCTL_WLAN_SWITCH_DISABLED;
    }

    firstSystemTime = sceKernelGetSystemTimeWide();

    g_Unk7 = -1;
    unk19 = gameModeData->unk19;
    while (1) {
        if (unk19 == 0) {
            unk19MinusTimeDelta = 0;
        } else {
            secondSystemTime = sceKernelGetSystemTimeWide();
            timeDelta = secondSystemTime - firstSystemTime;

            // t4 = *v0 - *s6; // currenttime_low - oldtime_low
            // t3 = *v0 - *s6; // currenttime_low - oldtime_low

            unk19MinusTimeDelta = unk19 - timeDelta;

            // t6 = *v0 < *s6; // (currenttime_low < oldtime_low)
            // Lower currenttime_high by 1 if (currenttime_low < oldtime_low)
            // t1 = t5 - t6;  // T5 - (1 || 0)
            // t0 = t1 < 0; // false?

            // This always equates to false, don't know why it's here
            if (((u32) (timeDelta >> 32)) < 0) {
                // a0 = ((u32) timeDelta) < gameModeData->unk19; // timedelta_low < unk19?
                unk19 = unk19MinusTimeDelta;
            } else if ((timeDelta > UINT32_MAX) ||
                       (unpackedArgs->connectionState == 0)) {
                // lui fp, 0
                ret = SCE_ERROR_NET_ADHOCCTL_TIMEOUT;
                // a0 = 0;
                err = 1;
            }

            if (!err) {
                unk19 = unk19MinusTimeDelta;
                ret = sceWlanDevAttach();

                // Attaching succeeded
                if (ret == 0 || (u32) ret == SCE_ERROR_NET_WLAN_ALREADY_ATTACHED) {
                    break;
                } else if ((ret < 0) && (u32) ret != SCE_ERROR_NET_WLAN_DEVICE_NOT_READY) {
                    // Error, but not a device not ready error causes this to return sceWlanDevAttach()
                    return ret;
                }

                sceKernelDelayThread(1000000);
                uVar7 = gameModeData->unk19;
            }
        }
    }

    unpackedArgs->unk5 &= 0xfffffffd;
    ret = sceNetConfigUpInterface(g_WifiAdapter4);
    if (ret < 0) {
        ret = sceNetConfigSetIfEventFlag(g_WifiAdapter4, unpackedArgs->eventFlags, 0x8);
        if (ret < 0) {
            err = 1;
        }
    }

    if(!err) {
        if (sceWlanGetSwitchState() == 0) {
            ret = SCE_ERROR_NET_ADHOCCTL_WLAN_SWITCH_DISABLED;
        } else {
            ret = MemsetAndBuildGameModeSSID(unpackedArgs, ssid);
            if (ret >= 0) {
                gameModeData->ssid_len = ret;
                sceKernelMemcpy(gameModeData->ssid, ssid, ret & 0xff);
                ret = FUN_000050c8(gameModeData, &channel, &unk14);
                uVar7 = WaitEventDeviceUpAndConnect(unpackedArgs);
            } else {
                err = 1;
            }
        }
    }


    ret = gameModeData->unk20;
    if ((ret & 1) == 0) {
        sceKernelMemset(ssid, 0, 33);
    }


}

s32 GetChannelAndSSID(struct unk_struct *unpackedArgs, char *ssid, u32 *channel) {
    s32 adhocChannel;
    s32 ret;

    // TODO: Why is this being memsetted here again?
    sceKernelMemset(ssid, 0, (sizeof(ssid)));
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
            ret++;
        }
    }

    return ret;
}

void Disconnect(struct unk_struct *unpackedArgs) {
    s32 unk;

    sceNetAdhocAuth_lib_0x2E6AA271();
    sceWlanSetHostDiscover(0, 0);
    sceWlanSetWakeUp(0, 0);
    sceNetConfigSetIfEventFlag(g_WifiAdapter, 0, 0);
    unk = 0;
    sceNet_lib_0xDA02F383(g_WifiAdapter, &unk);
    sceNetConfigDownInterface(g_WifiAdapter);
    sceWlanDevDetach();
    unpackedArgs->connectionState = 0;
    unpackedArgs->unk3 = 0;
}

s32 ScanAndConnect(struct unk_struct *unpackedArgs) {
    u32 err = 0;
    s32 lockRes = 1;
    s32 tmp;
    s32 ret;
    s32 i;

    // Stack:
    // 0x80 (0x2)
    // 0x76 (0xb) <char[14]>
    // 0x44 (0x1)
    // 0x40 (0x0)
    // 0x3c (0x1)
    // 0x38 (0x64)
    struct ScanParams params;
    u32 unk2;
    char channel;

    i = 0;
    ret = SCE_ERROR_NET_ADHOCCTL_ALREADY_CONNECTED;
    if (unpackedArgs->connectionState == 0) {
        ret = SCE_ERROR_NET_ADHOCCTL_WLAN_SWITCH_DISABLED;
        if (sceWlanGetSwitchState() != 0) {

            unpackedArgs->connectionState = 2;

            while (1) {
                i++;
                ret = sceWlanDevAttach();

                if (ret == 0 || (ret == (s32) SCE_ERROR_NET_WLAN_ALREADY_ATTACHED)) break;

                if ((ret >> 0x1f & (ret != (s32) SCE_ERROR_NET_WLAN_DEVICE_NOT_READY))) {
                    err = 1;
                } else if (unpackedArgs->timeout <= i) {
                    ret = SCE_ERROR_NET_ADHOCCTL_TIMEOUT;
                    err = 1;
                }

                if (err) {
                    unpackedArgs->connectionState = 0;
                    break;
                }

                sceKernelDelayThread(1000000);
            }

            if (!err) {
                ret = sceNetConfigUpInterface(g_WifiAdapter);
                if (ret >= 0) {
                    ret = sceNetConfigSetIfEventFlag(g_WifiAdapter, unpackedArgs->eventFlags,
                                                     SCE_NET_ADHOCCTL_DEVICE_UP);

                    if (ret >= 0) {
                        ret = SCE_ERROR_NET_ADHOCCTL_WLAN_SWITCH_DISABLED;
                        if (sceWlanGetSwitchState() != 0) {
                            lockRes = sceKernelLockLwMutex(&g_Mutex, 1);;
                            sceKernelMemset(&params, 0, (sizeof(params)));
                            sceKernelMemset(unpackedArgs->unk7, 0, (sizeof(unpackedArgs->unk7)));

                            params.type = BSS_TYPE_INDEPENDENT;

                            ret = sceUtilityGetSystemParamInt(SYSTEMPARAM_INT_ADHOC_CHANNEL, &tmp);
                            if (ret >= 0) {
                                i = 0;

                                channel = (char) tmp;

                                if (channel != 11 && channel != 6 && channel != 1) {
                                    // Zero out channel if it's not 11, 6 or 1
                                    channel = 0;
                                }

                                // TODO: memset done before, why is this needed?
                                // Clear 14 spots
                                while (i < 14) {
                                    params.channels[i] = 0;
                                    i++;
                                }

                                // Selects which channels to scan on
                                if (channel != 0) {
                                    params.channels[0] = channel;
                                } else {
                                    params.channels[0] = 11;
                                    params.channels[1] = 6;
                                    params.channels[2] = 1;
                                }

                                unpackedArgs->unk2 = 1920;

                                params.unk4 = 1;
                                params.unk5 = 0;
                                params.min_strength = 6;
                                params.max_strength = 100;

                                unk2 = 0;

                                ret = sceNet_lib_0x7BA3ED91(g_WifiAdapter, &params, &unpackedArgs->unk2,
                                                            (struct ScanData *) unpackedArgs->unk7, &unk2);
                                tmp = WaitEventDeviceUpAndConnect(unpackedArgs);
                                if ((tmp << 4) >> 0x14 == SCE_ERROR_FACILITY_NETWORK) {
                                    ret = tmp;
                                }
                            }
                        }
                    }
                    sceNetConfigSetIfEventFlag(g_WifiAdapter, 0, 0);
                    sceNetConfigDownInterface(g_WifiAdapter);
                }
                sceWlanDevDetach();
                unpackedArgs->connectionState = 0;
            }

            unpackedArgs->unk3 = 0;
            if (!lockRes) {
                sceKernelUnlockLwMutex(&g_Mutex, 1);
            }
        }
    }
    return ret;
}

inline int WaitFlag(struct unk_struct *unpackedArgs, u32 *outBits, s32 *connectionState) {
    u32 tmp = 0;
    while (1) {
        sceKernelWaitEventFlag(unpackedArgs->eventFlags, 0xFFF, 1, outBits, 0);

        if ((*outBits & 0x200) != 0) {
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

        if ((*outBits & SCE_NET_ADHOCCTL_DEVICE_UP) == 0) break;
        sceKernelClearEventFlag(unpackedArgs->eventFlags, ~SCE_NET_ADHOCCTL_DEVICE_UP);
        *connectionState = unpackedArgs->connectionState;
        if ((*connectionState == 1 || *connectionState == 4) || (*connectionState == 5)) {
            // iVar2 = FUN_00003cd8(unpackedArgs);
            if (*connectionState >= 0) {
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
            if ((*connectionState != 3) /* || (iVar2 = FUN_00003cd8(param_2) && connectionState >= 0 )*/) {
                break;
            }
            //FUN_0000554c(unpackedArgs, g_Unk2);
        }
        RunAdhocctlHandlers(0, /*actInThread*/ 0);
        unpackedArgs->unk5 = unpackedArgs->unk5 & 0xe0000000;
    }
    return 1;
}

s32 ThreadFunc(SceSize args, void *argp) {
    // Bypass compiler warning
    (void) (args);

    s32 connectionState;
    //u32 usedInFuncBufThing;
    s32 tmp;
    u32 outBits;

    struct unk_struct *unpackedArgs = (struct unk_struct *) argp;

    while (1) {

        if (WaitFlag(unpackedArgs, &outBits, &connectionState) == 0) {
            return 0;
        }

        if ((outBits & (1 << SCE_NET_ADHOCCTL_EVENT_CONNECT)) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, ~(1 << SCE_NET_ADHOCCTL_EVENT_CONNECT));
            connectionState = InitAdhoc(unpackedArgs);
            if (connectionState >= 0) {
                RunAdhocctlHandlers(SCE_NET_ADHOCCTL_EVENT_CONNECT, 0);
            } else {
                tmp = WaitEventDeviceUpAndConnect(unpackedArgs);
                if (tmp < 0) {
                    connectionState = tmp;
                }
                RunAdhocctlHandlers(SCE_NET_ADHOCCTL_EVENT_ERROR, /*actInThread*/ 0);
                unpackedArgs->unk5 = unpackedArgs->unk5 & 0xe0000000;
                continue;
            }
        }

        if ((outBits & (1 << SCE_NET_ADHOCCTL_EVENT_DISCONNECT)) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, ~(1 << SCE_NET_ADHOCCTL_EVENT_DISCONNECT));
            Disconnect(unpackedArgs);
            RunAdhocctlHandlers(SCE_NET_ADHOCCTL_EVENT_DISCONNECT, 0);
        }

        if ((outBits & (1 << SCE_NET_ADHOCCTL_EVENT_SCAN)) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, ~(1 << SCE_NET_ADHOCCTL_EVENT_SCAN));
            connectionState = ScanAndConnect(unpackedArgs);
            if (connectionState >= 0) {
                RunAdhocctlHandlers(SCE_NET_ADHOCCTL_EVENT_SCAN, 0);
            } else {
                tmp = WaitEventDeviceUpAndConnect(unpackedArgs);
                if (tmp < 0) {
                    connectionState = tmp;
                }
                RunAdhocctlHandlers(SCE_NET_ADHOCCTL_EVENT_ERROR, /*actInThread*/ 0);
                unpackedArgs->unk5 = unpackedArgs->unk5 & 0xe0000000;
                continue;
            }
        }
        // This seems to do something with gamemode
        if ((outBits & 0x10) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, ~0x10);
            connectionState = FUN_00003f00(unpackedArgs, &g_Unk2);
            //if (connectionState < 0) goto LAB_000038a8;
            //FUN_00002600(4, 0);
        }
        if ((outBits & 0x20) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, ~0x20);
            //connectionState = FUN_000046ac(param_2, 0x7e0);
            //if (connectionState < 0) goto LAB_000038a8;
            //FUN_00002600(4, 0);
        }
        if ((outBits & 0x40) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, ~0x40);
            //FUN_0000554c(param_2, 0x7e0);
            //FUN_00002600(2, 0);
        }
        if ((outBits & 0x80) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, ~0x80);
            //connectionState = FUN_000018a8(param_2);
            //if (connectionState < 0) goto LAB_000038a8;
            //FUN_00002600(1, 0);
        }
        if ((outBits & 0x100) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, ~0x100);
            //connectionState = FUN_00001b9c(param_2);
            //if (connectionState < 0) goto LAB_000038a8;
            //FUN_00002600(1, 0);
        }
        if ((outBits & 0x400) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, ~0x400);
            //connectionState = FUN_00001ec8(param_2);
            //if (connectionState < 0) goto LAB_000038a8;
            //FUN_00002d40(5, 1);
        }
        tmp = g_Unk.connectionState;

        if ((outBits & 0x800) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, ~0x800);
            //connectionState = FUN_000020ac(param_2);
            //if (connectionState < 0) goto LAB_000038a8;
            //FUN_00002d40(5, 1);
            tmp = g_Unk.connectionState;
        }
        g_Unk.connectionState = (s32) (tmp & 0xe0000000);
    }
}

s32 CreateLwMutex() {
    s32 ret;

    ret = sceKernelCreateLwMutex(&g_Mutex, "SceNetAdhocctl", 512, 0, 0);
    if (ret >= 0) {
        g_MutexInited = 1;
        ret = 0;
    }
    return ret;
}

void DeleteLwMutex() {
    if (g_MutexInited) {
        sceKernelDeleteLwMutex(&g_Mutex);
    }
    g_MutexInited = 0;
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

// GameMode analyse beacon frame
int GameModeParseBeaconFrame(struct unk_struct2 *param_1, char *channel, char *unk) {
    s32 ret;
    s32 adhocChannel;
    char *pChannel;
    struct ScanData *pScanData;
    struct ScanParams params;
    int bufferLen;
    u32 unk2;
    u32 bssType;
    s32 channelIdx;
    u32 channels = 0;
    u32 player;
    s32 i;
    u32 tmp;
    u32 playerCount;
    unsigned char game3;
    unsigned char game4;
    struct unk_struct3 *pG_Unk6;

    sceKernelMemset(&params, 0, (sizeof(struct ScanParams)));
    params.type = BSS_TYPE_INDEPENDENT;
    sceKernelMemcpy(params.channels, &g_AllChannels, 3);
    pScanData = g_ScanBuffer;
    params.unk4 = 1;
    params.min_strength = 6;
    params.max_strength = 500;
    params.unk5 = 0;
    bufferLen = 3072;
    sceKernelMemset(g_ScanBuffer, 0, bufferLen);
    unk2 = 0;
    ret = sceNet_lib_0x7BA3ED91(g_WifiAdapter4, &params, &bufferLen, g_ScanBuffer, &unk2);
    if (ret >= 0) {
        sceKernelMemset(g_Unk6, 0, (sizeof(g_Unk6)));
        if ((bufferLen != 0) && ((int) g_ScanBuffer != 0)) {
            bssType = pScanData->bssType;
            while (1) {
                if (bssType == BSS_TYPE_INDEPENDENT) {
                    // GameMode uses 2 as the first byte in vendor info
                    if (pScanData->gameModeData[0] == 2) {
                        channelIdx = 0;

                        // This seems to be getting the channel info
                        while (channelIdx < 3) {
                            if (pScanData->channel == *pChannel) {
                                break;
                            }
                            channelIdx++;
                            pChannel++;
                        }

                        if (channelIdx == 3) {
                            // First field is probably the next pointer
                            pScanData = pScanData->next;
                        } else {
                            game3 = pScanData->gameModeData[3];
                            // In GameMode this byte seems to be 0x0f
                            if (game3 < 0xf) {
                                pScanData = pScanData->next;
                            } else {
                                playerCount = pScanData->gameModeData[2];
                                game4 = pScanData->gameModeData[4]; // [ 08 ]

                                // TODO: This is total nuts, what is this even doing
                                if (game3 + (game4 * (playerCount - 1)) < 0x100) {
                                    if (pScanData->gameModeData[2] != 0) {
                                        while (1) {
                                            //          Example:
                                            //          15 + (    8 *      2) - 15)
                                            // a4 = (game3 + (game4 * player) - 15)
                                            // t4 = 0x920 + (channelIndex * 12)
                                            // t7 = 1
                                            // sra        a0,a4,31
                                            // srl        t9,a0,29
                                            // addu       t8,a4,t9 = (game3 + (game4 * player) - 15) +
                                            //                       (game3 + (game4 * player) - 15) (u)>> 31 (s)>> 29
                                            // lw         t9,0x8(t4)
                                            // sra        s2,t8,3 = (game3 + (game4 * player) - 15) +
                                            //                      ((game3 + (game4 * player) - 15) >> 31 >> 29) / 8
                                            // sllv       a0,t7,s2 = 1 << (game3 + (game4 * player) - 15) +
                                            //                            ((game3 + (game4 * player) - 15) >> 31 >> 29) / 8
                                            // or         v0,t9,a0 = or result with g_Unk6[(3 * channelIdx)].unk3
                                            tmp = game3 + (game4 * player);
                                            g_Unk6[(3 * channelIdx)].unk3 |=
                                                    1 << (((tmp < 15) ? tmp - 8 : tmp - 15) >> 3);
                                            player++;
                                            playerCount = pScanData->gameModeData[2];
                                            if (player >= playerCount) {
                                                break;
                                            }
                                            game3 = pScanData->gameModeData[3]; // [ 0F ]
                                            game4 = pScanData->gameModeData[4];
                                        }
                                    }
                                    g_Unk6[3 * channelIdx].unk2 += playerCount;
                                }
                                pScanData = pScanData->next;
                            }
                        }
                    } else {
                        pScanData = pScanData->next;
                    }
                } else {
                    pScanData = pScanData->next;
                }
                if (pScanData == NULL) break;
                bssType = pScanData->bssType;
            }
        }
        ret = sceUtilityGetSystemParamInt(SYSTEMPARAM_INT_ADHOC_CHANNEL, &adhocChannel);
        if (ret >= 0) {
            if (adhocChannel != 11 && adhocChannel != 6 && adhocChannel != 1) {
                adhocChannel = 0;
            }
            channelIdx = param_1->amountOfPlayers;

            i = 0;
            if (channelIdx >= 0) {
                while (i < channelIdx) {
                    tmp = i & 0x1f;
                    i++;
                    channels |= (1 << tmp);
                }
            }

            while (1) {
                pG_Unk6 = &g_Unk6[0];
                channelIdx = -1;
                tmp = 0;
                i = 0;
                while (i < 3) {
                    if (pG_Unk6->unk == 1 || ((adhocChannel != 0) && (g_AllChannels[i] != adhocChannel))) {
                        i++;
                        pG_Unk6++;
                    } else if (channelIdx == -1 || pG_Unk6->unk2 < tmp) {
                        tmp = pG_Unk6->unk2; // Prefer the lowest amount of players?
                        channelIdx = (s32) i;
                        i++;
                        pG_Unk6++;
                    }
                }

                if (channelIdx == -1) {
                    return SCE_ERROR_NET_ADHOCCTL_CHANNEL_NOT_AVAILABLE;
                }

                tmp = g_Unk6[(3 * channelIdx)].unk2;
                if ((tmp >= 8) || (tmp + param_1->amountOfPlayers >= 17)) {
                    return SCE_ERROR_NET_ADHOCCTL_CHANNEL_NOT_AVAILABLE;
                }

                i = 0;
                if (param_1->amountOfPlayers <= 29) {
                    tmp = channels;
                    while (i <= (29 - param_1->amountOfPlayers)) {
                        if (((g_Unk6[(3 * channelIdx)].unk2) & tmp) == 0) {
                            break;
                        }
                        i++;
                        tmp = channels << (i & 0x1f);
                    }
                }

                if (i <= param_1->amountOfPlayers - 29) {
                    g_Unk6[3 * channelIdx].unk = 1;
                    continue;
                }
                break;
            }
            *channel = g_AllChannels[channelIdx];
            *unk = (char) ((8 * i) + 15);
        }
    }
    return ret;
}

s32 FUN_000054d8(char param_1) {
    s32 ret;
    char unk[18];
    s32 unk2;

    sceKernelMemset(unk, 0, 24);
    ret = sceNet_lib_0xB20F84F8(g_WifiAdapter4, unk);
    if (ret >= 0) {
        unk2 = (unsigned char) unk[17];
        unk[17] = param_1;
        ret = sceNet_lib_0xAFA11338(g_WifiAdapter4, unk);
        if (ret >= 0) {
            ret = unk2;
        }
    }
    return ret;
}
