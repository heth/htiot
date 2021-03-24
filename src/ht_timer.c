#define _GNU_SOURCE
#include <stdint.h>
#include <string.h>
#include <sys/timerfd.h>
#include <pthread.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "ht_timer.h"
#include "ht_logger.h"


//#### Definitions
typedef enum { // Possible states in timer_node.run_state
	RUNNING     = 0x1,  // Set by _timer_tread while callback pthread running
	STOPPED     = 0x2   // Set by callback
} t_run_state;

typedef enum { // Possible states in timer_node.ctl_state
	ACTIVE      = 0x4,  // _timer_thread treats timer as active
	SUSPENDED   = 0x8,  // _timer_thread suspends timer while in this state
	DELETION    = 0x10, // timer_node entry set for deletion by _timer_thread_
} t_ctl_state;

//#### Module static 
//static struct module_static {
struct module_static {
	unsigned int max_timers;		// Maximum number of active timers
	int active_timers;	// Number of currently active timers
	int fdev;			// Eventfd to Notify a change in the timer_node linked list
	timer_node *node_head; 		// Head of linked list of timer_node's
	pthread_t main_thread_id;	// Thread id of main thread
	pthread_mutex_t timer_node_mutex;
};
/*static*/ struct module_static ms;

// Prototypes
static void * main_thread(void *data);
static void insert_node( timer_node *node);
static int check_node_exist(timer_node *check_node);

//#### Functions
// ht_timer_initialize:
// Input:
//	maxtimers - maximum number of allowed timers.
//	If set to 0 (zero)  half the allowable number of open files is used. See "ulimit -n" 
// Returns:
//  Max number of active timers. 0 on error
int ht_timer_init( unsigned int maxtimers ) {
	struct rlimit rl;	// Ressource limit structue - see getrlimit() man page
	int retcode;
	if (getrlimit( RLIMIT_NOFILE, &rl) != 0) {
		retcode = errno;
		log_warning("Call to getrtlimit() failed: %m");
		exit(retcode);
	}
	if ( maxtimers == 0 ) {
		ms.max_timers = rl.rlim_cur / 2;
	} else {
		if (maxtimers >= rl.rlim_cur - 2) {
			log_warning("Maxtimers to high %d - max open files %ld (See ulimit -n)",
				maxtimers, rl.rlim_cur);
		}
		ms.max_timers = maxtimers;
	}
	ms.active_timers = 0;

	if (pthread_mutex_init(&ms.timer_node_mutex, NULL) != 0) {
		retcode = errno;
		log_err("Call to ptread_mutex_init() failed: %m" );
		exit(retcode);
	}

	if ((ms.fdev = eventfd(0,0)) == -1) {
		retcode = errno;
		log_err("Call to eventfd() failed: %m");
		exit(retcode);
	}

	ms.node_head = NULL;
	if ((retcode=pthread_create( &ms.main_thread_id, NULL, main_thread, NULL))) {
		log_err("Unable to create main thread: %m");
		exit(retcode);
	}
	return(ms.max_timers);
}

void debugnode( FILE *fd ); //DEBUG
// ht_timer_create:
// Input:
//  interval  : Interval in milliseconds between handler is run
//  callback  : Function to be called when timer trigges
//  type      : SINGLE_SHOT or PERIODIC
//  user_data : A void pointer to a user-specific data structure
// Returns:
//  A timer id as a size_t and NULL if failed to create timer
timer_node *ht_timer_create(interval, callback, type, user_data) 
unsigned int interval;
timer_callback callback;
t_timer type;
void * user_data; 
{
	struct itimerspec timer_values;
	timer_node * node = NULL;

	node = (timer_node *)malloc(sizeof(timer_node));
	if ( node == NULL ) {
		log_err("Unable to allocate memory with malloc when creating a new timer: %m");
		return(NULL);
	}
	node->callback = callback;
	node->user_data = user_data;
	node->interval = interval;
	node->type = type;
	node->fd = -1; // Non-existing file descriptor
	node->run_state = STOPPED;
	node->ctl_state = ACTIVE;
	node->timer_tick = 0;
	node->timer_overrun = 0;
	node->callback_runs = 0;
	node->fd = timerfd_create(CLOCK_REALTIME,0);
	if (node->fd == -1) {
		log_err("Unable to open timerfd handle when creating a new timer: %m");
		free(node);
		return(NULL);
	}

	timer_values.it_value.tv_sec = node->interval / 1000;
	timer_values.it_value.tv_nsec = (node->interval % 1000) * 1000000;

	if (type == PERIODIC) {
		timer_values.it_interval.tv_sec = node->interval / 1000;
		timer_values.it_interval.tv_nsec = (node->interval % 1000) * 1000000;
	} else {
		timer_values.it_interval.tv_sec = 0;
		timer_values.it_interval.tv_nsec = 0;
	}
	if (timerfd_settime(node->fd, 0, &timer_values, NULL) == -1) { 
		log_err("Unable to settime on timerfd handle when creating a new timer: %m");
		close(node->fd);
		free(node);
		return(NULL);
	}
	if ( pthread_mutex_lock(&ms.timer_node_mutex)) {
		log_err("ht_timer_create: Mutex lock failed: %m");
		return(NULL);
	}

	insert_node( node );
	if ( pthread_mutex_unlock(&ms.timer_node_mutex)) {
		log_err("ht_timer_create: Mutex unlock failed");
		return(NULL);
	}
	// Signal to main_thread that a new timer in node_head list 
	if ( write(ms.fdev,&(uint64_t) {1}, sizeof(uint64_t)) != sizeof(uint64_t) ) {
		log_err("ht_timer_create: unable to write to eventfd: %d",8);
		free(node);
		return(NULL);
	}
	return(node);
}

int ht_timer_delete(timer_node *node) {

	if (check_node_exist(node)) {
		log_err("ht_timer_delete: Attempt to delete non-existing timer");
		return(EXIT_FAILURE);
	}

	if ( pthread_mutex_lock(&ms.timer_node_mutex)) {
		log_err("ht_timer_delete: Mutex lock failed");
		return(EXIT_FAILURE);
	}

	node->ctl_state = DELETION;

	if ( write(ms.fdev,&(uint64_t) {1}, sizeof(uint64_t)) != sizeof(uint64_t) ) {
		log_err("Error in delete timer: %m");
		pthread_mutex_unlock(&ms.timer_node_mutex);
		return(EXIT_FAILURE);
	}
	if ( pthread_mutex_unlock(&ms.timer_node_mutex)) {
		log_err("ht_timer_delete: Mutex unlock failed");
		return(EXIT_FAILURE);
	}
	return(EXIT_SUCCESS);

}

static void insert_node(timer_node *node) {
	timer_node *tmp_node;
	if (ms.node_head == NULL) { // If first timer created
		node->next = NULL;
		node->prev = NULL;
		ms.node_head = node;
		goto exit_ok;
	}
	tmp_node = ms.node_head;
			
	while (tmp_node->next != NULL && tmp_node->interval < node->interval)
		tmp_node = tmp_node->next;

	//Exception when new node is the last node (Highest interval)
	if (tmp_node->next == NULL && tmp_node->interval < node->interval) {
		tmp_node->next = node;
		node->prev = tmp_node;
		node->next = NULL;
		goto exit_ok;
	}

	// Insert node in list
	node->prev = tmp_node->prev;
	tmp_node->prev = node;
	node->next = tmp_node;
	if (node->prev != NULL) 
		node->prev->next = node;
	if ( tmp_node == ms.node_head )
		ms.node_head = node;

exit_ok: return; 
}

static int check_node_exist(timer_node *check_node) {
	timer_node *node;
	node = ms.node_head;

	while (node) {
		if (node == check_node) {
			return(EXIT_SUCCESS);
		}
		node = node->next;
	}
	return(EXIT_FAILURE);
}
// HETH DEBUG
void debugnode( FILE *fd ) {
	timer_node *node;
	int i = 0;
	node = ms.node_head;
	fprintf(fd, "-------------------------------------------------------------------------------------\n");
	while ( node ) {
		fprintf(fd, "node: %3d (%10p) next: %10p prev: %10p interval: %10d overrun: %7ld tick: %7ld ctl: %d run: %d type: %d\n", ++i, (void *) node,
		(void *) node->next, (void *) node->prev, node->interval, node->timer_overrun, node->timer_tick, node->ctl_state, node->run_state, node->type);
		node = node->next;
	}
	fflush(fd);
}


//############## All code below here is runstate of module. Controlled by main_thread() 
//Prototypes
static int fdlist_prepare( struct pollfd *fdlist );
static timer_node * fd2node(int fd);
static void timer_node_delete(timer_node *node);
static void * worker_thread( void * data);

// main_thread: Executes as a pthread running permanently while system is running
// and is responsible for starting worker_threads() to run the timers callback 
// functions at specified interval
static void * main_thread(void *data) {
	timer_node *node;
	struct pollfd *fdlist;	// To hold list of file-descriptors used by poll()
	uint64_t readbuf;
	int readsize;
	int retcode;
	int items, events, i;
	const int pollerrormax = 10;
	(void) data;	//Unused

	fdlist = (struct pollfd *) malloc(sizeof(struct pollfd) * ms.max_timers); 
	if ( fdlist == NULL ) {
		log_err("ht_timer main_thread failed to allocate memory with malloc: %m");
		fflush(stderr);
		pthread_exit(NULL);
	}

	// Prepare fdlist when started - only eventfd in list at this stage
	if ( pthread_mutex_lock(&ms.timer_node_mutex)) {
		log_err("Main_thread: Mutex lock failed");
		pthread_exit(NULL);
	}
	items = fdlist_prepare( fdlist );
	if ( pthread_mutex_unlock(&ms.timer_node_mutex)) {
		log_err("Main_thread: Mutex unlock failed");
		pthread_exit(NULL);
	}
	while( 1 ) {
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		pthread_testcancel(); // Never returns if thread is cancelled
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

		events = poll(fdlist, items, -1);	// Wait for timer events POLLIN
		if ( events == -1 ) { //DEBUG
			 log_err("ht_timer main_thread - poll() call failed: %m");
			 pthread_exit(NULL);
        }
			
		// Check fd's in fdlist to see which trigged the poll
		for ( i = 0; events > 0 ; i++ ) {	// Find timers with POLLIN flag set

			if ( fdlist[i].revents != POLLIN && fdlist[i].revents != 0 ) {
				log_err("revents not POLLIN fdlist[%i] = %hi", i, fdlist[i].revents);
				pthread_exit(NULL);
			}
			//Debugif ( fdlist[i].revents == POLLIN) {
			if ( fdlist[i].revents & POLLIN) {
				events--; //Event found
				// Read from fd to get event
				readsize = read(fdlist[i].fd, &readbuf, sizeof(uint64_t));
				// If data read wrong size - try next
				if (readsize != sizeof(uint64_t) ) {
					log_err("Wrong size read - readsize = %d: %m", readsize);
					continue;
				}
				if ( fdlist[i].fd == ms.fdev ) { // If event is from eventfd
					if ( pthread_mutex_lock(&ms.timer_node_mutex)) {
						log_err("Main_thread2: Mutex lock failed");
						pthread_exit(NULL);
					}
					items = fdlist_prepare(fdlist);
					if ( pthread_mutex_unlock(&ms.timer_node_mutex)) {
						log_err("Main_thread2: Mutex unlock failed");
						pthread_exit(NULL);
					}
					if (events != 0) {	// fdev ALWAYS last item!! Just precation - can be removed
						log_err("ht_timer main_thread - event counter mismatch!");
						pthread_exit(NULL);
					}
					continue;
				} 
				// Then it must be a timer
				if ( pthread_mutex_lock(&ms.timer_node_mutex)) {
					log_err("Main_thread3: Mutex lock failed");
					pthread_exit(NULL);
				}
				node = fd2node(fdlist[i].fd);
				if ( pthread_mutex_unlock(&ms.timer_node_mutex)) {
					log_err("Main_thread3: Mutex unlock failed");
					pthread_exit(NULL);
				}
				if (node == NULL) {	
					log_err("ht_timer main_thread - fd nonexistent in timer_node chain: fd %d", fdlist[i].fd);
					pthread_exit(NULL);
				}
				node->timer_tick++;
				switch ( node->ctl_state ) {
					case DELETION: // Disarm the timer. Removal in fdlist_prepare run
						break;
					case ACTIVE:
						//DEBUG_CODE
						if (node->run_state == RUNNING)  {
							node->timer_overrun++;
							continue;
						}
						if ( (retcode = pthread_create( &node->pthread, NULL, worker_thread, node)) != 0 ) {
							log_err("ERROR: Can't create pthread %m");
							// Continue operation despite pthread create error. 	
						} else {
							if ( (retcode = pthread_detach(node->pthread)) ) 
								log_err("ERROR: Can't detach pthread %m");
						}
						break;
					default:
						log_err("ht_timer main_thread - nonexisting node ctl_state: %d\n", node->ctl_state);
						pthread_exit(NULL);
						break;
				}
			}
		}
	}
}

static void worker_thread_cleanup(void *data) {
	timer_node *worker_node;
	worker_node = (timer_node *) data;
	if (worker_node->type == SINGLE_SHOT)	// Delete timer entry at next timerfd run
		worker_node->ctl_state = DELETION;
	worker_node->run_state = STOPPED;
}
		
static void * worker_thread(void * data) {
	timer_node *worker_node;
	worker_node = (timer_node *) data;
	
	pthread_cleanup_push(worker_thread_cleanup, data);
	worker_node->run_state = RUNNING;
	worker_node->callback_runs++;
	(*worker_node->callback)(worker_node->fd, worker_node->user_data);
	pthread_cleanup_pop(1);	// Pop and run pushed worker_thread_cleanup()
	pthread_exit(NULL);
}
//Prepare fdlist array with open files for poll() to catch events
// returns number of items in fdlist for poll to monitor
static int fdlist_prepare( struct pollfd *fdlist ) {
	int tim_cou; //Timer counter
	timer_node *node;

	node = ms.node_head; // Point at start of timer_node linked list
	tim_cou = 0;
	while (node) {
		if (node->ctl_state == DELETION && node->run_state == STOPPED) {
			timer_node_delete(node);
			tim_cou = 0;
			node = ms.node_head; //Start all over
			continue;
		}
		if (node->ctl_state == ACTIVE) { //DEBUG
			fdlist[tim_cou].fd = node->fd;
			fdlist[tim_cou].events = POLLIN;
			node = node->next;
			tim_cou++;
		}
	}
	//Add eventfd filehandle to list - used to trig poll when change in timer_node
	fdlist[tim_cou].fd = ms.fdev;
	fdlist[tim_cou].events = POLLIN;
	return(tim_cou + 1);
}

// Find and return timer_node with fd
static timer_node * fd2node(int fd) {
	timer_node *node = ms.node_head;

	while (node) {
		if (node->fd == fd) {
			if ( pthread_mutex_unlock(&ms.timer_node_mutex)) {
				log_err("fd2node: Mutex lock failed");
				//FATAL MAKE NICE EXIT exit;
			}
			return(node);
		}
		node = node->next;
	}
	return(NULL); // Failure if here: fd nonexistent
}

static void timer_node_delete(timer_node *node) {
	// Remove node from chain
	if (node->next == NULL && node->prev == NULL) {  // Last timer deleted
		ms.node_head = NULL;
		goto ok_exit;
	}
	if (node->prev != NULL) { 
		node->prev->next = node->next;
	} else { // First node in chain - must be at ms.node_head
		ms.node_head = node->next;
	}
	if (node->next != NULL)
		node->next->prev = node->prev;
ok_exit:
	if (node->fd >= 0) {
		close(node->fd);
	}
	free(node);
	return;
}
