/*
 This file is part of pathrate.

 pathrate is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 pathrate is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with pathrate; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*-------------------------------------------------
   pathrate : an end-to-end capcity estimation tool
   Author   : Constantinos Dovrolis (dovrolis@cc.gatech.edu )
              Ravi S Prasad            ( ravi@cc.gatech.edu )
              
   Release  : Ver 2.4.1
   Support  : This work was supported by the SciDAC
              program of the US department 
--------------------------------------------------*/


#ifdef LOCAL
#define EXTERN
#else
#define EXTERN extern
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <strings.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <float.h>
#include <fcntl.h>

#define MAX_V(a,b)	((a)>(b)? (a): (b))
#define MIN_V(a,b)	((a)<(b)? (a): (b))

typedef struct {
  double mode_value_lo;	/* Lower bandwidth value (Mbps) of local mode */
  double mode_value_hi;	/* Upper bandwidth value (Mbps) of local mode */
	long   mode_cnt;	/* number of measurements in local mode */ 
	long   bell_cnt;	/* number of measurements in "bell" of local mode */ 
  double bell_lo;		/* low threshold (Mbps) of bell */
	double bell_hi;		/* high threshold (Mbps) of bell */  
	double bell_kurtosis;		  
} mode_struct;

#define  UNIMPORTANT_MODE 	-1
#define  LOCAL_MODE 	1
#define  LAST_MODE 		-2

/* Code numbers sent from receiver to sender using the TCP control stream */
#define  TRAIN_LEN        	0x00000001
#define  NO_TRAINS      		0x00000002
#define  GAME_OVER         	0x00000003
#define  ACK_TRAIN        	0x00000004
#define  PCK_LEN          	0x00000005
#define  MAX_PCK_LEN       	0x00000006
#define  SEND			          0x00000007
#define  CONTINUE	        	0x00000008
#define  TRAIN_SPACING  		0x00000009
#define  SENT_TRAIN     		0x00000010
#define  NEG_ACK_TRAIN    	0x00000011

/* Port numbers (UDP for receiver, TCP for sender) */
#define  UDPRCV_PORT 		48698 
#define  TCPSND_PORT 		48699 

#define  UDP_BUFFER_SZ  	212536 

#define  MAX_CONSEC_LOSSES	30
#define  MAX_PACK_SZ    	16384

#define  MIN_TRAIN_SPACING	500000 	/* microseconds */	
#define  TRAIN_SPACING_SEC	1	/* seconds */

EXTERN long verbose ;
EXTERN long Verbose ;
EXTERN int sock_tcp, sock_udp;


/*
 *  * Weiling: BIN_CNT_TOLER_kernel_percent */
#define  BIN_CNT_TOLER_kernel_percent   0.1

