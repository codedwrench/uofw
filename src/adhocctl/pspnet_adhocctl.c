#include <common/types.h>

#include <common_imp.h>
#include <adhocctl/pspnet_adhocctl.h>
#include <pspnet/pspnet.h>
#include <registry/registry.h>
#include <usersystemlib_kernel.h>

/**
 * uofw/src/net/pspnet_adhocctl.c
 *
 * PSP Adhoc control - PSP Adhoc control networking libraries
 *
 * I honestly don't know what this does yet.
 * TODO: Figure that out
 */

const char g_DefSSIDPrefix[] = "PSP";
const char g_AdhocRegString[] = "/CONFIG/NETWORK/ADHOC";
const char g_SSIDPrefixRegKey[] = "ssid_prefix";

SCE_MODULE_INFO(
    "sceNetAdhocctl_Library",
    SCE_MODULE_ATTR_EXCLUSIVE_LOAD | SCE_MODULE_ATTR_EXCLUSIVE_START,
    1, 6);

SCE_MODULE_BOOTSTART("SceNetAdhocctlInit");
SCE_MODULE_REBOOT_BEFORE("SceNetAdhocctlBefore");
SCE_MODULE_REBOOT_PHASE("SceNetAdhocctlRebootPhase");

SCE_SDK_VERSION(SDK_VERSION);

s32 GetSSIDPrefix(char *ssidPrefix);

s32 GetSSIDPrefix(char *ssidPrefix)
{
    u32 prefixSize;
    struct RegParam regParams;
    u32 regHandle;
    u32 keyHandle;
    enum RegKeyTypes regKeyType;

    sceKernelMemset(&regParams, 0, (sizeof(struct RegParam)));
    regParams.unk3 = 1;
    regParams.regtype = 1;
    regParams.unk2 = 1;

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
    return (int) sceKernelMemcpy(ssidPrefix, g_DefSSIDPrefix, prefixSize);
}