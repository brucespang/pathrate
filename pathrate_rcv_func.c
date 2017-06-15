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


#include "pathrate.h"
#include "pathrate_rcv.h"

/*-----------------------------------------------------------------------------*
 *  * Weiling: function get_kurtosis
 *   * requires at least 3 samples,
 *    * returns -99999 if error
 *     *-----------------------------------------------------------------------------*/
double get_kurtosis(double* bell_array, int size){
  double kurtosis, var, mean, temp, diff;
  int i;

  if(size < 3)
    return -99999;

  temp = 0;
  for(i = 0; i < size; i++)
    temp += bell_array[i];
  mean = temp / size;

  temp = 0;
  for(i = 0; i < size; i++) {
    diff = bell_array[i] - mean;
    temp += diff * diff;
  }
  var = temp / size;

  if(var == 0)
    return -99999;

  temp = 0;
  for(i = 0; i < size; i++){
    diff = bell_array[i] - mean;
    temp += diff * diff * diff * diff;
  }

  kurtosis = temp / (var * var);
  return kurtosis;
}

void prntmsg(FILE *fp) {
  fprintf(fp, "%s", message);
  fflush(fp);
}

/*
  Send a message through the control stream
*/
void send_ctr_msg(long ctr_code)
{
  char ctr_buff[24];
  long ctr_code_n = htonl(ctr_code);
  memcpy((void*)ctr_buff, &ctr_code_n, sizeof(long));
  if (write(sock_tcp, ctr_buff, sizeof(long)) != sizeof(long)) {
    fprintf(stderr, "send control message failed:\n");
    exit(-1);
  }
  //printf ("Done sending %x\n", (ctr_code & 0x000000FF));
}


/*
        Receive an empty message from the control stream
*/
long recv_ctr_msg(int ctr_strm, char *ctr_buff)
{
  long ctr_code;
  if (read(ctr_strm, ctr_buff, sizeof(long)) != sizeof(long)){
    fprintf(stderr, "recv control message failed:\n");
    return(-1);
  }
  memcpy(&ctr_code, ctr_buff, sizeof(long));
  return(ntohl(ctr_code));
}


/*
  Print a bandwidth measurement (given in Mbps) in the appropriate units
*/
void print_bw(FILE *fp, double bw_v)
{
  if (bw_v < 1.0) {
    sprintf(message," %.0f kbps ", bw_v*1000);prntmsg(fp);
  }
  else if (bw_v < 15.0) {
    sprintf(message," %.1f Mbps ", bw_v);prntmsg(fp);
  }
  else {
    sprintf(message," %.0f Mbps ", bw_v);prntmsg(fp);
  }
}


/*
   Terminate measurements
*/
void termint(int exit_code) {
  int ctr_code;

  ctr_code = CONTINUE;
  send_ctr_msg(ctr_code);
  ctr_code = GAME_OVER;
  send_ctr_msg(ctr_code);
  fclose(pathrate_fp); close(sock_tcp); close(sock_udp);
  exit(exit_code);
}

/* Successful termination. Print result.  */
void happy_end(double bw_lo, double bw_hi)
{
  sprintf(message,"\n\n-------------------------------------------------\n");
  prntmsg(pathrate_fp);
  sprintf(message,"Final capacity estimate : ");prntmsg(pathrate_fp);
  print_bw(pathrate_fp, bw_lo);
  sprintf(message," to ");prntmsg(pathrate_fp);
  print_bw(pathrate_fp, bw_hi);
  sprintf(message,"\n");prntmsg(pathrate_fp);
  sprintf(message,"-------------------------------------------------\n\n");
  prntmsg(pathrate_fp);
  if(verbose){
    fprintf(stdout,"-------------------------------------------------\nFinal capacity estimate : ");
    print_bw(stdout,bw_lo);
    fprintf(stdout," to ");
    print_bw(stdout,bw_hi);
    fprintf(stdout," \n-------------------------------------------------\n");
  }
  if (netlog) { /* print result for netlooger */
    struct tm *tm;
    struct hostent *rcv_host, *snd_host;
    char rcv_name[256];
    struct timeval curr_time;

    gettimeofday(&curr_time,NULL);
    tm = gmtime(&curr_time.tv_sec);
    fprintf(netlog_fp,"DATE=%4d",tm->tm_year+1900);
    print_time(netlog_fp,tm->tm_mon+1);
    print_time(netlog_fp,tm->tm_mday);
    print_time(netlog_fp,tm->tm_hour);
    print_time(netlog_fp,tm->tm_min);
    if (tm->tm_sec <10) {
      fprintf(netlog_fp,"0");
      fprintf(netlog_fp,"%1.6f",tm->tm_sec+curr_time.tv_usec/1000000.0);
    }
    else{
      fprintf(netlog_fp,"%1.6f",tm->tm_sec+curr_time.tv_usec/1000000.0);
    }
    gethostname(rcv_name, 255);
    rcv_host = gethostbyname(rcv_name);
    if(strcmp(rcv_name, "\0")!=0) fprintf(netlog_fp," HOST=%s",rcv_host->h_name);
    else fprintf(netlog_fp," HOST=NO_NAME");
    fprintf(netlog_fp," PROG=pathrate");
    fprintf(netlog_fp," LVL=Usage");
    if ((snd_host = gethostbyname(hostname)) == 0) {
        snd_host = gethostbyaddr(hostname,256,AF_INET);
    }
    fprintf(netlog_fp," PATHRATE.SNDR=%s",snd_host->h_name);
    fprintf(netlog_fp," PATHRATE.CAPL=%.1fMbps",bw_lo);
    fprintf(netlog_fp," PATHRATE.CAPH=%.1fMbps\n",bw_hi);
    fclose(netlog_fp);
  }
}

/* print time */
void print_time(FILE *fp, int time)
{
  if( time<10){
    fprintf(fp,"0");
    fprintf(fp,"%1d",time);
  }
  else{
    fprintf(fp,"%2d",time);
  }
}

/*
  Compute the time difference in microseconds
  between two timeval measurements
*/
double time_to_us_delta(struct timeval tv1, struct timeval tv2)
{
   double time_us;
   time_us = (double)
     ((tv2.tv_sec-tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec));
   return time_us;
}

void time_copy(struct timeval time_val_old, struct timeval *time_val_new)
{
   (*time_val_new).tv_sec =time_val_old.tv_sec;
   (*time_val_new).tv_usec=time_val_old.tv_usec;
}


/*
  Order an array of doubles using bubblesort
*/
void order(double unord_arr[], double ord_arr[], long no_elems)
{
  long i,j;
  double temp;
  for (i=0; i<no_elems; i++) ord_arr[i]=unord_arr[i];
  for (i=1; i<no_elems; i++)
    for (j=i-1; j>=0; j--) {
      if (ord_arr[j+1]<ord_arr[j]) {
         temp=ord_arr[j]; ord_arr[j]=ord_arr[j+1]; ord_arr[j+1]=temp;
      }
      else break;
    }
}


/*
  Compute the average of the set of measurements <data>.
*/
double get_avg(double data[], long no_values)
{
   long i;
   double sum_;
   sum_ = 0;
   for (i=0; i<no_values; i++) sum_ += data[i];
   return (sum_ / (double)no_values);
}



/*
  Compute the standard deviation of the set of measurements <data>.
*/
double get_std(double data[], long no_values)
{
   long i;
   double sum_, mean;
   mean = get_avg(data, no_values);
   sum_ = 0;
   for (i=0; i<no_values; i++) sum_ += pow(data[i]-mean, 2.);
   return sqrt(sum_ / (double)(no_values-1));
}


/*
  Detect a local mode in the set of measurements <ord_data>.
  Take into account only the valid (unmarked) measurements (vld_data[i]=1).
  The bin width of the local mode detection process is <bin_wd>.
  The set has <no_values> elements, and it is ordered in increasing sequence.
  The function returns the center value of the modal bin.
  Also, the following call-by-ref arguments return:
    mode_cnt:    # of measurements in modal bin
    bell_cnt:    # of measurements in entire mode
             (modal bin+surrounding bins of the same `bell')
    bell_lo:     low bandwidth threshold of modal `bell'
    bell_hi:     high bandwidth threshold of modal `bell'
*/
double get_mode(double ord_data[], short vld_data[],
    double bin_wd, int no_values, mode_struct *curr_mode)
{
  int
    i, j, done, tmp_cnt,
    mode_lo_ind, mode_hi_ind, bell_lo_ind, bell_hi_ind,
    bin_cnt,  bin_lo_ind,  bin_hi_ind,
    lbin_cnt, lbin_lo_ind=0, lbin_hi_ind=0,  /* left bin */
    rbin_cnt, rbin_lo_ind=0, rbin_hi_ind=0;  /* right bin */
  double
    mode_lo, mode_hi, bin_lo,  bin_hi;
  /*Weiling: bin_toler, used as */
  double bin_cnt_toler;


  /* Check if all measurements have been already marked */
  j=0;
  for (i=0; i<no_values; i++) j+=vld_data[i];
  if (j==0) return LAST_MODE; /* no more modes */
  #ifdef MODES
    printf("\n%d valid measurements\n", j);
  #endif
  /* Initialize mode struct */
  curr_mode->mode_cnt=0;
  curr_mode->bell_cnt=0;
  curr_mode->bell_lo=0;
  curr_mode->bell_hi=0;

  /*
    Find the bin of the primary mode from non-marked values
  */

  /* Find window of length bin_wd with maximum number of consecutive values */
  mode_hi=0; mode_hi_ind=0; mode_lo=0; mode_lo_ind=0;
  tmp_cnt=0;
  for (i=0;i<no_values;i++) {
    if (vld_data[i]) {
      j=i;
      while (j<no_values && vld_data[j] && ord_data[j]<=ord_data[i]+bin_wd) j++;
      if (tmp_cnt<j-i) {
        tmp_cnt = j-i;
        mode_lo_ind = i;
        mode_hi_ind = j-1;
      }
    }
  }

  curr_mode->mode_cnt  = tmp_cnt;
  mode_lo   = ord_data[mode_lo_ind];
  mode_hi   = ord_data[mode_hi_ind];
  #ifdef MODES
    printf("Central mode bin from %.3f to %.3f (%d msrms)", mode_lo, mode_hi, *mode_cnt);
    printf(" - (%d-%d)\n", mode_lo_ind, mode_hi_ind);
  #endif

  curr_mode->bell_cnt  = tmp_cnt;
  curr_mode->bell_lo  = mode_lo;
  curr_mode->bell_hi  = mode_hi;
  bell_lo_ind     = mode_lo_ind;
  bell_hi_ind     = mode_hi_ind;

  /*
        Find all the bins at the *left* of the central bin that are part of the
        same mode's bell. Stop when another local mode is detected.
  */
  bin_cnt    = curr_mode->mode_cnt;
  bin_lo_ind =  mode_lo_ind;
  bin_hi_ind =  mode_hi_ind;
  bin_lo     =  mode_lo;
  bin_hi     =  mode_hi;

    /* Weiling:  noise tolerance is determined by bin_cnt_toler, and it's
     * proportional to previous bin_cnt instead of constant BIN_NOISE_TOLER . */

  done=0;
  bin_cnt_toler = BIN_CNT_TOLER_kernel_percent * (bin_cnt);
  do {
      /* find window of measurements left of the leftmost modal bin with
       (at most) bin_wd width and with the maximum number of measurements */
    lbin_cnt=0;
    if (bin_lo_ind>0) {
      for (i=bin_hi_ind-1; i>=bin_lo_ind-1; i--) {
        tmp_cnt=0;
        for (j=i; j>=0; j--) {
          if (ord_data[j]>=ord_data[i]-bin_wd) tmp_cnt++;
          else break;
        }
        if (tmp_cnt >= lbin_cnt) {
          lbin_cnt    = tmp_cnt;
          lbin_lo_ind = j+1;
          lbin_hi_ind = i;
        }
      }
    }
    #ifdef MODES
         printf("Left bin: %.3f to %.3f", ord_data[lbin_lo_ind], ord_data[lbin_hi_ind]);
         printf(" (%d msrms)", lbin_cnt);
         printf(" - (%d-%d)", lbin_lo_ind, lbin_hi_ind);
    #endif

    if (lbin_cnt>0) {
      if (lbin_cnt < bin_cnt+bin_cnt_toler) {
         /* the bin is inside the modal bell */
        #ifdef MODES
          printf(" - Inside mode");
        #endif

        /* update bell counters */
        /* Weiling:
         In prevoius version (2.3.0), the count and lo threshold counters are updated
         only if bin has a significant number of measurements
         In this version (2.3.1), the count and lo threshold counters are updated anyway
        */

        curr_mode->bell_cnt += bin_lo_ind-lbin_lo_ind;
        curr_mode->bell_lo   = ord_data[lbin_lo_ind];
        bell_lo_ind = lbin_lo_ind;

        /* reset bin counters for next iteration */
        bin_cnt     = lbin_cnt;
        bin_lo_ind  = lbin_lo_ind;
        bin_hi_ind  = lbin_hi_ind;
        bin_lo      = ord_data[lbin_lo_ind];
        bin_hi      = ord_data[lbin_hi_ind];
        bin_cnt_toler = BIN_CNT_TOLER_kernel_percent * (bin_cnt);
      }
      else {
        /* the bin is outside the modal bell */
        done=1;
        #ifdef MODES
        printf(" - Outside mode");
        #endif
      }
      if (bin_lo_ind <= 1) done=1;
    }
    else done=1;
    #ifdef MODES
      printf("\n");
    #endif
  } while (!done);




  /*
    Find all the bins at the *right* of the central bin that are part of the
    same mode's bell. Stop when another local mode is detected.
  */
  bin_cnt    = curr_mode->mode_cnt;
  bin_lo_ind =  mode_lo_ind;
  bin_hi_ind =  mode_hi_ind;
  bin_lo     =  mode_lo;
  bin_hi     =  mode_hi;

  done=0;
  do {
    /* find window of measurements right of the rightmost modal bin with
       (at most) bin_wd width and with the maximum number of measurements */
    rbin_cnt=0;
    if (bin_hi_ind<no_values-1) {
      for (i=bin_lo_ind+1; i<=bin_hi_ind+1; i++) {
        tmp_cnt=0;
        for (j=i; j<no_values; j++) {
           if (ord_data[j]<=ord_data[i]+bin_wd) tmp_cnt++;
           else break;
         }
        if (tmp_cnt >= rbin_cnt) {
          rbin_cnt    = tmp_cnt;
          rbin_lo_ind = i;
          rbin_hi_ind = j-1;
        }
      }
    }
    #ifdef MODES
       printf("Right bin: %.3f to %.3f", ord_data[rbin_lo_ind], ord_data[rbin_hi_ind]);
       printf(" (%d msrms)", rbin_cnt);
       printf(" - (%d-%d)", rbin_lo_ind, rbin_hi_ind);
    #endif

    if (rbin_cnt>0) {
      if (rbin_cnt < bin_cnt+bin_cnt_toler) {
              /* the bin is inside the modal bell */
        #ifdef MODES
          printf(" - Inside mode");
        #endif

        /* update bell counters */
        /* Weiling:
         In prevoius version (2.3.0), the count and lo threshold counters are updated
         only if bin has a significant number of measurements
         In this version (2.3.1), the count and lo threshold counters are updated anyway
        */

        curr_mode->bell_cnt += rbin_hi_ind-bin_hi_ind;
        curr_mode->bell_hi   = ord_data[rbin_hi_ind];
        bell_hi_ind = rbin_hi_ind;

            /* reset bin counters for next iteration */
        bin_cnt     = rbin_cnt;
        bin_lo_ind  = rbin_lo_ind;
        bin_hi_ind  = rbin_hi_ind;
        bin_lo      = ord_data[rbin_lo_ind];
        bin_hi      = ord_data[rbin_hi_ind];
        bin_cnt_toler = BIN_CNT_TOLER_kernel_percent * (bin_cnt);
      }
      else {
              /* the bin is outside the modal bell */
        done=1;
        #ifdef MODES
          printf(" - Outside mode");
        #endif
      }
      if (rbin_hi_ind >= no_values-2) done=1;
    }
    else done=1;
    #ifdef MODES
      printf("\n");
    #endif
  } while (!done);

  /* Mark the values that make up this modal bell as invalid */
  for (i=bell_lo_ind; i<=bell_hi_ind; i++) vld_data[i]=0;

  /* Report mode characteristics */
  if (curr_mode->mode_cnt > BIN_NOISE)
  {
    sprintf(message,"\t* Mode:"); prntmsg(pathrate_fp);
    if (verbose) prntmsg(stdout);
    print_bw(pathrate_fp, mode_lo);
    if (verbose) print_bw(stdout, mode_lo);
    sprintf(message,"to"); prntmsg(pathrate_fp);
    if (verbose) prntmsg(stdout);
    print_bw(pathrate_fp, mode_hi);
    if (verbose) print_bw(stdout, mode_hi);
    sprintf(message," - %ld measurements\n\t  Modal bell: %ld measurements - low :",
        curr_mode->mode_cnt, curr_mode->bell_cnt); prntmsg(pathrate_fp);
    if (verbose) prntmsg(stdout);
    print_bw(pathrate_fp, curr_mode->bell_lo);
    if (verbose) print_bw(stdout, curr_mode->bell_lo);
    sprintf(message,"- high :"); prntmsg(pathrate_fp);
    if (verbose) prntmsg(stdout);
    print_bw(pathrate_fp, curr_mode->bell_hi);
    if (verbose) print_bw(stdout, curr_mode->bell_hi);
    sprintf(message,"\n");prntmsg(pathrate_fp);
    if (verbose) prntmsg(stdout);
    /* Weiling: calculate bell_kurtosis*/
    curr_mode->bell_kurtosis = get_kurtosis(ord_data + bell_lo_ind , bell_hi_ind - bell_lo_ind + 1);
    if(curr_mode->bell_kurtosis == -99999){
      sprintf(message, "\nWarning!!! bell_kurtosis == -99999\n"); prntmsg(pathrate_fp);
      if (verbose) prntmsg(stdout);
      return UNIMPORTANT_MODE;
    }

  /* Return the center of the mode, as the average between the high and low
       ends of the corresponding bin */
    curr_mode->mode_value_lo = mode_lo;
    curr_mode->mode_value_hi = mode_hi;
    return (LOCAL_MODE);
  }
  else return UNIMPORTANT_MODE;
}

void sig_alrm(int signo)
{
  (void)signo;
    return;
}

void *ctr_listen(void *arg)
{
  fd_set readset;
  long ret_val ;
  char ctr_buff[8];
  pthread_t  *dad_id;

  dad_id = (pthread_t *) arg;

  FD_ZERO(&readset);
  FD_SET(sock_tcp,&readset);
  if (select(sock_tcp+1,&readset,NULL,NULL,NULL) > 0 ){
    if ( FD_ISSET(sock_tcp,&readset) ){
      if (( ret_val = recv_ctr_msg(sock_tcp,ctr_buff)) != -1 ){

        if (ret_val == SENT_TRAIN) {
          pthread_kill(*dad_id, SIGALRM);
        }
        else {
          if (verbose) printf("\n\nSender sent terminate signal. Exiting...\n");
          termint(-1);
        }
      }
    }
  }
  pthread_exit(NULL);
}

/* Receive a complete packet train from the sender */
int recv_train(int train_len, int * round, struct timeval *time[]) {
  int exp_pack_id=0, exp_train_id=0;
  int pack_id, round_id, train_id;
  char pack_buf[1600];
  long ctr_code;
  int pack_sz = 1500;
  int bad_train=0;
  pthread_t th_id, my_id;
  pthread_attr_t attr ;

  struct sigaction sigstruct ;

  sigstruct.sa_handler = sig_alrm ;
  sigemptyset(&sigstruct.sa_mask);
  sigstruct.sa_flags = 0 ;
  #ifdef SA_INTERRUPT
     sigstruct.sa_flags |= SA_INTERRUPT ;
  #endif
  sigaction(SIGALRM , &sigstruct,NULL );

  *time = (struct timeval *) malloc((train_len+1)*sizeof(struct timeval));
  if (time == NULL) {
    fprintf(stderr,"Pathrate_rcv: In recv_train: Insufficient memory\n");
    exit(-1);
  }

  pthread_attr_init(&attr);
  my_id = pthread_self();
  if(pthread_create(&th_id, &attr, ctr_listen, (void *) &my_id)){
    perror("Pathrate_rcv: In recv_train:");
    fprintf(stderr,"Pathrate_rcv: In recv_train: Error creating thread\n");
    exit(-1);
  }

  recv_train_done = 0;
  if (!retransmit) {
    (*round)++;
  }
  ctr_code = SEND | ((*round)<<8);
  send_ctr_msg(ctr_code);
  do {
    if (recvfrom(sock_udp, pack_buf, pack_sz, 0, (struct sockaddr*)0, (socklen_t*)0) == -1) {
      /* interrupted??? */
      if (errno == EINTR) {
        if (exp_pack_id==train_len+1) recv_train_done = 1;
        pthread_join(th_id, NULL);

        sigstruct.sa_handler = SIG_DFL ;
        sigemptyset(&sigstruct.sa_mask);
        sigstruct.sa_flags = 0 ;
        sigaction(SIGALRM , &sigstruct,NULL );

        if (recv_train_done) {
          /* Send control message to ACK burst */
          retransmit=0;
          ctr_code = ACK_TRAIN;
          send_ctr_msg(ctr_code);
          return(bad_train);
        }
        else {
          /* send signal to recv_train */
          retransmit=1;
          ctr_code = NEG_ACK_TRAIN;
          send_ctr_msg(ctr_code);
          return(2);
        }
      }
      else {
#ifdef DEBUG
  fprintf(stderr,"DEBUG: Why here???\n");
#endif
        perror("RECVFROM:");
        termint(-1);
      }
    }
    else {
      gettimeofday(*time+exp_pack_id, (struct timezone*)0);
      memcpy(&pack_id, pack_buf, sizeof(int));
      pack_id=ntohl(pack_id);
      memcpy(&train_id, pack_buf+sizeof(int), sizeof(int));
      train_id=ntohl(train_id);
      memcpy(&round_id, pack_buf+2*sizeof(int), sizeof(int));
      round_id=ntohl(round_id);

      // printf("Pack %d, %d, %d, %d, %d\n", pack_id, train_id, round_id, exp_pack_id, exp_train_id);

      if (round_id!=(*round)) {
        bad_train = 1;
      }
      else {
        if (pack_id==0) {
          bad_train=0;
          exp_pack_id=1;
          exp_train_id=train_id;
        }
        else if (pack_id==exp_pack_id ) {// && train_id==exp_train_id)
          exp_pack_id++;
        }
        else if (train_id != exp_train_id) {
          bad_train = 2;
        }
      }
    }
  }while (1);
}

/* Print help on the options */
void help(void) {
  fprintf (stderr,"pathrate_rcv options\n");
  fprintf (stderr,"-s <host> : sender host (required)\n");
  fprintf (stderr,"-Q        : quick termination mode\n");
  fprintf (stderr,"-q        : quite mode\n");
  fprintf (stderr,"-v        : verbose mode\n");
  fprintf (stderr,"-o <file> : print log in file\n");
  fprintf (stderr,"-O <file> : append log in file\n");
  fprintf (stderr,"-N <file> : print result in ULM format in file\n");
  fprintf (stderr,"-h|-H     : print this help and exit \n");
  fprintf (stderr,"usage: pathrate_rcv [-H|-h] [-Q] [-q|-v] [-o|-O <filename>] [-N <filename>] -s <sender>\n");
  exit(-1);
}

/* Trying long trains to detect capacity in case interrupt
 * coalescing detected.
 */
int gig_path(int pack_sz, int round, int k_to_u_latency){
  int i, j, est = 0, ctr_code, tmp;
  int train_len=200, bad_train, no_of_trains = 10;
  struct timeval *pkt_time = NULL;
  double cap[300],ord_cap[300];

  sprintf(message,"Test with Interrupt Coalesence\n  %d Trains of length: %d \n", no_of_trains, train_len);
  prntmsg(pathrate_fp);
  if(Verbose) prntmsg(stdout);
  for (j=0; j<no_of_trains; j++){
    int disps[300];
    int num_b2b = 1;
    int k=0;
    int id[300];
    double tmp_cap, adr;

    sprintf(message,"  Train id: %d -> \n\t", j);
    prntmsg(pathrate_fp);
    if(Verbose) prntmsg(stdout);
    ctr_code = TRAIN_LEN | (train_len<<8);
    send_ctr_msg(ctr_code);
    ctr_code=PCK_LEN | (pack_sz<<8);
    send_ctr_msg(ctr_code);
    if ( pkt_time != NULL ) free (pkt_time);
    bad_train = recv_train(train_len, &round, &pkt_time);
    if (bad_train == 2) {/* train too big... try smaller */
      train_len-=20;
      sprintf(message,"Reducing train size to %d\n", train_len);
      //sprintf(message,"\n");
      prntmsg(pathrate_fp);
      if(Verbose) prntmsg(stdout);
      if (train_len < 100 && est < 5) {
        printf("Insufficient number of packet dispersion estimates.\n");
        fprintf(pathrate_fp,"Insufficient number of packet dispersion estimates.\n");
        printf("Probably a 1000Mbps path.\n");
        fprintf(pathrate_fp,"Probably a 1000Mbps path.\n");
        termint(-1);
      }
    }
    else {
      adr = (train_len - 2)*12000 /time_to_us_delta(pkt_time[1], pkt_time[train_len]);
      for (i=2; i<=train_len; i++){
        disps[i] = time_to_us_delta(pkt_time[i-1], pkt_time[i]);
        // fprintf(stderr,"%d %d, ",i, disps[i]);
      }
      id[k++]=1;
	    num_b2b = 1;
      for (i=2; i<train_len; i++){
        if ( (num_b2b<=5) ||(disps[i] < num_b2b*1.5*k_to_u_latency) ){
           num_b2b++;
        }
        else{
          id[k++]=i;
	        num_b2b = 1;
        }
      }
      for (i=1; i<k-1; i++) {
        tmp_cap = 12000.0*(id[i+1]-id[i]-1)/
	           (time_to_us_delta(pkt_time[id[i]], pkt_time[id[i+1]]));
        if (tmp_cap >= .9*adr) {
          cap[est]=tmp_cap;
          print_bw(pathrate_fp, cap[est++]);
          if (Verbose) print_bw(stdout, cap[est-1]);
          sprintf(message,"\n\t");
          prntmsg(pathrate_fp);
          if(Verbose) prntmsg(stdout);
        }
      }
      sprintf(message, "Number of jump detected = %d\n", k);
      prntmsg(pathrate_fp);
      if (Verbose) prntmsg(stdout);
    }
  }
  if ( est >1 ) {
    order(cap,ord_cap,est);
    tmp = (int)(.9 * est );
    happy_end(ord_cap[tmp-1], ord_cap[tmp]);
  }
  else {
    printf("Insufficient number %d of packet dispersion estimates.\n", est);
    fprintf(pathrate_fp,"Insufficient number of packet dispersion estimates.\n");
    printf("Probably a 1000Mbps path.\n");
    fprintf(pathrate_fp,"Probably a 1000Mbps path.\n");
    termint(-1);
  }

  termint(0);
  return(1);
}

int intr_coalescence(struct timeval time[],int len, double latency)
{
  double delta[100];
  int b2b=0;
  int i;


  for (i=2;i<len;i++)
  {
    delta[i] = time_to_us_delta(time[i-1],time[i]);
#ifdef DEBUG
   fprintf(stderr,"DEBUG: i %d, disp[i] %.2f\n", i, delta[i]);
#endif
    if ( delta[i] <= 2.5 * latency )
    {
      b2b++ ;
    }
  }
#ifdef DEBUG
  fprintf(stderr,"DEBUG: b2b %d len %d\n",b2b,len);
#endif
  if ( b2b > .6*len ) return 1;
  else return 0;
}
