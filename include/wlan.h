#ifndef WLAN_H
#define WLAN_H

// Used in wlan driver
struct wlanDrv_Params {
    u16 unk;
    u16 unk2;
    u16 unk3;
};

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
s32 sceWlanDrv_lib_56F467CA(struct wlanDrv_Params* unk);

/**
 * Seems to set parameter on wifi card, possibly beacon period?
 * @param unk Parameters set.
 * @return <0 error, one such error is 0x80410d13 (Bad params)
 */
s32 sceWlanDrv_lib_2D0FAE4E(struct wlanDrv_Params* unk);

#endif // WLAN_H
