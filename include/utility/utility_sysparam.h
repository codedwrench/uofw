/* Copyright (C) 2021 The uOFW team
   See the file COPYING for copying permission.
*/

/**
 * uofw/include/utility/utility_sysparam.h
 *
 */

#ifndef UTILITY_SYSPARAM_H
#define UTILITY_SYSPARAM_H

#define SYSTEMPARAM_STRING_NICKNAME   1
#define SYSTEMPARAM_INT_ADHOC_CHANNEL 2

s32 sceUtilityGetSystemParamInt(s32 id, s32 *value);
s32 sceUtilityGetSystemParamString(s32 id, char *value, u32 size);

#endif // UTILITY_SYSPARAM_H
