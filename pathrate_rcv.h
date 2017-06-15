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

#define  MAX_TRAIN_LEN      50
#define  MAX_IGNORED_MSRMS	0.3
#define  IGNORE_LIM_FACTOR	4

#define  MIN_PACK_SZ_P1		572 
#define  SIZE_INCR_STEP	 	25	

#define  NO_TRAINS_P1		1000
#define  NO_TRAINS_P2		500

#define  COEF_VAR_THR 		0.05
#define  ADR_REDCT_THR 		0.90

#define  BIN_NOISE	   	15
#define  BIN_CNT_TOLER		5	
#define  MAX_NO_MODES	 	1000

EXTERN int sock_tcp, sock_udp;
EXTERN char message[1024];
EXTERN FILE *pathrate_fp;	
EXTERN FILE *netlog_fp;	
EXTERN int netlog;
EXTERN char hostname[256];
EXTERN int recv_train_done;
EXTERN int retransmit;

EXTERN void prntmsg(FILE *fp);
EXTERN void help(void);
EXTERN void send_ctr_msg(long ctr_code) ;
EXTERN long recv_ctr_msg(int ctr_strm, char *ctr_buff);
EXTERN void print_bw(FILE *fp, double bw_v) ;
EXTERN void termint(int exit_code);
EXTERN void happy_end(double bw_lo, double bw_hi);
EXTERN double time_to_us_delta(struct timeval tv1, struct timeval tv2) ;
EXTERN void time_copy(struct timeval time_val_old, struct timeval *time_val_new) ;
EXTERN void order(double unord_arr[], double ord_arr[], long no_elems);
EXTERN double get_avg(double data[], long no_values);
EXTERN double get_std(double data[], long no_values);
/*Weiling: *bell_kurtosis*/
EXTERN double get_mode(double ord_data[], short vld_data[], double bin_wd, 
       int no_values, mode_struct *curr_mode);
EXTERN void sig_alrm(int signo);
EXTERN void *ctr_listen(void *arg);
EXTERN int recv_train(int train_len, int *round, struct timeval *time[]) ;
EXTERN int gig_path(int pack_sz, int round, int k_to_u_latency);
EXTERN void print_time(FILE *fp, int time);
EXTERN int intr_coalescence(struct timeval time[],int len, double latency);
