/* Copyright (C) 2021 The uOFW team
See the file COPYING for copying permission.
*/

#ifndef PSPNET_ADHOC_AUTH_H
#define PSPNET_ADHOC_AUTH_H

/**
 * Initializes the adhoc auth module.
 * @return errorcode on failure, 0 on success.
 */
s32 sceNetAdhocAuthInit();

/**
 * Termination of what has been initialized with sceNetAdhocAuth_lib_89F2A732()
 * @return 0 if successful.
 */
s32 sceNetAdhocAuth_lib_0x2E6AA271();

/**
 * Initializes something in the the sceNet auth lib.
 * @param name Name of the interface.
 * @param unk
 * @param unk2
 * @param unk3
 * @return 0 if successful.
 */
s32 sceNetAdhocAuth_lib_0x89F2A732(const char* name, s32 unk, s32 unk2, s32* unk3);

/**
 * Terminates the adhoc auth module.
 * @return not sure yet.
 */
s32 sceNetAdhocAuthTerm();

#endif // PSPNET_ADHOC_AUTH_H
