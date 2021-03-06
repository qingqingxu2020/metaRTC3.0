//
// Copyright (c) 2019-2022 yanggaofeng
//
#ifndef YANGWEBRTC_YANGUDPHANDLE_H_
#define YANGWEBRTC_YANGUDPHANDLE_H_

#include <yangrtc/YangUdpHandleH.h>
#ifdef __cplusplus
extern "C"{
#endif
int32_t yang_create_rtcudp(YangUdpHandle* udp,char* remoteIp,int32_t remotePort,int32_t plocalPort);
void yang_destroy_rtcudp(YangUdpHandle* udp);
void yang_start_rtcudp(YangUdpHandle* udp);
void yang_stop_rtcudp(YangUdpHandle* udp);
int32_t yang_rtc_sendData(YangUdpHandle* udp,char* p,int32_t plen);
#ifdef __cplusplus
}
#endif
#endif /* YANGWEBRTC_YANGUDPHANDLE_H_ */
