#include "main.h"

void *thrd_func_for_offset(void *__colun){
    
	struct Coluns	*colun  = (struct Coluns *)__colun;
	uint16_t		RTTcount;	
	int8_t			success_count, bestResult, offsetDir, running = 1, offset_fails,res;
	uint8_t			skew_position = 0;
	uint32_t		total_rtt = 0;
	uint64_t		sec;
	float			clockSkew, skewTMP;
	struct timeval		skew_clock, currTime;
	struct SkewData		*currentNode = NULL;
	d1 = d2 = 0;
    	//-----------------------------------------------------------------------------------
	printf("New colun %p, channel %d\n",colun, colun->channel);

    //------------------Get delays Range---------------------------------------------------
    RTTcount = RTT_REQUEST_COUNT;
rtt_start:;
    if(currTime.tv_sec - colun->online.tv_sec > COLUNS_OFFLINE_TIMEOUT) goto close_colun;
    colun->minRTT = 0;
    colun->maxRTT = RTT_REQUEST_TIMEOUT;
    offset_fails=0;
    if((RTTcount = getRTT(colun,&colun->minRTT,&colun->maxRTT)) == 0) {
		printf("RTT get error\n");
        goto close_colun;
	}
	getRTTInterval(colun->rttGausa,&colun->minRTT,&colun->maxRTT,RTTcount);
	free(colun->rttGausa);
	printf("%p: Range  %d-%d\n",colun,colun->minRTT,colun->maxRTT);
	printf("--------------------------------------------------\n");

    //--------------------------------------------------------------------------------	
	while(currTime.tv_sec - colun->online.tv_sec<COLUNS_OFFLINE_TIMEOUT){
        gettimeofday(&currTime,NULL);
        if(currTime.tv_sec-colun->online.tv_sec>COLUNS_STOP_SERVICE_TIMEOUT){
            printf("Colun '%p' don't responce...\n", colun);
            sleep(1);
            continue;
        }
		if(colun->status == STATUS_NONE  || colun->status == STATUS_OFFSET){
            colun->status = STATUS_OFFSET;
			if((res = getOffsetFromColun(colun,&sec,&skew_clock)) == -1){
                if(offset_fails>10)goto rtt_start;
                offset_fails++;
                continue;
            }else if(res == -2){
                continue;
            }
		}
		else {
            usleep(100000);
			continue;
		}
        
		
        colun->status = STATUS_NONE;
        if(colun->skew != NULL){
			if(success_count == SKEW_COUNT){
				colun->skew = deleteFirstNode(colun->skew);
				success_count--;

			}
			currentNode = addNewNode(currentNode,&sec,&skew_clock );
			skewTMP = calculateSkewTime(colun->skew);
            colun->actualSkewLR = calculateSkewLR(colun->skew);
            colun->actualSkew = skewTMP;
            colun->newSkew = YES;
			//printf("%p : SkewMead = %.3f\t, SkewLR %.3f\n",colun, colun->actualSkew, colun->actualSkewLR*-1);
			printf("%p : SkewLR %.3f\n",colun, colun->actualSkewLR*-1);
			success_count++;
		}
		else{
			colun->skew = addNewNode(colun->skew,&sec, &skew_clock);
			currentNode = colun->skew;
			success_count++;
		}
    if(currTime.tv_sec-colun->online.tv_sec<COLUNS_STOP_SERVICE_TIMEOUT)sleep(DELAY);
    gettimeofday(&currTime,NULL);
	}
close_colun:;
    if(colun->pref != NULL){
        colun->pref->next = colun->next;
    }else{ 
        //printf("New head %p\n",colun->next);
        colunsHead = colun->next;
    }
    if(colun->next !=NULL){
        colun->next->pref = colun->pref;
    }else {
        //printf("New last %p\n",colun->pref);
        colunsLast = colun->pref;
    }
    memset(colun,'\0',sizeof(struct Coluns));
    free(colun);
    //printTree(colunsHead);
    colunsCount--;
    printf("Colun '%p' is offline, close thread and realise all resoureces...\n",colun);

}


//######################################################################################################################################################
void *action_thrd_func(){
    printf("\tAction thread has been started\n");
    playingStatus = STOP_S;	
	int16_t		byte, i,y;
	unsigned char	buffer[BUFFSIZE];
	int32_t		fd_wav, count, bytes;
    uint16_t    tmp;
    struct Coluns   *currColun;
	
	while(isExit == 0 ){
        recvfrom(sock_unix,buffer,100,0,NULL,NULL );
		switch(buffer[0]){
            case VOLUME_S:
                    buff_play[0] = VOLUME;
                    buff_play[1] = buffer[1];
                    currColun = colunsHead;
                    while(currColun != NULL){
                        sendto(sock,buff_play,2,0,(struct sockaddr*)&currColun->sock,sizeof(currColun->sock));
                        currColun = currColun->next;
                    }
                break;
			case PLAY_S:
				if(colunsCount == 0){ 
                    printf("Don't have any colun for play\n");
                    continue;
                }
                if(playingStatus == PLAY_S || playingStatus == PAUSE_S){
                    stopPlay();
                }
                lastMusicPos = (uint16_t)buffer[1];
				music = getMusicFileById(fileList->head, lastMusicPos);
                if(music == NULL){
                    music = fileList->head;
                    lastMusicPos = 0;
                }
                //printf("START -->> %s\n", music->name);
				switch(music->format){
					case MP3:
						samples = parseMP3File( &wavHead, music);
						fileSize = music->sizeTotal/music->channels;	
                        //printf("fileSize %d\n", fileSize);
						break;
					case WAV:
                        printf("WAV is not supported\n");
						//samples = parseWAVFile( &wavHead, fileName);
						//fileSize = (wavHead.chunkSize-36)/2;	
						break;
					default:
                        printf("Format = %d, is not supported\n",music->format);
				}
				if(samples == NULL){
					printf("Samples = NULL\n");
					break;
				}
			 	rightChannel  = (struct monoSamples_t *)malloc(fileSize);
				leftChannel  = (struct monoSamples_t *)malloc(fileSize);
                getLeftRightChannels(samples, rightChannel, leftChannel, music);
				
				playingStatus = PLAY_S;
                currColun = colunsHead;
                while(currColun != NULL){
                     pthread_create(&currColun->thrdData,NULL,&thrd_func_for_send_data,currColun);
                     currColun = currColun->next;
                }
                pthread_create(&thrd_for_start_play,NULL,&thrd_func_start_play_when_is_ready,NULL);
				break;
			case STOP_S:
				if(playingStatus == PLAY_S){
					stopPlay();
				}
				break;
			case PAUSE_S:
				//if(playingStatus == PLAY_S){
                  //  playingStatus = PAUSE_S;
				    pausePlay();
                //}else if(playingStatus == PAUSE_S) {
                  //  playingStatus = PLAY_S;
				    //pausePlay();
                //}
				break;
            case SET_S:
                setPlay((uint16_t)(buffer[2]<<8 | buffer[1]));
            break;
			default:
				printf("Action thread switch default\n");

		}
	}
}

//######################################################################################################################################################
void *thrd_func_for_send_data(void *__colun){
	struct Coluns	*colun  = (struct Coluns *)__colun;
    //printf("\tthrd_func_for_send_data %p -- START\n",colun);
	uint16_t		pass = 1450;
	unsigned char		buff_audio[pass+5];
	int32_t			curr_pos=0, count = 0;
	uint32_t		i=0, y,packets_count;	
	struct timeval		tp1,tp2;
	struct monoSamples_t	*pcm_data;	
	buff_audio[0] = PREPARE;
	colun->packet.status = NO;
	memcpy(&buff_audio[1], &music->rate,4);
	memcpy(&buff_audio[5], &fileSize,4);
	//---------------------------------------------------------------------------------------------
    while(colun->status != STATUS_DATA){
        if(colun->status == STATUS_NONE){
            colun->status = STATUS_DATA;
		}
        usleep(250000);
	}
	//printf("DATA: start\n");
	switch(colun->channel){
		case 1:
			pcm_data = leftChannel;
			break;
		case 2:
			pcm_data = rightChannel;
			break;
            default:
		       	pcm_data = leftChannel;
	}
	//-------------------------------------------------------------------------------------------
    while(colun->isPrepared == NO){
		sendto(sock,buff_audio,9,0,(struct sockaddr *)&colun->sock,sizeof(colun->sock));
		gettimeofday(&tp1,NULL);
		gettimeofday(&tp2,NULL);
        
	    //printf("Prepare -- >> send\n");
		while(getmsdiff(&tp1.tv_usec,&tp2.tv_usec) < RESEND_PACKET_DELAY){
            if(colun->isPrepared == YES)break;
            gettimeofday(&tp2,NULL);
		}

	}
	//-----------------------------------------------------------------------------------------------------------
	
    packets_count = fileSize/pass;
	while(colun->status == STATUS_DATA){
		if(playingStatus == STOP_S)break;
		buff_audio[0] = LOST_PACKETS; 
		sendto(sock,buff_audio,1,0,(struct sockaddr *)&colun->sock,sizeof(colun->sock));
		gettimeofday(&tp1,NULL);
		gettimeofday(&tp2,NULL);
		while(getmsdiff(&tp1.tv_usec,&tp2.tv_usec) < RESEND_PACKET_DELAY){
			if(pcm_data == NULL)break;
			if(colun->isLost == YES){
				colun->isLost = NO;
				if(colun->lostS == NO){
					colun->status = STATUS_NONE;
				}else{
					buff_audio[0] = DATA;
					for(y = 0; y < colun->lostS; y ++){
						memcpy(&buff_audio[1],&colun->lostP[y],4);
						if(playingStatus == STOP_S)break;
                        //printf("1\n");
						memcpy(&buff_audio[5],&pcm_data[colun->lostP[y]*pass/2],pass);
						sendto(sock,buff_audio,pass+5,0,(struct sockaddr *)&colun->sock,sizeof(colun->sock));
					}
				}
			}
			gettimeofday(&tp2,NULL);
		}
		if((100*colun->lostP[0])/(packets_count) > 5 && playingStatus != STOP_S){
            isReadyToStart = YES;
            colun->isReadyToPlay = YES;
        }
	}
    //printf("\tthrd_func_for_send_data -- STOP\n");
    colun->status = STATUS_NONE;
    colun->isPrepared = NO;
    colun->isReadyToPlay = NO;
    colun->isLost = NO;
  //  printf("DATA: stop\n");
}

//######################################################################################################################################################
void *thrd_func_for_send_skew(){
    printf("\tSkew thread has been started\n");
    float		    deltaSkew;
    struct Coluns   *currColun;
    buff_skew[0]=SKEW;
    while(1){
        if(colunsCount <2 || isReadyToSendSkew(colunsHead)==NO){
            sleep(1);
            continue; 
        }
    currColun = colunsHead;
    while(currColun != NULL){
                deltaSkew = getSkewLRDelta(colunsHead, &currColun->actualSkewLR);
                currColun->newSkew = NO;
				memcpy(&buff_skew[1],&deltaSkew,4);
				sendto(sock,buff_skew,5,0,(struct sockaddr*)&currColun->sock,sizeof(currColun->sock));
                //printf("%p: DeltaSkew = %.3f\n",&currColun, deltaSkew);	
                currColun = currColun->next;
        }
        sleep(1);
    } 
}

//######################################################################################################################################################
void *thrd_func_broadcast_coluns_detect(){
    printf("\tColuns detect thread has been started\n");
    
    char msg[1];
    int i;
    msg[0] = BROADCAST;
    char ip[15];
    while(1){
        broadcastHeartBeat.sin_addr.s_addr = inet_addr("192.168.1.218");
        sendto(sock,msg,1,0,(struct sockaddr*)&broadcastHeartBeat,sizeof(broadcastHeartBeat));
        broadcastHeartBeat.sin_addr.s_addr = inet_addr("192.168.1.219");
        sendto(sock,msg,1,0,(struct sockaddr*)&broadcastHeartBeat,sizeof(broadcastHeartBeat));
        broadcastHeartBeat.sin_addr.s_addr = inet_addr("192.168.1.203");
        sendto(sock,msg,1,0,(struct sockaddr*)&broadcastHeartBeat,sizeof(broadcastHeartBeat));
        sleep(1);
    }
    
}

//######################################################################################################################################################

void *thrd_func_send_second_to_client(){
    //printf("\t thrd_func_send_second_to_client\n");
    buff_duration[0] = DURATION;
    timer(&timeToStart);
    unsigned char   __buffToPlay[2];
    musicCurrentPosition = 1;
    while(musicCurrentPosition <= music->duration && playingStatus != STOP_S){
        if(playingStatus == PLAY_S){
            //printf("%d\n",musicCurrentPosition);
            memcpy(&buff_duration[1],&musicCurrentPosition,2);
            sendto(sock,buff_duration,3,0,(struct sockaddr*)&broadcastHeartBeat,sizeof(broadcastHeartBeat));
            musicCurrentPosition++;
            sleep(1);
        }
        else if(playingStatus == PAUSE_S){
                usleep(200000);
                continue;
        }
    
    }
    if(playingStatus == PLAY_S){
        if(rightChannel)free(rightChannel);
	    if(leftChannel)free(leftChannel);
	    if(samples)free(samples);
        playingStatus = STOP;
        isReadyToStart = NO;
        lastMusicPos++;
        __buffToPlay[0] = PLAY_S;
        __buffToPlay[1] = (unsigned char)lastMusicPos;
        
        sendto(sock_unix,__buffToPlay,2,0, (struct sockaddr *)&serv_unix, strlen(serv_unix.sun_path) + sizeof(serv_unix.sun_family));

    }else if(playingStatus == STOP_S){ 
        isReadyToStart = NO;
    }
}

//######################################################################################################################################################
void *thrd_func_start_play_when_is_ready(){
    while(playingStatus == PLAY_S){
        if(isReadyToPlay(colunsHead) == YES){
            startPlay();
            break;
        }
        usleep(100000);
    }
}


void *thrd_func_for_watch_dir(){
    int inotify_fd;
    keep_running = 1;
    
    inotify_fd = open_inotify_fd();
    if (inotify_fd > 0){
        queue_t q;
        q = queue_create(128);
        int wd;
        int index;
        wd = 0;
	    wd = watch_dir(inotify_fd, MUSIC_PATH, IN_DELETE | IN_CREATE);
        if (wd > 0) {
            process_inotify_events(q, inotify_fd);
	    }
        close_inotify_fd (inotify_fd);
        queue_destroy (q);
    }

}
