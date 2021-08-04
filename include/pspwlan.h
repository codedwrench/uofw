/* Copyright (C) 2021 The uOFW team
   See the file COPYING for copying permission.
*/

#ifndef PSPWLAN_H
#define PSPWLAN_H

s32 sceWlanSetHostDiscover(s32 unk, s32 unk2);
s32 sceWlanSetWakeUp(s32 unk, s32 unk2);
s32 sceWlanDevAttach();
s32 sceWlanDevDetach();
s32 sceWlanGetSwitchState();

#endif // PSPWLAN_H
