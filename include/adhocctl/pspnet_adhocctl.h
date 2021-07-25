/* Copyright (C) 2021 The uOFW team
See the file COPYING for copying permission.
*/

#ifndef PSPNET_ADHOCCTL_H
#define PSPNET_ADHOCCTL_H

extern const char g_WifiAdapter[];
extern const char g_DefSSIDPrefix[];
extern const char g_AdhocRegString[];
extern const char g_SSIDPrefixRegKey[];

#define SCE_NET_EVENT_ADHOCCTL_CONNECT       1
#define SCE_NET_EVENT_ADHOCCTL_DISCONNECT    2

#define ADHOC_SUBTYPE_NORMALMODE 'L'
#define ADHOC_SUBTYPE_GAMEMODE   'G'

enum AdhocIds {
    SCE_NET_ADHOCCTL_ADHOCTYPE_NORMAL = 0,
    SCE_NET_ADHOCCTL_ADHOCTYPE_RESERVED,
    SCE_NET_ADHOCCTL_ADHOCTYPE_GAMESHARE
};

struct ProductStruct {
    enum AdhocIds adhocId;
    char product[9]; // Product Id, e.g. UCUS98612
};

struct unk_struct {
    SceUID eventFlags; // 0x0
    SceUID kernelFlags; // 0x4
    s32 connectionState; // 0x8
    s32 unk; // 0xC
    s32 unk2; // 0x10 (buffer size? gets filled with 0x780 (1920))
    s32 timeout; // 0x14
    s32 unk3; // 0x18, could contain 0x80410b83
    s32 unk4; // 0x1C
    u32 unk5; // 0x20, bitfield (& 0x2, 0x10 checked, 0xf, 0xc0000001; written)
    char ssidSuffix[8]; // 0x24
    char unk7[1920]; // 0x2c (buffer?)
    char unk8[28]; // 0x7ac
    struct ProductStruct product; // 0x7c8
};

struct unk_struct2 {
    char unk[179];
    s16 unk2; // 0x70
    s32 unk3; // 0x72
    u32 unk4; // 0xb4
    u32 unk5; // 0xb8
};

/**
 * Inits the adhocctl module.
 * @param stackSize The stacksize adhocctl can use.
 * @param priority The priority of the thread.
 * @param product Contains the productId of the game, e.g. UCUS98612
 * @return An errorcode e.g. SCE_ERROR_NET_ADHOCCTL_ALREADY_INITIALIZED
 */
s32 sceNetAdhocctlInit(s32 stackSize, s32 priority, struct ProductStruct* product);

#endif
