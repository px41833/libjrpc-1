#ifndef _DBG_H_
#define _DBG_H_

#define DEBUG

#if defined(DEBUG)
#define _dbg(tag, fmt, args...) \
	do { printf("[%s %s:%d:%s] " fmt, tag, __FILE__, \
		__LINE__, __func__, ## args); fflush(stdout);} while (0)
#define _trc()		_dbg("TRC", "%s\n", "")
#else
#define _dbg(tag, fmt, args...)		{}
#define _trc()						{}
#endif

#endif
