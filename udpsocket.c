#include "main.h"

void createUDPSocket(int32_t *__sock, uint16_t  *__port){
	struct sockaddr_in	serv;
	
	if ((*__sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
    		perror("socket udp");
    		exit(EXIT_FAILURE);
   	}
   
    serv.sin_family=AF_INET;
	serv.sin_port=htons(*__port);
	serv.sin_addr.s_addr=htonl(INADDR_ANY);
	
	if(bind(*__sock, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        	perror("bind udp");
        	close(*__sock);
        	exit(EXIT_FAILURE);
    	}

}

void createUNIXSocket(){
	if((sock_unix  = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0 ){
		perror("socket unix");
        	exit(EXIT_FAILURE);
	}
	

   	serv_unix.sun_family=AF_UNIX;
	strcpy(serv_unix.sun_path,UNIX_SOCKET);

	if(bind(sock_unix, (struct sockaddr *)&serv_unix, strlen(serv_unix.sun_path) + sizeof(serv_unix.sun_family)) < 0){			
        	perror("bind unix");
        	exit(EXIT_FAILURE);
	}
	
}void initBroadcastMessage(int *__sock){
    int     broadcastPermission = 1;
    int     len;
    if(setsockopt(*__sock, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission,sizeof(broadcastPermission)) < 0)perror("setsockopt() failed");
    memset(&broadcastHeartBeat,0,sizeof(broadcastHeartBeat));
    broadcastHeartBeat.sin_family = AF_INET;
    broadcastHeartBeat.sin_port = htons(9999);
    broadcastHeartBeat.sin_addr.s_addr = inet_addr("192.168.1.255");
    len = sizeof(broadcastHeartBeat);

}
