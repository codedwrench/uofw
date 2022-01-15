/* Copyright (C) 2021 The uOFW team
See the file COPYING for copying permission.
*/

#include <common/types.h>

#include <adhoc/pspnet_adhoc.h>
#include <adhoc/pspnet_adhoc_auth.h>
#include <adhoc/pspnet_adhocctl.h>
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

// 0x6418 - Debug ADHOCTYPE warning
const char g_AdhocRegString[] = "/CONFIG/NETWORK/ADHOC"; // 0x650C
const char g_SSIDPrefixRegKey[] = "ssid_prefix";         // 0x6524
const char g_WifiAdapter[] = "wlan";                     // 0x6534
const char g_SSIDSeparator = '_';                        // 0x653C
const char g_ThreadName[] = "SceNetAdhocctl";            // 0x6540
const char g_DefSSIDPrefix[] = "PSP";                    // 0x6550
const char g_WifiAdapter2[] = "wlan";                    // 0x6554
const char g_WifiAdapter3[] = "wlan";                    // 0x655C
const char g_WifiAdapter4[] = "wlan";                    // 0x6564
const char g_MutexName[] = "SceNetAdhocctl";             // 0x656C
const char g_Unk9;                                       // 0x657B
const char g_AllChannels[3] = {11, 6, 1};    // 0x657C

int g_Init = 0; // 0x66D0
char g_SSIDPrefix[4]; // 0x66D4

struct unk_struct g_Unk; // 0x66D8

struct unk_struct2 g_Unk2; // 0x6EB0

// Vars at: [((fptr) 0x4), (0xC)] * (i * 0x10)
struct AdhocHandler *g_Unk4[4]; // 0x6F70

struct AdhocHandler *g_Unk5[4]; // 0x6FB0

// Seems to store data per channel
struct unk_struct3 g_Unk6[3]; // 0x6FF0

struct GameModeArea g_GameModeArea; // 0x7014

// 0x00 = next?
// 0x0a = channel
struct ScanData g_ScanBuffer[32]; // 0x7418 = 3072 bytes

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

s32 ScanAndConnect(struct unk_struct *unpackedArgs);

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
        ret = sceKernelWaitEventFlag(unpackedArgs->eventFlags, SCE_NET_ADHOCCTL_EVENT_DEVICE_UP, 1, &outBits, 0);
        if (ret < 0) {
            return ret;
        }

        sceKernelClearEventFlag(unpackedArgs->eventFlags, ~SCE_NET_ADHOCCTL_EVENT_DEVICE_UP);

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

int HandleEvent(struct unk_struct *unpackedArgs, s32 *eventAddr, s32 *unk) {
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
                sceNetAdhocAuth_lib_2E6AA271();
                // TODO: What does this mean?
                unpackedArgs->unk3 = 0x80410B84;
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

int WaitAndHandleEvent(struct unk_struct *unpackedArgs) {
    s32 ret;
    u32 outBits;
    s32 eventAddr;
    s32 unk;

    ret = sceKernelPollEventFlag(unpackedArgs->eventFlags, SCE_NET_ADHOCCTL_EVENT_DEVICE_UP, SCE_KERNEL_EW_OR,
                                 &outBits);

    if (ret != (s32) SCE_ERROR_KERNEL_EVENT_FLAG_POLL_FAILED) {
        if (ret >= 0) {
            if ((outBits & SCE_NET_ADHOCCTL_EVENT_DEVICE_UP) != 0) {
                sceKernelClearEventFlag(unpackedArgs->eventFlags, ~SCE_NET_ADHOCCTL_EVENT_DEVICE_UP);
                ret = HandleEvent(unpackedArgs, &eventAddr, &unk);
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
        (sceNetConfigSetIfEventFlag(g_WifiAdapter, unpackedArgs->eventFlags, SCE_NET_ADHOCCTL_EVENT_DEVICE_UP) >=
         0)) /* Event Flag 8, adapter inited? */ {

        sceKernelMemset(ssid, 0, (sizeof(ssid)));
        ret = GetChannelAndSSID(unpackedArgs, ssid, &channel);
        if (ret >= 0) {
            if (sceWlanGetSwitchState() == 0) {
                ret = SCE_ERROR_NET_ADHOCCTL_WLAN_SWITCH_DISABLED;
                err = 1;
            } else {
                // This function needs to be disassembled
                ret = sceNet_lib_D5B64E37(g_SSIDPrefix, ssid, ret, channel);

                if ((((WaitAndHandleEvent(unpackedArgs) << 4) >> 0x14) !=
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
                    // As clock	- The number of micro seconds until the alarm occurs.
                    clocks[0] = 1000000;
                    clocks[1] = 500000;
                    clocks[2] = 5;
                    clocks[3] = 30000000;
                    clocks[4] = 300000000;

                    // Disassembler points to connectionState as being called from the global, so give as global for now.
                    ret = sceNetAdhocAuthCreateStartThread(g_SSIDPrefix, 0x30, 0x2000, clocks,
                                                           0x10, nickname);
                    if (ret < 0) {
                        err = 1;
                    } else {
                        unpackedArgs->connectionState = 1;
                        unpackedArgs->channel = channel;
                    }
                }
            }

            if (err) {
                sceNetAdhocAuth_lib_2E6AA271();
                sceNetConfigSetIfEventFlag(g_SSIDPrefix, 0, 0);
                unk2 = 0;
                sceNet_lib_DA02F383(g_SSIDPrefix, &unk2);
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


s32 GameModeWaitForPlayersReJoin(struct unk_struct2 *gameModeData, int not_used, SceUInt64 oldtimeStamp,
                                 s32 sockId, const char *data, u32 dataSize) {
    MacAddress macList[15];
    s32 ret;
    s32 ptpAcceptSockId;
    s32 counter = 0;
    SceUInt64 newTimeStamp;
    MacAddress incomingMac;
    MacAddress *macToTestAgainst;
    u16 incomingPort;
    u32 timeLeftUs;
    s32 i;
    s32 tmp;

    // Fix compiler being a dick
    not_used = 0;
    not_used++;
    not_used = 0;

    ret = 0;
    sceKernelMemset(macList, 0, (sizeof(MacAddress) * 15));

    if (gameModeData->amountOfPlayers < 2) {
        if ((gameModeData->unk23 < counter) &&
            (ret == (s32) SCE_ERROR_NET_ADHOC_TIMEOUT)) {
            ret = 0;
        }
        return ret;
    }

    while (1) {
        if (gameModeData->timeout == 0) {
            timeLeftUs = 0;
        } else {
            newTimeStamp = sceKernelGetSystemTimeWide();

            if (((newTimeStamp < oldtimeStamp) ||
                 (newTimeStamp - oldtimeStamp > UINT32_MAX) ||
                 (newTimeStamp - oldtimeStamp >= gameModeData->timeout))) {
                return SCE_ERROR_NET_ADHOCCTL_TIMEOUT;
            }

            timeLeftUs = gameModeData->timeout - (newTimeStamp - oldtimeStamp);
        }

        // TODO: Check what gets returned here.
        ptpAcceptSockId = sceNetAdhocPtpAccept(sockId, (unsigned char *) incomingMac, &incomingPort, timeLeftUs, 0);
        if (ptpAcceptSockId < 0) {
            ret = ptpAcceptSockId;
            if ((gameModeData->unk23 < counter) && ret == (s32) SCE_ERROR_NET_ADHOC_TIMEOUT) {
                ret = 0;
            }
            return ret;
        }

        if (incomingPort == 32767) {
            i = 1; // 1 because we skip our own mac
            if (1 < gameModeData->amountOfPlayers) {
                macToTestAgainst = &gameModeData->playerMacs[0];
                while (i < gameModeData->amountOfPlayers) {
                    macToTestAgainst += sizeof(MacAddress);

                    // If we found the mac address in the expected list...
                    if (sceNetMemcmp((const char *) macToTestAgainst, incomingMac, 6) == 0) {
                        break;
                    }
                    i++;
                }
            }

            // Failurestate
            if (gameModeData->amountOfPlayers == i) {
                sceNetAdhocPtpClose(ptpAcceptSockId, 0);

            }

            // Do this only after the first player was received.
            // Seems that it checks any incoming macs to the list it already had to avoid duplicate macs.
            // If the Mac is already there close the new connection
            if (counter > 0) {
                i = counter;
                macToTestAgainst = macList;
                while (i != 0) {
                    tmp = sceNetMemcmp((const char *) macToTestAgainst, (const char *) incomingMac, 6);
                    i--;
                    macToTestAgainst += sizeof(MacAddress);
                    if (tmp == 0) {
                        ret = -1;
                        sceNetAdhocPtpClose(ptpAcceptSockId, 0);
                    }
                }
            }

            if (gameModeData->timeout != 0) {
                newTimeStamp = sceKernelGetSystemTimeWide();

                if (((newTimeStamp < oldtimeStamp) ||
                     (newTimeStamp - oldtimeStamp > UINT32_MAX) ||
                     (newTimeStamp - oldtimeStamp >= gameModeData->timeout))) {
                    if (ptpAcceptSockId >= 0) {
                        sceNetAdhocPtpClose(ptpAcceptSockId, 0);
                    }
                    return SCE_ERROR_NET_ADHOCCTL_TIMEOUT;
                } else {
                    timeLeftUs = gameModeData->timeout - (newTimeStamp - oldtimeStamp);
                }
            } else {
                timeLeftUs = 0;
            }

            ret = sceNetAdhocPtpSend(ptpAcceptSockId, data, &dataSize, timeLeftUs, 0);
            if (ret >= 0) {
                if (gameModeData->timeout != 0) {
                    newTimeStamp = sceKernelGetSystemTimeWide();

                    if (((newTimeStamp < oldtimeStamp) ||
                         (newTimeStamp - oldtimeStamp > UINT32_MAX) ||
                         (newTimeStamp - oldtimeStamp >= gameModeData->timeout))) {
                        if (ptpAcceptSockId >= 0) {
                            sceNetAdhocPtpClose(ptpAcceptSockId, 0);
                        }
                        return SCE_ERROR_NET_ADHOCCTL_TIMEOUT;
                    } else {
                        timeLeftUs = gameModeData->timeout - (newTimeStamp - oldtimeStamp);
                    }
                } else {
                    timeLeftUs = 0;
                }
            } else {
                if (ptpAcceptSockId >= 0) {
                    sceNetAdhocPtpClose(ptpAcceptSockId, 0);
                }

                if ((gameModeData->unk23 < counter) && ret == (s32) SCE_ERROR_NET_ADHOC_TIMEOUT) {
                    ret = 0;
                }

                return ret;
            }

            if (timeLeftUs >= 3000000) {
                timeLeftUs = 3000000;
            }

            ret = sceNetAdhocPtpFlush(ptpAcceptSockId, timeLeftUs, 0);
            if ((ret < 0) && ((timeLeftUs != 3000000) ||
                              (ret != (s32) SCE_ERROR_NET_ADHOC_TIMEOUT &&
                               ret != (s32) SCE_ERROR_NET_ADHOC_DISCONNECTED))) {
                if (ptpAcceptSockId >= 0) {
                    sceNetAdhocPtpClose(ptpAcceptSockId, 0);
                }

                if ((gameModeData->unk23 < counter) && ret == (s32) SCE_ERROR_NET_ADHOC_TIMEOUT) {
                    ret = 0;
                }
                return ret;
            }

            ret = sceNetAdhocPtpClose(ptpAcceptSockId, 0);
            if (ret < 0) {

                // TODO: just inline this
                if ((gameModeData->unk23 < counter) && ret == (s32) SCE_ERROR_NET_ADHOC_TIMEOUT) {
                    ret = 0;
                }
                return ret;
            }

            sceKernelMemcpy(macList + (sizeof(MacAddress) * counter), macList, 6);
            counter++;
        } else {
            sceNetAdhocPtpClose(ptpAcceptSockId, 0);
        }
        ptpAcceptSockId = -1;

        // If the counter got higher than the amount of players, stop
        if (gameModeData->amountOfPlayers < counter) {
            if (ptpAcceptSockId >= 0) {
                sceNetAdhocPtpClose(ptpAcceptSockId, 0);
            }

            if ((gameModeData->unk23 < counter) && ret == (s32) SCE_ERROR_NET_ADHOC_TIMEOUT) {
                ret = 0;
            }

            return ret;
        }
    }
}

// CreateEnterGameMode?
s32 CreateEnterGamemode(struct unk_struct *unpackedArgs, struct unk_struct2 *gameModeData) {
    s32 ret;
    char channel;
    char *unk;
    char unk2;
    struct wlanDrv_Params unk3;
    SceUInt64 firstSystemTime;
    SceUInt64 secondSystemTime;
    char ssid[33];
    char nickname[128];
    s32 clocks[5];
    struct CreateJoinData createData;
    s32 tmp;
    s32 tmp2;
    s32 err = 0;
    u32 totalSize;
    u32 bufferSize;
    s32 i;

    if (unpackedArgs->connectionState != 0) {
        return SCE_ERROR_NET_ADHOCCTL_ALREADY_CONNECTED;
    }

    if (sceWlanGetSwitchState() == 0) {
        return SCE_ERROR_NET_ADHOCCTL_WLAN_SWITCH_DISABLED;
    }

    firstSystemTime = sceKernelGetSystemTimeWide();

    g_GameModeArea.unk1 = -1;
    while (1) {
        if (gameModeData->timeout == 0) {
            // Don't do anything
        } else {
            secondSystemTime = sceKernelGetSystemTimeWide();

            if (((secondSystemTime < firstSystemTime) ||
                 ((secondSystemTime - firstSystemTime) > UINT32_MAX) ||
                 ((secondSystemTime - firstSystemTime) >= gameModeData->timeout))) {

                ret = SCE_ERROR_NET_ADHOCCTL_TIMEOUT;
                err = 1;
                break;
            }
        }

        if (!err) {
            ret = sceWlanDevAttach();

            // Attaching succeeded
            if (ret == 0 || (u32) ret == SCE_ERROR_NET_WLAN_ALREADY_ATTACHED) {
                break;
            } else if ((ret < 0) && (u32) ret != SCE_ERROR_NET_WLAN_DEVICE_NOT_READY) {
                // Error, but not a device not ready error causes this to return sceWlanDevAttach()
                return ret;
            }

            sceKernelDelayThread(1000000);
            tmp = (s32) gameModeData->timeout;
        }
    }

    if (!err) {
        unpackedArgs->unk5 &= ~(0x2);
        ret = sceNetConfigUpInterface(g_WifiAdapter4);
        if (ret < 0) {
            ret = sceNetConfigSetIfEventFlag(g_WifiAdapter4, unpackedArgs->eventFlags, 0x8);
            if (ret < 0) {
                err = 1;
            }
        }
    }

    if (!err) {
        if (sceWlanGetSwitchState() == 0) {
            ret = SCE_ERROR_NET_ADHOCCTL_WLAN_SWITCH_DISABLED;
            err = 1;
        } else {
            ret = gameModeData->unk21;
            if ((ret & 1) == 0) {
                sceKernelMemset(ssid, 0, 33);
                ret = MemsetAndBuildGameModeSSID(unpackedArgs, ssid);
                if (ret >= 0) {
                    gameModeData->ssid_len = ret;
                    sceKernelMemcpy(gameModeData->ssid, ssid, ret & 0xff);
                    ret = GameModeParseBeaconFrame(gameModeData, &channel, &unk2);
                    tmp = WaitAndHandleEvent(unpackedArgs);
                    // Check if we got a network error
                    if ((tmp << 4) >> 0x14 != SCE_ERROR_FACILITY_NETWORK) {
                        if (ret >= 0) {
                            ret = gameModeData->unk21;
                            gameModeData->unk15 = unk2;
                            gameModeData->channel = channel;
                        } else {
                            err = 1;
                        }
                    } else {
                        ret = tmp;
                        err = 1;
                    }
                } else {
                    err = 1;
                }
            }
            // If previous function was successful, or we got here from the get go
            // Breaks here on hosting gamemode
            if (!err) {
                if ((ret & 2) != 0) {
                    ret = FUN_000054d8(4);
                    if (ret >= 0) {
                        g_GameModeArea.unk1 = ret;
                        ret = sceWlanDrv_lib_5BAA1FE5(1);
                        if (ret < 0) {
                            err = 1;
                        }
                    } else {
                        err = 1;
                    }

                    if (!err) {
                        sceKernelMemset(&createData, 0, (sizeof(struct CreateJoinData)));
                        createData.ssid_len = gameModeData->ssid_len;
                        sceKernelMemcpy(createData.ssid, gameModeData->ssid, createData.ssid_len);
                        createData.channel = gameModeData->channel;
                        createData.beaconPeriod = gameModeData->beaconPeriod;
                        createData.bssType = BSS_TYPE_INDEPENDENT;
                        createData.capabilities = CAPABILITY_SHORT_PREAMBLE +
                                                  CAPABILITY_IBSS;
                        unk = 0;

                        // Seems to scan for a network specifically, or set parameters to create a network?
                        ret = sceNet_lib_03164B12(g_WifiAdapter4, &createData, unk);

                        tmp = WaitAndHandleEvent(unpackedArgs);
                        if ((tmp << 4) >> 0x14 != SCE_ERROR_FACILITY_NETWORK) {
                            if (ret >= 0) {
                                ret = ScanAndConnect(unpackedArgs);
                                // Why twice
                                if (ret >= 0) {
                                    WaitAndHandleEvent(unpackedArgs);
                                    if (ret < 0) {
                                        err = 1;
                                    }
                                } else {
                                    err = 1;
                                }
                            } else {
                                err = 1;
                            }
                        }

                        if (!err) {
                            ret = sceUtilityGetSystemParamString(SYSTEMPARAM_STRING_NICKNAME, nickname,
                                                                 sizeof(nickname));
                            if (ret >= 0) {
                                ret = WaitAndHandleEvent(unpackedArgs);
                                if (ret < 0) {
                                    err = 1;
                                }
                            } else {
                                err = 1;
                            }

                            if (!err) {
                                // Used as first arg in sceKernelSetAlarm in several places
                                // As clock	- The number of micro seconds until the alarm occurs.
                                clocks[0] = 1000000;
                                clocks[1] = 500000;
                                clocks[2] = 5;
                                clocks[3] = 30000000;
                                clocks[4] = 300000000;

                                ret = sceNetAdhocAuthCreateStartThread(g_WifiAdapter4, 0x30, 0x2000,
                                                                       clocks, 0x10, nickname);
                                if (ret >= 0) {
                                    bufferSize = gameModeData->playerDataSize + gameModeData->amountOfPlayers *
                                                                                sizeof(MacAddress);

                                    // The data added under here amounts to 25 bytes
                                    bufferSize += 25;

                                    g_GameModeArea.unk2 = 1;

                                    // For some reason the size that gets put in g_Unk7.unk4 is 6 bytes lower
                                    // 2 PTPRecvs get done in the joining part, 1 is 6 bytes big, maybe this is why
                                    totalSize = bufferSize + 19;

                                    g_GameModeArea.unk3 = 1;

                                    // g_Unk7 has everything in big endian
                                    // Swap endianness on total size
                                    g_GameModeArea.gameModeDataSize = pspWsbw(totalSize);

                                    // These are probably tags, TAG 1
                                    g_GameModeArea.gameModeDataTag1 = 1;

                                    g_GameModeArea.playerDataSize = pspWsbh(gameModeData->playerDataSize);

                                    if (gameModeData->playerDataSize != 0) {
                                        sceKernelMemcpy(g_GameModeArea.playerDataBuffer, &gameModeData->playerData,
                                                        gameModeData->playerDataSize);
                                    }

                                    // Keep array location in tmp
                                    tmp = g_GameModeArea.playerDataSize;

                                    // TAG 2
                                    g_GameModeArea.playerDataBuffer[tmp] = 2;
                                    tmp++;

                                    // Swap endianness here as well; this size is 2 bytes in size
                                    *((u16 *) &g_GameModeArea.playerDataBuffer[tmp]) =
                                            pspWsbh(gameModeData->amountOfPlayers * (sizeof(MacAddress)));

                                    tmp += 2;

                                    if (gameModeData->amountOfPlayers > 0) {
                                        // Copy player mac addresses
                                        for (i = 0; i < gameModeData->amountOfPlayers; i++) {
                                            // Copy Mac Address to 3 bytes beyond the start + 6 * Amount of Addresses
                                            // already assigned
                                            tmp += (i * (s32) (sizeof(MacAddress)));
                                            sceKernelMemcpy(&g_GameModeArea.playerDataBuffer[tmp],
                                                            gameModeData->playerMacs[i], (sizeof(MacAddress)));
                                        }
                                    }

                                    // TAG 3
                                    g_GameModeArea.playerDataBuffer[tmp] = 3;
                                    tmp++;

                                    // Endian-swapped size 2
                                    *((u16 *) &g_GameModeArea.playerDataBuffer[tmp]) = 0x0200;
                                    tmp += 2;

                                    // contained 40 00 in test
                                    *((u16 *) &g_GameModeArea.playerDataBuffer[tmp]) = pspWsbh(gameModeData->unk12);
                                    tmp += 2;

                                    // TAG 4
                                    g_GameModeArea.playerDataBuffer[tmp] = 4;
                                    tmp++;

                                    // Endian-swapped size 1
                                    *((u16 *) &g_GameModeArea.playerDataBuffer[tmp]) = 0x0100;
                                    tmp += 2;

                                    // contained 0F in test, ((8*i)+15)
                                    // i = 0
                                    *((u16 *) &g_GameModeArea.playerDataBuffer[tmp]) = pspWsbh(gameModeData->unk15);
                                    tmp += 2;

                                    // TAG 5
                                    g_GameModeArea.playerDataBuffer[tmp] = 5;
                                    tmp++;

                                    // Endian-swapped size 1
                                    *((u16 *) &g_GameModeArea.playerDataBuffer[tmp]) = 0x0100;
                                    tmp += 2;

                                    g_GameModeArea.playerDataBuffer[tmp] = gameModeData->unk16[0];
                                    tmp++;

                                    tmp = sceNetAdhocPtpListen
                                            ((unsigned char *) &gameModeData->playerMacs, 32769, 0x2000, 200000,
                                             (int) (gameModeData->timeout / 200000) + 1,
                                             gameModeData->amountOfPlayers - 1, 0);
                                    if (tmp >= 0) {
                                        // Seems to do something related to accepting PTP connections
                                        // No idea why it uses the first system time and not the second one
                                        ret = GameModeWaitForPlayersReJoin(gameModeData, 0, firstSystemTime, tmp,
                                                                           (char *) &g_GameModeArea.unk2,
                                                                           bufferSize);

                                        if (ret >= 0) {
                                            ret = sceNetAdhocPtpClose(tmp, 0);
                                            if (ret >= 0) {
                                                sceNetAdhocAuth_lib_6CE209A3();
                                                // TODO run this through the debugger
                                                ret = sceWlanDrv_lib_56F467CA(&unk3);
                                                if (ret >= 0) {
                                                    unk3.unk = gameModeData->beaconPeriod - 2;
                                                    unk3.unk2 = 0xff;
                                                    if (gameModeData->beaconPeriod == 0) {
                                                        pspBreak(SCE_BREAKCODE_DIVZERO);
                                                    }
                                                    unk3.unk3 = (1000 / gameModeData->beaconPeriod) + 1;

                                                    ret = sceWlanDrv_lib_2D0FAE4E(&unk3);
                                                    tmp = WaitAndHandleEvent(unpackedArgs);
                                                    if ((ret >= 0) && (tmp >= 0)) {
                                                        // Host mac?
                                                        sceKernelMemcpy(&gameModeData->multiCastMacAddress,
                                                                        gameModeData->playerMacs, 6);

                                                        // Multicast and Locally administered Mac address
                                                        gameModeData->multiCastMacAddress[0] |= 0b11;
                                                        gameModeData->unk17 = 1;

                                                        // Current player is player 0
                                                        *((char *) &gameModeData->currentMac + 1) = 0;

                                                        // Swap endianness on this one as well
                                                        *((char *) &gameModeData->unk14) =
                                                                (char) gameModeData->amountOfPlayers;

                                                        sceNetAdhocAuth_lib_72AAC6D3(gameModeData->playerDataSize,
                                                                                     gameModeData->playerData);

                                                        tmp = gameModeData->amountOfPlayers;
                                                        gameModeData->amountOfPlayers = 0;

                                                        tmp2 = 0;
                                                        ret = sceNet_lib_0D633F53(g_WifiAdapter4, &gameModeData->unk,
                                                                                  &gameModeData->playerMacs[0], &tmp2);

                                                        gameModeData->amountOfPlayers = tmp;

                                                        tmp = WaitAndHandleEvent(unpackedArgs);
                                                        if ((tmp << 4) >> 0x14 != SCE_ERROR_FACILITY_NETWORK) {
                                                            if (ret >= 0) {
                                                                unpackedArgs->connectionState = 3; // Connected?
                                                                unpackedArgs->channel = gameModeData->channel;
                                                            } else {
                                                                err = 1;
                                                            }
                                                        } else {
                                                            ret = tmp;
                                                            err = 1;
                                                        }
                                                    } else {
                                                        if (ret >= 0) {
                                                            ret = tmp;
                                                        }
                                                        err = 1;
                                                    }
                                                } else {
                                                    err = 1;
                                                }
                                            } else {
                                                err = 1;
                                            }
                                        } else {
                                            sceNetAdhocPtpClose(tmp, 0);
                                        }
                                    } else {
                                        ret = tmp;
                                        err = 1;
                                    }
                                } else {
                                    err = 1;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (err) {
        sceNetAdhocAuth_lib_72AAC6D3(0, 0);
        sceNetAdhocAuth_lib_2E6AA271();
        sceNetConfigSetIfEventFlag(g_WifiAdapter4, 0, 0);
        tmp = 0;
        sceNet_lib_DA02F383(g_WifiAdapter4, &tmp);
        if ((gameModeData->unk21 & 2) != 0) {
            if (sceWlanDrv_lib_5BAA1FE5(0)) {
                if (g_GameModeArea.unk1 >= 0) {
                    FUN_000054d8((char) g_GameModeArea.unk1);
                }
            }
        }
        sceNetConfigDownInterface(g_WifiAdapter4);
        sceWlanDevDetach();
    }

    return ret;
}

// Seems to loop until specified amount of bytes is received or timed out
int GameModeReceiveSpecifiedAmountOfData(struct unk_struct2 *gameModeData, s32 unk, SceUInt64 oldTimeStamp, s32 sockId,
                                         void *buffer, s32 amountOfBytes) {
    int ret;
    SceUInt64 newTimeStamp;
    int timeLeft;
    int amountLeft;

    ret = 0;

    if (amountOfBytes >= 0) {
        do {
            if (gameModeData->timeout == 0) {
                timeLeft = 0;
            } else {
                newTimeStamp = sceKernelGetSystemTimeWide();
                timeLeft = gameModeData->timeout - (newTimeStamp - oldTimeStamp);
                if (unk == 0) {
                    if (((newTimeStamp < oldTimeStamp) ||
                         ((newTimeStamp - oldTimeStamp) > UINT32_MAX) ||
                         ((newTimeStamp - oldTimeStamp) >= gameModeData->timeout))) {
                        return SCE_ERROR_NET_ADHOCCTL_TIMEOUT;
                    }
                }
            }

            amountLeft = amountOfBytes;

            ret = sceNetAdhocPtpRecv(sockId, buffer, &amountLeft, timeLeft, 0);
            if (ret < 0) {
                return ret;
            }

            amountOfBytes -= amountLeft;
        } while (0 < amountOfBytes);
    }

    return ret;
}

// This runs through the tags given in CreateEnterGameMode, gives the data for a given tag
static u8* GetGameModeDataSizeForTag(char tag, u8* dataPtr, int gameModeDataSize, u32* destination) {
    u8 charValue;
    u8 *workPtr;
    u8 *endDataPtr;
    u16 shortValue;

    // Apparently we go to the end of the data?
    endDataPtr = dataPtr + gameModeDataSize;

    // Seems to do a pointer compare, to check if we are at the end of the data
    if (dataPtr < endDataPtr) {
        workPtr = dataPtr + 1;

        while (dataPtr < endDataPtr) {
            charValue = *dataPtr;
            dataPtr = workPtr + 2;

            // If we have less than 2 bytes to go, we apparently return
            if ((int)endDataPtr - (int)workPtr < 2) {
                return (u8 *)0x0;
            }

            // Apparently we now copy 2 bytes into shortValue
            sceKernelMemcpy(&shortValue, workPtr, 2);

            // Swap endianness
            shortValue = pspWsbh(shortValue);

            // Data left always needs to be bigger than the given value on that location
            if ((int)endDataPtr - (int)dataPtr < (int)shortValue) {
                return (u8 *)0x0;
            }

            // We stop when the value given matches our first parameter
            // I guess this is some kind of Header - Data construction, where the header contains the amount of bytes we
            // have
            if (charValue == tag) {
                *destination = shortValue;
                return dataPtr;
            }

            // We skip the amount of bytes that is given in the thing?
            dataPtr = dataPtr + shortValue;

            workPtr = dataPtr + 1;
        }
    }
    return (u8 *)0x0;
}

static s32 HandleJoinGameModeError(s32 ret, s32 sockId, struct unk_struct2 *gameModeData) {
    s32 tmp = 0;

    if (sockId >= 0) {
        sceNetAdhocPtpClose(sockId, 0);
    }

    sceNetAdhocAuth_lib_72AAC6D3(0, 0);
    sceNetAdhocAuth_lib_2E6AA271();
    sceNetConfigSetIfEventFlag(g_WifiAdapter4, 0, 0);
    sceNet_lib_DA02F383(g_WifiAdapter4, &tmp);
    if ((gameModeData->unk21 & 2) != 0) {
        sceWlanDrv_lib_5BAA1FE5(0);
        if (g_GameModeArea.unk1 >= 0) {
            FUN_000054d8((char) g_GameModeArea.unk1);
        }
    }

    sceNetConfigDownInterface(g_WifiAdapter4);
    sceWlanDevDetach();

    return ret;
}

// 46ac
s32 JoinGameMode(struct unk_struct *unpackedArgs, struct unk_struct2 *gameModeData) {
    s32 ret;
    u32 tagSize;
    char unk2;
    struct wlanDrv_Params unk3;
    SceUInt64 firstSystemTime;
    SceUInt64 secondSystemTime;
    char ssid[33];
    char nickname[128];
    s32 clocks[6];
    u8 macAddress[6];
    struct CreateJoinData joinData;
    struct ScanParams scanParams;
    struct ScanData *scanPtr;
    u8* dataPtr;
    s32 sockId = -1;
    s32 tmp;
    s32 tmp2;
    s32 bufferSize;
    s32 gameModeDataSize;
    u32 timeLeftUs;

    if (unpackedArgs->connectionState != 0) {
        return SCE_ERROR_NET_ADHOCCTL_ALREADY_CONNECTED;
    }

    if (sceWlanGetSwitchState() == 0) {
        return SCE_ERROR_NET_ADHOCCTL_WLAN_SWITCH_DISABLED;
    }

    firstSystemTime = sceKernelGetSystemTimeWide();

    g_GameModeArea.unk1 = -1;
    while (1) {
        if (gameModeData->timeout != 0) {
            // Don't do anything
        } else {
            secondSystemTime = sceKernelGetSystemTimeWide();

            if (((secondSystemTime < firstSystemTime) ||
                 ((secondSystemTime - firstSystemTime) > UINT32_MAX) ||
                 ((secondSystemTime - firstSystemTime) >= gameModeData->timeout))) {

                ret = SCE_ERROR_NET_ADHOCCTL_TIMEOUT;
                return HandleJoinGameModeError(ret, sockId, gameModeData);
            }
        }

        ret = sceWlanDevAttach();

        // Attaching succeeded
        if (ret == 0 || (u32) ret == SCE_ERROR_NET_WLAN_ALREADY_ATTACHED) {
            break;
        } else if ((ret < 0) && (u32) ret != SCE_ERROR_NET_WLAN_DEVICE_NOT_READY) {
            // Error, but not a device not ready error causes this to return sceWlanDevAttach()
            return HandleJoinGameModeError(ret, sockId, gameModeData);
        }

        sceKernelDelayThread(1000000);
    }

    unpackedArgs->unk5 &= ~(0x2);
    ret = sceNetConfigUpInterface(g_WifiAdapter4);
    if (ret < 0) {
        ret = sceNetConfigSetIfEventFlag(g_WifiAdapter4, unpackedArgs->eventFlags, 0x8);
        if (ret < 0) {
            return HandleJoinGameModeError(ret, sockId, gameModeData);
        }
    }

    if (sceWlanGetSwitchState() == 0) {
        ret = SCE_ERROR_NET_ADHOCCTL_WLAN_SWITCH_DISABLED;
        return HandleJoinGameModeError(ret, sockId, gameModeData);
    } else {
        if ((gameModeData->unk21 & 1) == 0) {
            sceKernelMemset(ssid, 0, 33);
            ret = MemsetAndBuildGameModeSSID(unpackedArgs, ssid);
            if (ret < 0) {
                return HandleJoinGameModeError(ret, sockId, gameModeData);
            }

            gameModeData->ssid_len = ret;
            sceKernelMemcpy(gameModeData->ssid, ssid, ret & 0xff);
        }

        // If previous function was successful, or we got here from the get go
        // Breaks here on hosting gamemode
        if ((gameModeData->unk21 & 2) != 0) {
            ret = FUN_000054d8(4);
            if (ret >= 0) {
                g_GameModeArea.unk1 = ret;
                ret = sceWlanDrv_lib_5BAA1FE5(1);
                if (ret < 0) {
                    return HandleJoinGameModeError(ret, sockId, gameModeData);
                }
            } else {
                return HandleJoinGameModeError(ret, sockId, gameModeData);
            }

            sceKernelMemset(&scanParams, 0, (sizeof(struct ScanParams)));
            scanParams.ssid_len = gameModeData->ssid_len;
            sceKernelMemcpy(scanParams.ssid, gameModeData->ssid, gameModeData->ssid_len);
            scanParams.type = BSS_TYPE_INDEPENDENT;
            sceKernelMemcpy(scanParams.channels, g_AllChannels, 3);

            scanParams.unk4 = 1;
            scanParams.min_strength = 6;
            scanParams.max_strength = 100;
            scanParams.unk5 = 0;

            while (1) {
                bufferSize = 3072;
                sceKernelMemset(g_ScanBuffer, 0, bufferSize);
                unk2 = 0;

                ret = sceNet_lib_ScanNetworks(g_WifiAdapter, &scanParams, &bufferSize,
                                              (struct ScanData *) g_ScanBuffer, (u32 *) &unk2);
                tmp = WaitAndHandleEvent(unpackedArgs);
                if ((tmp << 4) >> 0x14 == SCE_ERROR_FACILITY_NETWORK) {
                    break;
                }

                if (ret < 0) {
                    return HandleJoinGameModeError(ret, sockId, gameModeData);
                    break;
                }
                if (gameModeData->timeout == 0) {
                    // Don't do anything
                } else {
                    secondSystemTime = sceKernelGetSystemTimeWide();

                    if (((secondSystemTime < firstSystemTime) ||
                         ((secondSystemTime - firstSystemTime) > UINT32_MAX) ||
                         ((secondSystemTime - firstSystemTime) >= gameModeData->timeout))) {

                        ret = SCE_ERROR_NET_ADHOCCTL_TIMEOUT;
                        return HandleJoinGameModeError(ret, sockId, gameModeData);
                    }

                    timeLeftUs = gameModeData->timeout - (secondSystemTime - firstSystemTime);
                }

                if (bufferSize != 0 && g_ScanBuffer != NULL) {
                    // s0 = 0x2

                    scanPtr = g_ScanBuffer;

                    while (scanPtr != NULL) {
                        if (sceNetMemcmp(scanPtr->ssid, gameModeData->ssid, 32) != 0) {
                            scanPtr = scanPtr->next;
                            continue;
                        }

                        if (scanPtr->bssType == 2) {
                            scanPtr = scanPtr->next;
                            continue;
                        }

                        sceKernelMemset(&joinData, 0, sizeof(struct CreateJoinData));
                        sceKernelMemcpy(&joinData.bssid, scanPtr->bssid,
                                        sizeof(struct ScanData) - sizeof(void *));

                        joinData.unk2 = 1;
                        unk2 = 0;
                        ret = sceNet_lib_389728AB(g_WifiAdapter4, &joinData, (char*) &tagSize);
                        tmp = WaitAndHandleEvent(unpackedArgs);
                        if ((tmp << 4) >> 0x14 != SCE_ERROR_FACILITY_NETWORK) {
                            if (ret >= 0) {
                                ret = ScanAndConnect(unpackedArgs);
                                // Why twice
                                if (ret >= 0) {
                                    WaitAndHandleEvent(unpackedArgs);
                                    if (ret < 0) {
                                        return HandleJoinGameModeError(ret, sockId, gameModeData);
                                    }
                                } else {
                                    return HandleJoinGameModeError(ret, sockId, gameModeData);
                                }
                            } else {
                                return HandleJoinGameModeError(ret, sockId, gameModeData);
                            }
                        } else {
                            return HandleJoinGameModeError(ret, -1, gameModeData);
                        }

                        ret = sceUtilityGetSystemParamString(SYSTEMPARAM_STRING_NICKNAME,
                                                             nickname,
                                                             sizeof(nickname));
                        if (ret >= 0) {
                            ret = WaitAndHandleEvent(unpackedArgs);
                            if (ret < 0) {
                                return HandleJoinGameModeError(ret, sockId, gameModeData);
                            }
                        } else {
                            return HandleJoinGameModeError(ret, sockId, gameModeData);
                        }


                        // Used as first arg in sceKernelSetAlarm in several places
                        // As clock	- The number of micro seconds until the alarm occurs.
                        clocks[0] = -1;
                        clocks[1] = 500000;
                        clocks[2] = 5;
                        clocks[3] = 5;
                        clocks[4] = 30000000;
                        clocks[5] = 300000000;

                        ret = sceNetAdhocAuthCreateStartThread(g_WifiAdapter4,
                                                               0x30, 0x2000,
                                                               clocks, 0x10, nickname);

                        if (ret < 0) {
                            return HandleJoinGameModeError(ret, sockId, gameModeData);
                        }

                        ret = sceNetGetLocalEtherAddr(macAddress);
                        if (ret < 0) {
                            return HandleJoinGameModeError(ret, sockId, gameModeData);
                        }

                        // local_3c = param_2->playermacs
                        // uVar14 = param_2->field_0xb4
                        // goto LAB_00004a80;

                        sockId = sceNetAdhocPtpOpen(macAddress, 32769, (u8 *) gameModeData->playerMacs,
                                                    32769, 8192, 200000, (s32) (gameModeData->timeout / 200000) + 1, 0);

                        if (sockId < 0) {
                            return HandleJoinGameModeError(sockId, sockId, gameModeData);
                        }

                        if (gameModeData->timeout == 0) {
                            // Don't do anything
                        } else {
                            secondSystemTime = sceKernelGetSystemTimeWide();

                            if (((secondSystemTime < firstSystemTime) || ((secondSystemTime - firstSystemTime) >
                                                                          UINT32_MAX) ||
                                 ((secondSystemTime - firstSystemTime) >= gameModeData->timeout))) {
                                ret = SCE_ERROR_NET_ADHOCCTL_TIMEOUT;
                                return HandleJoinGameModeError(ret, sockId, gameModeData);
                            }

                            timeLeftUs = gameModeData->timeout - (secondSystemTime - firstSystemTime);

                            ret = sceNetAdhocPtpConnect(sockId, timeLeftUs, 0);

                            if (ret < 0) {
                                if (ret == (s32) SCE_ERROR_NET_ADHOC_REFUSED) {
                                    sceNetAdhocPtpClose(sockId, 0);
                                    sceKernelDelayThread(1000000);
                                    continue;
                                } else {
                                    if (sockId >= 0) {
                                        sceNetAdhocPtpClose(sockId, 0);
                                    }

                                    return HandleJoinGameModeError(ret, sockId, gameModeData);
                                }
                            }

                            // This seems to get some info from the host for what to grab next
                            ret = GameModeReceiveSpecifiedAmountOfData(gameModeData, 0, firstSystemTime, sockId,
                                                                       &g_GameModeArea.unk2,
                                                                       6);

                            if (ret < 0) {
                                return HandleJoinGameModeError(ret, sockId, gameModeData);
                            }

                            if ((g_GameModeArea.unk2 != 1) || (g_GameModeArea.unk3 != 1)) {
                                ret = SCE_ERROR_NET_ADHOCCTL_UNKOWN_ERR2;
                                return HandleJoinGameModeError(ret, sockId, gameModeData);
                            }

                            sceKernelMemcpy(&gameModeDataSize, &g_GameModeArea.gameModeDataSize, 4);
                            gameModeDataSize = (s32) pspWsbw(gameModeDataSize);

                            ret = SCE_ERROR_NET_ADHOCCTL_GAMEMODE_DATA_TOO_BIG;
                            if (gameModeDataSize >= 0x3fb) {
                                return HandleJoinGameModeError(ret, sockId, gameModeData);
                            } else {
                                // Would this be 0x2F for Bomberman? PSPLink says yes!
                                ret = GameModeReceiveSpecifiedAmountOfData(gameModeData, (gameModeDataSize < 0x3fb),
                                                                           firstSystemTime, sockId, &g_GameModeArea.gameModeDataTag1,
                                                                           (s32) gameModeDataSize);
                                if (ret < 0) {
                                    return HandleJoinGameModeError(ret, sockId, gameModeData);
                                }

                                ret = sceNetAdhocPtpClose(sockId, 0);
                                sockId = -1;
                                if (ret < 0) {
                                    return HandleJoinGameModeError(ret, sockId, gameModeData);
                                }

                                // Tag 1 is probably a pointer to the playerData
                                dataPtr = GetGameModeDataSizeForTag(1, &g_GameModeArea.gameModeDataTag1, gameModeDataSize, &tagSize);
                                if (dataPtr == 0) {
                                    ret = SCE_ERROR_NET_ADHOCCTL_GAMEMODE_COULD_NOT_OBTAIN_PLAYERDATA;
                                    return HandleJoinGameModeError(ret, sockId, gameModeData);
                                }

                                // It expects a short it seems
                                if (tagSize > 16) {
                                    ret = SCE_ERROR_NET_ADHOCCTL_GAMEMODE_COULD_NOT_OBTAIN_PLAYERDATA;
                                    return HandleJoinGameModeError(ret, sockId, gameModeData);
                                }

                                gameModeData->playerDataSize = (s16) tagSize;
                                if (gameModeData->playerDataSize != 0) {
                                    // Now that we have the size, go grab all the playerdata
                                    sceKernelMemcpy(&gameModeData->playerData,dataPtr,
                                                    gameModeData->playerDataSize);
                                }

                                // Tag 2 is amount of players times 6,
                                // so that would probably be the size of the mac address array
                                dataPtr = GetGameModeDataSizeForTag(2, &g_GameModeArea.gameModeDataTag1,
                                                                    gameModeDataSize, &tagSize);

                                // Not less than 2 players and not more than 16 players
                                if ((dataPtr == NULL) ||
                                    (tagSize < (2 * sizeof(MacAddress))) || (tagSize > (16 * sizeof(MacAddress))) ||
                                    ((tagSize % sizeof(MacAddress)) != 0)) {
                                    ret = SCE_ERROR_NET_ADHOCCTL_GAMEMODE_COULD_NOT_OBTAIN_PLAYERCOUNT;
                                    return HandleJoinGameModeError(ret, sockId, gameModeData);
                                }

                                gameModeData->amountOfPlayers = (u16) tagSize / 6;
                                tmp = 0;

                                // Now copy the player mac addresses over
                                if (0 < gameModeData->amountOfPlayers) {
                                    for (tmp = 0; tmp < gameModeData->amountOfPlayers; tmp++) {
                                        sceKernelMemcpy(gameModeData->playerMacs, dataPtr +
                                                                 (tmp * (sizeof(MacAddress))), sizeof(MacAddress));

                                        // I hope this actually increases the pointer
                                        dataPtr += sizeof(MacAddress);
                                    }
                                }

                                // Tag 3 is Unknown so far, but it clearly expects it to be of size short
                                // Which does match what we put into it when creating a game
                                dataPtr = GetGameModeDataSizeForTag(3, &g_GameModeArea.gameModeDataTag1,
                                                                    gameModeDataSize, &tagSize);
                                if ((dataPtr == 0) || (tagSize != 2)) {
                                    ret = SCE_ERROR_NET_ADHOCCTL_GAMEMODE_COULD_NOT_OBTAIN_TAG3;
                                    return HandleJoinGameModeError(ret, sockId, gameModeData);
                                }

                                sceKernelMemcpy(&gameModeData->unk12,dataPtr,2);
                                gameModeData->unk12 = (s16) pspWsbh((u32)gameModeData->unk12);

                                // Tag 4 is Unknown so far, expects size 1
                                dataPtr = GetGameModeDataSizeForTag(4, &g_GameModeArea.gameModeDataTag1,
                                                                    gameModeDataSize, &tagSize);
                                if ((dataPtr == (u8 *)0x0) || (tagSize != 1)) {
                                    ret = SCE_ERROR_NET_ADHOCCTL_GAMEMODE_COULD_NOT_OBTAIN_TAG4;
                                    return HandleJoinGameModeError(ret, sockId, gameModeData);
                                }
                                gameModeData->unk15 = (char) *dataPtr;

                                // Tag 5 is Unknown so far, expects size 1
                                dataPtr = GetGameModeDataSizeForTag(5, &g_GameModeArea.gameModeDataTag1,
                                                                    gameModeDataSize, &tagSize);

                                if ((dataPtr == (u8 *)0x0) || (tagSize != 1)) {
                                    ret = SCE_ERROR_NET_ADHOCCTL_GAMEMODE_COULD_NOT_OBTAIN_TAG5;
                                    return HandleJoinGameModeError(ret, sockId, gameModeData);
                                }

                                gameModeData->unk16[0] = (s8) *dataPtr;

                                sceNetAdhocAuth_lib_6CE209A3();
                                ret = sceWlanDrv_lib_56F467CA(&unk3);
                                if (ret < 0) {
                                    return HandleJoinGameModeError(ret, sockId, gameModeData);
                                }
                                unk3.unk = *(s16 *) &scanPtr->beaconPeriod + -2;
                                unk3.unk2 = 0xff;
                                if (scanPtr->beaconPeriod == 0) {
                                    pspBreak(SCE_BREAKCODE_DIVZERO);
                                }

                                unk3.unk3 = (1000 / gameModeData->beaconPeriod) + 1;

                                ret = sceWlanDrv_lib_2D0FAE4E(&unk3);
                                tmp = WaitAndHandleEvent(unpackedArgs);
                                if ((ret >= 0) && (tmp >= 0)) {
                                    // Host mac?
                                    sceKernelMemcpy(&gameModeData->multiCastMacAddress,
                                                    gameModeData->playerMacs, 6);

                                    // Multicast and Locally administered Mac address
                                    gameModeData->multiCastMacAddress[0] |= 0b11;
                                    gameModeData->beaconPeriod = (s16) scanPtr->beaconPeriod;

                                    tmp = 1;

                                    // This seems to get our own player
                                    if (gameModeData->amountOfPlayers > 1) {
                                        dataPtr = (u8*) gameModeData->playerMacs;
                                        while (tmp < gameModeData->amountOfPlayers) {
                                            dataPtr += (sizeof(MacAddress));

                                            // We stop if we found ourselves apparently
                                            if (sceNetMemcmp((const char*)dataPtr,
                                                                  (const char*)macAddress,6) == 0)
                                            {
                                                break;
                                            }
                                            tmp++;
                                        }
                                    }

                                    gameModeData->unk17 = 1;

                                    // I think this pushes it to the left of the short
                                    // currentMac points to the mac corresponding to this PSP
                                    *(char *)(gameModeData->currentMac + 1) = (s8) tmp;

                                    // Why does it need this
                                    *((char *) &gameModeData->unk14) =
                                            (char) gameModeData->amountOfPlayers;

                                    sceNetAdhocAuth_lib_72AAC6D3(gameModeData->playerDataSize,
                                                                 gameModeData->playerData);

                                    tmp = gameModeData->playerDataSize;
                                    gameModeData->playerDataSize = 0;
                                    tmp2 = 0;

                                    ret = sceNet_lib_0D633F53(g_WifiAdapter4, &gameModeData->unk,
                                                              &gameModeData->playerMacs[0], &tmp2);

                                    gameModeData->playerDataSize = (s16)tmp;

                                    tmp = WaitAndHandleEvent(unpackedArgs);
                                    if ((tmp << 4) >> 0x14 != SCE_ERROR_FACILITY_NETWORK) {
                                        if (tmp < 0) {
                                            sockId = -1;
                                            return HandleJoinGameModeError(ret, sockId, gameModeData);
                                        }

                                        if (ret < 0) {
                                            return HandleJoinGameModeError(ret, sockId, gameModeData);
                                        }

                                        unpackedArgs->connectionState = 3; // Connected?
                                        unpackedArgs->channel = gameModeData->channel;

                                        if (sockId >= 0) {
                                            sceNetAdhocPtpClose(sockId, 0);
                                        }

                                        sceNetAdhocAuth_lib_72AAC6D3(0,0);
                                        sceNetAdhocAuth_lib_2E6AA271();
                                        sceNetConfigSetIfEventFlag(g_WifiAdapter4,0,0);
                                        tmp = 0;
                                        sceNet_lib_DA02F383(g_WifiAdapter4,&tmp);
                                        if ((gameModeData->unk21 & 2) != 0) {
                                            sceWlanDrv_lib_5BAA1FE5(0);
                                            if (g_GameModeArea.unk1 >= 0) {
                                                FUN_000054d8((char) g_GameModeArea.unk1);
                                            }
                                        }

                                        sceNetConfigDownInterface(g_WifiAdapter4);
                                        sceWlanDevDetach();

                                        break;
                                    }
                                } else {
                                    if (ret >= 0) {
                                        ret = tmp;
                                    }
                                    return HandleJoinGameModeError(ret, sockId, gameModeData);
                                }
                            }
                        }
                    }
                }
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
            ret++;
        }
    }

    return ret;
}

void Disconnect(struct unk_struct *unpackedArgs) {
    s32 unk;

    sceNetAdhocAuth_lib_2E6AA271();
    sceWlanSetHostDiscover(0, 0);
    sceWlanSetWakeUp(0, 0);
    sceNetConfigSetIfEventFlag(g_WifiAdapter, 0, 0);
    unk = 0;
    sceNet_lib_DA02F383(g_WifiAdapter, &unk);
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
                                                     SCE_NET_ADHOCCTL_EVENT_DEVICE_UP);

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

                                ret = sceNet_lib_ScanNetworks(g_WifiAdapter, &params, &unpackedArgs->unk2,
                                                              (struct ScanData *) unpackedArgs->unk7, &unk2);
                                tmp = WaitAndHandleEvent(unpackedArgs);
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

        if ((*outBits & SCE_NET_ADHOCCTL_EVENT_DEVICE_UP) == 0) break;
        sceKernelClearEventFlag(unpackedArgs->eventFlags, ~SCE_NET_ADHOCCTL_EVENT_DEVICE_UP);
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

        // Clears most flags?
        unpackedArgs->unk5 = unpackedArgs->unk5 & 0xe0000000;
    }
    return 1;
}

void DisconnectGameMode(struct unk_struct *unpackedArgs, struct unk_struct2 *gameModeData)
{
    s32 unk;

    unk = 0;
    sceNet_lib_DA02F383(g_WifiAdapter4,&unk);
    sceNetAdhocAuth_lib_72AAC6D3(0,0);
    sceNetAdhocAuth_lib_2E6AA271();
    sceNetConfigSetIfEventFlag(g_WifiAdapter4,0,0);
    unk = 0;
    sceNet_lib_DA02F383(g_WifiAdapter4, &unk);
    if ((gameModeData->unk21 & 2) != 0) {
        sceWlanDrv_lib_5BAA1FE5(0);
        if (g_GameModeArea.unk1 >= 0) {
            FUN_000054d8((char) g_GameModeArea.unk1);
        }
    }
    sceNetConfigDownInterface(g_WifiAdapter4);
    sceWlanDevDetach();
    unpackedArgs->connectionState = 0;
    unpackedArgs->unk3 = 0;
}


int RunScanOnChannel1(struct unk_struct *unpackedArgs)

{
    s32 tmp;
    s32 ret;
    s32 paramSize;
    u32 unk;

    struct ScanData scanData;
    struct ScanParams params;
    paramSize = sizeof(struct ScanParams);
    
    tmp = sceWlanGetSwitchState();
    ret = SCE_ERROR_NET_ADHOCCTL_WLAN_SWITCH_DISABLED;
    if (tmp != 0) {
        sceKernelMemset(&params, 0, sizeof(struct ScanParams));
        sceKernelMemset(&scanData,0,sizeof(struct ScanData));

        // Apparently we only scan on channel 1 for whatever this function does
        params.channels[0] = 1;
        params.type = BSS_TYPE_INDEPENDENT;
        tmp = 1;
        while (tmp < 14) {
            params.channels[tmp] = 0;
            tmp++;
        }

        params.unk4 = 1;
        params.min_strength = 6;
        params.max_strength = 100;
        params.unk5 = 0;
        unk = 0;
        sceNet_lib_ScanNetworks(g_WifiAdapter, &params, &paramSize, &scanData, &unk);
        tmp = WaitAndHandleEvent(unpackedArgs);

        if(((tmp << 4) >> 0x14) != SCE_ERROR_FACILITY_NETWORK) {
            ret = 0;
        }
    }
    return ret;
}


s32 Join(struct unk_struct *unpackedArgs) {
    s32 ret;
    s32 tmp = 0;
    s32 clocks[5];
    s32 unk;
    u32 channel;
    char nickname[128];
    char ssid[33];
    struct CreateJoinData createJoinData;

    ret = 0;

    // We want to start out disconnected
    if (unpackedArgs->connectionState != 0) {
        return SCE_ERROR_NET_ADHOCCTL_ALREADY_CONNECTED;
    }

    // WLAN-Switch needs to be on
    if (sceWlanGetSwitchState() == 0) {
        return SCE_ERROR_NET_ADHOCCTL_WLAN_SWITCH_DISABLED;
    }

    while (1) {
        tmp++;
        ret = sceWlanDevAttach();
        if (ret == 0 || ret == (s32) SCE_ERROR_NET_WLAN_ALREADY_ATTACHED) {
            break;
        }

        // If any error which is not SCE_ERROR_NET_WLAN_DEVICE_NOT_READY, return errorcode
        if ((ret >> 0x1f) && (ret != (s32) SCE_ERROR_NET_WLAN_DEVICE_NOT_READY)) {
            return ret;
        }

        if (unpackedArgs->timeout <= (s32) tmp) {
            return SCE_ERROR_NET_ADHOCCTL_TIMEOUT;
        }

        sceKernelDelayThread(1000000);
    }

    // Flag 2 seems to be cleared here
    unpackedArgs->unk5 &= ~(0x2);

    ret = sceNetConfigUpInterface(g_WifiAdapter);
    if (ret >= 0) {
        ret = sceNetConfigSetIfEventFlag(g_WifiAdapter, unpackedArgs->eventFlags, 8);
        if (ret >= 0) {
            ret = RunScanOnChannel1(unpackedArgs);
            if (ret >= 0) {
                sceKernelMemset(ssid, 0, sizeof(ssid));
                tmp = GetChannelAndSSID(unpackedArgs, ssid, &channel);
                if (tmp >= 0) {
                    ret = WaitAndHandleEvent(unpackedArgs);
                    if (ret >= 0) {
                        sceKernelMemset(&createJoinData, 0, sizeof(struct CreateJoinData));
                        sceKernelMemcpy(createJoinData.ssid, ssid, sizeof(createJoinData.ssid));
                        createJoinData.ssid_len =  tmp;
                        createJoinData.bssType = 2;
                        createJoinData.channel = (char) channel;
                        createJoinData.capabilities = CAPABILITY_SHORT_PREAMBLE +
                                                      CAPABILITY_IBSS;;
                        createJoinData.beaconPeriod = 100;

                        unk = 0;
                        ret = sceNet_lib_03164B12(g_WifiAdapter, &createJoinData, (char*)&unk);
                        tmp = WaitAndHandleEvent(unpackedArgs);

                        if (((tmp << 4) >> 0x14) != SCE_ERROR_FACILITY_NETWORK) {
                            if (ret >= 0) {
                                ret = RunScanOnChannel1(unpackedArgs);
                                if (ret >= 0) {
                                    ret = WaitAndHandleEvent(unpackedArgs);

                                    if (ret >= 0) {
                                        ret = sceUtilityGetSystemParamString(SYSTEMPARAM_STRING_NICKNAME,
                                                                             nickname, sizeof(nickname));

                                        if (ret >= 0) {
                                            ret = WaitAndHandleEvent(unpackedArgs);
                                            if(ret >= 0) {
                                                clocks[0] = 1000000;
                                                clocks[1] = 500000;
                                                clocks[2] = 5;
                                                clocks[3] = 30000000;
                                                clocks[4] = 300000000;
                                                ret = sceNetAdhocAuthCreateStartThread(g_WifiAdapter, 0x30,
                                                                                       0x2000, clocks, 0x10,
                                                                                       nickname);
                                                if (ret >= 0) {
                                                    unpackedArgs->connectionState = 1;
                                                    unpackedArgs->channel = channel;

                                                    return ret;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            ret = tmp;
                        }
                    }
                } else {
                    ret = tmp;
                }
            }

            sceNetAdhocAuth_lib_2E6AA271();
            sceNetConfigSetIfEventFlag(g_WifiAdapter, 0, 0);
            unk = 0;
            sceNet_lib_DA02F383(g_WifiAdapter, &unk);
            sceNetConfigDownInterface(g_WifiAdapter);
            sceWlanDevDetach();
        }
    }
    return ret;
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

        // Init, maybe connnect?
        if ((outBits & (1 << 1)) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, ~(1 << 1));
            connectionState = InitAdhoc(unpackedArgs);
            if (connectionState >= 0) {
                RunAdhocctlHandlers(SCE_NET_ADHOCCTL_EVENT_CONNECT, 0);
            } else {
                tmp = WaitAndHandleEvent(unpackedArgs);
                if (tmp < 0) {
                    connectionState = tmp;
                }
                RunAdhocctlHandlers(SCE_NET_ADHOCCTL_EVENT_ERROR, /*actInThread*/ 0);
                unpackedArgs->unk5 &= ~(0x1);
                continue;
            }
        }

        // Disconnect
        if ((outBits & (1 << 2)) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, ~(1 << 2));
            Disconnect(unpackedArgs);
            RunAdhocctlHandlers(SCE_NET_ADHOCCTL_EVENT_DISCONNECT, 0);
        }

        // Scan
        if ((outBits & (1 << 3)) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, ~(1 << 3));
            connectionState = ScanAndConnect(unpackedArgs);
            if (connectionState >= 0) {
                RunAdhocctlHandlers(SCE_NET_ADHOCCTL_EVENT_SCAN, 0);
            } else {
                tmp = WaitAndHandleEvent(unpackedArgs);
                if (tmp < 0) {
                    connectionState = tmp;
                }
                RunAdhocctlHandlers(SCE_NET_ADHOCCTL_EVENT_ERROR, /*actInThread*/ 0);
                unpackedArgs->unk5 &= ~(0x1);
                continue;
            }
        }

        // This seems to do something with gamemode
        // GameMode Create game
        if ((outBits & (1 << 4)) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, ~(1 << 4));
            connectionState = CreateEnterGamemode(unpackedArgs, &g_Unk2);
            if (connectionState >= 0) {
                RunAdhocctlHandlers(SCE_NET_ADHOCCTL_EVENT_DEVICE_UP, 0);
            } else {
                tmp = WaitAndHandleEvent(unpackedArgs);
                if (tmp < 0) {
                    connectionState = tmp;
                }
                RunAdhocctlHandlers(SCE_NET_ADHOCCTL_EVENT_ERROR, /*actInThread*/ 0);
                unpackedArgs->unk5 &= ~(0x1);
                continue;
            }
        }

        // GameMode Join game
        if ((outBits & (1 << 5)) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, ~0x20);
            connectionState = JoinGameMode(unpackedArgs, &g_Unk2);
            if (connectionState >= 0) {
                RunAdhocctlHandlers(SCE_NET_ADHOCCTL_EVENT_DEVICE_UP, 0);
            } else {
                tmp = WaitAndHandleEvent(unpackedArgs);
                if (tmp < 0) {
                    connectionState = tmp;
                }
                RunAdhocctlHandlers(SCE_NET_ADHOCCTL_EVENT_ERROR, /*actInThread*/ 0);
                unpackedArgs->unk5 &= ~(0x1);
                continue;
            }
        }

        // GameMode Disconnect
        if ((outBits & (1 << 6)) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, ~0x40);
            DisconnectGameMode(unpackedArgs, &g_Unk2);

            RunAdhocctlHandlers(SCE_NET_ADHOCCTL_EVENT_DISCONNECT, /*actInThread*/ 0);
        }

        // Adhocdiscover?
        if ((outBits & (1 << 7)) != 0) {
            sceKernelClearEventFlag(unpackedArgs->eventFlags, ~(1 << 7));

            connectionState = Join(unpackedArgs);
            if (connectionState >= 0) {
                RunAdhocctlHandlers(SCE_NET_ADHOCCTL_EVENT_DEVICE_UP, 0);
            } else {
                tmp = WaitAndHandleEvent(unpackedArgs);
                if (tmp < 0) {
                    connectionState = tmp;
                }
                RunAdhocctlHandlers(SCE_NET_ADHOCCTL_EVENT_ERROR, /*actInThread*/ 0);
                unpackedArgs->unk5 &= ~(0x1);
                continue;
            }

            RunAdhocctlHandlers(SCE_NET_ADHOCCTL_EVENT_CONNECT, /*actInThread*/ 0);
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
    const char *pChannel;
    struct ScanData *pScanData;
    struct ScanParams params;
    int bufferLen;
    u32 unk2;
    u32 bssType;
    s32 channelIdx;
    u32 playerIdx = 0;
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
    ret = sceNet_lib_ScanNetworks(g_WifiAdapter4, &params, &bufferLen, g_ScanBuffer, &unk2);
    if (ret >= 0) {
        sceKernelMemset(g_Unk6, 0, (sizeof(g_Unk6)));
        if ((bufferLen != 0) && ((int) g_ScanBuffer != 0)) {
            bssType = pScanData->bssType;
            while (1) {
                if (bssType == BSS_TYPE_INDEPENDENT) {
                    // GameMode uses 2 as the first byte in vendor info
                    if (pScanData->gameModeData[0] == 2) {
                        channelIdx = 0;
                        pChannel = g_AllChannels;

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
                                    player = 0;
                                    if (playerCount != 0) {
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
            playerCount = param_1->amountOfPlayers;

            i = 0;
            if (playerCount > 0) {
                while ((u32) i < playerCount) {
                    tmp = i & 0x1f;
                    i++;
                    playerIdx |= (1 << tmp);
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
                    tmp = playerIdx;
                    while (i <= (29 - param_1->amountOfPlayers)) {
                        if (((g_Unk6[(3 * channelIdx)].unk2) & tmp) == 0) {
                            break;
                        }
                        i++;
                        tmp = playerIdx << (i & 0x1f);
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
    ret = sceNet_lib_B20F84F8(g_WifiAdapter4, unk);
    if (ret >= 0) {
        unk2 = (unsigned char) unk[17];
        unk[17] = param_1;
        ret = sceNet_lib_AFA11338(g_WifiAdapter4, unk);
        if (ret >= 0) {
            ret = unk2;
        }
    }
    return ret;
}
