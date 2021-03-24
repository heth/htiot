#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include "ht_logger.h"
// A Thread-safe application logger/debug message module for daemon or console mode
// See ht_logger.h for definition of log macros like log_err()
/*
  printf family of functions in Linux support "%m" conversion specifier - others dont.
  uncomment NO_PERCENT_M_SUPPORT below to implement functionality here
*/
static void panic_log(char *message);

static int loglevel = LOG_INFO;	// Default syslog severity level to log 
static int logtoterm = 1;
static int logtosyslog = 0;

//#define NO_PERCENT_M_SUPPORT
void ht_log_to( int syslog, int terminal) {
	logtoterm = terminal;
	logtosyslog = syslog;
}

int ht_log_level_get( void ) {
	return(loglevel);
}

void ht_log_level_set( int level ) {
	if ( level <= LOG_DEBUG && level >= LOG_EMERG)
		loglevel = level;
		int oldlevel = setlogmask( LOG_UPTO(level) );
		(void) oldlevel;
}
//ht_log_init is optional and can be called if another Idendity, facility and/or level is requred.
// See syslog(3) man page for details.
//Note: Remember to check facility is enabled in your syslog daemon. (fx. /etc/rsyslog.conf)
void ht_log_init( const char *ident, int level, int facility ) {
	openlog(ident, LOG_ODELAY, facility);
	ht_log_level_set( level );
}
#ifdef NO_PERCENT_M_SUPPORT
static void add_errno_string(char *strout, int strout_size, const char *strin);
#endif

#define MAX_MESSAGE_SIZE 1024	
void ht_log(int level,  const char *fmt, ...) {
	va_list list;
	char *format;
	if (level > loglevel)
		return; // Message should not be logged
	va_start(list, fmt);
#ifdef NO_PERCENT_M_SUPPORT
	format = (char *) malloc(2 * MAX_MESSAGE_SIZE * sizeof(char));
#else
	format = (char *) malloc(MAX_MESSAGE_SIZE * sizeof(char));
#endif
	if (format == NULL) {
		panic_log("PANIC: Malloc error in ht_logger - log_error");
		exit(errno);
	}
#ifdef NO_PERCENT_M_SUPPORT
	add_errno_string(format+MAX_MESSAGE_SIZE, MAX_MESSAGE_SIZE, fmt);
	vsnprintf(format, MAX_MESSAGE_SIZE, format+MAX_MESSAGE_SIZE, list);
#else
	vsnprintf(format, MAX_MESSAGE_SIZE, fmt, list);
#endif
	if (logtosyslog)
		syslog(level,"%s", format);
	if (logtoterm)
	fprintf(stderr, "Log level %d: %s", level, format);
	free(format);
}
//Paniclog - last resort before crashing - try and tell what's happening
static void panic_log(char *message) {

	ssize_t res = write(STDERR_FILENO, message, strlen(message)+1);
	(void) res; 
}

#ifdef NO_PERCENT_M_SUPPORT
// Find %m in *strin and insert strerror message from errno here - if present
// Complete message copied to *strout including strerror message - if present :-)
static void add_errno_string(char *strout, int strout_size, const char *strin) {
	int in, out, in_err;
	char *strerr;

	for( in=0,out=0; strout_size > 0; in++, out++) {
		if (strin[in] == '\0') {
			strout[out] = '\0';
			return; //Finish copying full message
		}
		if (strin[in] == '%' && strin[in+1] == 'm') {
			in+=2; //Jump over "%m"
			strerror_r(errno, strout + out, strout_size-1); //Debug Change '2' to errno
			while( strout[out] != 0 && strout_size > 0) {
				out++;
				strout_size--;
			}
			// Indicate message truncated by '>' as last character. 
			if (strout_size == 0 && strout[out] != '\0') {
				strout[out-2] = '>';	// Chance could be last character in full message is '>'
				strout[out-1] = '\0';
				return; //Finish copying full message
			}
		}
		strout[out] = strin[in];
		strout_size--;
	}

	// Indicate message truncated by '>' as last character. 
	if (strout_size == 0 && strin[in] != '\0') {
		strout[out-2] = '>';	// Chance could be last character in full message is '>'
		strout[out-1] = '\0';
	}

}
#endif
