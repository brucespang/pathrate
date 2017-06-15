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

EXTERN int iterative;
EXTERN long recv_ctr_msg(int ctr_strm, char *ctr_buff) ;
EXTERN void send_ctr_msg(int ctr_strm, long ctr_code) ;
EXTERN double time_to_us_delta(struct timeval tv1, struct timeval tv2);
EXTERN void help(void);
