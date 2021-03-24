
// A Thread-safe application logger/debug message module for daemon or console mode
#ifndef SYS_SYSLOG_H
#include <syslog.h>
#endif
#ifndef HT_LOGGER_H
#define HT_LOGGER_H
/*
  printf family of functions in Linux support "%m" conversion specifier - others dont.
  uncomment NO_PERCENT_M_SUPPORT below to implement functionality here
*/
//#define NO_PERCENT_M_SUPPORT
extern int ht_log_level_get( void );
extern void ht_log_level_set( int level );
extern void ht_log_init( const char *ident, int level, int facility );
extern void ht_log_to( int syslog, int terminal);
//ht_log prototype with printf argument check
void ht_log(int level, const char *, ...)
    __attribute__((__format__(__printf__,2,3)));
//Macros for easy log facility in user code
//////////////__extension__ // To silence gcc -Wpedantic (ISO C does not permit named variadic macros)
// had som undefined behavior with the follwing approach when no arguments beside the format text.
//  #define log_emerg(x,...) __extension__ ht_log(LOG_EMERG,x, ##__VA_ARGS__) 
__extension__
#define log_emerg(printf_syntax...) \
	__extension__ ht_log(LOG_EMERG,printf_syntax) 		// Syslog 0 (Highest severity)
#define log_alert(printf_syntax...) \
	__extension__ ht_log(LOG_ALERT,printf_syntax)	 	// Syslog 1
#define log_crit(printf_syntax...) \
	__extension__ ht_log(LOG_CRIT,printf_syntax) 		// Syslog 2
#define log_err(printf_syntax...)  \
	__extension__ ht_log(LOG_ERR, printf_syntax) 		// Syslog 3
#define log_warning(printf_syntax...) \
	__extension__ ht_log(LOG_WARNING, printf_syntax)	// Syslog 4
#define log_notice(printf_syntax...) \
	__extension__ ht_log(LOG_NOTICE,printf_syntax)		// Syslog 5
#define log_info( printf_syntax...) \
	__extension__ ht_log(LOG_INFO, printf_syntax)  		// Syslog 6
#define log_debug(printf_syntax...) \
	__extension__ ht_log(LOG_DEBUG,printf_syntax) 		// Syslog 7 (Lowest severity)
#endif

