//
// Copyright (c) 2019-2022 yanggaofeng
//
#include "YangSctp.h"
#include <yangrtc/YangRtcDtls.h>
#include <yangutil/sys/YangLog.h>
#include <yangutil/sys/YangEndian.h>




int32_t g_sctp_conn_output(void *addr, void *buf, size_t length, uint8_t tos, uint8_t set_df)
{
	YangSctp* sctp=(YangSctp*)addr;
	int32_t err=Yang_Ok;
	if(sctp->send_dtls_msg) sctp->send_dtls_msg(sctp->user,buf,length);

	return err;

}
int g_sctp_receive_cb(struct socket *sock, union sctp_sockstore addr, void *data,
          size_t datalen, struct sctp_rcvinfo rcv, int flags, void *ulp_info)
{
	YangSctp* sctp=(YangSctp*)ulp_info;
	uint16_t streamId = rcv.rcv_sid;
	uint32_t ppid     = ntohl(rcv.rcv_ppid);
	uint16_t ssn      = rcv.rcv_ssn;
	int32_t eor = flags & MSG_EOR;
	yang_trace("\nstreamId=%d,ssn=%d,ppid=%d,eor=%d,len=%d,data=%s",streamId,ssn,ppid,eor,datalen,ppid==DATA_CHANNEL_PPID_STRING?data:"[0/1]");
	//if(ppid==50) return 1;
	if(ppid==DATA_CHANNEL_PPID_STRING){
		if(sctp->receive_msg) sctp->receive_msg(sctp->user,streamId, ssn, ppid, flags, (char*)data, datalen);
	}

/**
	rcv.rcv_ppid = ntohl(rcv.rcv_ppid);
	    switch (ppid) {
	        case DATA_CHANNEL_PPID_DCEP:
	        	//yang_sctp_sendData(sctp,data, datalen);
	            break;
	        case DATA_CHANNEL_PPID_BINARY:
	        case DATA_CHANNEL_PPID_BINARY_EMPTY:
	        case DATA_CHANNEL_PPID_STRING:
	        case DATA_CHANNEL_PPID_STRING_EMPTY:
	            break;
	        default:
	            yang_error("Unknown PPID SCTP message %d", rcv.rcv_ppid);
	            break;
	    }
**/


	yang_free(data);
	return Yang_Ok;
}

void yang_sctp_sendData(YangSctp* sctp,char* data,int32_t nb,int32_t isBinary){

	memset(&sctp->spa, 0, sizeof(struct sctp_sendv_spa));
	sctp->spa.sendv_flags |= SCTP_SEND_SNDINFO_VALID;
	sctp->spa.sendv_sndinfo.snd_sid = sctp->streamId;
	sctp->spa.sendv_sndinfo.snd_flags =SCTP_EOR;

	yang_put_be32(&sctp->spa.sendv_sndinfo.snd_ppid,isBinary ? DATA_CHANNEL_PPID_BINARY : DATA_CHANNEL_PPID_STRING);
	int32_t ret = usrsctp_sendv(sctp->local_sock, data,isBinary?nb:nb+1, NULL, 0, &sctp->spa, sizeof(sctp->spa), SCTP_SENDV_SPA, 0);
	if(ret<0){
		yang_error("sctp send error(%d)",ret);
	}

}

void yang_sctp_receiveData(YangSctp* sctp,uint8_t* data,int32_t nb){
	usrsctp_conninput(sctp, data, nb, 0);
}


void yang_create_sctp(YangSctp* sctp){
	//rel
	sctp->ordered=1;
	sctp->maxRetransmits=-1;
	struct sockaddr_conn localConn, remoteConn;
	struct sctp_paddrparams peerAddrParams;

	usrsctp_init(0, g_sctp_conn_output, NULL);
	usrsctp_sysctl_set_sctp_ecn_enable(0);


	memset(&localConn, 0, sizeof(struct sockaddr_conn));
	memset(&remoteConn, 0, sizeof(struct sockaddr_conn));



	localConn.sconn_family = AF_CONN;
	localConn.sconn_port=htons(YANG_SCTP_ASSOCIATION_DEFAULT_PORT);
	localConn.sconn_addr = sctp;

	remoteConn.sconn_family = AF_CONN;
	remoteConn.sconn_port=htons(YANG_SCTP_ASSOCIATION_DEFAULT_PORT);
	remoteConn.sconn_addr = sctp;

	sctp->local_sock = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP, g_sctp_receive_cb, NULL, 0, sctp);

	usrsctp_register_address(sctp);

	usrsctp_set_non_blocking(sctp->local_sock, 1);

	struct linger linger_opt;
	linger_opt.l_onoff = 1;
	linger_opt.l_linger = 0;
	usrsctp_setsockopt(sctp->local_sock, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt));

	uint32_t on = 1;
	usrsctp_setsockopt(sctp->local_sock, IPPROTO_SCTP, SCTP_NODELAY, &on, sizeof(on));

	struct sctp_event event;
	uint16_t event_types[] = {SCTP_ASSOC_CHANGE,   SCTP_PEER_ADDR_CHANGE,      SCTP_REMOTE_ERROR,
			SCTP_SHUTDOWN_EVENT, SCTP_ADAPTATION_INDICATION, SCTP_PARTIAL_DELIVERY_EVENT};
	memset(&event, 0, sizeof(event));
	event.se_assoc_id = SCTP_FUTURE_ASSOC;
	event.se_on = 1;
	for (uint32_t i = 0; i < 6; i++) {
		event.se_type = event_types[i];
		usrsctp_setsockopt(sctp->local_sock, IPPROTO_SCTP, SCTP_EVENT, &event, sizeof(struct sctp_event));
	}

	struct sctp_initmsg initmsg;
	memset(&initmsg, 0, sizeof(struct sctp_initmsg));
	initmsg.sinit_num_ostreams = 300;
	initmsg.sinit_max_instreams = 300;
	usrsctp_setsockopt(sctp->local_sock, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(struct sctp_initmsg));

	usrsctp_bind(sctp->local_sock, (struct sockaddr*) &localConn, sizeof(localConn));


	int32_t status = 0;
	status = usrsctp_connect(sctp->local_sock, (struct sockaddr*) &remoteConn, sizeof(remoteConn));
	if(status >= 0 || errno == EINPROGRESS){

	}
	memset(&peerAddrParams, 0, sizeof(struct sctp_paddrparams));
	memcpy(&peerAddrParams.spp_address, &remoteConn, sizeof(remoteConn));
	peerAddrParams.spp_flags = SPP_PMTUD_DISABLE;
	peerAddrParams.spp_pathmtu = YANG_SCTP_MTU;
	usrsctp_setsockopt(sctp->local_sock, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS, &peerAddrParams, sizeof(peerAddrParams));

}
void yang_destroy_sctp(YangSctp* sctp){
	usrsctp_deregister_address(sctp);
	usrsctp_set_ulpinfo(sctp->local_sock, NULL);
	usrsctp_shutdown(sctp->local_sock, SHUT_RDWR);
	usrsctp_close(sctp->local_sock);


	while (usrsctp_finish() != 0) {
		yang_usleep(1000);
	}
}

