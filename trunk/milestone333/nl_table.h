#ifndef _NL_TABLE_H_
#define _NL_TABLE_H_

#include <cnet.h>
#include "nl_packet.h"

#define	ALL_LINKS	(-1)

extern void reboot_NL_table(void);

extern int NL_ackexpected(CnetAddr address);
extern int NL_nextpackettosend(CnetAddr address);
extern int NL_packetexpected(CnetAddr address);

extern void inc_NL_packetexpected(CnetAddr address);
extern void inc_NL_ackexpected(CnetAddr address);

extern int NL_linksofminhops(CnetAddr address);
extern void NL_savehopcount(CnetAddr address, int trans_time, int link);

extern int NL_get_has_resent(CnetAddr address);
extern void NL_set_has_resent(CnetAddr address, unsigned short int value);

extern void NL_updatelastsendtest(NL_PACKET *last);
extern int NL_updateminmtu(CnetAddr address, int min_mtu, int link);
extern int NL_gettesthascome(CnetAddr address);
extern void NL_inctesthascome(CnetAddr address);
extern int NL_minmtu(CnetAddr address);
extern int NL_gethopcount(CnetAddr address);
extern int NL_gettablesize();
extern int NL_getminmtubyid(int id);
extern NL_PACKET * NL_getlastsendtest(CnetAddr address);
extern int NL_getdestbyid(int id);
#endif
