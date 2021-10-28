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
    u32 channel;                  // 0xc
    s32 unk2;                     // 0x10 (buffer size? gets filled with 0x780 (1920))
    s32 timeout;                  // 0x14
    u32 unk3;                     // 0x18, could contain 0x80410b83
    s32 stackSpace;               // 0x1c
    u32 unk5;                     // 0x20, bitfield (& 0x2, 0x10 checked, 0xf, 0xc0000001; written)
    char ssidSuffix[8];           // 0x24
    char unk7[1920];              // 0x2c (buffer?)
    char unk8[28];                // 0x7ac
    struct ProductStruct product; // 0x7c8
};


// Gamemode stuff?
struct unk_struct2 {
    s32 amountOfPlayers;           // 0x00 (an amount of players) // 2
    MacAddress playerMacs[16];     // 0x04 starts with host mac
    s32 unk;                       // 0x64
    char unk2[2];                  // 0x68
    char multiCastMacAddress[6];   // 0x6a - Host mac or-ed with 3
    u8 ssid_len;                   // 0x70 - 18
    u8 channel;                    // 0x71 - 01
    char ssid[32];                 // 0x72 - PSP_AULES00469_G_K3JM27c
    s16 playerDataSize;            // 0x92 - 10 00 // Maybe this is used for key exchange?
    s32 playerData;                // 0x94 - BD 74 46 29
    s16 unk8;                      // 0x96 - 10 45
    s16 unk9;                      // 0x9a - 70 76
    s32 unk10;                     // 0x9c - B3 85 13 F2
    s32 unk11;                     // 0xa0 - D3 A1 8A D2
    s16 unk12;                     // 0xa4 - 40 00
    s16 unk13;                     // 0xa6 - 00 00
    s16 unk14;                     // 0xa8 - 10 00
    char unk15;                    // 0xaa ((8*i)+15)? - 0F
    char unk16[3];                 // 0xab - 08 00 00
    s16 unk17;                     // 0xae - 01 00
    s16 unk18;                     // 0xb0 - 00 00
    s16 beaconPeriod;              // 0xb2 - 0C 00
    u32 timeout;                   // 0xb4 - 80 C3 C9 01 <-> 01C9C380 == 30000000 interrupt timer?
    s32 unk21;                     // 0xb8 Some flag?
    s32 unk22;                     // ??
    s32 unk23;
};

struct unk_struct3 {
    s32 unk;  // 0x0
    u32 unk2; // 0x4
    s32 unk3; // 0x8
};

struct unk_struct4 {
    s32 unk1; // 0x0  - FF FF FF FF
    u8  unk2; // 0x4  - 01
    u8  unk3; // 0x5  - 01
    u32 unk4; // 0x6  - 00 00 00 2F
    u8  unk5; // 0xa  - 01
    u16 playerDataSize; // 0xb  - 00 10 - Player data size
    char playerDataBuffer[1015]; // Data gap
    // Player data - dynamic size - 7E A5 76 79 - part is in unk_struct2->unk7 it seems
    // 02
    // Amount of players * 6 (MacField Size) - 00 0C
    // Mac addresses
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
