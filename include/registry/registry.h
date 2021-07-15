#ifndef REGISTRY_H
#define REGISTRY_H

enum RegKeyTypes 
{
  REG_TYPE_DIR = 1,
  REG_TYPE_s32,
  REG_TYPE_STR,
  REG_TYPE_BIN
};

struct RegParam
{
  u32 regtype;
  char name[256];
  u32 namelen;
  u32 unk2;
  u32 unk3;
};

/**
 * Open the registry
 *
 * @param reg - A filled in ::RegParam structure
 * @param mode - Open mode (set to 1)
 * @param h - Pos32er to a registry handle to receive the registry handle
 *
 * @return 0 on success, < 0 on error
 */
s32 sceRegOpenRegistry(struct RegParam *reg, s32 mode, u32 *h);

/**
 * Close the registry
 *
 * @param h - The open registry handle
 *
 * @return 0 on success, < 0 on error
 */
s32 sceRegCloseRegistry(u32 h);

/**
 * Open a registry directory
 *
 * @param h - The open registry handle
 * @param name - The path to the dir to open (e.g. /CONFIG/SYSTEM)
 * @param mode - Open mode (can be 1 or 2, probably read or read/write
 * @param hd - Pos32er to a registry handle to receive the registry dir handle
 *
 * @return 0 on success, < 0 on error
 */
s32 sceRegOpenCategory(u32 h, const char *name, s32 mode, u32 *hd);

/**
 * Close the registry directory
 *
 * @param hd - The open registry dir handle
 *
 * @return 0 on success, < 0 on error
 */
s32 sceRegCloseCategory(u32 hd);

/**
 * Get a key's information
 *
 * @param hd - The open registry dir handle
 * @param name - Name of the key
 * @param hk - Pos32er to a REGHANDLE to get registry key handle
 * @param type - Type of the key, on of ::RegKeyTypes
 * @param size - The size of the key's value in bytes
 *
 * @return 0 on success, < 0 on error
 */
s32 sceRegGetKeyInfo(u32 hd, const char *name, u32 *hk, enum RegKeyTypes *type, SceSize *size);

/**
 * Get a key's value
 *
 * @param hd - The open registry dir handle
 * @param hk - The open registry key handler (from ::sceRegGetKeyInfo)
 * @param buf - Buffer to hold the value
 * @param size - The size of the buffer
 *
 * @return 0 on success, < 0 on error
 */
s32 sceRegGetKeyValue(u32 hd, u32 hk, void *buf, SceSize size);

#endif
