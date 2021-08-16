#ifndef PSPNET_H
#define PSPNET_H

/**
* uofw/include/pspnet/pspnet.h
*
* Library for PSP network functions.
*/

/**
 * Returns the length of a string.
 *
 * @param str The string to get the length of.
 *
 * @return The length of the string.
 */
u32 sceNetStrlen(const char *str);

/**
 * Brings an interface down.
 *
 * @param name Name of the interface.
 *
 * @return 0 if successful.
 */
s32 sceNetConfigDownInterface(const char* name);

/**
 * Brings an interface up.
 *
 * @param name Name of the interface.
 *
 * @return 0 if successful.
 */
s32 sceNetConfigUpInterface(const char *name);

/**
 * Gets an event to use?
 *
 * @param name Name of the interface.
 * @param eventAddr the event address gets saved here?
 * @param unk
 *
 * @return The event?
 */
s32 sceNetConfigGetIfEvent(const char *name, s32* eventAddr, s32* unk);

/**
 * Sets the event flag bitmask of a device.
 *
 * @param name Name of the interface.
 * @param eventFlags UID of the eventflag.
 * @param bitMask contains options for the eventflag as a bitmask.
 *
 * @return 0 if successful.
 */
s32 sceNetConfigSetIfEventFlag(const char *name, SceUID eventFlags, u32 bitMask);

/**
 * Seems to have something to do with wifi scanning.
 * @param name Name of the interface.
 * @param unk Unknown.
 * @param unk2 Unknown, seems to be buffer length.
 * @param unk3 Unknown, seems to be buffer.
 * @param unk4 Unknown, seems to be set to 0.
 * @return
 */
s32 sceNet_lib_0x7BA3ED91(const char *name, void *unk, s32 unk2, char *unk3, u32* unk4);

/**
 * Unknown function.
 *
 * @param name Name of the interface.
 * @param ssid SSID of the adhoc network to be formed.
 * @param ssidLen Length of the ssid.
 * @param channel Channel to be used.
 *
 * return something <0 on failure.
 */
s32 sceNet_lib_0xD5B64E37(const char *name, const char *ssid, u32 ssidLen, u32 channel);

/**
 * Unknown function.
 *
 * @param name Name of the interface.
 * @param unk
 *
 * return something <0 on failure.
 */
s32 sceNet_lib_0xDA02F383(const char* name, s32* unk);

#endif /* PSPNET_H */
