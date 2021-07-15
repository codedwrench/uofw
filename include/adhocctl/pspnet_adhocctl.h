#ifndef PSPNET_ADHOCCTL_H
#define PSPNET_ADHOCCTL_H

extern const char g_DefSSIDPrefix[];
extern const char g_AdhocRegString[];
extern const char g_SSIDPrefixRegKey[];

enum AdhocIds {
    SCE_NET_ADHOCCTL_ADHOCTYPE_NORMAL = 0,
    SCE_NET_ADHOCCTL_ADHOCTYPE_RESERVED,
    SCE_NET_ADHOCCTL_ADHOCTYPE_GAMESHARE
};

struct ProductStruct {
    enum AdhocIds adhocId;
    char product[10]; // Product Id, e.g. UCUS98612
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
