#include "main.h"



int8_t readWavHead(int32_t __fd,struct wavHead_t *__wavHead){
	int8_t	bytes;
	bytes = read(__fd, __wavHead, 44);
	if(bytes !=44)return -1;
	return 1;
}


//###########################################################################################################################
void getLeftRightChannels(struct stereoSample_t *__samples, struct monoSamples_t *__right, struct monoSamples_t *__left, struct MusicFiles *__music){
	uint32_t	i;
	for(i = 0; i < __music->sizeTotal/__music->blockAlign; i++){
		memcpy(&__right[i].channel,&__samples[i].channelRight,2);
		memcpy(&__left[i].channel,&__samples[i].channelLeft,2);
	}
	
}

//###########################################################################################################################
struct stereoSample_t * parseMP3File(struct wavHead_t *__wavHead, struct MusicFiles *__music){
	mpg123_handle *mh;	
	int	err,channels,encoding, total=0;
	long rate;
	unsigned char * __buffer, *__buffer2;
	size_t	size, done;
	char    *fullFileName = (char *)malloc(strlen(__music->pwd)+ 1 + strlen(__music->name));
	sprintf(fullFileName,"%s/%s",__music->pwd,__music->name);

    
    mpg123_init();
 	mh = mpg123_new(NULL, &err);
	size = mpg123_outblock(mh);
	__buffer2 = (unsigned char*) malloc(size * sizeof(unsigned char));
	mpg123_open(mh, fullFileName);
    mpg123_getformat(mh, &rate, &channels,&encoding);
	__buffer = (unsigned char *)malloc(__music->size);
	
	while (mpg123_read(mh, __buffer2, size, &done) == MPG123_OK){                                         
		memcpy(&__buffer[total],&__buffer2[0], done);
		total +=done;
	}    
    __music->sizeTotal = total;
	struct stereoSample_t *output = (struct stereoSample_t *) malloc(total);
	memcpy(output,__buffer,total);

	mpg123_close(mh);
	mpg123_delete(mh);
	mpg123_exit();
	free(__buffer);
	free(__buffer2);
	return output;
}

//###########################################################################################################################
struct stereoSample_t * parseWAVFile(struct wavHead_t *__wavHead, unsigned char *__fileName){
	int size, fd_wav;
	unsigned char *_buffer;
	struct stereoSample_t *output;
	if((fd_wav = open(__fileName,O_RDONLY)) == -1)return NULL;
	if(readWavHead(fd_wav,&wavHead)>0){
	}else return NULL;
	size = (wavHead.chunkSize - 36);
	output  = (struct stereoSample_t *)malloc(size);
	if(read(fd_wav, output, size) == size){
		close(fd_wav);
		return output;
	}
	return NULL;
}


//###########################################################################################################################

int8_t  isReadyToPlay(struct Coluns *__colun){
    if(colunsCount == 0)return NO;
    if(__colun !=NULL){
        if(__colun->isReadyToPlay == NO){ return NO;}
        return isReadyToPlay(__colun->next);
    }
    return YES;
}

//###########################################################################################################################


//###########################################################################################################################
void startPlay(){
	struct timeval	colunOffset, timeMaster, timeColun;
    struct Coluns   *currColun;
    buff_play[0] = PLAY;
	gettimeofday(&timeMaster,NULL);
    currColun = colunsHead;
	while(currColun != NULL){
		colunOffset = getLastOffset(currColun->skew);
		timeColun.tv_sec = timeMaster.tv_sec - colunOffset.tv_sec;
		timeColun.tv_usec = timeMaster.tv_usec - colunOffset.tv_usec;
		if(timeColun.tv_usec<0){
	    	timeColun.tv_sec--;
			timeColun.tv_usec +=1000000;
		}
        changeTime(&timeColun,1,0);
		memcpy(&buff_play[1],&timeColun.tv_sec,4);
		memcpy(&buff_play[5],&timeColun.tv_usec,4);
		sendto(sock,buff_play,9,0,(struct sockaddr*)&currColun->sock,sizeof(currColun->sock));
	    usleep(100000);
		sendto(sock,buff_play,9,0,(struct sockaddr*)&currColun->sock,sizeof(currColun->sock));
        currColun = currColun->next;
    }

    //timeMaster.tv_sec+=1;
    changeTime(&timeMaster,1,0);
    timeToStart = timeMaster;
    pthread_create(&thrd_duration,NULL,&thrd_func_send_second_to_client,NULL);
}

//###########################################################################################################################

void pausePlay(){
    struct timeval	colunOffset, timeMaster, timeColun;
    struct Coluns   *currColun;

    buff_play[0] =  PAUSE;
	gettimeofday(&timeMaster,NULL);
    currColun = colunsHead;
	while(currColun != NULL){
		colunOffset = getLastOffset(currColun->skew);
		timeColun.tv_sec = timeMaster.tv_sec - colunOffset.tv_sec;
		timeColun.tv_usec = timeMaster.tv_usec - colunOffset.tv_usec;
		if(timeColun.tv_usec<0){
	    	timeColun.tv_sec--;
			timeColun.tv_usec +=1000000;
		}
        //printf("%d.%d  | ", timeColun.tv_sec, timeColun.tv_usec);
        changeTime(&timeColun,0,500000);
        //printf("%d.%d\n", timeColun.tv_sec, timeColun.tv_usec);
		memcpy(&buff_play[1],&timeColun.tv_sec,4);
		memcpy(&buff_play[5],&timeColun.tv_usec,4);
		sendto(sock,buff_play,9,0,(struct sockaddr*)&currColun->sock,sizeof(currColun->sock));
        currColun = currColun->next;
    }
    changeTime(&timeMaster,0,500000);
    timer(&timeMaster);
    if(playingStatus == PLAY_S){
        playingStatus = PAUSE_S;
    }else if(playingStatus == PAUSE_S) {
        playingStatus = PLAY_S;
    }

}

void setPlay(uint16_t __pos){
    struct timeval	colunOffset, timeMaster, timeColun;
    struct Coluns   *currColun;
    buff_play[0] =  SET;
	gettimeofday(&timeMaster,NULL);
    currColun = colunsHead;
	while(currColun != NULL){
		colunOffset = getLastOffset(currColun->skew);
		timeColun.tv_sec = timeMaster.tv_sec - colunOffset.tv_sec;
		timeColun.tv_usec = timeMaster.tv_usec - colunOffset.tv_usec;
		if(timeColun.tv_usec<0){
	    	timeColun.tv_sec--;
			timeColun.tv_usec +=1000000;
		}
		//timeColun.tv_sec+=1;
        changeTime(&timeColun,0,500000);
		memcpy(&buff_play[1],&timeColun.tv_sec,4);
		memcpy(&buff_play[5],&timeColun.tv_usec,4);
        memcpy(&buff_play[9],&__pos,2);
		sendto(sock,buff_play,11,0,(struct sockaddr*)&currColun->sock,sizeof(currColun->sock));
        currColun = currColun->next;
    }

    changeTime(&timeMaster,0,500000);
    timer(&timeMaster);
    musicCurrentPosition = __pos;
}


//###########################################################################################################################
void stopPlay(){
    struct timeval	colunOffset, timeMaster, timeColun;
    struct Coluns   *currColun;
    buff_play[0] = STOP;
    isReadyToStart = 0;
	playingStatus = STOP_S;
    currColun = colunsHead;
	while(currColun != NULL){
		pthread_join(currColun->thrdData,NULL);
	    currColun = currColun->next;
    }
    if(rightChannel)free(rightChannel);
	if(leftChannel)free(leftChannel);
	if(samples)free(samples);

	gettimeofday(&timeMaster,NULL);
    currColun = colunsHead;
	while(currColun != NULL){
		colunOffset = getLastOffset(currColun->skew);
		timeColun.tv_sec = timeMaster.tv_sec - colunOffset.tv_sec;
		timeColun.tv_usec = timeMaster.tv_usec - colunOffset.tv_usec;
        //printf("Time server %d,  Time colun %d\n",timeMaster.tv_sec, timeSlave.tv_sec);
		if(timeColun.tv_usec<0){
	    	timeColun.tv_sec--;
			timeColun.tv_usec +=1000000;
		}
        changeTime(&timeColun,0,500000);
		memcpy(&buff_play[1],&timeColun.tv_sec,4);
		memcpy(&buff_play[5],&timeColun.tv_usec,4);
		sendto(sock,buff_play,9,0,(struct sockaddr*)&currColun->sock,sizeof(currColun->sock));
        currColun = currColun->next;
    }
}

//###########################################################################################################################
struct MusicFiles *addNewFile(struct MusicFiles *__head, char *__fileName, char *__pwd ){
	struct MusicFiles *newNode  = (struct MusicFiles *)malloc(sizeof(struct MusicFiles));
        switch(getFormat(__fileName)){
        case MP3:
            parseMetaMP3(newNode,__fileName,__pwd);
            break;
        case WAV:
            printf("WAV it not support yet\n");
            return __head;
            break;
        default:
            printf("Unkownw format\n");
            return __head;
    }
    newNode->next = NULL;
	if(__head == NULL)return newNode;
	__head->next = newNode;
	return newNode;
}

//###########################################################################################################################
void parseMetaMP3(struct MusicFiles *__newNode, char *__fileName, char *__pwd){
    int     err, channels, encoding;
    long    rate;
    char    *fullFileName = (char *)malloc(strlen(__pwd)+ 1 + strlen(__fileName));
	sprintf(fullFileName,"%s/%s",__pwd,__fileName);
    
    mpg123_id3v1    *info;
    mpg123_handle *mh;	

    mpg123_init();
 	mh = mpg123_new(NULL, &err);
    mpg123_open(mh, fullFileName);
    mpg123_id3(mh,&info,NULL);
    
    if(info != NULL){
        __newNode->title = strdup(info->title);
        __newNode->artist = strdup(info->artist);
        __newNode->album = strdup(info->album);
        __newNode->year = strdup(info->year);
    }
    else{
        __newNode->title = __fileName;
        __newNode->artist = NULL;
        __newNode->album = NULL;
        __newNode->year = NULL;
    }
	mpg123_getformat(mh, &rate, &channels,&encoding);
    musicId++;
    __newNode->rate = rate;
    __newNode->channels = channels;
	__newNode->size = mpg123_length(mh)*mpg123_encsize(encoding)*channels;
    __newNode->duration = mpg123_length(mh)/rate;
	__newNode->blockAlign = mpg123_encsize(encoding)*channels;
    __newNode->format = 1;
    __newNode->name = __fileName;
    __newNode->pwd = __pwd;
    __newNode->id = musicId;
    mpg123_close(mh);
	mpg123_delete(mh);
	mpg123_exit();
    free(fullFileName);
    
    printf("Id: %d\n",__newNode->id);
    printf("Name: %s\n",__newNode->name);
    //printf("PWD: %s\n",__newNode->pwd);
    //printf("Tile: %s\n",__newNode->title);
    //printf("Artist: %s\n",__newNode->artist);
    //printf("Album: %s\n",__newNode->album);
    //printf("Year: %s\n",__newNode->year);
    //printf("Duration: %d\n",__newNode->duration);
    //printf("FORMAT: %d\n",__newNode->format);
    //printf("--------------------------\n");

}


//###########################################################################################################################
char *getFileName(struct MusicFiles *__files, int __pos){
	static int deep = 0;
	if(__files != NULL){
		if(__pos == deep){
			deep = 0;
			char *fullFileName = (char *)malloc(12 + strlen(__files->name));
			sprintf(fullFileName,"%s/%s",MUSIC_PATH,__files->name);
			return fullFileName;
		}
		deep++;
		return getFileName(__files->next, __pos);
	}
}

struct MusicFiles *getMusicFileById(struct MusicFiles *__files, int __id){
	if(__files != NULL){
		if(__id == __files->id){
			return __files;
		}
		return getMusicFileById(__files->next, __id);
	}
    return NULL;
}


//###########################################################################################################################
void getFilesNameFromDir(music_t __m,char *__pDir){
	DIR *dir = opendir (__pDir);
	//struct MusicFiles *head = NULL, *curr;
	struct dirent *pent = NULL;
	 while (pent = readdir (dir)){
		if (pent->d_name[0] == '.')continue;
		if(__m->head !=NULL){
            __m->tail = addNewFile(__m->tail, pent->d_name, __pDir);
        }
        else {
            musicId = 0;
			__m->tail = __m->head = addNewFile(__m->head,pent->d_name, __pDir);
		}
    }
}



//###########################################################################################################################
void sendMusicList(struct MusicFiles *__head){
	unsigned char __buff[1400];
    int16_t size;
    //memset(__buff,'\0',1400);
	__buff[0] = 210;
	if(__head !=NULL){
	    sprintf(&__buff[1],"{\"id\":\"%d\",\"title\":\"%s\",\"artist\":\"%s\",\"album\":\"%s\",\"duration\":\"%d\",\"year\":\"%s\"}",__head->id,__head->title,__head->artist,__head->album,__head->duration,__head->year);
		size = strlen(__buff);
        //printf("JSON : %s\n",__buff);
        sendto(sock,__buff,size,0, (struct sockaddr *)&client, sizeof(client));
	    return sendMusicList(__head->next);
    }
}

//###########################################################################################################################
void sendMusic(struct MusicFiles *__head){
	unsigned char __buff[1400];
    int16_t size;
	__buff[0] = 210;
	if(__head !=NULL){
	    sprintf(&__buff[1],"{\"id\":\"%d\",\"title\":\"%s\",\"artist\":\"%s\",\"album\":\"%s\",\"duration\":\"%d\",\"year\":\"%s\"}",__head->id,__head->title,__head->artist,__head->album,__head->duration,__head->year);
		size = strlen(__buff);
        sendto(sock,__buff,size,0, (struct sockaddr *)&client, sizeof(client));
    }
}

//###########################################################################################################################
uint8_t getFormat(char *__file){
    switch(__file[strlen(__file)-1]){
		case '3':
            return 1;
		case 'v':
		    return 2;
        default:
            return 0;
	}

}


//###########################################################################################################################


music_t music_create(){
    music_t m;
    m = malloc (sizeof (struct music_struct));
    if (m == NULL)exit(-1);
    m->head = m->tail = NULL;
    return m;
}

