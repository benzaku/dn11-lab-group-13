#ifndef _DLL_BASIC_H_
#define _DLL_BASIC_H_

#include <cnet.h>

/* ------- DECLARATIONS FOR A MINIMAL RELIABLE DATALINK LAYER -------- */

#define	MAX_FRAME_SIZE	(MAX_MESSAGE_SIZE + 1024)
extern int down_to_datalink(int link, char *packet, size_t length);
extern void reboot_DLL(void);
extern void inc_semphore();
extern int get_semphore();
extern void dec_semphore();

#endif
