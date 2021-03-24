/*ht_timer.h*/
#ifndef HT_TIMER_H
#define HT_TIMER_H
#include <stdlib.h>
//#include <pthread.h>

typedef void (*timer_callback)(size_t timer_id, void * user_data);
typedef enum
{
TIMER_SINGLE_SHOT = 0, /*Periodic Timer*/
TIMER_PERIODIC         /*Single Shot Timer*/
} t_timer;
//timer_node - household information for each timer created
//HeTH: Ulogisk de hedder timer_ når de andre ikke gør
struct timer_node {
    int                 fd;         // filedescriptor handle to timerfd
	timer_callback      callback;   // function to be called at elapsed time
	void                *user_data; // Pointer to data delivered to callback
	unsigned int        interval;   // Time in milliseconds - ms
	unsigned long		timer_tick;	// Number of times timer trigged.
	unsigned long		timer_overrun;	// Number of times callback method still running
										// when timer trigs. 
	unsigned long		callback_runs;	// Number of times callback method has run
	int                 run_state;  // Worker pthread state - see t_run_state
	int                 ctl_state;  // Control state - writeable by control components
	t_timer             type;       // Type of timer - single-shot or continiuos
	pthread_t           pthread;   // Each timer has its own pthread
	struct timer_node   *prev;      // Pointer to previous item in linked list
	struct timer_node   *next;      // Pointer to next item in linked list
};
typedef struct timer_node timer_node;


extern int ht_timer_initialize(unsigned int maxtimers);
extern timer_node *ht_timer_create(unsigned int interval, timer_callback handler, t_timer type, void * user_data);
extern int ht_timer_delete(timer_node *timer_node_pointer);

#endif

