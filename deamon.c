#include "main.h"

int main (int argc, char *argv[]){
	FILE*	pidfile;
	int	pid;
	readAttr(argc,argv);
	switch(args.mode){
		case 1:
			pid=fork();
			switch(pid){
				case 0:
					setsid();
					chdir("/");
					close(STDIN_FILENO);
					close(STDOUT_FILENO);
					close(STDERR_FILENO);
					start();
					_exit(0);
				break;
				case -1:
					printf("Error starting program\n");
				break;
				default:
					//pidfile=fopen("/var/run/gpsmon-mod5.pid","w");
					//fprintf(pidfile,"%d",pid);
					printf("The program was started  '%d'\n",pid);
					sleep(1);
					//fclose(pidfile);
				break;
			}
		break;
		case 0:
			//pidfile=fopen("/var/run/gpsmon-mod5.pid","w");
			//fprintf(pidfile,"%d",getpid());
			printf("PID %d\n", getpid());
            start();
		break;

	}
return 0;
}
