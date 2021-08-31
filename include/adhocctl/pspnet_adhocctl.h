/* Copyright (C) 2021 The uOFW team
See the file COPYING for copying permission.
*/

#ifndef PSPNET_ADHOCCTL_H
#define PSPNET_ADHOCCTL_H

extern const char g_WifiAdapter[];
extern const char g_DefSSIDPrefix[];
extern const char g_AdhocRegString[];
extern const char g_SSIDPrefixRegKey[];

#define SCE_NET_ADHOCCTL_EVENT_ERROR         0x0
#define SCE_NET_ADHOCCTL_EVENT_CONNECT       0x1
#define SCE_NET_ADHOCCTL_EVENT_DISCONNECT    0x2
#define SCE_NET_ADHOCCTL_EVENT_SCAN          0x3
// TODO: Is this truly what this means?
#define SCE_NET_ADHOCCTL_DEVICE_UP           0x4

#define ADHOC_SUBTYPE_NORMALMODE 'L'
#define ADHOC_SUBTYPE_GAMEMODE   'G'

enum BssTypes {
    BSS_TYPE_INFRASTRUCTURE = 1,
    BSS_TYPE_INDEPENDENT    = 2
};

enum AdhocIds {
    SCE_NET_ADHOCCTL_ADHOCTYPE_NORMAL = 0,
    SCE_NET_ADHOCCTL_ADHOCTYPE_RESERVED,
    SCE_NET_ADHOCCTL_ADHOCTYPE_GAMESHARE
};

struct ProductStruct {
    enum AdhocIds adhocId;
    char product[9]; // Product Id, e.g. UCUS98612
};

typedef char MacAddress[6];

// This matches wireshark capabilities (the first 2 bytes of it)
enum CapabilityMasks {
    CAPABILITY_ESS             = 0x1,
    CAPABILITY_IBSS            = 0x2,
    CAPABILITY_CF_POLLABLE     = 0x4,
    CAPABILITY_CF_POLLREQ      = 0x8,
    CAPABILITY_PRIVACY         = 0x10,
    CAPABILITY_SHORT_PREAMBLE  = 0x20,
    CAPABILITY_PBCC            = 0x40,
    CAPABILITY_CHANNEL_AGILITY = 0x80
};

struct unk_struct {
    SceUID eventFlags;            // 0x0
    SceUID tid;                   // 0x4
    s32 connectionState;          // 0x8
    u32 channel;                  // 0xC
    s32 unk2;                     // 0x10 (buffer size? gets filled with 0x780 (1920))
    s32 timeout;                  // 0x14
    u32 unk3;                     // 0x18, could contain 0x80410b83
    s32 stackSpace;               // 0x1C
    u32 unk5;                     // 0x20, bitfield (& 0x2, 0x10 checked, 0xf, 0xc0000001; written)
    char ssidSuffix[8];           // 0x24
    char unk7[1920];              // 0x2c (buffer?)
    char unk8[28];                // 0x7ac
    struct ProductStruct product; // 0x7c8
};


// Gamemode stuff?
struct unk_struct2 {
    s32 amountOfPlayers;           // 0x00 (an amount of players) // 2
    MacAddress playerMacs[18];     // 0x04 starts with host mac
    u8 ssid_len;                   // 0x70
    u8 channel;                    // 0x71
    char ssid[32];                 // 0x72
    char unk7[8];                  // 0x92
    s16 unk8;                      // 0x9a
    s32 unk9;                      // 0x9c
    s32 unk10;                     // 0xa0
    char unk11[6];                 // 0xa4
    char unk12;                    // 0xaa ((8*i)+15)?
    char unk13[7];                 // 0xab
    s16 unk14;                     // 0xb2
    s32 unk15;                     // 0xb4
    s32 unk16;                     // 0xb8 Some flag?
};

struct unk_struct3 {
    s32 unk;  // 0x0
    u32 unk2; // 0x4
    s32 unk3; // 0x8
};

typedef void(*sceNetAdhocctlHandler)(s32 flag, s32 err, void *unk);

struct AdhocHandler {
    s32 handlerIdx;                 // 0x0
    sceNetAdhocctlHandler handler;  // 0x4
    void *handlerArg;               // 0x8
    s32 funcGlobalArea;             // 0xc $gp gets set to here?
} __attribute__((packed));

/**
 * Inits the adhocctl module.
 * @param stackSize The stacksize adhocctl can use.
 * @param priority The priority of the thread.
 * @param product Contains the productId of the game, e.g. UCUS98612
 * @return An errorcode e.g. SCE_ERROR_NET_ADHOCCTL_ALREADY_INITIALIZED
 */
s32 sceNetAdhocctlInit(s32 stackSize, s32 priority, struct ProductStruct* product);

#endif
