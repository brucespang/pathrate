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


#define LOCAL
#include "pathrate.h"
#include "pathrate_rcv.h"

int main(int argc, char* argv[])
{
  extern char *optarg;

  struct hostent *host_snd;
  
  struct sockaddr_in snd_tcp_addr, rcv_udp_addr;
  
  int
    opt_len, 
    rcv_buff_sz, 
    abort_phase1=0, 
    outlier_lim,
    pack_incr_step,
    tmp,
    mss;

  int errflg=0;
  int file=0;
  int append=0;
  int gothostname=0;
  int quick_term=0;
  
  int
    i, j, c,
    sum_rtt,
    ctr_code,
    round,
    train_len,
    prev_train_len=2,
    train_len_P1,
    max_train_len,
    no_pack_sizes=40,
    no_trains,
    no_trains_P1,
    no_trains_per_size,
    trains_msrd=0,
    trains_rcvd=0,
    train_no,
    trains_per_size=0,
    no_modes_P1=0,
    no_modes_P2=0,
    cap_mode_ind=0,
    bad_tstamps=0,
    train_spacing,
    min_train_spacing,
    max_train_spacing,
    max_pack_sz,
    min_pack_sz_P1,
    pack_sz;
  
  short
    enough_data,
    path_overflow,
    adr_narrow,
    bad_train,
    bad_trains,
    measurs_vld_P1[NO_TRAINS_P1],
    measurs_vld_P2[NO_TRAINS_P2];
  
  char
    filename[75],
    random_data[MAX_PACK_SZ],
    pack_buf[MAX_PACK_SZ];
  
  
  struct timeval current_time, first_time, *time_stmps=NULL;
  
  double
    bw_msr,
    bin_wd,
    bw_range,
    avg_bw,
    rtt,
    adr,
    std_dev,
    delta,
    min_possible_delta,
    min_OSdelta[500],
    ord_min_OSdelta[500],
    mode_value,
    merit,
    max_merit,
    measurs_P1[NO_TRAINS_P1],
    measurs_P2[NO_TRAINS_P2],
    ord_measurs_P1[NO_TRAINS_P1],
    ord_measurs_P2[NO_TRAINS_P2];
  
  mode_struct 
    curr_mode,
    modes_P1[MAX_NO_MODES], 
    modes_P2[MAX_NO_MODES];
  
  time_t  localtm; 
  
  
  /* Print histogram of bandwidth measurements */
  
  
  
  /* 
    Check command line arguments 
  */
  verbose = 1;
  netlog = 0;
  append = 0;
  while ((c = getopt(argc, argv, "s:N:vQhHqo:O:")) != EOF){
    switch (c) {
      case 's':              
        gothostname = 1;
        strcpy(hostname,optarg);
        break;
      case 'Q':
        quick_term = 1;
        break;
      case 'q':
        Verbose=0;
        verbose=0;
        break;
      case 'v':
        Verbose=1;
        break;
      case 'o':
        file=1;
        strcpy(filename,optarg);
        break;
      case 'O':
        file=1;
        append=1;
        strcpy(filename,optarg);
        break;
      case 'N':
        netlog=1;
        strcpy(filename,optarg);
        break;
      case 'H':
        help() ;
        errflg++;
        break ;
      case 'h':
        help() ;
        errflg++;
        break ;
      case '?':
        errflg++; 
    }
  }
  
  if (file){
    if (append){
      pathrate_fp = fopen(filename,"a");
      fprintf(pathrate_fp, "\n\n");
    }
    else{
      pathrate_fp = fopen(filename,"w");
      fprintf(pathrate_fp, "\n\n");
    }
  }
  else{
     pathrate_fp = fopen("pathrate.output" , "a" ) ;  
     fprintf(pathrate_fp, "\n\n");
  }
  if (netlog) {
    netlog_fp = fopen(filename,"a");
  }
  if (errflg || !gothostname) {
    if (!gothostname) fprintf(stderr,"Need to specify sender's hostname!\n");
    (void)fprintf(stderr, "usage: pathrate_rcv [-H|-h] [-Q] [-q|-v] [-o|-O <filename>]\
 [-N <filename>] -s <sender>\n");
    exit (-1);
  } 
  
  if ((host_snd = gethostbyname(hostname)) == 0) {
    /* check if the user gave ipaddr */
    if ( ( snd_tcp_addr.sin_addr.s_addr = inet_addr(hostname) ) == -1 ) {
      fprintf(stderr,"%s: unknown host\n", hostname);
      exit(-1);
    }
    else
      host_snd = gethostbyaddr(hostname,256,AF_INET);
  }

  /* Print arguments in the log file */
  for (i=0; i<argc; i++)
    fprintf(pathrate_fp, "%s ", argv[i]);
  fprintf(pathrate_fp, "\n");

  
  /* Print date and path info at trace file */ 
  localtm = time(NULL); 
  gethostname(pack_buf, 256);
  sprintf(message,"\tpathrate run from %s to %s on %s", 
          hostname, pack_buf, ctime(&localtm));
  prntmsg(pathrate_fp);
  if(verbose) prntmsg(stdout);
  
  /* 
    Create data stream: UDP socket
   */
  if ((sock_udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
     perror("socket(AF_INET,SOCK_DGRAM):");
     exit(-1);
  }
  bzero((char*)&rcv_udp_addr, sizeof(rcv_udp_addr));
  rcv_udp_addr.sin_family         = AF_INET;
  rcv_udp_addr.sin_addr.s_addr    = htonl(INADDR_ANY);
  rcv_udp_addr.sin_port           = htons(UDPRCV_PORT);
  if (bind(sock_udp, (struct sockaddr*)&rcv_udp_addr, sizeof(rcv_udp_addr)) < 0) {
     perror("bind(sock_udp):");
     exit(-1);
  }
  rcv_buff_sz = UDP_BUFFER_SZ;
  if (setsockopt(sock_udp,SOL_SOCKET,SO_RCVBUF,(char*)&rcv_buff_sz,sizeof(rcv_buff_sz))<0){
     perror("setsockopt(sock_udp,SOL_SOCKET,SO_RCVBUF):");
     exit(-1);
  }
  
  
  /* 
    Create control stream: TCP connection
  */
  if ((sock_tcp = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket(AF_INET, SOCK_STREAM):");
    exit(-1);
  }
  opt_len = 1;
  if (setsockopt(sock_tcp, SOL_SOCKET, SO_REUSEADDR, (char*)&opt_len, sizeof(opt_len)) < 0) {
     perror("setsockopt(sock_tcp,SOL_SOCKET,SO_REUSEADDR):");
     exit(-1);
  }
  bzero((char*)&snd_tcp_addr, sizeof(snd_tcp_addr));
  snd_tcp_addr.sin_family         = AF_INET;
  memcpy((void*)&(snd_tcp_addr.sin_addr.s_addr), host_snd->h_addr, host_snd->h_length);
  snd_tcp_addr.sin_port           = htons(TCPSND_PORT);
  if (connect(sock_tcp,(struct sockaddr*)&snd_tcp_addr,sizeof(snd_tcp_addr))<0) {
     perror("Make sure that pathrate_snd runs at sender:");
     exit(-1);
  }
  
  
  /*
          Estimate round-trip time
  */
  sum_rtt=0;
  ctr_code=0; 
  for(i=0; i<10; i++) {
     char mybuff[1024];
     gettimeofday(&first_time, (struct timezone*)0);
     send_ctr_msg(ctr_code);
     recv_ctr_msg(sock_tcp, mybuff);
     gettimeofday(&current_time, (struct timezone*)0);
     if (i>0) sum_rtt += time_to_us_delta(first_time, current_time);/* ignore first rtt */
  }
  rtt = (double) (sum_rtt/9000.);
  sprintf(message,"\t--> Average round-trip time: %.1fms\n\n", rtt); 
  prntmsg(pathrate_fp);
  if (verbose) prntmsg(stdout);
  
  /* 
    Determine minimum train spacing based on rtt 
  */ 
  min_train_spacing = MIN_TRAIN_SPACING/1000; /* make msec */
  if (min_train_spacing < rtt*1.25) {       
    /* if the rtt is not much smaller than the specified train spacing,
     * increase minimum train spacing based on rtt */
    min_train_spacing = rtt*1.25; 
  }
  max_train_spacing = 2 * min_train_spacing;
  train_spacing = min_train_spacing;
  ctr_code = TRAIN_SPACING | (train_spacing<<8);
  send_ctr_msg(ctr_code);
  
  /* 
    Send maximum packet size to sender (based on TCP MSS) 
  */
  opt_len = sizeof(mss);
  if (getsockopt(sock_tcp, IPPROTO_TCP, TCP_MAXSEG, (char*)&mss, &opt_len)<0){
     perror("getsockopt(sock_tcp,IPPROTO_TCP,TCP_MAXSEG):");
     exit(-1);
  }
  
  max_pack_sz = mss;
  if (max_pack_sz == 0) 
    max_pack_sz = 1472;   /* Make it Ethernet sized MTU */
  else {
    max_pack_sz = mss+12;
    if (max_pack_sz > MAX_PACK_SZ) max_pack_sz = MAX_PACK_SZ; 
  }
  ctr_code = MAX_PCK_LEN | (max_pack_sz<<8);
  send_ctr_msg(ctr_code);
  
  /* Also set the minimum packet size for Phase I */
  min_pack_sz_P1 = MIN_PACK_SZ_P1;
  if (min_pack_sz_P1>max_pack_sz) min_pack_sz_P1=max_pack_sz;
  
  /* 
    Measure minimum latency for kernel-to-user transfer of packet.
  */
  srandom(getpid()); /* Create random payload; does it matter? */
  for (i=0; i<MAX_PACK_SZ-1; i++) random_data[i]=(char)(random()&0x000000ff);
  bzero((char*)&pack_buf, MAX_PACK_SZ);
  memcpy(pack_buf+2*sizeof(long), random_data, (MAX_PACK_SZ-1)-2*sizeof(sizeof(long)));
  
  for (j=0; j<no_pack_sizes; j++) {
    pack_sz = max_pack_sz;
    for (i=0; i<10; i++) {
      sendto(sock_udp, pack_buf, pack_sz, 0, (struct sockaddr*)&rcv_udp_addr,
          sizeof(rcv_udp_addr));
      gettimeofday(&first_time, (struct timezone*)0);
      recvfrom(sock_udp, pack_buf, pack_sz, 0, (struct sockaddr*)0, (int*)0);
      gettimeofday(&current_time, (struct timezone*)0);
      delta = time_to_us_delta(first_time, current_time);
      min_OSdelta[j*10+i] = delta;
    }
  }
  /* Use median  of measured latencies to avoid outliers */
  order(min_OSdelta, ord_min_OSdelta, no_pack_sizes*10);
  min_possible_delta = ord_min_OSdelta[(int) (no_pack_sizes*10)/2];  
  
  /* Total per-packet processing time: transfer from NIC to kernel, 
   * processing at the kernel, and transfer at user space. The multiplicative 
   * factor 3 is based on packet processing 
   * times reported in the literature */ 

  min_possible_delta *= 3; 
  sprintf(message,"--> Minimum acceptable packet pair dispersion: %.0f usec\n", 
      min_possible_delta);
  prntmsg(pathrate_fp);
  if (Verbose) prntmsg(stdout);

  /* The default initial train-length in Phase I is 2 packets (packet pairs) */
  train_len_P1 = 2;  
  
  /* 
    Keep a  unique identifier for each round in pathrate's execution. 
       This id is used to ignore between trains of previous rounds.  
  */
  round = 1;
  
  /*
    --------------------------------------------------
    Discover maximum feasible train length in the path
    (stop at a length that causes 3 lossy trains)
    --------------------------------------------------
  */
  sprintf(message,"\n-- Maximum train length discovery -- \n");
  prntmsg(pathrate_fp);
  
  /* Send packet size  to sender -> maximum at this phase */
  no_trains = 1;
  pack_sz=max_pack_sz;
  ctr_code = PCK_LEN | (pack_sz<<8);
  send_ctr_msg(ctr_code);
  
  /* Initial train length */
  train_len=train_len_P1; 
  path_overflow=0;
  bad_trains = 0;
  trains_msrd=0;
  
  /* Repeat for each train length */
  while (train_len <= MAX_TRAIN_LEN && !path_overflow) {
    /* Send train length to sender */
    if (!retransmit) {
      ctr_code = TRAIN_LEN | (train_len<<8);
      send_ctr_msg(ctr_code);
    }
    /* Wait for a complete packet train */
    if ( time_stmps != NULL ) free(time_stmps);
    bad_train = recv_train(train_len, &round, &time_stmps);

    /* Compute dispersion and bandwidth measurement */
    if (bad_train == 2) {
      if( bad_trains++ > 3) path_overflow = 1;
    }
    else if (!bad_train) {
       delta = time_to_us_delta(time_stmps[1], time_stmps[train_len]);
       bw_msr = ((28+pack_sz) << 3) * (train_len-1) / delta;
       sprintf(message,"\tTrain length: %d ->\t", train_len);
       prntmsg(pathrate_fp);
       if (Verbose) prntmsg(stdout);
       print_bw(pathrate_fp, bw_msr); 
       if (Verbose) print_bw(stdout, bw_msr); 
       if (delta > min_possible_delta * (train_len-1)) {
         sprintf(message,"\n");
         prntmsg(pathrate_fp);
         if (Verbose) prntmsg(stdout);
         /* Acceptable measurement */
         measurs_P1[trains_msrd++] = bw_msr;
       }
       else {
         sprintf(message,"(ignored) \n");
         prntmsg(pathrate_fp);
         if (Verbose) prntmsg(stdout);
       }
  
       /* Very slow link; the packet trains take more than their spacing
        * to be transmitted
        */  
       if (delta > train_spacing*1000) {
         path_overflow=1;
         /* Send control message to double train spacing */
         //ctr_code = CONTINUE;
         //send_ctr_msg(ctr_code);
         train_spacing = max_train_spacing;
         ctr_code = TRAIN_SPACING | (train_spacing<<8);
         send_ctr_msg(ctr_code);
       }
      
       /* Increase train length */
       if (!retransmit) {
         prev_train_len = train_len;
         if (train_len<6) train_len ++; 
         else if (train_len<12) train_len += 2; 
         else train_len += 4;
       }
    }
  }
  retransmit = 0;
  
  if (path_overflow || train_len>MAX_TRAIN_LEN) 
    max_train_len=prev_train_len;  /* Undo the previous increase. */
  else 
    max_train_len = train_len;
  sprintf(message,"\t--> Maximum train length: %d packets \n\n", max_train_len);
  prntmsg(pathrate_fp);
  if (Verbose) prntmsg(stdout); 
  /* Tell sender to continue with  next phase */
  //ctr_code = CONTINUE;
  //send_ctr_msg(ctr_code);
  
  /* Ravi: Test IC here and continue if its not IC */ 
  /* get one more train with max_train_len */

  ctr_code = TRAIN_LEN | (max_train_len<<8);
  send_ctr_msg(ctr_code);
  do {
    if ( time_stmps != NULL ) free(time_stmps);
    bad_train = recv_train(max_train_len, &round, &time_stmps);
  }while(bad_train);
  
  tmp = intr_coalescence(time_stmps, max_train_len, min_possible_delta/3);
  if (tmp) {
    sprintf(message,"Detected Interrupt Coalescence... \n");
    prntmsg(pathrate_fp);
    if (Verbose) prntmsg(stdout); 
    gig_path(max_pack_sz, round++,min_possible_delta/3);
  }
  
  /*
    ---------------------------------------------------------
    Check for possible multichannel links and traffic shapers 
    ---------------------------------------------------------
  */
  
  sprintf(message,"\n--Preliminary measurements with increasing packet train lengths--\n");
  
  prntmsg(pathrate_fp);
  if (Verbose) prntmsg(stdout); 
  /* Send number of trains to sender (five of them is good enough..) */
  no_trains = 7;  // Ravi: No need to send this to sender... we ask for each train now.
  
  /* Send packet size to sender -> maximum at this phase */
  pack_sz=max_pack_sz;
  ctr_code=PCK_LEN | (pack_sz<<8);
  send_ctr_msg(ctr_code);
  train_len=train_len_P1;
  do {
    /* Send train length to sender */
    sprintf(message,"  Train length: %d -> ", train_len);
    prntmsg(pathrate_fp);
    if (Verbose) prntmsg(stdout);

    if (!retransmit) {
      ctr_code = TRAIN_LEN | (train_len<<8);
      send_ctr_msg(ctr_code);
    }
    /* Tell sender to start sending packet trains */
    trains_rcvd=0;
    do {  /* for each train-length */ 
      /* Wait for a complete packet train */
      if ( time_stmps != NULL ) free(time_stmps);
      bad_train = recv_train(train_len, &round, &time_stmps);

      /* Compute dispersion and bandwidth measurement */
      if (!bad_train) {
        delta = time_to_us_delta(time_stmps[1], time_stmps[train_len]);
        bw_msr = ((28+pack_sz) << 3) * (train_len-1) / delta;
        print_bw(pathrate_fp, bw_msr);
        if (Verbose) print_bw(stdout, bw_msr);
        if (delta > min_possible_delta * (train_len-1)) { 
          /* Acceptable measurement */
           measurs_P1[trains_msrd++] = bw_msr;
        }
        trains_rcvd++;
      }
    }while(trains_rcvd<no_trains);
    sprintf(message,"\n");
    prntmsg(pathrate_fp);
    if (Verbose) prntmsg(stdout);
  
    if (!retransmit) {
      train_len++;
      /* Tell sender to continue with next phase */
      //ctr_code = CONTINUE;
      //send_ctr_msg(ctr_code);
    }
  } while ( train_len <= MIN_V(max_train_len,10));
  
  /* Actual number of trains measured */
  trains_msrd--;

  /*
    ------------------------------------------------------------------------
    Estimate bandwidth resolution (bin width) and check for "quick estimate" 
    ------------------------------------------------------------------------
  */
  /* Calculate average and standard deviation of last measurements,
     ignoring the five largest and the five smallest values  */
  order(measurs_P1,ord_measurs_P1,trains_msrd);
  enough_data=1;
  if (trains_msrd > 30) {
    avg_bw = get_avg(ord_measurs_P1+5, trains_msrd-10);
    std_dev = get_std(ord_measurs_P1+5, trains_msrd-10);
    outlier_lim=(int) (trains_msrd/10);
    bw_range=ord_measurs_P1[trains_msrd-outlier_lim-1]-ord_measurs_P1[outlier_lim];
  }
  else  {
    avg_bw = get_avg(ord_measurs_P1, trains_msrd);
    std_dev = get_std(ord_measurs_P1, trains_msrd);
    bw_range=ord_measurs_P1[trains_msrd-1]-ord_measurs_P1[0];
  }  
  
  
  /* Estimate bin width based on previous measurements */
  /* Weiling: bin_wd is increased to be twice of earlier value */

  if (bw_range < 1.0)  /* less than 1Mbps */ 
    bin_wd = bw_range*0.25; 
  else
    bin_wd = bw_range*0.125; 

  if (bin_wd == 0) bin_wd = 20.0;
  sprintf(message,"\n\t--> Capacity Resolution: ");
  prntmsg(pathrate_fp);
  if (verbose) prntmsg(stdout);
  print_bw(pathrate_fp, bin_wd); 
  if (verbose) print_bw(stdout, bin_wd);
  sprintf(message,"\n");
  prntmsg(pathrate_fp); 
  if (verbose) prntmsg(stdout);
  /* Check for quick estimate (when measurements are not very spread) */
  if( (std_dev/avg_bw<COEF_VAR_THR) || quick_term) {
    if (quick_term)
      sprintf(message," - Requested Quick Termination");
    else
      sprintf(message,"`Quick Termination' - Sufficiently low measurement noise");
    prntmsg(pathrate_fp);
    if(verbose) prntmsg(stdout);
    sprintf(message,"\n\n--> Coefficient of variation: %.3f \n", std_dev/avg_bw);
    prntmsg(pathrate_fp); 
    if(verbose) prntmsg(stdout);
    happy_end(avg_bw-bin_wd/2., avg_bw+bin_wd/2.);
    termint(0);
  }
  
  /* Tell sender to continue with next phase */
  //ctr_code = CONTINUE;
  //send_ctr_msg(ctr_code);
  
  /*
    ----------------------------------------------------------
    Phase I: Discover local modes with packet pair experiments
    ----------------------------------------------------------
  */
  sprintf(message,"\n\n-- Phase I: Detect possible capacity modes -- \n\n");
  prntmsg(pathrate_fp);
  if (verbose) prntmsg(stdout);
  sleep(1);
  
  /* Phase I uses packet pairs, i.e., 2 packets (the default; it may be larger) */
  train_len = train_len_P1;
  if (train_len > max_train_len) train_len = max_train_len;
  
  /* Compute number of different packet sizes  in Phase I */
  pack_incr_step = (max_pack_sz - min_pack_sz_P1) / no_pack_sizes + 1;
  pack_sz=min_pack_sz_P1;
  
  /* Compute number of trains  per packet size */
  no_trains_per_size = NO_TRAINS_P1 / no_pack_sizes;
  
  /* Number of trains in Phase I */
  no_trains = NO_TRAINS_P1;
  
  /* Send train spacing to sender */
  train_spacing = min_train_spacing;
  ctr_code = TRAIN_SPACING | (train_spacing<<8);
  send_ctr_msg(ctr_code);
  
  trains_msrd=0;
  retransmit = 0;
  /* Compute new packet size (and repeat for several packet sizes) */
  for (i=0; i<no_pack_sizes; i++){
     if (!retransmit) {
       /* Send packet size to sender */
       ctr_code = PCK_LEN | (pack_sz<<8);
       send_ctr_msg(ctr_code);
    
       /* Send train length to sender */
       ctr_code = TRAIN_LEN | (train_len<<8);
       send_ctr_msg(ctr_code);
    
       sprintf(message,"\t-> Train length: %2d - Packet size: %4dB -> %2d%% completed\n",
           train_len, pack_sz+28, i*100/no_pack_sizes);
       prntmsg(pathrate_fp);
       if (verbose) prntmsg(stdout);
       bad_tstamps = 0; 
       trains_per_size = 0; 
     }

     do {
        /* Wait for a complete packet train */
        if ( time_stmps != NULL ) free(time_stmps);
        bad_train = recv_train(train_len, &round, &time_stmps);
  
          /* Compute dispersion and bandwidth measurement */
        if (!bad_train) {
          delta = time_to_us_delta(time_stmps[1], time_stmps[train_len]);
          bw_msr = ((28+pack_sz) << 3) * (train_len-1) / delta;
          sprintf(message,"\tMeasurement-%d:", trains_per_size+1);
          prntmsg(pathrate_fp);
          if (Verbose) prntmsg(stdout);
          print_bw(pathrate_fp, bw_msr);
          if (Verbose) print_bw(stdout, bw_msr);
          sprintf(message," (%.0f usec)",delta);
          prntmsg(pathrate_fp); 
          if (Verbose) prntmsg(stdout);
  
          /* Acceptable measurement */
          if (delta > min_possible_delta*(train_len-1)) {
             measurs_P1[trains_msrd++] = bw_msr;
          }
          else {
            bad_tstamps++;
            sprintf(message," (ignored)");
            prntmsg(pathrate_fp);
            if (Verbose) prntmsg(stdout);
          }  
  
          /* # of trains received in this packet size iteration */
          trains_per_size++;
          sprintf(message,"\n");
          prntmsg(pathrate_fp);
          if (Verbose) prntmsg(stdout);
       }
     } while(trains_per_size < no_trains_per_size);
     if(Verbose) fprintf(stdout,"\n");

     /* Tell sender to continue with next phase */
     pack_sz+=pack_incr_step;
     if (pack_sz > max_pack_sz) pack_sz = max_pack_sz;
     /***** dealing with ignored measurements *****/
     if (bad_tstamps > no_trains_per_size/IGNORE_LIM_FACTOR){
       /* If this is greater than max_train_len? :Ravi */
       train_len+=1;
       if (train_len > MAX_V(max_train_len/4,2)) {
          abort_phase1=1;  
       }
       pack_sz+=275;
       if (pack_sz > max_pack_sz) pack_sz = max_pack_sz;
       if (!abort_phase1){
         sprintf(message,"\n\tToo many ignored measurements..\n\tAdjust train length: %d packets\n\tAdjust packet size: %d bytes\n\n",
              train_len, MIN_V(pack_sz,max_pack_sz)+28); 
         prntmsg(pathrate_fp);
         if (Verbose) prntmsg(stdout);
       }
       else 
         break;
     }
  }
  
  /* Compute number of valid (non-ignored) measurements */
  no_trains = trains_msrd-1;
  
  /* ---------------------------------------------------------
    Detect all local modes in Phase I 
     --------------------------------------------------------- */
  if (!abort_phase1) {
    /* Order measurements */
    order(measurs_P1,ord_measurs_P1,no_trains);
    
    /* Detect and store all local modes */
    sprintf(message,"\n\n-- Local modes : In Phase I --\n");
    prntmsg(pathrate_fp);
    if (verbose) prntmsg(stdout);

    /* Mark all measurements as valid (needed for local mode detection) */
    for (train_no=0; train_no<no_trains; train_no++) measurs_vld_P1[train_no]=1;
    no_modes_P1=0; 
    while ((mode_value = 
      get_mode(ord_measurs_P1, measurs_vld_P1, bin_wd, no_trains, &curr_mode)) != LAST_MODE) 
    {
      /* the modes are ordered based on the number of measurements
         in the modal bin (strongest mode first) */  
            if (mode_value != UNIMPORTANT_MODE) {
        modes_P1[no_modes_P1++]= curr_mode;
        /*modes_P1[no_modes_P1].mode_value    = mode_value;
        modes_P1[no_modes_P1].mode_cnt      = mode_cnt;
        modes_P1[no_modes_P1].bell_cnt      = bell_cnt;
        modes_P1[no_modes_P1].bell_lo       = bell_lo;
        modes_P1[no_modes_P1].bell_hi       = bell_hi;
        modes_P1[no_modes_P1].bell_kurtosis = bell_kurtosis;
        no_modes_P1++; */
        if (no_modes_P1 >= MAX_NO_MODES) {
          fprintf(stderr,"Increase MAX_NO_MODES constant\n\n");
                            termint(-1);
        }
      }
    }
    sprintf(message,"\n");
    prntmsg(pathrate_fp);
    if (verbose) prntmsg(stdout);
  }
  else {
    sprintf(message,"\n\tAborting Phase I measurements..\n\tToo many ignored measurements\n\tPhase II will report lower bound on path capacity.\n");
    prntmsg(pathrate_fp);
    if (verbose) prntmsg(stdout);
     
  }
  no_trains_P1 = no_trains;
  /* Tell sender to continue with next phase */
  //ctr_code = CONTINUE;
  //send_ctr_msg(ctr_code);
    
  sleep(1);
             
  
  /*
    -------------------------------------------------
    Phase II: Packet trains with maximum train length 
    -------------------------------------------------
  */
  sprintf(message,"\n\n-- Phase II: Estimate Asymptotic Dispersion Rate (ADR) -- \n\n");
  prntmsg(pathrate_fp);
  if (verbose) prntmsg(stdout);
  
  /* Train spacing in Phase II */
  train_spacing = max_train_spacing;
  ctr_code = TRAIN_SPACING | (train_spacing<<8);
  send_ctr_msg(ctr_code);
  
  /* Determine train length for Phase II. Tell sender about it. */
  /* Note: the train length in Phase II is normally the maximum train length, determined 
     in the "Maximum Train Length Discovery" phase which was executed earlier.
     The following constraint is only used in very low capacity paths. 
   */
  train_len = max_train_len;
  //train_len = (long) (((avg_bw*0.5) * (max_train_spacing*1000)) / (max_pack_sz*8)); 
  //if (train_len > max_train_len) train_len = max_train_len;
  //if (train_len < train_len_P1)  train_len = train_len_P1;
  ctr_code = TRAIN_LEN | (train_len<<8);
  send_ctr_msg(ctr_code);
  
  /* Packet size in Phase II. Tell sender about it. */
  pack_sz = max_pack_sz;
  ctr_code = PCK_LEN | (pack_sz<<8);
  send_ctr_msg(ctr_code);
  
  /* Number of trains in Phase II. Tell sender about it. Ravi:Not anymore..*/
  no_trains = NO_TRAINS_P2;
  
  sprintf(message,"\t-- Number of trains: %d - Train length: %d - Packet size: %dB\n",
                  no_trains, train_len, pack_sz+28);
  prntmsg(pathrate_fp);
  if (verbose) prntmsg(stdout);

  /* Tell sender to start sending */
  round++;
  trains_msrd=0;
  trains_rcvd=0;
  bad_tstamps=0;
  
  do {
     /* Wait for a complete packet train */
     if ( time_stmps != NULL ) free(time_stmps);
     bad_train = recv_train(train_len, &round, &time_stmps);
  
    /* Compute dispersion and bandwidth measurement */
     if (!bad_train) {
        delta = time_to_us_delta(time_stmps[1], time_stmps[train_len]);
        bw_msr = ((28+pack_sz) << 3) * (train_len-1) / delta;
        sprintf(message,"\tMeasurement-%4d out of %3d:", trains_rcvd+1, no_trains);
        prntmsg(pathrate_fp);
        if (Verbose) prntmsg(stdout);
        print_bw(pathrate_fp, bw_msr);
        if (Verbose) print_bw(stdout, bw_msr);
        sprintf(message," (%.0f usec)", delta);
        prntmsg(pathrate_fp); 
        if (Verbose) prntmsg(stdout);
  
      /* Acceptable measurement */
        if (delta > min_possible_delta*(train_len-1)) {
                              /* store bw value */
           measurs_P2[trains_msrd++] = bw_msr;
        }
        else {
          bad_tstamps++;
          sprintf(message," (ignored)");
          prntmsg(pathrate_fp);
          if (Verbose) prntmsg(stdout);
        }  
  
        trains_rcvd++;
        sprintf(message,"\n");
        prntmsg(pathrate_fp);
        if (Verbose) prntmsg(stdout);
     }
  } while(trains_rcvd<no_trains);
  
  /* Tell sender to continue with next phase */
  //ctr_code = CONTINUE;
  //send_ctr_msg(ctr_code);
  
  /* Compute number of valid (non-ignored measurements) */
  no_trains = trains_msrd-1;
  
  /* ---------------------------------------------------------
       Detect all local modes in Phase II 
     --------------------------------------------------------- */
  
  /* Order measurements */
  order(measurs_P2,ord_measurs_P2,no_trains);
  
  /* Detect and store all local modes in Phase II */
  sprintf(message,"\n-- Local modes : In Phase II --\n");
  prntmsg(pathrate_fp);
  if (verbose) prntmsg(stdout);

  /* Mark all measurements as valid (needed for local mode detection) */
  for (train_no=0; train_no<no_trains; train_no++) measurs_vld_P2[train_no]=1;
  no_modes_P2=0; 
  while ((mode_value = 
        get_mode(ord_measurs_P2, measurs_vld_P2, bin_wd, no_trains, &curr_mode)) != LAST_MODE)
  {
    /* the modes are ordered based on the number of measurements
       in the modal bin (strongest mode first) */  
    if (mode_value != UNIMPORTANT_MODE) {
      modes_P2[no_modes_P2++]= curr_mode;
      /* modes_P2[no_modes_P2].mode_value    = mode_value;
      modes_P2[no_modes_P2].mode_cnt      = mode_cnt;
      modes_P2[no_modes_P2].bell_cnt      = bell_cnt;
      modes_P2[no_modes_P2].bell_lo       = bell_lo;
      modes_P2[no_modes_P2].bell_hi       = bell_hi;
      modes_P2[no_modes_P2].bell_kurtosis = bell_kurtosis;
      no_modes_P2++; 
      */
      if (no_modes_P2 >= MAX_NO_MODES) {
        fprintf(stderr,"Increase MAX_NO_MODES constant\n\n");
        termint(-1);
      }
    }
  }
  sprintf(message,"\n");
  prntmsg(pathrate_fp);
  if (Verbose) prntmsg(stdout);
  
  /* 
     If the Phase II measurements are distributed in a very narrow fashion
    (i.e., low coefficient of variation, and std_dev less than bin width),
     and the ADR is not much lower than the earlier avg bandwidth estimate,
     the ADR is a good estimate of the capacity mode. This happens when 
     the narrow link is of significantly lower capacity than the rest of the links, 
     and it is only lightly used. 
  */
  /* Compute ADR estimate */
  adr = get_avg(ord_measurs_P2+10, no_trains-20);
  std_dev = get_std(ord_measurs_P2+10, trains_msrd-20);
  adr_narrow=0;
  if (no_modes_P2==1 && std_dev/adr<COEF_VAR_THR && adr/avg_bw>ADR_REDCT_THR) {
     //sprintf(message,"    The capacity estimate will be based on the ADR mode.\n");
     //prntmsg(pathrate_fp);
     //if (Verbose) prntmsg(stdout);
     adr = (modes_P2[0].mode_value_lo + modes_P2[0].mode_value_hi)/2.0;
     adr_narrow=1;  
  }
  if (no_modes_P2 > 1){
    sprintf(message,"\n    WARNING: Phase II did not lead to unimodal distribution.\n\
        The ADR estimate may be wrong. Run again later. \n\n"); 
    prntmsg(pathrate_fp);
    if (Verbose) prntmsg(stdout);
    max_merit=0.;
    for(i=0;i<no_modes_P2;i++) {
      sprintf(message,"\t");
      prntmsg(pathrate_fp);
      if (Verbose) prntmsg(stdout);
      print_bw(pathrate_fp, modes_P2[i].mode_value_lo);
      if (Verbose) print_bw(stdout, modes_P2[i].mode_value_lo);
      sprintf(message,"to");
      prntmsg(pathrate_fp);
      if (Verbose) prntmsg(stdout);
      print_bw(pathrate_fp, modes_P2[i].mode_value_hi);
      if(Verbose) print_bw(stdout, modes_P2[i].mode_value_hi);
      // merit = (modes_P2[i].mode_cnt / (double)modes_P2[i].bell_cnt)
      // Weiling: merit is calculated using kurtosis as the narrowness of the bell
      merit = modes_P2[i].bell_kurtosis
            * (modes_P2[i].mode_cnt / (double)no_trains);
      sprintf(message," - Figure of merit: %.2f\n", merit);
      prntmsg(pathrate_fp);
      if (Verbose) prntmsg(stdout);
      if (merit > max_merit) {
         max_merit = merit;
         cap_mode_ind = i;
      }
    }
    adr = (modes_P2[cap_mode_ind].mode_value_lo+modes_P2[cap_mode_ind].mode_value_lo)/2.0;
  }
  sprintf(message,"--> Asymptotic Dispersion Rate (ADR) estimate:");
  prntmsg(pathrate_fp);
  if (Verbose) prntmsg(stdout);
  print_bw(pathrate_fp, adr); 
  if (Verbose) print_bw(stdout, adr); 
  sprintf(message,"\n");
  prntmsg(pathrate_fp);
  if (Verbose) prntmsg(stdout);
  
  /*
    ---------------------------------------------------------
    The end of the story... Final capacity estimate 
    ---------------------------------------------------------
  */
  
      
  /* 
    Final capacity estimate: the Phase I mode that is larger than ADR,
    and it has the maximum "figure of merit". This figure of metric is the 
    "density fraction" of the mode, times the "strength fraction" of the mode.
    The "density fraction" is the number of measurements in the central bin of the mode, 
    divided by the number of measurements in the entire bell of that mode.
    The "strength fraction" is the number of measurements in the central bin of the mode, 
    divided by the number of measurements in the entire Phase I phase.
  */
  if (!abort_phase1){
    sprintf(message,"\n--> Possible capacity values:\n");
    prntmsg(pathrate_fp);
    if (Verbose) prntmsg(stdout);
    max_merit=0.;
    for(i=0;i<no_modes_P1;i++){
      /* Give possible capacity modes */
      if (modes_P1[i].mode_value_hi > adr) {
        sprintf(message,"\t");
        prntmsg(pathrate_fp); 
        if (Verbose) prntmsg(stdout);
        print_bw(pathrate_fp, modes_P1[i].mode_value_lo);
        if (Verbose) print_bw(stdout, modes_P1[i].mode_value_lo);
        sprintf(message,"to");
        prntmsg(pathrate_fp); 
        if (Verbose) prntmsg(stdout);
        print_bw(pathrate_fp, modes_P1[i].mode_value_hi);
        if (Verbose) print_bw(stdout, modes_P1[i].mode_value_hi);
        // merit = (modes_P1[i].mode_cnt / (double)modes_P1[i].bell_cnt) 
        // Weiling: merit is calculated using kurtosis as the narrowness of the bell
        merit = modes_P1[i].bell_kurtosis 
              * (modes_P1[i].mode_cnt / (double)no_trains_P1); 
        sprintf(message," - Figure of merit: %.2f\n", merit);
        prntmsg(pathrate_fp); 
        if (Verbose) prntmsg(stdout);
        if (merit > max_merit) {
          max_merit =  merit;
          cap_mode_ind = i;
        }
      }
    }
    
    
    //if (max_merit>0. && !adr_narrow) 
    if (max_merit>0.) 
    {
      happy_end(modes_P1[cap_mode_ind].mode_value_lo,
        modes_P1[cap_mode_ind].mode_value_hi);
      termint(0);
    }
    /* 
      If there is no Phase I mode that is larger than the ADR, 
      //or if Phase II gave a very narrow distribution,
         the final capacity estimate is the ADR. 
    */
    else { 
      /* If there are multiple modes in Phase II (not unique estimate for ADR), 
       * the best choice for the capacity mode is the largest mode of Phase II 
       */ 
      sprintf(message,"\t");
      prntmsg(pathrate_fp); 
      if (Verbose) prntmsg(stdout);
      print_bw(pathrate_fp, adr-bin_wd/2.);
      if (Verbose) print_bw(stdout, adr-bin_wd/2.);
      sprintf(message,"to");
      prntmsg(pathrate_fp); 
      if (Verbose) prntmsg(stdout);
      print_bw(pathrate_fp, adr+bin_wd/2.);
      if (Verbose) print_bw(stdout, adr+bin_wd/2.);
      sprintf(message," - ADR mode\n");
      prntmsg(pathrate_fp); 
      if (Verbose) prntmsg(stdout);
      
      happy_end(adr-bin_wd/2., adr+bin_wd/2.);
      termint(0);
    }
  }
  else{
    max_merit=0.;
    sprintf(message,"--> Phase I was aborted.\n--> The following estimate is a lower bound for the path capacity.\n ");
    prntmsg(pathrate_fp);
    if (Verbose) prntmsg(stdout);
    happy_end(adr-bin_wd/2., adr+bin_wd/2.);
    termint(0);
  }
  exit(-1);
}
