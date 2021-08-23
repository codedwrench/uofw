#ifndef PSPNET_H
#define PSPNET_H

struct ScanData {
    struct ScanData *next;            // 0x0
    char bssid[6];                    // 0x4
    char channel;                     // 0xa
    unsigned char ssidLen;            // 0xb
    char ssid[32];                    // 0xc
    unsigned int bssType;             // 0x2c
    unsigned int beaconPeriod;        // 0x30
    unsigned int dtimPeriod;          // 0x34 - probably true, but couldn't find in resulting data
    unsigned int timeStamp;           // 0x38 - probably true, but couldn't find in resulting data
    unsigned int localTime;           // 0x3c - probably true, but couldn't find in resulting data
    unsigned short atim;              // 0x40
    unsigned short capabilities;      // 0x42
    char rate[8];                     // 0x44 - Matches wireshark rates
    unsigned short rssi;              // 0x4a - Percentage 0-100%                      V- Counter
    unsigned char  gameModeData[18];  // 0x4c - First byte is 2 on gamemode data [ 02 05 02 0f 08 ]
                                      //        Third byte seems to be player count
};

struct ScanParams {
    s32 type;          // 0x0  Type, this parameter controls what type of networks are shown
    char unk2[6];      // 0x4  BSSID? Setting a BSSID did not seem to have an effect
    char channels[14]; // 0xa
    s32 ssid_len;      // 0x18 SSID length
    char ssid[32];     // 0x1c SSID Setting this makes it filter on a specific ssid
    s32 unk4;          // 0x3c Changing this from 1 to 2 had no effect
    s32 unk5;          // 0x40
    s32 min_strength;  // 0x44
    s32 max_strength;  // 0x48
};

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
 * @param scanParams Tells the library how it should scan, with what filters and such.
 * @param size Size of the scandata.
 * @param scanData Where the data from the scan is stored.
 * @param unk4 Unknown Seems to be set to 0.
 * @return 0 if successful.
 */
s32 sceNet_lib_0x7BA3ED91(const char *name, struct ScanParams* scanParams, s32* size, struct ScanData* scanData, u32* unk4);

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
