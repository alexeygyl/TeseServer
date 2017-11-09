#include "main.h"


static int watched_items;

int open_inotify_fd (){
    int fd;
    watched_items = 0;
    fd = inotify_init ();
    if (fd < 0){
        perror ("inotify_init () = ");
    }
return fd;
}


int close_inotify_fd (int fd){
    int r;
    if ((r = close (fd)) < 0){
        perror ("close (fd) = ");
    }
    watched_items = 0;
return r;
}

void handle_event(queue_entry_t event){
    char *cur_event_filename = NULL;
    char *cur_event_file_or_dir = NULL;
    int cur_event_wd = event->inot_ev.wd;
    int cur_event_cookie = event->inot_ev.cookie;
    unsigned long flags;
    if(event->inot_ev.len){
        cur_event_filename = event->inot_ev.name;
    }
    if( event->inot_ev.mask & IN_ISDIR ){
        return;
        //cur_event_file_or_dir = "Dir";
    }
    //else {
    //    cur_event_file_or_dir = "File";
    //}
    
    flags = event->inot_ev.mask & ~(IN_ALL_EVENTS | IN_UNMOUNT | IN_Q_OVERFLOW | IN_IGNORED );
    
    switch (event->inot_ev.mask & (IN_ALL_EVENTS | IN_UNMOUNT | IN_Q_OVERFLOW | IN_IGNORED)){
        case IN_DELETE:
            printf ("DELETE: %s \"%s\" on WD #%i\n",cur_event_file_or_dir, cur_event_filename, cur_event_wd);
        break;
        case IN_CREATE:
            printf ("CREATE: %s \"%s\" on WD #%i\n",cur_event_file_or_dir, cur_event_filename, cur_event_wd);
            fileList->tail = addNewFile(fileList->tail,cur_event_filename,MUSIC_PATH);
            sendMusic(fileList->tail);
        break;
        default:
            printf ("UNKNOWN EVENT \"%X\" OCCURRED for file \"%s\" on WD #%i\n",event->inot_ev.mask, cur_event_filename, cur_event_wd);
        break;
    }
    
    if (flags & (~IN_ISDIR)){
        flags = event->inot_ev.mask;
        printf ("Flags=%lX\n", flags);
    }
}

void handle_events (queue_t q){
    queue_entry_t event;
    while (!queue_empty (q)){
        event = queue_dequeue (q);
        handle_event (event);
        free (event);
    }
}

int read_events (queue_t q, int fd){
    char buffer[16384];
    size_t buffer_i;
    struct inotify_event *pevent;
    queue_entry_t event;
    ssize_t r;
    size_t event_size, q_event_size;
    int count = 0;

    r = read (fd, buffer, 16384);
    if (r <= 0)
    return r;
    buffer_i = 0;
    while (buffer_i < r){
        /* Parse events and queue them. */
        pevent = (struct inotify_event *) &buffer[buffer_i];
        event_size =  offsetof (struct inotify_event, name) + pevent->len;
        q_event_size = offsetof (struct queue_entry, inot_ev.name) + pevent->len;
        event = malloc (q_event_size);
        memmove (&(event->inot_ev), pevent, event_size);
        queue_enqueue (event, q);
        buffer_i += event_size;
        count++;
    }
    //printf ("\n%d events queued\n", count);
return count;
}

int event_check (int fd){
    fd_set rfds;
    FD_ZERO (&rfds);
    FD_SET (fd, &rfds);
return select (FD_SETSIZE, &rfds, NULL, NULL, NULL);
}

int process_inotify_events (queue_t q, int fd){
    while(keep_running && (watched_items > 0)){
        if(event_check (fd) > 0){
	        int r;
	        r = read_events (q, fd);
	        if (r < 0){
	            break;
            }else{
	            handle_events (q);
	        }
	    }
    }
  return 0;
}


int watch_dir (int fd, const char *dirname, unsigned long mask){
    int wd;
    wd = inotify_add_watch (fd, dirname, mask);
    if (wd < 0){
        printf ("Cannot add watch for \"%s\" with event mask %lX", dirname,mask);
        fflush (stdout);
        perror (" ");
    }else{
        watched_items++;
        //printf ("Watching %s WD=%d\n", dirname, wd);
        //printf ("Watching = %d items\n", watched_items); 
    }
return wd;
}


int ignore_wd (int fd, int wd){
    int r;
    r = inotify_rm_watch (fd, wd);
    if (r < 0){
        perror ("inotify_rm_watch(fd, wd) = ");
    }else{
        watched_items--;
    }
return r;
}

int queue_empty (queue_t q){
    return q->head == NULL;
}

queue_t queue_create(){
    queue_t q;
    q = malloc (sizeof (struct queue_struct));
    if (q == NULL)exit(-1);
    q->head = q->tail = NULL;
    return q;
}

void queue_destroy (queue_t q){
    if(q != NULL){
        while (q->head != NULL){
            queue_entry_t next = q->head;
            q->head = next->next_ptr;
            next->next_ptr = NULL;
            free (next);
        }
        q->head = q->tail = NULL;
        free (q);
    }
}

void queue_enqueue (queue_entry_t d, queue_t q){
    d->next_ptr = NULL;
    if (q->tail){
        q->tail->next_ptr = d;
        q->tail = d;
    }
    else{
        q->head = q->tail = d;
    }
}

queue_entry_t  queue_dequeue (queue_t q){
    queue_entry_t first = q->head;
    if (first){
        q->head = first->next_ptr;
        if (q->head == NULL){
            q->tail = NULL;
        }
        first->next_ptr = NULL;
    }
return first;
}

