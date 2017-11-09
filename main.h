#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <math.h>
#include <sys/un.h>
#include <fcntl.h>
#include <mpg123.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/inotify.h>
#include <stddef.h>
#include <sys/inotify.h>

#include <alsa/asoundlib.h>

#define MUSIC_PATH "/home/music"

#define ARG "hDp:"
#define UNIX_SOCKET "/run/tese.sock"
#define	BUFFSIZE 1026
#define	BUFFSIZE_U 100
#define INIT_REQUEST 0x01                                                                                     
//#define INIT_RESPONCE 0x02                                                                                    
#define OFFSET_RESPONCE 0x03                                                                                  
#define OFFSET_REQUEST 0x04
#define RTT_RESPONCE 0x05                                                                                  
#define RTT_REQUEST 0x06
#define CLOCK_SYNC_REQUEST 0x10
#define CLOCK_SYNC_RESPONCE 0x11
#define LOST_PACKETS 0x12
#define BROADCAST 0xff
#define TRUE 1
#define FALSE 0
#define YES 1
#define NO 0
#define ERROR -1


#define PREPARE 0xa3
#define PLAY 0xa0
#define STOP 0xa1
#define PAUSE 0xa2
#define VOLUME 0xa4
#define SET 0xa5
#define DATA 0xa9
#define SKEW 0xa8

#define STATUS_NONE 0
#define STATUS_OFFSET 1
#define STATUS_DATA 2

#define PLAY_S 200
#define STOP_S 201
#define PAUSE_S 202
#define VOLUME_S 203
#define SET_S 204

#define MUSIC_LIST 210

#define DURATION 230

#define RESEND_PACKET_DELAY 200000
#define COLUNSIZE 8
#define START_SYNC 0xff
#define OFFSET_REQUEST_COUNT 100
#define RTT_REQUEST_COUNT 255
#define	SKEW_COUNT 50
#define OFFSET_REQUEST_TIMEOUT 65000
#define RTT_REQUEST_TIMEOUT 65000
#define DELAY 10
#define MAX_RTT 10000
#define CLOCK_SYNC_COUNT 10
#define COLUNS_OFFLINE_TIMEOUT 10
#define COLUNS_STOP_SERVICE_TIMEOUT 5
#define MAX_COLUNS 7


#define UNKNOWN 0
#define MP3 1
#define WAV 2

#pragma pack(push, 1)

//tmp variable for debug
int8_t	d1,d2;

struct args{
	uint16_t	port;
	uint8_t		mode;
}args;

typedef struct{
	uint8_t		status;
	uint32_t	seq;
	struct	timeval	time;
}Request_offset;

struct SkewData{
	uint64_t	sec_master;
	struct timeval	offset;
	struct SkewData	*next;
};

struct RTTGausa{
	uint16_t	rtt;
	struct RTTGausa	*next;
};



struct Coluns{
    pthread_t           thrdMain;
    pthread_t           thrdData;
	struct sockaddr_in	sock;
	Request_offset		packet;
	uint32_t		    expected_p;
	uint16_t		    maxRTT;
	uint16_t		    minRTT;
	float			    actualSkew;
    float               actualSkewLR;
    int8_t              newSkew;
	struct SkewData		*skew;
	struct RTTGausa		*rttGausa;
	int8_t		    	channel;
	int32_t			    status;
	uint8_t 			lostS;
	uint32_t    		lostP[256];
    struct timeval    online;
    uint8_t             isPrepared:1;
    uint8_t             isReadyToPlay:1;
    uint8_t             isLost:6;
    struct Coluns       *next;
    struct Coluns       *pref;
};
struct Coluns *colunsHead, *colunsLast;



struct MusicFiles{
    uint32_t        id;
    char			*name;
    char            *pwd;
    char            *title;
	char            *artist;
    char            *album;
    char            *year;
    uint16_t        duration;
    uint8_t         format;
    uint32_t        rate;
    int32_t         size;
    int32_t         sizeTotal;
    uint16_t        channels;
    uint16_t        blockAlign;
	struct MusicFiles	*next;
};


struct music_struct{
    struct MusicFiles   *head;
    struct MusicFiles   *tail;
};
typedef struct music_struct *music_t;


#pragma pack (pop)

struct sockaddr_un	serv_unix;
int8_t			    colunsCount;
int32_t			    sock, client_len, sock_unix, fileSize;
struct sockaddr_in	client, broadcastHeartBeat;
unsigned char		buff[BUFFSIZE],request[5], buff_play[9],rttRequest[5],buff_skew[5], buff_duration[3];
int8_t			    bytes_r, isExit;
pthread_t		    thrd, action_thrd,skew_thrd, thrd_coluns_detect;
pthread_t           thrd_duration, thrd_for_start_play, thrd_for_watch_dir;
struct timeval		tstart,tend, timeToStart;
char			    isSendingData, isReadyToStart;
music_t         	fileList;
struct MusicFiles   *music;
uint32_t            musicId;
uint8_t	            playingStatus;
uint16_t            lastMusicPos, musicCurrentPosition;
//struct timeval	    colunOffset, timeMaster, timeSlave;
int keep_running;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct wavHead_t{
	int32_t		chunkId;// RIFF: 0x52494646 in big-endian 
	int32_t		chunkSize;
	int32_t		format;//WAVE: 0x57415645 in big-endian
	int32_t		subchunk1Id;//fmt: 0x666d7420 in big-endian
	int32_t		subchunk1Size;
	uint16_t	audioFormat;// if is not 1, than have some compression
	uint16_t	numChannels;// number of Channels
	uint32_t	sampleRate;// Sampling frequency
	uint32_t	byteRate;//A quantity of bytes per second
	uint16_t	blockAlign;// The quantity of byte per sample, including all channels
	uint16_t	bitsPerSample; // The quantity of bits per sample 
	int32_t		subchunk2Id;//data:0x64617461 inв big-endian
	int32_t		subchunk2Size;//Data size
};
struct wavHead_t	wavHead;

struct stereoSample_t{
	unsigned char	channelLeft[2];
	unsigned char	channelRight[2];
};

struct monoSamples_t{
	unsigned char	channel[2];
};

struct stereoSample_t	*samples;
struct monoSamples_t	*rightChannel, *leftChannel;


//---------------------------FUNCTIONS--------------------------------------------------
//void printTree(struct Coluns *__colun);
void readAttr(int _argc, char ** _argv);
void start(void);
struct Coluns *addNewColun(struct Coluns *__colun ,struct sockaddr_in *__colunAddr, int8_t __channel);
struct Coluns *getColunByAddr(struct Coluns *__colun ,struct sockaddr_in *__colunAddr);
int32_t getmsdiff(suseconds_t *__usec1, suseconds_t *__usec2);
int8_t isReadyToSendSkew();
void timer(struct timeval *__time);
void changeTime(struct timeval *__time, int32_t sec, int32_t usec);
//---------------------SOCKET_FUNCTIONS------------------------------------------------
void createUDPSocket(int32_t *__sock, uint16_t  *__port);
void createUNIXSocket();
void initBroadcastMessage(int *__sock);

//---------------------TREAHD_FUNCTIONS-------------------------------------------------
void *thrd_func_for_offset(void *__colun);
void *thrd_func_for_send_data(void *__colun);
void *action_thrd_func();
void *thrd_func_broadcast_coluns_detect();
void *thrd_func_for_send_skew();
void *thrd_func_send_second_to_client();
void *thrd_func_start_play_when_is_ready();
void *thrd_func_for_watch_dir();
//--------------------OFFSET AND SKEW FUNCTIONS-----------------------------------------
struct timeval getLastOffset(struct SkewData *__skew);
//float getSkewDelta(struct Coluns *__colun, float *__skew);
float getSkewLRDelta(struct Coluns *__colun, float *__skew);
void printSkewMead(struct SkewData *__skew);
float calculateSkewTime(struct SkewData *__skew);
float calculateSkewLR(struct SkewData *__skew);
int8_t getOffsetFromColun(struct Coluns *__colun, uint64_t *__sec, struct timeval *__offset);
struct SkewData *addNewNode(struct SkewData *__current, uint64_t *__sec, struct timeval *__offset);
struct SkewData *deleteFirstNode(struct SkewData *__head);

//--------------------INTERVAL FUNCTIONS-----------------------------------------------
uint16_t getRTT(struct Coluns *__colun, uint16_t *__min, uint16_t *__max);
struct RTTGausa *addNewNodeRTTGausa(struct RTTGausa *__current, uint16_t __rtt);
uint16_t RTT_MMM(struct RTTGausa *__head, uint16_t __min, uint16_t __max);
uint32_t rttGausaAverage(struct RTTGausa *__head, uint16_t __min, uint16_t __max, uint8_t __status);
uint32_t rttGausaDesvision(struct RTTGausa *__head, uint32_t __avr, uint16_t __min, uint16_t __max, uint8_t __status);
void rttGausaEleminateTree(struct RTTGausa *__head);
void getRTTInterval(struct RTTGausa *__head, uint16_t *__min, uint16_t *__max, uint16_t __count);

//--------------------MUSICS FUNCTIONS---------------------------------------------------
music_t music_create();
void sendMusicList(struct MusicFiles *__head);
void sendMusic(struct MusicFiles *__head);
char *getFileName(struct MusicFiles *__files, int __pos);
struct MusicFiles *getMusicFileById(struct MusicFiles *__files, int __pos);
struct MusicFiles *addNewFile(struct MusicFiles *__head, char *__fileName, char *__pwd );
void getFilesNameFromDir(music_t __m,char *__pDir);
void parseMetaMP3(struct MusicFiles *__newNode, char *__fullName, char *__pwd);
uint8_t getFormat(char *__file);
void startPlay();
void stopPlay();
void pausePlay();
void setPlay(uint16_t __pos);
int8_t readWavHead(int32_t __fd, struct wavHead_t *__wavHead);
struct stereoSample_t * parseMP3File(struct wavHead_t *__wavHead, struct MusicFiles *__music);
struct stereoSample_t * parseWAVFile(struct wavHead_t *__wavHead, unsigned char *__fileName);
void getLeftRightChannels(struct stereoSample_t *__samples, struct monoSamples_t *__right, struct monoSamples_t *__left, struct MusicFiles *__music);
int8_t  isReadyToPlay();


//--------------------QUEUE FUNCTIONS---------------------------------------------------
#ifndef __EVENT_QUEUE_H
#define __EVENT_QUEUE_H

struct queue_entry;

struct queue_entry{
    struct queue_entry *next_ptr;
    struct inotify_event inot_ev;
};

typedef struct queue_entry *queue_entry_t;


struct queue_struct{
    struct queue_entry *head;
    struct queue_entry *tail;
};

typedef struct queue_struct *queue_t;

int queue_empty(queue_t q);
queue_t queue_create();
void queue_destroy(queue_t q);
void queue_enqueue(queue_entry_t d, queue_t q);
queue_entry_t queue_dequeue (queue_t q);

#endif
//--------------------INOTIFY FUNCTIONS---------------------------------------------------
#ifndef __INOTIFY_UTILS_H
#define __INOTIFY_UTILS_H


void handle_event (queue_entry_t event);
int read_event (int fd, struct inotify_event *event);
int event_check (int fd);
int process_inotify_events (queue_t q, int fd);
int watch_dir (int fd, const char *dirname, unsigned long mask);
int ignore_wd (int fd, int wd);
int close_inotify_fd (int fd);
int open_inotify_fd ();


#endif
