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
 * Terminates the adhoc auth module.
 * @return not sure yet.
 */
s32 sceNetAdhocAuthTerm();

#endif // PSPNET_ADHOC_AUTH_H
