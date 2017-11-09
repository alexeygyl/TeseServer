#include "main.h"

void start(void){
    struct Coluns *colun;
	uint32_t	tmp;
	createUNIXSocket();
	createUDPSocket(&sock, &args.port);
    fileList = music_create();
	getFilesNameFromDir(fileList,MUSIC_PATH);
    initBroadcastMessage(&sock);
    pthread_create(&action_thrd,NULL,&action_thrd_func,NULL);
	pthread_create(&skew_thrd,NULL,&thrd_func_for_send_skew,NULL);
	pthread_create(&thrd_coluns_detect,NULL,&thrd_func_broadcast_coluns_detect,NULL);
	pthread_create(&thrd_for_watch_dir,NULL,&thrd_func_for_watch_dir,NULL);
    client_len = sizeof(client);
	colunsCount = 0;
	isExit = 0;
	int count = 0;
	request[0] = OFFSET_REQUEST;
	rttRequest[0] = RTT_REQUEST;
	struct timeval	tv;
	while(isExit == 0){
		bytes_r = recvfrom(sock,buff,BUFFSIZE,0,(struct sockaddr*)&client, &client_len);
		switch(buff[0]){
            case BROADCAST:
                //printf("\tBROADCAST....\n");
            break;
            case INIT_REQUEST:
                if(colunsCount == 0){
                    colunsLast = colunsHead = addNewColun(colunsHead,&client,buff[1]);
                    pthread_create(&colunsLast->thrdMain,NULL,&thrd_func_for_offset,colunsLast);
                    //printTree(colunsHead);
                    //printf("0:New colun %p, count %d\n",colunsLast, colunsCount);
                    break;
                }
                colun = getColunByAddr(colunsHead,&client);
                if(colun != NULL){
                    gettimeofday(&colun->online,NULL);
                }
                else {
                    colunsLast = addNewColun(colunsLast,&client, buff[1]);
                    pthread_create(&colunsLast->thrdMain,NULL,&thrd_func_for_offset,colunsLast);
                    //printTree(colunsHead);
                    //printf("1:New colun %p, count %d\n",colunsLast, colunsCount);
                }
			break;
		
            case OFFSET_RESPONCE:
                colun = getColunByAddr(colunsHead,&client);
				if(colun !=NULL){
					memcpy(&tmp, &buff[1],4);
					if(colun->expected_p != tmp)continue;
					memcpy(&colun->packet.time.tv_sec,&buff[5],4);
					memcpy(&colun->packet.time.tv_usec,&buff[9],4);
					colun->packet.seq = tmp;
					colun->packet.status = 1;
				}
			break;
			case RTT_RESPONCE:
                colun = getColunByAddr(colunsHead,&client);
				if(colun != NULL){
					memcpy(&tmp, &buff[1],4);
					if(colun->expected_p != tmp)continue;
					colun->packet.seq = tmp;
					colun->packet.status = YES;

				}
			break;
            
			case PLAY_S:
				sendto(sock_unix,buff,BUFFSIZE_U,0, (struct sockaddr *)&serv_unix, strlen(serv_unix.sun_path) + sizeof(serv_unix.sun_family));
                break;
			case VOLUME_S:
				sendto(sock_unix,buff,BUFFSIZE_U,0, (struct sockaddr *)&serv_unix, strlen(serv_unix.sun_path) + sizeof(serv_unix.sun_family));
			break;
            case STOP_S:
				sendto(sock_unix,buff,1,0, (struct sockaddr *)&serv_unix, strlen(serv_unix.sun_path) + sizeof(serv_unix.sun_family));
			break;
			case PAUSE_S:
				sendto(sock_unix,buff,1,0, (struct sockaddr *)&serv_unix, strlen(serv_unix.sun_path) + sizeof(serv_unix.sun_family));
			break;
            case SET_S:
				sendto(sock_unix,buff,BUFFSIZE_U,0, (struct sockaddr *)&serv_unix, strlen(serv_unix.sun_path) + sizeof(serv_unix.sun_family));
			break;

			case MUSIC_LIST:
				sendMusicList(fileList->head);
				break;

			case DATA:
                colun = getColunByAddr(colunsHead,&client);
				if(colun != NULL ){
					memcpy(&tmp, &buff[1],4);
					if(colun->expected_p != tmp)continue;
					colun->packet.seq = tmp;
					colun->packet.status = YES;
				}
				break;
			case PREPARE:
                colun = getColunByAddr(colunsHead,&client);
				if(colun != NULL ){
					colun->isPrepared = YES;
				}
				break;
			case LOST_PACKETS:
                colun = getColunByAddr(colunsHead,&client);
				if(colun != NULL){
					colun->lostS = buff[1];
					memcpy(&colun->lostP,&buff[2],buff[1]*4);
					colun->isLost= YES;
				}
				break;
            default:
                printf("Unknow tag '%d\n'", buff[0]);

		}
	}
}

