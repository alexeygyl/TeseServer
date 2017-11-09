#include "main.h"




struct Coluns *getColunByAddr(struct Coluns *__colun ,struct sockaddr_in *__colunAddr){
	if(__colun !=NULL){
        if(__colun->sock.sin_addr.s_addr == __colunAddr->sin_addr.s_addr)return __colun;
        else return getColunByAddr(__colun->next,__colunAddr);
    }
    return NULL;
}

//###########################################################################################################################

struct Coluns *addNewColun(struct Coluns *__colun ,struct sockaddr_in *__colunAddr, int8_t __channel){
    struct Coluns *newColun = (struct Coluns *)malloc(sizeof(struct Coluns));
	newColun->sock = *__colunAddr;
	newColun->channel = __channel;
	newColun->skew = NULL;
	newColun->rttGausa = NULL;
	newColun->status = STATUS_NONE;
    newColun->isPrepared = NO;
    newColun->isReadyToPlay = NO;
    newColun->isLost = NO;
    gettimeofday(&newColun->online,NULL);
    colunsCount++;
    if(__colun!=NULL){
	    __colun->next = newColun;
        newColun->pref = __colun;
    }else{
        newColun->pref = NULL;
        newColun->next = NULL;
    }

    return newColun;
}


//###########################################################################################################################

int32_t getmsdiff(suseconds_t *__usec1, suseconds_t *__usec2){
	if(*__usec2 >= *__usec1)return *__usec2 - *__usec1;
	else return 1000000 - *__usec1 + *__usec2;
}


//###########################################################################################################################
uint16_t getRTT(struct Coluns *__colun, uint16_t *__min, uint16_t *__max){
    uint32_t		i;
	struct timeval		tp1,tp2;
	uint16_t		rtt,count = 0;
	uint16_t		pos, res[1000], max=0;
    int16_t         y;
	struct RTTGausa		*currentNode = NULL;
	memset(&res,'\0',1000*2);
	for(i = 1; i <= RTT_REQUEST_COUNT; i++){
		memcpy(&rttRequest[1],&i,4);
		gettimeofday(&tp1,NULL);
		sendto(sock,rttRequest,5,0, (struct sockaddr*)&__colun->sock, sizeof(__colun->sock));
        __colun->expected_p = i;
		__colun->packet.status = 0;
		gettimeofday(&tp2,NULL);
		while(getmsdiff(&tp1.tv_usec,&tp2.tv_usec) < RTT_REQUEST_TIMEOUT){
			if(__colun->packet.status == 1){
				__colun->packet.status = 0;
				if(i == __colun->packet.seq){
					rtt =  getmsdiff(&tp1.tv_usec,&tp2.tv_usec);
					count++;
                	//printf("\tRTT %d: Time 1 %d.%d, Time 2 %d.%d\n",rtt, tp1.tv_sec,tp1.tv_usec,tp2.tv_sec,tp2.tv_usec);
					if(__colun->rttGausa !=NULL)currentNode = addNewNodeRTTGausa(currentNode,rtt);
					else{	
					       	__colun->rttGausa = addNewNodeRTTGausa(__colun->rttGausa,rtt);
						currentNode = __colun->rttGausa;
					}

					pos = rtt/100;
					if(pos<1000)res[pos]++;
				    break;
                }

			}
			gettimeofday(&tp2,NULL);
		}
        
	}
    if(count == 0 )return 0;
    //for(y = 0; y < 1000; y++)if(res[y]!=0)printf(" RTT %d-%d : %d\n" ,y*100, y*100+99, res[y]);	    
    for(y = 0; y < 1000; y++){
		if(res[y]*100/count >=5){
			*__min = y*100;
			break;
		}
	}
	for(y = 999; y >=0; y--){
        if(res[y]*100/count >=5){
			*__max = y*100+99;
			break;
		}
	}
	return count;

}

//###########################################################################################################################
//---------------------------------------------------------------------------------------------------------------------------------------
struct RTTGausa *addNewNodeRTTGausa(struct RTTGausa *__current, uint16_t __rtt){
	struct RTTGausa *newNode = (struct RTTGausa *)malloc(sizeof(struct RTTGausa));
	newNode->rtt = __rtt;
	newNode->next = NULL;
	if(__current !=NULL){
		__current->next = newNode;
		return newNode;
	}
	else return newNode;
}

//###########################################################################################################################
uint16_t RTT_MMM(struct RTTGausa *__head, uint16_t __min, uint16_t __max){
	static uint32_t	total = 0, max = 0, min = RTT_REQUEST_TIMEOUT, count = 0;
	uint16_t	out;
	while(__head !=NULL){
		if(__head->rtt > __min && __head->rtt < __max){
			max = max > __head->rtt?max:__head->rtt;
			min = min < __head->rtt?min:__head->rtt;
			total += __head->rtt;
			count ++;
		}
		return RTT_MMM(__head->next, __min, __max);
	}
	out = count;
    
	//printf("Mead = %d/%d = %d, Range = %d-%d \n",total,count, total/count, __min, __max);
	total = 0;
	max = 0;
	min = RTT_REQUEST_TIMEOUT;
	count = 0;
	return out;
}

//###########################################################################################################################
//---------------------------------------------------------------------------------------------------------------------------------------
uint32_t rttGausaAverage(struct RTTGausa *__head, uint16_t __min, uint16_t __max, uint8_t __status){
    static uint32_t	total,count;
	if(__status == 0 ){
		total = 0;
		count = 0;
	}
	while(__head != NULL){	
		if(__head->rtt>__min && __head->rtt < __max){
			total += __head->rtt;
			count ++;
		}
		return rttGausaAverage(__head->next, __min, __max, 1);
	}
	return total/count;
}
//###########################################################################################################################
//---------------------------------------------------------------------------------------------------------------------------------------
uint32_t rttGausaDesvision(struct RTTGausa *__head, uint32_t __avr, uint16_t __min, uint16_t __max, uint8_t __status){
    static double	total, count;
	if(__status == 0 ){
		total = 0;
		count = 0;
	}
	while(__head != NULL){
		if(__head->rtt > __min && __head->rtt < __max){
			total += (__head->rtt - __avr)*(__head->rtt - __avr);
			count ++;
		}
		return rttGausaDesvision(__head->next, __avr, __min, __max,1);
	}
	return (uint32_t)sqrt(total/count);
}

//###########################################################################################################################
void getRTTInterval(struct RTTGausa *__head, uint16_t *__min, uint16_t *__max, uint16_t __count){
    int i;
	uint32_t	rttAverage, rttDesvision;
	uint16_t	currRTTcount;
	for(i = 0; i < 10;i++){
		rttAverage = rttGausaAverage(__head, *__min, *__max,0);
		rttDesvision = rttGausaDesvision(__head,rttAverage ,*__min, *__max,0);
        
        if(rttAverage <=rttDesvision*1.5)*__min=0;
        else *__min = rttAverage - rttDesvision*1.5;
        *__max = rttAverage + rttDesvision*1.5;
		
        currRTTcount = RTT_MMM(__head, *__min, *__max);
		if(currRTTcount*100/__count < 60)break;
	}

}

//###########################################################################################################################

int8_t getoffset(struct timeval *__master, struct timeval *__slave, struct timeval *__offset,uint16_t *__rtt){	
    __offset->tv_sec = __master->tv_sec - __slave->tv_sec;
	__offset->tv_usec = __master->tv_usec - (__slave->tv_usec - *__rtt/2);
    if((int)__offset->tv_usec < 0){
		__offset->tv_usec += 1000000;
		__offset->tv_sec--;
		return 1;
	}
	return 0;
}

//###########################################################################################################################

int8_t getOffsetFromColun(struct Coluns *__colun, uint64_t *__sec, struct timeval *__offset){
	uint32_t		    total_offset[2], min[2], max[2], i, minN,maxN, totalN;
	uint8_t			    count_offset[2], type, countN;
	struct timeval		tp1,tp2,tv_offset;
	uint16_t		    rtt;
	time_t			    sec[2];
    
	
    minN = min[0] = min[1] = 1000000;
	maxN = max[0] = max[1] = 0; 
	totalN = total_offset[0] = total_offset[1] = 0;
	countN = count_offset[0] = count_offset[1] = 0;	
	
    for(i = 1; i <= OFFSET_REQUEST_COUNT; i++){
		memcpy(&request[1],&i,4);
		gettimeofday(&tp1,NULL);
		sendto(sock,request,5,0, (struct sockaddr*)&__colun->sock, sizeof(__colun->sock));
		__colun->expected_p = i;
		__colun->packet.status = NO;
		gettimeofday(&tp2,NULL);
		while(getmsdiff(&tp1.tv_usec,&tp2.tv_usec) < OFFSET_REQUEST_TIMEOUT){
			if(__colun->packet.status == YES){		
				if(i == __colun->packet.seq){
					__colun->packet.status = NO;
					rtt =  getmsdiff(&tp1.tv_usec,&tp2.tv_usec);
					if(rtt < __colun->maxRTT && rtt > __colun->minRTT){
						type = getoffset(&tp1,&__colun->packet.time,&tv_offset,&rtt);
						sec[type] = tv_offset.tv_sec;
                        
                        if(minN > (int)tv_offset.tv_usec)minN = tv_offset.tv_usec;
                        if(maxN < (int)tv_offset.tv_usec)maxN = tv_offset.tv_usec;

                        totalN += tv_offset.tv_usec;
						countN++;
                       //printf("%d : %p:  RTT=%d: master=%d.%d, slave=%d.%d, Offset=%d.%d\n",i,__colun,rtt,(int)tp1.tv_sec,(int)tp1.tv_usec,(int)__colun->packet.time.tv_sec,(int) __colun->packet.time.tv_usec,(int)tv_offset.tv_sec,(int)tv_offset.tv_usec);
                    }
					break;
				}else{
					__colun->packet.status = 0;
				}
			}
			gettimeofday(&tp2,NULL);
		}
	}
    if(countN*100/OFFSET_REQUEST_COUNT< 20)return -1;
    if((maxN - minN) > (__colun->maxRTT - __colun->minRTT)*10) return -2;
	*__sec = tp1.tv_sec;
	__offset->tv_usec = (totalN - minN - maxN)/(countN -2);
	__offset->tv_sec = tv_offset.tv_sec; 
    return 1;

}



//###########################################################################################################################
/*
float getSkewDelta(struct Coluns *__colun, float *__skew){
	static float	total = 0.0, count = 0.0;
    float           skew;
	if(colunsCount <= 1)return 0.0;
    if(__colun != NULL){
		total+=__colun->actualSkew;
        count++;
        return getSkewDelta(__colun->next,__skew);
	}
    skew =  *__skew - total/count;	
    count = total = 0.0;
	return skew;
}
*/
//###########################################################################################################################
float getSkewLRDelta(struct  Coluns *__colun, float *__skew){
	static float	total = 0.0, count = 0.0;
    float           skew;
    if(colunsCount <= 1)return 0.0;
    if(__colun != NULL){
		total+=__colun->actualSkewLR;
        count++;
        return getSkewLRDelta(__colun->next,__skew);
	}
    
	skew = total/count - *__skew;	
    //printf("Total %.3f, count %.3f, __Skew %.3f, SKEW %.3f\n",total, count,*__skew,skew);
    count = total = 0.0;
	return skew;	
}

//###########################################################################################################################

float calculateSkewLR(struct SkewData *__skew){
    float                   a;
    float                   Ymead, Xmead, XYmead, XXmead;
    static int64_t          Ytotal, Xtotal, XYtotal, XXtotal;
    static uint8_t          count;
    static uint32_t         Xfirst;
    static struct timeval   Yfirst;
    if(Xfirst == 0 ){
        Xfirst    = __skew->sec_master;
        Yfirst  = __skew->offset;
        count       = 1;
        Ytotal = Xtotal = XYtotal = XXtotal = 0;
        if(__skew->next != NULL)return calculateSkewLR(__skew->next);
        return 0;
    }
   //printf("-------------------------------------------------------------------------\n"); 
    
    count++;
    Ytotal  += __skew->offset.tv_usec - Yfirst.tv_usec;
    Xtotal  += __skew->sec_master - Xfirst;
    XXtotal += (__skew->sec_master - Xfirst)*(__skew->sec_master - Xfirst);
    XYtotal += (__skew->sec_master - Xfirst)*( __skew->offset.tv_usec - Yfirst.tv_usec);
    
    if(__skew->next != NULL){
        return calculateSkewLR(__skew->next);
    }
    
    Xfirst = 0;
    //printf("Xtotal %d, Ytotal %d, XYtotal = %d, XXtotal %d\n",Xtotal,Ytotal,XYtotal,XXtotal);
    Ymead   = (float)Ytotal/(float)count;
    Xmead   = (float)Xtotal/(float)count;
    XYmead  = (float)XYtotal/(float)count;
    XXmead  = (float)XXtotal/(float)count;

    ///printf("sXmead %.3f, sYmead %.3f, sXYmead = %.3f, sXXmead %.3f\n",Xmead,Ymead,XYmead,XXmead);
    
    a = (XYmead - Xmead*Ymead)/(XXmead - Xmead*Xmead);
    return a;


}

//###########################################################################################################################
float calculateSkewTime(struct SkewData *__skew){
	float			tmp;
	static uint8_t		count;
	static float		min,max, total;
	static uint64_t		head_time;
	static struct timeval	head_offset;
	if(head_time == 0){
		head_time = __skew->sec_master;
		head_offset = __skew->offset;
		count = 1;
		min = 1000000;
		max = -1000000;
		total = 0;
		if(__skew->next!=NULL)return calculateSkewTime(__skew->next);
		return 0;
	}
	if(head_offset.tv_sec == __skew->offset.tv_sec){
		tmp = (float)(head_offset.tv_usec - __skew->offset.tv_usec) / (float)(__skew->sec_master - head_time );
	}
	else {
		if(head_offset.tv_usec < __skew->offset.tv_usec){
			tmp = (float)(1000000 + head_offset.tv_usec - __skew->offset.tv_usec) / (float)(__skew->sec_master - head_time );
		}
		else{
			tmp = (float)(head_offset.tv_usec - __skew->offset.tv_usec - 1000000) / (float)(__skew->sec_master - head_time );
		}
	}
	min = min > tmp ? tmp : min;
	max = max < tmp ? tmp : max;
	total += tmp;
	if(__skew->next != NULL){
		count++;
		return calculateSkewTime(__skew->next);
	}	
	head_time = 0;
	if(count>5) return (total - min - max)/(float)(count - 2);
	else return total/(float)count;
}

//###########################################################################################################################

struct timeval getLastOffset(struct SkewData *__skew){
	if(__skew->next != NULL){
		return getLastOffset(__skew->next);
	}
	else return __skew->offset;
}

//###########################################################################################################################

struct SkewData *addNewNode(struct SkewData *__current, uint64_t *__sec, struct timeval *__offset){
	struct SkewData	*newNode;
	newNode = (struct SkewData*)malloc(sizeof(struct SkewData));
	newNode->sec_master = *__sec;
	newNode->offset = *__offset;
	newNode->next = NULL;
	if(__current!=NULL){
		__current->next = newNode;
		return newNode;
	}
	else{
		return newNode;
	}
	
}

//###########################################################################################################################
struct SkewData *deleteFirstNode(struct SkewData *__head){
	struct SkewData *secondNode = __head->next;
    memset(__head,'\0',sizeof(struct SkewData));
	free(__head);
	return secondNode;
}

//###########################################################################################################################
//###########################################################################################################################

int8_t isReadyToSendSkew(struct Coluns *__colun){
        if(colunsCount == 0)return NO;
        if(__colun !=NULL){
            if(__colun->newSkew == 0)return NO;
            return isReadyToSendSkew(__colun->next);
        }
        return YES;
}

//###########################################################################################################################

void timer(struct timeval *__time){
		struct timeval tp;
		gettimeofday(&tp,NULL);
		while(__time->tv_sec > tp.tv_sec)gettimeofday(&tp,NULL);
		while(__time->tv_usec > tp.tv_usec)gettimeofday(&tp,NULL);
}

//###########################################################################################################################

void changeTime(struct timeval *__time, int32_t sec, int32_t usec){
    __time->tv_sec +=   sec;
    __time->tv_usec +=  usec;
    if((int)__time->tv_usec >= 1000000){
        __time->tv_sec++;
        __time->tv_usec -=1000000;
    }
}
