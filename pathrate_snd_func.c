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
#include "pathrate_snd.h"

/* 
	Receive a message from the control stream 
*/
long recv_ctr_msg(int ctr_strm, char *ctr_buff) 
{ 
	long ctr_code;
  if (read(ctr_strm, ctr_buff, sizeof(long)) != sizeof(long)) 
	  return(-1);
	memcpy(&ctr_code, ctr_buff, sizeof(long)); 
	return(ntohl(ctr_code));
}

/*
    Send an empty message to the control stream
*/
void send_ctr_msg(int ctr_strm, long ctr_code) 
{
  char ctr_buff[8];
  long ctr_code_n = htonl(ctr_code);
  memcpy(ctr_buff, &ctr_code_n, sizeof(long));
  if (write(ctr_strm, ctr_buff, sizeof(long)) != sizeof(long)) {
    fprintf(stderr, "send control message failed:\n");
    exit(-1);
  }
}

/*
    Compute the time difference in microseconds between two timeval measurements
*/
double time_to_us_delta(struct timeval tv1, struct timeval tv2)
{
	double time_us;
	time_us= (double) ((tv2.tv_sec-tv1.tv_sec)*1000000 + (tv2.tv_usec-tv1.tv_usec));
	return time_us;
}

/*
 *  Help
 *  */
void help(){
  fprintf (stderr,"pathrate_snd options\n");
  fprintf (stderr,"-i        : iterative mode\n");
  fprintf (stderr,"-q        : quite mode\n");
  fprintf (stderr,"-v        : verbose mode\n");
  fprintf (stderr,"-o <file> : print log in file\n");
  fprintf (stderr,"-H|-h     : print this help and exit\n");
}
