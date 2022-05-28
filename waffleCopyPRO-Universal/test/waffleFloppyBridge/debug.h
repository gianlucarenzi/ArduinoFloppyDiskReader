/*
 * $Id: debug.h,v 1.5 2015/06/30 14:26:10 gianluca Exp $
 */
#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

/* ANSI Eye-Candy ;-) */
#define ANSI_RED    "\x1b[31m"
#define ANSI_GREEN  "\x1b[32m"
#define ANSI_YELLOW "\x1b[1;33m"
#define ANSI_BLUE   "\x1b[1;34m"
#define ANSI_RESET  "\x1b[0m"

#define WITH_TIMESTAMP
//#undef  WITH_TIMESTAMP

#define printR(fmt, args...) \
	{ \
		fprintf(stdout, fmt, ## args); \
	}

#define printRaw(type, fmt, args...) \
	{ \
		fprintf(stdout, "%s " type " (%s): " fmt, __FILE__, __func__, ## args); \
	}

#define printRaw_E(type, fmt, args...) \
	{ \
		fprintf(stderr, "%s " type " (%s): " fmt, __FILE__, __func__, ## args); \
	}


// /////////////////////
// APPLICATION DEBUGGING
// /////////////////////
#ifdef WITH_TIMESTAMP
	#define DBG_N(fmt, args...) \
	  { if (debuglevel >= DBG_NOISY) {\
			struct timeval t; \
			gettimeofday(&t, NULL); \
			fprintf(stdout, ANSI_YELLOW); \
			fprintf(stdout, "[%08ld:%06ld] ", t.tv_sec, t.tv_usec); \
			printRaw("NOISY", fmt,## args); \
			fprintf(stdout, ANSI_RESET); \
			fflush(stdout); \
		} \
	  }
#else
	#define DBG_N(fmt, args...) \
	  { if (debuglevel >= DBG_NOISY) {\
			fprintf(stdout, ANSI_YELLOW); \
			printRaw("NOISY", fmt,## args); \
			fprintf(stdout, ANSI_RESET); \
			fflush(stdout); \
		} \
	  }
#endif

#ifdef WITH_TIMESTAMP
	#define DBG_V(fmt, args...) \
	  { if (debuglevel >= DBG_VERBOSE) {\
			struct timeval t; \
			gettimeofday(&t, NULL); \
			fprintf(stdout, ANSI_BLUE); \
			fprintf(stdout, "[%08ld:%06ld] ", t.tv_sec, t.tv_usec); \
			printRaw("VERBOSE", fmt,## args); \
			fprintf(stdout, ANSI_RESET); \
			fflush(stdout); \
		} \
	  }
#else
#define DBG_V(fmt, args...) \
	  { if (debuglevel >= DBG_VERBOSE) {\
			fprintf(stdout, ANSI_BLUE); \
			printRaw("VERBOSE", fmt,## args); \
			fprintf(stdout, ANSI_RESET); \
			fflush(stdout); \
		} \
	  }
#endif

#ifdef WITH_TIMESTAMP
	#define DBG_I(fmt, args...) \
	  { if (debuglevel >= DBG_INFO) {\
			struct timeval t; \
			gettimeofday(&t, NULL); \
			fprintf(stdout, ANSI_GREEN); \
			fprintf(stdout, "[%08ld:%06ld] ", t.tv_sec, t.tv_usec); \
			printRaw("INFO", fmt,## args); \
			fprintf(stdout, ANSI_RESET); \
			fflush(stdout); \
		} \
	  }
#else
	#define DBG_I(fmt, args...) \
	  { if (debuglevel >= DBG_INFO) {\
			fprintf(stdout, ANSI_GREEN); \
			printRaw("INFO", fmt,## args); \
			fprintf(stdout, ANSI_RESET); \
			fflush(stdout); \
		} \
	  }
#endif

#ifdef WITH_TIMESTAMP
	#define DBG_E(fmt, args...) \
	  { \
		struct timeval t; \
		gettimeofday(&t, NULL); \
		fprintf(stdout, ANSI_RED); \
		fprintf(stdout, "[%08ld:%06ld] ", t.tv_sec, t.tv_usec); \
		printRaw("ERROR", fmt,## args); \
		fprintf(stdout, ANSI_RESET); \
		fflush(stdout); \
	  }
#else
	#define DBG_E(fmt, args...) \
	  { \
		fprintf(stderr, ANSI_RED); \
		printRaw_E("ERROR", fmt, ## args); \
		fprintf(stderr, ANSI_RESET); \
		fflush(stderr); \
	  }
#endif

// /////////////////////
// THREAD      DEBUGGING
// /////////////////////

#ifdef WITH_TIMESTAMP
	#define THREAD_NOISY(fmt, args...) \
	{\
		if (debuglevelThread >= DBG_NOISY) {\
			struct timeval t; \
			gettimeofday(&t, NULL); \
			fprintf(stdout, ANSI_YELLOW); \
			fprintf(stdout, "[%08ld:%06ld] ", t.tv_sec, t.tv_usec); \
			printRaw("THREAD NOISY ", fmt,## args); \
			fprintf(stdout, ANSI_RESET); \
			fflush(stdout); \
		}\
	}
#else
	#define THREAD_NOISY(fmt, args...) \
	{\
		if (debuglevelThread >= DBG_NOISY) {\
			fprintf(stdout, ANSI_YELLOW); \
			printRaw("THREAD NOISY ", fmt,## args); \
			fprintf(stdout, ANSI_RESET); \
			fflush(stdout); \
		}\
	}
#endif


#ifdef WITH_TIMESTAMP
	#define THREAD_VERBOSE(fmt, args...) \
	{\
		if (debuglevelThread >= DBG_VERBOSE) {\
			struct timeval t; \
			gettimeofday(&t, NULL); \
			fprintf(stdout, ANSI_BLUE); \
			fprintf(stdout, "[%08ld:%06ld] ", t.tv_sec, t.tv_usec); \
			printRaw("THREAD VERBOSE ", fmt,## args); \
			fprintf(stdout, ANSI_RESET); \
			fflush(stdout); \
		}\
	}
#else
	#define THREAD_VERBOSE(fmt, args...) \
	{\
		if (debuglevelThread >= DBG_VERBOSE) {\
			fprintf(stdout, ANSI_BLUE); \
			printRaw("THREAD VERBOSE ", fmt,## args); \
			fprintf(stdout, ANSI_RESET); \
			fflush(stdout); \
		}\
	}
#endif

#ifdef WITH_TIMESTAMP
	#define THREAD_PRINT(fmt, args...) \
	{\
		if (debuglevelThread >= DBG_INFO) {\
			struct timeval t; \
			gettimeofday(&t, NULL); \
			fprintf(stdout, ANSI_GREEN); \
			fprintf(stdout, "[%08ld:%06ld] ", t.tv_sec, t.tv_usec); \
			printRaw("THREAD PRINT ", fmt,## args); \
			fprintf(stdout, ANSI_RESET); \
			fflush(stdout); \
		}\
	}
#else
	#define THREAD_PRINT(fmt, args...) \
	{\
		if (debuglevelThread >= DBG_INFO) {\
			fprintf(stdout, ANSI_GREEN); \
			printRaw("THREAD PRINT ", fmt,## args); \
			fprintf(stdout, ANSI_RESET); \
			fflush(stdout); \
		}\
	}
#endif

#ifdef WITH_TIMESTAMP
	#define THREAD_ERROR(fmt, args...) \
	{\
		struct timeval t; \
		gettimeofday(&t, NULL); \
		fprintf(stdout, ANSI_RED); \
		fprintf(stdout, "[%08ld:%06ld] ", t.tv_sec, t.tv_usec); \
		printRaw("THREAD ERROR ", fmt,## args); \
		fprintf(stdout, ANSI_RESET); \
		fflush(stdout); \
	}
#else
	#define THREAD_ERROR(fmt, args...) \
	{\
		fprintf(stdout, ANSI_RED); \
		printRaw("THREAD ERROR", fmt,## args); \
		fprintf(stdout, ANSI_RESET); \
		fflush(stdout); \
	}
#endif

// /////////////////////
// DRIVER      DEBUGGING
// /////////////////////

#ifdef WITH_TIMESTAMP
	#define DRIVER_NOISY(fmt, args...) \
	{\
		if (debuglevelDriver >= DBG_NOISY) {\
			struct timeval t; \
			gettimeofday(&t, NULL); \
			fprintf(stdout, ANSI_YELLOW); \
			fprintf(stdout, "[%08ld:%06ld] ", t.tv_sec, t.tv_usec); \
			printRaw("DRIVER NOISY ", fmt,## args); \
			fprintf(stdout, ANSI_RESET); \
			fflush(stdout); \
		}\
	}
#else
	#define DRIVER_NOISY(fmt, args...) \
	{\
		if (debuglevelDriver >= DBG_NOISY) {\
			fprintf(stdout, ANSI_YELLOW); \
			printRaw("DRIVER NOISY ", fmt,## args); \
			fprintf(stdout, ANSI_RESET); \
			fflush(stdout); \
		}\
	}
#endif


#ifdef WITH_TIMESTAMP
	#define DRIVER_VERBOSE(fmt, args...) \
	{\
		if (debuglevelDriver >= DBG_VERBOSE) {\
			struct timeval t; \
			gettimeofday(&t, NULL); \
			fprintf(stdout, ANSI_BLUE); \
			fprintf(stdout, "[%08ld:%06ld] ", t.tv_sec, t.tv_usec); \
			printRaw("DRIVER VERBOSE ", fmt,## args); \
			fprintf(stdout, ANSI_RESET); \
			fflush(stdout); \
		}\
	}
#else
	#define DRIVER_VERBOSE(fmt, args...) \
	{\
		if (debuglevelDriver >= DBG_VERBOSE) {\
			fprintf(stdout, ANSI_BLUE); \
			printRaw("DRIVER VERBOSE ", fmt,## args); \
			fprintf(stdout, ANSI_RESET); \
			fflush(stdout); \
		}\
	}
#endif

#ifdef WITH_TIMESTAMP
	#define DRIVER_PRINT(fmt, args...) \
	{\
		if (debuglevelDriver >= DBG_INFO) {\
			struct timeval t; \
			gettimeofday(&t, NULL); \
			fprintf(stdout, ANSI_GREEN); \
			fprintf(stdout, "[%08ld:%06ld] ", t.tv_sec, t.tv_usec); \
			printRaw("DRIVER PRINT ", fmt,## args); \
			fprintf(stdout, ANSI_RESET); \
			fflush(stdout); \
		}\
	}
#else
	#define DRIVER_PRINT(fmt, args...) \
	{\
		if (debuglevelDriver >= DBG_INFO) {\
			fprintf(stdout, ANSI_GREEN); \
			printRaw("DRIVER PRINT ", fmt,## args); \
			fprintf(stdout, ANSI_RESET); \
			fflush(stdout); \
		}\
	}
#endif

#ifdef WITH_TIMESTAMP
	#define DRIVER_ERROR(fmt, args...) \
	{\
		struct timeval t; \
		gettimeofday(&t, NULL); \
		fprintf(stdout, ANSI_RED); \
		fprintf(stdout, "[%08ld:%06ld] ", t.tv_sec, t.tv_usec); \
		printRaw("DRIVER ERROR ", fmt,## args); \
		fprintf(stdout, ANSI_RESET); \
		fflush(stdout); \
	}
#else
	#define DRIVER_ERROR(fmt, args...) \
	{\
		fprintf(stdout, ANSI_RED); \
		printRaw("DRIVER ERROR", fmt,## args); \
		fprintf(stdout, ANSI_RESET); \
		fflush(stdout); \
	}
#endif


#define DBG_ERROR   0
#define DBG_INFO    1
#define DBG_VERBOSE 2
#define DBG_NOISY   3

#endif
