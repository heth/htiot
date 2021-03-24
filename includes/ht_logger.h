
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

//ht_log prototype with printf argument check
void ht_log(int level, const char *, ...)
    __attribute__((__format__(__printf__,2,3)));
//Macros for easy log facility in user code
//////////////__extension__ // To silence gcc -Wpedantic (ISO C does not permit named variadic macros)
// had som undefined behavior with the follwing approach when no arguments beside the format text.
//  #define log_emerg(x,...) __extension__ ht_log(LOG_EMERG,x, ##__VA_ARGS__) 
__extension__
#define log_emerg(...) __extension__ ht_log(LOG_EMERG,__VA_ARGS__) 		// Syslog 0 (Highest severity)
#define log_alert(...) __extension__ ht_log(LOG_ALERT,x, ##__VA_ARGS__) 		// Syslog 1
#define log_crit(...) __extension__ ht_log(LOG_CRIT,x, ##__VA_ARGS__) 			// Syslog 2
#define log_err(...) __extension__ ht_log(LOG_ERR, __VA_ARGS__) 			// Syslog 3
//#define log_err(x,...) __extension__ ht_log(LOG_ERR,x, ##__VA_ARGS__) 			// Syslog 3
//#define __extension__ log_err(x, ...) __extension__ ht_log(LOG_ERR,x, ##__VA_ARGS__) 			// Syslog 3
//#define __extension__ log_warning(x,...) ht_log(LOG_WARNING,x, ## __VA_ARGS__ ) 	// Syslog 4
//#define log_warning(x,args...) __extension__ ht_log(LOG_WARNING,x, ## args ) 	// Syslog 4
#define log_warning(...) __extension__ ht_log(LOG_WARNING, __VA_ARGS__ ) 	// Syslog 4
#define log_notice(...) __extension__ ht_log(LOG_NOTICE,__VA_ARGS__)	 	// Syslog 5
#define log_info(...) __extension__ ht_log(LOG_INFO,__VA_ARGS__) 			// Syslog 6
#define log_debug(...) __extension__ ht_log(LOG_DEBUG,__VA_ARGS__) 		// Syslog 7 (Lowest severity)
__extension__
#endif

