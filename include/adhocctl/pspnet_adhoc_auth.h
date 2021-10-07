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
 * Creates and starts thread in the the sceNetAdhocAuth lib. (sceNetAdhocAuth_driver_0x89F2A732)
 * @param name Name of the interface.
 * @param priority Priority of the thread to create.
 * @param stackSize Size that the stack for the thread needs to be.
 * @param clocks Used in sceKernelSetAlarm functions as number of micro seconds till the alarm occurs.
 * @param unk
 * @param nickname The username in use on the PSP.
 * @return 0 if successful.
 */
s32 sceNetAdhocAuthCreateStartThread(const char *name, s32 priority, s32 stackSize, s32 *clocks, s32 unk,
                                     const char *nickname);

/**
 * Terminates the adhoc auth module.
 * @return not sure yet.
 */
s32 sceNetAdhocAuthTerm();

#endif // PSPNET_ADHOC_AUTH_H
