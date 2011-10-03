#include <cnet.h>
#include "milestone3.h"

/* ------- DECLARATIONS FOR A MINIMAL RELIABLE DATALINK LAYER -------- */

//#define	MAX_FRAME_SIZE	(MAX_MESSAGE_SIZE + 1024)
#define	MAX_FRAME_SIZE	(sizeof(NL_PACKET))
extern	int	down_to_datalink(int link, char *packet, size_t length);
extern	void	reboot_DLL(void);
extern void inc_semphore();
extern int get_semphore();
extern void dec_semphore();
