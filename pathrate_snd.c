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
#include "pathrate_snd.h"

int main(int argc, char* argv[])
{
  
  struct sockaddr_in snd_udp_addr, snd_tcp_addr, rcv_udp_addr, rcv_tcp_addr;
  
  struct hostent *host_rcv;
  
  int opt_len, sock_udp, sock_tcp, ctr_strm, send_buff_sz, sleep_secs=1,
          rcv_tcp_adrlen, 
          i,
          round_id=1, round_id_n,
          train_id, train_id_n,
          pack_id, pack_id_n,
          ctr_code,
          ctr_code_cmnd,
          ctr_code_data,
          pack_sz=1500, 
          max_pack_sz,
          train_len=3,
          no_trains=1,
          train_spacing,
          train_no,
          trains_ackd, 
          trains_lost,
          file=0,
          errflg=0;
  
  char ctr_buff[8], pack_buf[MAX_PACK_SZ], random_data[MAX_PACK_SZ];
  char c, filename[256];

  struct timeval sleep_time, current_time, prior_sleep;
  
  short reset_flag, done, sleep_usecs;
  
  time_t localtm;
  
  FILE *pathrate_fp=NULL; 
  
  
  /* 
    Check command line arguments 
  */
  verbose = 1;
  iterative = 0;
  while ((c = getopt(argc, argv, "ivhHqo:")) != EOF)
  switch (c) {
    case 'i':
      iterative = 1;
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
    case 'h':
      help() ;
      errflg++;
      break ;
    case 'H':
      help() ;
      errflg++;
      break ;
    case '?':
      errflg++; 
  }
  if (errflg) {
    (void)fprintf(stderr, "usage: pathrate_snd [-i] [-H|-h] [-q|-v] [-o <filename>] \n");
    exit (-1);
  } 
  if (file){
    pathrate_fp = fopen(filename,"w");
    fprintf(pathrate_fp, "\n\n");
  }

  /* Control stream: TCP connection */
  if ((sock_tcp=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket(AF_INET,SOCK_STREAM,0):");
    exit(-1);
  }
  opt_len=1;
  if (setsockopt(sock_tcp, SOL_SOCKET, SO_REUSEADDR, (char*)&opt_len, sizeof(opt_len)) < 0) {
    perror("setsockopt(SOL_SOCKET,SO_REUSEADDR):");
    exit(-1);
  }
  bzero((char*)&snd_tcp_addr, sizeof(snd_tcp_addr));
  snd_tcp_addr.sin_family         = AF_INET;
  snd_tcp_addr.sin_addr.s_addr    = htonl(INADDR_ANY);
  snd_tcp_addr.sin_port           = htons(TCPSND_PORT);
  if (bind(sock_tcp, (struct sockaddr*)&snd_tcp_addr, sizeof(snd_tcp_addr)) < 0) {
    perror("bind(sock_tcp):");
    exit(-1);
  }
  if (listen(sock_tcp,1) < 0) {
    perror("listen(sock_tcp,1):");
    exit(-1);
  }
  
  
  /* 
    Data stream: UDP socket 
  */
  if ((sock_udp=socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket(AF_INET,SOCK_DGRAM,0):");
    exit(-1);
  }
  bzero((char*)&snd_udp_addr, sizeof(snd_udp_addr));
  snd_udp_addr.sin_family         = AF_INET;
  snd_udp_addr.sin_addr.s_addr    = htonl(INADDR_ANY);
  snd_udp_addr.sin_port           = htons(0);
  if (bind(sock_udp, (struct sockaddr*)&snd_udp_addr, sizeof(snd_udp_addr)) < 0) {
    perror("bind(sock_udp):");
    exit(-1);
  }
  send_buff_sz = UDP_BUFFER_SZ;
  if (setsockopt(sock_udp, SOL_SOCKET, SO_SNDBUF, (char*)&send_buff_sz, sizeof(send_buff_sz)) < 0) {
    perror("setsockopt(SOL_SOCKET,SO_SNDBUF):");
    exit(-1);
  }
  
  
  /*
          Check if select can give (roughly) sub-second time intervals
  */
  sleep_time.tv_sec = 0; 
  sleep_time.tv_usec = MIN_TRAIN_SPACING;
  gettimeofday(&prior_sleep,(struct timezone*)0);
  for (i=1;i<=10;i++){
    select(1,NULL,NULL,NULL,&sleep_time);
    sleep_time.tv_sec = 0;
    sleep_time.tv_usec=MIN_TRAIN_SPACING;
  }
  gettimeofday(&current_time,(struct timezone*)0);
  if ((time_to_us_delta(prior_sleep,current_time)<15*MIN_TRAIN_SPACING) &&
      (time_to_us_delta(prior_sleep,current_time)>5*MIN_TRAIN_SPACING))
    {sleep_usecs=1; train_spacing = MIN_TRAIN_SPACING;} 
  else 
    {sleep_usecs=0; train_spacing = TRAIN_SPACING_SEC;}
  
  
  do {
    if (file) fprintf(pathrate_fp,"\n\nWaiting for receiver to establish control stream => ");
    if (verbose) fprintf(stdout,"\n\nWaiting for receiver to establish control stream => ");
    fflush(stdout);
    
    /* 
      Wait until receiver attempts to connect, starting new measurement cycle
    */
    rcv_tcp_adrlen = sizeof(rcv_tcp_addr);
    ctr_strm = accept(sock_tcp, (struct sockaddr*)&rcv_tcp_addr, &rcv_tcp_adrlen);
    if (ctr_strm < 0) {
       perror("accept(sock_tcp):");
       exit(-1);
    }
    if (verbose) printf("OK\n");
    if (file) fprintf(pathrate_fp,"OK\n");
    
    localtm = time(NULL); gethostname(pack_buf, 256);
    host_rcv=gethostbyaddr((char*)&(rcv_tcp_addr.sin_addr), 
    sizeof(rcv_tcp_addr.sin_addr), AF_INET); 
    if (host_rcv!=NULL){
      if (file) fprintf(pathrate_fp,"Receiver %s starts measurements on %s", 
          host_rcv->h_name, ctime(&localtm));
      if (verbose) printf("Receiver %s starts measurements on %s", 
          host_rcv->h_name, ctime(&localtm));
    }
    else{
      if (file) fprintf(pathrate_fp,"Unknown receiver starts measurements on %s", 
          ctime(&localtm));
      if (verbose) printf("Unknown receiver starts measurements on %s", 
          ctime(&localtm));
    }
    
    
    /*
      Form receiving UDP address
    */
    bzero((char*)&rcv_udp_addr, sizeof(rcv_udp_addr));
    rcv_udp_addr.sin_family         = AF_INET;
    rcv_udp_addr.sin_addr.s_addr    = rcv_tcp_addr.sin_addr.s_addr; 
    rcv_udp_addr.sin_port           = htons(UDPRCV_PORT);
    
    
    
    /* 
      Bounce a number of empty messages back to the receiver, in order to measure RTT 
    */
    for (i=0; i<10; i++) {
      ctr_code=recv_ctr_msg(ctr_strm, ctr_buff);
      send_ctr_msg(ctr_strm, ctr_code);
    }
    
    /* 
      Create random packet payload to deal with links that do payload compression
    */
    srandom(getpid());
    for (i=0; i<MAX_PACK_SZ-1; i++) 
      random_data[i]=(char)(random()&0x000000ff);
    bzero((char*)&pack_buf, MAX_PACK_SZ);
    memcpy(pack_buf+2*sizeof(long), random_data, (MAX_PACK_SZ-1)-2*sizeof(sizeof(long)));
    
    
    if (file) fprintf(pathrate_fp, "Measurements are in progress. Please wait..\n");
    if (verbose) printf("Measurements are in progress. Please wait..\n");
    fflush(stdout);
    
    
    
    
    /*
      loop: 
        1) Get control messages for next phase (until SEND command)
        2) Send packets for that phase (until complete, or CONTINUE command)
    */
    
    reset_flag=0; 
    train_id=0;
    trains_lost=0;
    done=0;
    
    while(!done) {
      do {
        /* Wait until a control message arrives (unless if reset_flag=1) */
        /* If reset_flag=1, a control message is already here */
        if (!reset_flag) {
          ctr_code=recv_ctr_msg(ctr_strm, ctr_buff);
        }
        reset_flag=0;
    
        /* Get the command and the data fields from the control message */
        ctr_code_cmnd = ctr_code & 0x000000ff;
        ctr_code_data = ctr_code >> 8;
    
        switch(ctr_code_cmnd) { 
          /* Get maximum packet size from receiver */
          case MAX_PCK_LEN:
            max_pack_sz=ctr_code_data;
            if(Verbose){
              printf("--> Maximum packet size: %d bytes \n", max_pack_sz+28);
              if (file)
              fprintf(pathrate_fp,"--> Maximum packet size: %d bytes \n", max_pack_sz+28);
            }
            break;
      
          /* Get packet size from receiver */
          case PCK_LEN:
            pack_sz=ctr_code_data;
            if (Verbose){
              printf("--> Packet size: %d bytes \n", pack_sz+28);
              if (file)
                fprintf(pathrate_fp,"--> Packet size: %d bytes \n", pack_sz+28);
            }
            break;
      
          /* Get train length from receiver */
          case TRAIN_LEN:
            train_len=ctr_code_data;
            if (Verbose){
              printf("--> New train length: %d\n", train_len);
              if (file) 
                 fprintf(pathrate_fp,"--> New train length: %d\n", train_len);
            }
            break;
      
          /* Get number of trains from receiver */
          case NO_TRAINS:
            //no_trains=ctr_code_data;
            no_trains=1;
            if (Verbose){
              printf("--> New number of trains: %d\n", no_trains);
              if (file) 
                fprintf(pathrate_fp,"--> New number of trains: %d\n", no_trains);
            }
            break;
      
          /* End of measurements */
          case GAME_OVER:
            localtm = time(NULL); 
            if (file) fprintf(pathrate_fp,"Receiver terminates measurements on %s", ctime(&localtm));
            if (verbose) printf("Receiver terminates measurements on %s", ctime(&localtm));
            close(ctr_strm);
            sleep(1);
            done=1;
            break;
      
          /* Skip sending trains and go to next input phase of control messages */ 
          case CONTINUE:
            if (Verbose) {
              printf("--> Continue with next round of measurements \n");
              if (file)
                fprintf(pathrate_fp,"--> Continue with next round of measurements \n");
            }
            break;
      
          /* An ACK for a packet train; ignore at this point  */
          case NEG_ACK_TRAIN:
            if (Verbose) {
              printf("--> Redundant NEG_ACK \n");
              if (file) 
                fprintf(pathrate_fp,"--> Redundant NEG_ACK \n");
            }
            break;
      
          case ACK_TRAIN:
            if (Verbose) {
              printf("--> Redundant ACK \n");
              if (file)
                 fprintf(pathrate_fp,"--> Redundant ACK \n");
            }
            break;
      
          /* Train spacing between successive packet pairs/trains */
          case TRAIN_SPACING:
            train_spacing=ctr_code_data; /* msec */
            if (sleep_usecs) {
              sleep_time.tv_sec = train_spacing/1000;
              sleep_time.tv_usec = train_spacing*1000 - sleep_time.tv_sec*1000000;
              if (Verbose) {
                printf("Time period between packet pairs/trains: %d msec\n",train_spacing); 
                if (file) 
                  fprintf(pathrate_fp,"Time period between packet pairs/trains: %d msec\n",train_spacing); 
              }
            }
            else {
              sleep_secs = (int)floor(train_spacing/1000.)+1; 
              if (Verbose) {
                printf("Time period between packet pairs/trains: %d msec\n",sleep_secs*1000);
                if (file)
                   fprintf(pathrate_fp,"Time period between packet pairs/trains: %d msec\n",sleep_secs*1000);
              }
            }
            break;
      
          /* Start sending the packet trains with the specified
            packet size, train length, and number of trains */ 
          case SEND:
            round_id=ctr_code_data;
            if (Verbose) {
              printf("--> New round number: %d\n", round_id);
              if (file) 
                 fprintf(pathrate_fp,"--> New round number: %d\n", round_id);
            }
            break;
      
          default:
            printf("Unknown control message.. aborting\n");
            done=1;
        }
      } while(ctr_code_cmnd!=SEND && !done);
    
      if (ctr_code_cmnd==SEND) {
         /* Send <no_trains> of length <train_len> with packets of size <pack_sz>.
         * NOTE: We always send one more packet in the train. The first packet
         * (and the corresponding spacing) is ignored, because the processing 
         * of that packet takes longer (due to cache misses). 
         * That first packet has pack_id=0.  */

         train_no=0; trains_ackd=0; reset_flag=0;
         do {
           /* Send train of <train_len> packets. 
            * Each packet carries a packet id (unique in each train),
            * a train id (unique in each round), and a round id (unique
            * in the entire execution).  */
           train_id++; 
           train_id_n = htonl(train_id);
           memcpy(pack_buf+sizeof(long),&train_id_n,sizeof(long));
           round_id_n = htonl(round_id);
           memcpy(pack_buf+2*sizeof(long),&round_id_n,sizeof(long));
           for (pack_id=0; pack_id <= train_len; pack_id++) {
             pack_id_n = htonl(pack_id);
             memcpy(pack_buf, &pack_id_n, sizeof(long));
             sendto(sock_udp, pack_buf, pack_sz, 0, (struct sockaddr*)&rcv_udp_addr,
                sizeof(rcv_udp_addr));
           }
           train_no++;
           if (Verbose) {
              printf("\tTrain-%d:", train_no);
              if (file)
                 fprintf(pathrate_fp,"\tTrain-%d:", train_no);
           }
           
           /* Introduce some delay between consecutive packet trains */
           if (sleep_usecs){
              select(1, NULL, NULL, NULL, &sleep_time);
              sleep_time.tv_sec = train_spacing/1000;
              sleep_time.tv_usec = train_spacing*1000 - sleep_time.tv_sec*1000000;
           }
           else 
             sleep(sleep_secs);
    
           /* Send SENT_TRAIN ctr msgs and wait for ACK/NEG_ACK message */
           ctr_code = SENT_TRAIN;
           send_ctr_msg(ctr_strm,ctr_code);

           ctr_code=recv_ctr_msg(ctr_strm,ctr_buff);
           if ((ctr_code & 0x000000ff)==ACK_TRAIN) {   /* the train has been ACKed */
             trains_ackd++;
             trains_lost=0;
             if (Verbose) {
               printf("  => ACKed\n");
               if (file)
                  fprintf(pathrate_fp,"  => ACKed\n");
             }
           }
           else if ((ctr_code & 0x000000ff)==NEG_ACK_TRAIN) {
             /* Received negative ack, Retransmit */
             if (Verbose){
               printf("  => Retransmit\n");
               if (file)
                  fprintf(pathrate_fp,"  => Retransmit\n");
             }
             /* Primitive congestion avoidance */
             if (++trains_lost > MAX_CONSEC_LOSSES) {
               if (file){
                 fprintf(pathrate_fp,"Too many losses.."); 
                 fprintf(pathrate_fp,"Aborting measurements to avoid congestion \n\n");
               }
               if (verbose){
                 printf("Too many losses.."); 
                 printf("Aborting measurements to avoid congestion \n\n");
               }
               /* Send GAME_OVER signal to rcvr on TCP, then  quit*/
               ctr_code = GAME_OVER;
               send_ctr_msg(ctr_strm,ctr_code);
               sleep(1);
               close(ctr_strm);
               done=1;
             }
           }
           else {   
             /* The receiver has already started a new phase, 
              * and sends different control messages. 
              * Abort this phase, and start the next. */
             printf("DEBUG:: Enter here with code %x\n", (ctr_code & 0x000000FF));
             reset_flag=1;
             trains_lost=0;
           }
         } while (trains_ackd < no_trains && !reset_flag && !done);
      } /* if (ctr_command==SEND) */
    } /* while (!done)  */
  } while(iterative);  /* Repeat forever if in iterative mode */
  
  
  exit(0); 
}
