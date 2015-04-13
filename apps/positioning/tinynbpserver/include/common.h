#ifndef __COMMON_H
#define __COMMON_H

#include<ap.h>
#include<station.h>

extern fd_set readset;
extern struct ap aps[3];
extern int maxfd;
extern int respoutputfmt ;
extern int showrequestresp ;
extern int stationchannel ;
extern int rtt_count;
extern u_int16_t transmit_mode;
extern int transmit_rate;
extern int no_of_aps;
extern int hidestation;
extern int showcircles;
extern int wakeup_station;
extern int no_of_stations;
extern int new_ap;
extern struct station station[];


#endif


