#ifndef WLAN_H
#define WLAN_H

/**
 * Seems to set a parameter on the wifi card?
 * @param unk
 * @return <0 on error
 */
s32 sceWlanDrv_lib_5BAA1FE5(s32 unk);

/**
 * Seems to grab parameter from wifi card, possibly beacon period?
 * Parameter seems to be set in sceWlanDrv_lib_2D0FAE4E.
 * Which seems to be set to beacon period -2.
 * @param unk Seems to put something in here
 * @return <0 error, one such error is 0x80410d13 (Bad params)
 */
s32 sceWlanDrv_lib_56F467CA(s32* unk);

#endif // WLAN_H
