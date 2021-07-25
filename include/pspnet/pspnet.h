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
u32 sceNetStrlen(const char* str);

/**
 * Brings an interface up.
 *
 * @param name Name of the interface.
 *
 * @return 0 if successful.
 */
s32 sceNetConfigUpInterface(const char* name);

/**
 * Sets the event flag bitmask of a device.
 *
 * @param name Name of the interface.
 * @param eventFlags UID of the eventflag.
 * @param bitMask contains options for the eventflag as a bitmask.
 *
 * @return 0 if successful.
 */
s32 sceNetConfigSetIfEventFlag(const char* name, SceUID eventFlags, u32 bitMask);

#endif /* PSPNET_H */
