/* Copyright (C) 2021 The uOFW team
See the file COPYING for copying permission.
*/

#ifndef PSPNET_ADHOC_H
#define PSPNET_ADHOC_H

/**
 * Wait for a PTP (TCP-like) connection.
 * @param srcmac Source MAC-address.
 * @param srcport Source port.
 * @param bufsize Buffer size.
 * @param delay Retry interval.
 * @param count Number of retries.
 * @param queue Connection queue length.
 * @param unk Unknown.
 * @return Socket Id when successful, <0 on failure.
 */
s32 sceNetAdhocPtpListen(u8* srcmac, u16 srcport, s32 bufsize, u32 delay,
                         s32 count, s32 queue, s32 unk);

/**
 * Opens a PTP (TCP-like) socket.
 * @param srcmac Local mac address for the socket.
 * @param srcport Source port for the socket.
 * @param destmac Destination mac for the socket.
 * @param destport Destination port for the socket.
 * @param bufsize Size of the buffer.
 * @param delay Interval between retries in microseconds.
 * @param count The amount of retries.
 * @param unk1 Unknown.
 * @return Socket Id when successful, < 0 on failure.
 */
s32 sceNetAdhocPtpOpen(u8* srcmac, u16 srcport, u8* destmac, u16 destport, u32 bufsize, u32 delay, s32 count, s32 unk1);

/**
 * Wait for connection created by sceNetAdhocPtpOpen().
 * @param sockId Socket id to connect.
 * @param timeout Timeout in microseconds.
 * @param nonblock Set true if non-blocking
 * @return 0 on success, <0 on failure.
 */
s32 sceNetAdhocPtpConnect(s32 sockId, u32 timeout, s32 nonblock);

/**
 * Closes a PTP (TCP-like) socket.
 * @param sockId Socket id to close.
 * @param unk Unknown.
 * @return Socket Id when successful, <0 on failure.
 */
s32 sceNetAdhocPtpClose(s32 sockId, s32 unk);

/**
 * Accept an incoming PTP (TCP-like) connection.
 * @param sockId Socket id to use.
 * @param mac Connecting peers mac.
 * @param port Port to use.
 * @param timeout Timeout in microseconds.
 * @param nonblock Set to 0 to block, 1 for non-blocking.
 * @return Socket Id when successful, <0 on failure.
 */
s32 sceNetAdhocPtpAccept(s32 sockId, unsigned char *mac, u16* port, u32 timeout, s32 nonblock);

/**
 * Receives data on an incoming PTP (TCP-like) connection.
 * @param sockId Socket id to use.
 * @param data Received data.
 * @param dataLength Length of received data.
 * @param timeout Timeout in microseconds.
 * @param nonblock Set to 0 to block, 1 for non-blocking.
 * @return Number of bytes received when successful, <0 on failure.
 */
s32 sceNetAdhocPtpRecv(s32 sockId, void* data, void* dataLength, u32 timeout, s32 nonblock);

/**
 * Send data on a PTP (TCP-like) connection.
 * @param sockId Socket id to use.
 * @param data Connecting peers mac.
 * @param dataSize Size of data to send
 * @param timeout Timeout in microseconds.
 * @param nonblock Set to 0 to block, 1 for non-blocking.
 * @return Socket Id when successful, <0 on failure.
 */
s32 sceNetAdhocPtpSend(s32 sockId, const char* data, u32 *dataSize, u32 timout, s32 nonblock);

/**
 * Wait for data in buffer to be sent on PTP (TCP-like) connection.
 * @param sockId Socket id to use.
 * @param timeout Timeout in microseconds.
 * @param nonblock Set to 0 to block, 1 for non-blocking.
 * @return Socket Id when successful, <0 on failure.
 */
s32 sceNetAdhocPtpFlush(s32 sockId, u32 timeout, s32 nonblock);

#endif //PSPNET_ADHOC_H
