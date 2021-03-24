#include <stdio.h>
#include <unistd.h>
#include <pthread.h> //Consider this !!
#include "ht_timer.h"
#include "ht_logger.h"
//DEBUG
#include <assert.h>

extern void debugnode( FILE *fd );
void handler_hygge( size_t timer_id, void * user_data);
void  daemon_create(void);
int main( void ) {
	int i;
	char buf[100];
	FILE *log;
	daemon_create();
	//Child running here
	//ht_log_init("Henriks IOT", LOG_DEBUG, LOG_DAEMON);
	ht_log_init("Henriks IOT", LOG_INFO, LOG_DAEMON);
	log_info("Program Started\n");
	log = fopen("/tmp/htiot.log", "w");
	if (log == NULL) {
		printf("Could not open file\n");
		return(0);
	}
	fprintf(log, "Creating logfile\n");
	fflush(log);

	ht_timer_init(512);
	ht_timer_create(10000, handler_hygge, PERIODIC, "I am periodic");
	i=1;
	while( 1 ) {
		snprintf(buf, 100, "I am single number %d",i);
		ht_timer_create(100, handler_hygge, SINGLE_SHOT, buf);
		sleep(1);
		i++;
		if (i % 10 == 0) 
			debugnode( log );
	}
}

void  daemon_create(void) {
    pid_t pid;

    pid = fork();
    if(pid < 0){
        log_err("Error in fork: %m");
        exit(1);
    }
    if(pid > 0){
        exit(0);
    }
    if(setsid() < 0){
        log_err("Error in setsid: %m");
        exit(1);
    }
	if (   freopen("/dev/null", "r", stdin) == NULL
    	|| freopen("/dev/null", "w", stdout) == NULL
    	|| freopen("/dev/null", "w", stderr) == NULL ) {
    		log_err("When creating daemon freopen from STDIN/OUT/ERR failed: %m");
    }
	ht_log_to(1,0);	// Log to syslog only when in daemon mode
	
}

void handler_hygge( size_t timer_id, void * user_data) {
	pthread_t id;
	static int i = 0;
	id = pthread_self();
	(void) timer_id;

	log_info("handler_hygge count %d message: %s", ++i, (char *) user_data);
}

