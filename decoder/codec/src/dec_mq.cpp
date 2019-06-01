/***************************************************************************************
 *
 * Copyright (C) 2004 Broad Motion, Inc.
 *
 * All rights reserved.
 *
 * http://www.broadmotion.com
 *
 **************************************************************************************/

#include <string.h>
#include "dec_stru.h"

#include <assert.h>
#include <stdio.h>

extern FILE * file_mq_a2;
extern FILE * file_mq_c2;

// table C.2: {Qe, NMPS, NLPS, SW}
const unsigned int table_mq[47][4] = {
  {0x5601,  1,  1, 1},
  {0x3401,  2,  6, 0},
  {0x1801,  3,  9, 0},
  {0x0ac1,  4, 12, 0},
  {0x0521,  5, 29, 0},
  {0x0221, 38, 33, 0},
  {0x5601,  7,  6, 1},
  {0x5401,  8, 14, 0},
  {0x4801,  9, 14, 0},
  {0x3801, 10, 14, 0},
  {0x3001, 11, 17, 0},
  {0x2401, 12, 18, 0},
  {0x1c01, 13, 20, 0},
  {0x1601, 29, 21, 0},
  {0x5601, 15, 14, 1},
  {0x5401, 16, 14, 0},
  {0x5101, 17, 15, 0},
  {0x4801, 18, 16, 0},
  {0x3801, 19, 17, 0},
  {0x3401, 20, 18, 0},
  {0x3001, 21, 19, 0},
  {0x2801, 22, 19, 0},
  {0x2401, 23, 20, 0},
  {0x2201, 24, 21, 0},
  {0x1c01, 25, 22, 0},
  {0x1801, 26, 23, 0},
  {0x1601, 27, 24, 0},
  {0x1401, 28, 25, 0},
  {0x1201, 29, 26, 0},
  {0x1101, 30, 27, 0},
  {0x0ac1, 31, 28, 0},
  {0x09c1, 32, 29, 0},
  {0x08a1, 33, 30, 0},
  {0x0521, 34, 31, 0},
  {0x0441, 35, 32, 0},
  {0x02a1, 36, 33, 0},
  {0x0221, 37, 34, 0},
  {0x0141, 38, 35, 0},
  {0x0111, 39, 36, 0},
  {0x0085, 40, 37, 0},
  {0x0049, 41, 38, 0},
  {0x0025, 42, 39, 0},
  {0x0015, 43, 40, 0},
  {0x0009, 44, 41, 0},
  {0x0005, 45, 42, 0},
  {0x0001, 45, 43, 0},
  {0x5601, 46, 46, 0}
};

#if !defined(USE_MARCO_MQ)
static int mqd_bytein(stru_mqd *mqd);
static int mqd_renorme(stru_mqd *mqd);

/**************************************************************************************/
/* static                               mqd_bytein                                    */
/**************************************************************************************/
static int mqd_bytein(stru_mqd *mqd)
{
  if(*mqd->buffer == 0xff) {
    if(*(mqd->buffer + 1) > 0x8f) {
      mqd->ct =  8;
	  mqd->c += 0xff00;
    } else {
      ++mqd->buffer;
      mqd->c  += (*mqd->buffer << 9);
      mqd->ct =  7;
    }
  } else {
    ++mqd->buffer;
    mqd->c  +=   (*mqd->buffer) << 8;
    mqd->ct =    8;
  }

  return 0;
}

/**************************************************************************************/
/* static                              mqd_renorme                                    */
/**************************************************************************************/
static int mqd_renorme(stru_mqd *mqd)
{
  do {
    if(!mqd->ct)    mqd_bytein(mqd);
    mqd->a <<= 1;
    mqd->c <<= 1;
    --mqd->ct;
  } while(!(mqd->a & 0x8000));

  return 0;
}
#endif

/**************************************************************************************/
/*                                        mqd_reset                                   */
/**************************************************************************************/
int mqd_reset(stru_mqd *mqd)
{
  // table D.7
  memset(mqd->cx_states, 0, sizeof(mqd->cx_states));

  mqd->cx_states[ 0][0] = 4;
  mqd->cx_states[17][0] = 3;
  mqd->cx_states[18][0] = 46;

  return 0;
}

/**************************************************************************************/
/*                                        mqd_init                                    */
/**************************************************************************************/
int mqd_init(stru_mqd *mqd, unsigned char *buffer)
{
  mqd->buffer =   buffer;
  mqd->c      =   (*mqd->buffer) <<16;

  mqd_bytein(mqd);

  mqd->a      =   0x8000;
  mqd->c      <<= 7;
  mqd->ct     -=  7;

  return 0;
}

/**************************************************************************************/
/*                                     mqd_init_raw                                   */
/**************************************************************************************/
int mqd_init_raw(stru_mqd *mqd, unsigned char *buffer)
{
  mqd->buffer = buffer;
  mqd->c      = 0;
  mqd->ct     = 0;

  return 0;
}

#if !defined(USE_MARCO_MQ)
/**************************************************************************************/
/*                                        mqd_dec                                     */
/**************************************************************************************/
void mqd_dec(stru_mqd *mqd, unsigned short *bit, unsigned char ctx)
{
  unsigned int idx;
  unsigned int qe;

  idx   = mqd->cx_states[ctx][0];
  *bit  = mqd->cx_states[ctx][1];
  qe    = table_mq[idx][0];


  mqd->a -= qe;

  if((mqd->c >> 16) < qe) {
    if(mqd->a < qe) {
      mqd->cx_states[ctx][0] = table_mq[idx][1];
    } else {
      *bit ^=  1;
      mqd->cx_states[ctx][1] ^= table_mq[idx][3];
      mqd->cx_states[ctx][0] = table_mq[idx][2];
    }

    mqd->a = qe;
    mqd_renorme(mqd);
  } else {
    mqd->c -= (qe << 16);

	if(!(mqd->a & 0x8000)) {
      if(mqd->a < qe) {
        *bit ^= 1;
        mqd->cx_states[ctx][1] ^= table_mq[idx][3];
        mqd->cx_states[ctx][0] = table_mq[idx][2];
      } else {
        mqd->cx_states[ctx][0] = table_mq[idx][1];
      }

      mqd_renorme(mqd);
    }
  }

#if defined(B_FPGA_MQ_TB)
		  fprintf(file_mq_a2, "%08X\n", mqd->a);
		  fprintf(file_mq_c2, "%08X\n", mqd->c);
#endif

}

/**************************************************************************************/
/*                                      mqd_dec_raw                                   */
/**************************************************************************************/
void mqd_dec_raw(stru_mqd *mqd, unsigned short *bit)
{
  if(!mqd->ct) {
    if(mqd->c == 0xFF) {
      mqd->ct = 7;
      mqd->c = *mqd->buffer++;
      if(mqd->c == 0xFF) {    // check terminating marker: 0xffff, should never happen
        mqd->ct = 8;
        --mqd->buffer;
      }
    } else {
      mqd->ct = 8;
      mqd->c = *mqd->buffer++;
    }
  }

  --mqd->ct;
  *bit = (mqd->c >> mqd->ct) & 0x1;
}
#endif

/**************************************************************************************/
/*                                        mqd_flush                                   */
/**************************************************************************************/
int mqd_flush(stru_mqd *mqd, int term, int pterm)
{
  // error checking if needed

  return 0;
}

/**************************************************************************************/
/*                                    mqd_flush_raw                                   */
/**************************************************************************************/
int mqd_flush_raw(stru_mqd *mqd, int term)
{
  // error checking if needed

  return 0;
}
