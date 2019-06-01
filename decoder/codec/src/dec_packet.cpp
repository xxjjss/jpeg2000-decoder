/***************************************************************************************
 *
 * Copyright (C) 2004 Broad Motion, Inc.
 *
 * All rights reserved.
 *
 * http://www.broadmotion.com
 *
 **************************************************************************************/

#if defined(WIN32)
  #include <windows.h>
#else
#define UINT_MAX	0xffffffff
#endif

#include "dec_stru.h"
#include "debug.h"

static inline int dec_num_coding_pass(stru_stream *stream);
static int dec_prec(stru_opt *s_opt, stru_stream *stream, stru_precinct *prec,
                    int layer);
static int dec_restore_coding_pass(stru_stream *stream, stru_precinct *prec);
static int dec_read_packet( stru_opt *s_opt, stru_stream *stream,
                            stru_precinct *prec1, stru_precinct *prec2, stru_precinct *prec3,
                            int resolution, int layer, short pkt_idx, int skip_to_next_component);
static int dec_lrcp(stru_dec_param *dec_param, stru_opt *s_opt, stru_stream *stream, int poc_index );
static int dec_rlcp(stru_dec_param *dec_param, stru_opt *s_opt, stru_stream *stream, int poc_index );
static int dec_rpcl(stru_dec_param *dec_param, stru_opt *s_opt, stru_stream *stream, int poc_index );
static int dec_pcrl(stru_dec_param *dec_param, stru_opt *s_opt, stru_stream *stream, int poc_index );
static int dec_cprl(stru_dec_param *dec_param, stru_opt *s_opt, stru_stream *stream, int poc_index );
static void param_switch  (stru_dec_param *dec_param, int comp_id) ;

/**************************************************************************************/
/* static                          dec_num_coding_pass                                */
/**************************************************************************************/
static inline int dec_num_coding_pass(stru_stream *stream)
{
//   int rc;
// 
//   // Table B.4
//   rc = ostream_get_bit(stream);
//   if(rc == 0)     { rc = 1;     return rc; }
//   rc = ostream_get_bit(stream);
//   if(rc == 0)     { rc = 2;     return rc; }
//   rc = ostream_get_int(stream, 2);
//   if(rc != 3)     { rc += 3;    return rc; }
//   rc = ostream_get_int(stream, 5);
//   if(rc != 31)    { rc += 6;    return rc; }
//   rc = ostream_get_int(stream, 7);
//                   { rc += 37;   return rc; }
// 
	int rc = 1; 
	if ((rc += ostream_get_bit(stream)) == 2 )
		if ((rc +=  ostream_get_bit(stream)) == 3)
			if((rc +=  ostream_get_int(stream, 2)) >= 6)
				if((rc +=  ostream_get_int(stream, 5)) >= 37)
					rc +=  ostream_get_int(stream, 7);

	return rc;
}

/**************************************************************************************/
/* static                             dec_prec                                        */
/**************************************************************************************/
static int dec_prec(stru_opt *s_opt, stru_stream *stream, stru_precinct *prec,
                    int layer)
{
  int included, lblock_inc, cbw_num, cbh_num, cb_num, pass_num, cumlen, log_t2bypass;
  stru_coding_pass *pass1, *pass2;
  stru_code_block *cur_cb;

//   DEBUG_PRINT(("prec at R%d B%d has CB[%d %d]\n ", prec->resolution, prec->subband, prec->ncb_bw, prec->ncb_ph));

  for(cbh_num = 0; cbh_num < prec->ncb_ph; ++cbh_num) {
    for(cbw_num = 0; cbw_num < prec->ncb_pw; ++cbw_num) {
      cb_num = cbw_num + cbh_num * prec->ncb_pw;
      cur_cb = prec->codeblock + cbw_num + cbh_num * prec->ncb_bw;

      pass1 = cur_cb->pass + cur_cb->idx_t2pass;

      if(!cur_cb->has_t2pass) {       // first time inclusion
        tagtree_dec(stream, prec->incltree->node + cb_num, layer + 1, &included);
      } else {                        // has been included at least once before
        included = ostream_get_bit(stream);
      }

      if(!included)   continue;

      if(!cur_cb->has_t2pass) {

// 		  while(prec->imsbtree->node->low == prec->imsbtree->node->value)
// 		  {
			  tagtree_dec(stream, prec->imsbtree->node + cb_num, UINT_MAX /*prec->imsbtree->node->low + 1*/, &included);
// 		  }
		  cur_cb->missing_bit_plan = (prec->imsbtree->node + cb_num)->value;
      }

      // error resilience, seek to next packet
//      if((prec->incltree->node + cb_num)->value != layer || cur_cb->missing_bit_plan > 74) {
      if(cur_cb->missing_bit_plan > 74) {
        cur_cb->missing_bit_plan = 0;
        return -1;
      }

      //code block included, skip to first relevant pass
      cur_cb->has_t2pass = 1;
      pass1->layer = layer;
      pass1->num_t2layer = cur_cb->num_t2pass = dec_num_coding_pass(stream);
      cur_cb->num_pass += cur_cb->num_t2pass;



      // error resilience, seek to next packet
      if(cur_cb->num_pass > (1 << 7)) {
        cur_cb->num_pass = 0;
        return -1;
      }

      lblock_inc = ostream_get_bit(stream);
      while(lblock_inc) {
        lblock_inc = ostream_get_bit(stream);
        ++cur_cb->lblock;
      }


      // decode coding pass length
      if(s_opt->conf_termall) {
        cumlen = (cur_cb->idx_t2pass) ? (pass1 - 1)->cumlen : 0;
        for(pass_num = cur_cb->num_t2pass; pass_num; --pass_num, ++pass1) {
          pass1->cumlen = ostream_get_int(stream, cur_cb->lblock);
          if(pass1->cumlen > (1 << 15))   return -1;        // error resilience, seek to next packet
          pass1->cumlen += cumlen;
          cumlen = pass1->cumlen;
        }
      } else if(!s_opt->conf_bypass) {
        pass2 = pass1 + cur_cb->num_t2pass - 1;
        cumlen = (cur_cb->idx_t2pass) ? (pass1 - 1)->cumlen : 0;
        pass2->cumlen = ostream_get_int(stream, cur_cb->lblock + int_log2(cur_cb->num_t2pass));
        if(pass2->cumlen > (1 << 15))   return -1;          // error resilience, seek to next packet
        pass2->cumlen += cumlen;
      } else {
        cumlen = (cur_cb->idx_t2pass) ? (pass1 - 1)->cumlen : 0;
        for(pass2 = pass1, pass_num = cur_cb->idx_t2pass;
            pass_num < cur_cb->idx_t2pass + cur_cb->num_t2pass; ++pass_num, ++pass2) {

          if( pass_num != cur_cb->idx_t2pass + cur_cb->num_t2pass - 1 &&
              (pass_num < 9 || pass_num % 3 == 1))    continue;

          log_t2bypass = int_log2((unsigned int)(pass2 - pass1 + 1));
          pass2->cumlen = ostream_get_int(stream, cur_cb->lblock + log_t2bypass);
          if(pass2->cumlen > (1 << 15))   return -1;        // error resilience, seek to next packet
          pass2->cumlen += cumlen;
          cumlen = pass2->cumlen;
          pass1 = pass2 + 1;
        }
      }
// 	  {
// 		  // 		  static int nn = 0;
// 		  DEBUG_PRINT(("  == Set cb[%d:%d] on[%d:%d] Size[%d:%d] Bp:%d Np:%d \n", cbw_num, cbh_num, cur_cb->x0, cur_cb->y0, cur_cb->width, cur_cb->height,cur_cb->missing_bit_plan, cur_cb->num_pass));
// 		  // 		  DEBUG_PAUSE( nn % 100 == 0 && nn > 3300); //|| nn == 537*/
// 	  }
    }
  }

  return 0;
}

/**************************************************************************************/
/* static                        dec_restore_coding_pass                               */
/**************************************************************************************/
static int dec_restore_coding_pass(stru_stream *stream, stru_precinct *prec)
{
  int cbw_num, cbh_num, cumlen = 0;
  stru_code_block *cur_cb;
  stru_coding_pass *pass1, *pass2;
//   DEBUG_PRINT(("    == prec at R%d B%d has CB[%d %d]\n", prec->resolution, prec->subband, prec->ncb_bw, prec->ncb_ph));

  for(cbh_num = 0; cbh_num < prec->ncb_ph; ++cbh_num) {
    for(cbw_num = 0; cbw_num < prec->ncb_pw; ++cbw_num) {
      cur_cb = prec->codeblock + cbw_num + cbh_num * prec->ncb_bw;

#if _DEBUG
// 		DEBUG_PAUSE(cbw_num==0 && cbh_num==2 && cur_cb->x0==60 && cur_cb->y0==132 && cur_cb->width==60 && cur_cb->height == 3 &&cur_cb->missing_bit_plan==6 && cur_cb->num_pass ==14 && cur_cb->num_t2pass==14);
// 	  DEBUG_PRINT(("      == Set cb[%d:%d] on[%d:%d] Size[%d:%d] Bp:%d Np:%d T2p:%d Buf:", cbw_num, cbh_num, cur_cb->x0, cur_cb->y0, cur_cb->width, cur_cb->height,cur_cb->missing_bit_plan, cur_cb->num_pass, cur_cb->num_t2pass));
	  int check = cur_cb->num_t2pass;
#endif

      if(cur_cb->num_t2pass) {
        pass1 = cur_cb->pass + cur_cb->idx_t2pass;
        pass2 = pass1 + cur_cb->num_t2pass - 1;
        cumlen = (cur_cb->idx_t2pass) ? (pass1 - 1)->cumlen : 0;

//         pass1->buffer = NULL;
        pass1->buffer = ostream_restore_bytes(stream, pass2->cumlen - cumlen);

        cur_cb->idx_t2pass += cur_cb->num_t2pass;
        cur_cb->num_t2pass = 0;
      }
// #if _DEBUG
// 		{
// 			// 		  static int nn = 0;
// 			if (check)
// 			{
// 				DEBUG_PRINT(("<len: %d> %02x %02x %02x ....\n", pass2->cumlen - cumlen, pass1->buffer[0], pass1->buffer[1], pass1->buffer[2]));
// 			}
// 			else
// 			{
// 				DEBUG_PRINT(("NULL\n"));
// 			}
// 			// 		  DEBUG_PAUSE( nn % 100 == 0 && nn > 3300); //|| nn == 537*/
// 		}
// #endif   
	}
  }

  return 0;
}

/**************************************************************************************/
/* static                         dec_read_packet                                     */
/**************************************************************************************/
static int dec_read_packet( stru_opt *s_opt, stru_stream *stream,
                            stru_precinct *prec1, stru_precinct *prec2, stru_precinct *prec3,
                            int resolution, int layer, short pkt_idx, int skip_to_next_component)
{
  int empty;
  int len, type, content;

  if ( skip_to_next_component)
  {
 //exclude_stuff_bytes = true, bool skip_to_marker = false
	  int code;
	  int length;
	  int force_skip = 0;
	  int valid_code = 0;
	  while (1)
	  {
		  do {
			  length = 0;
			  force_skip = 0;
			  int byte = ostream_read_bytes(stream, 1);
			  if (byte == MASK_CTX)
			  {
				  valid_code = 1;
				  code = ostream_read_bytes(stream, 1);
				  code += (byte<<8); 
				  if ((code == MK_SOP /*sop*/) || (code == MK_SOT /*SOT*/))
				  { // Want to be really sure that this is not a corrupt marker code.
					  length = ostream_read_bytes(stream, 2);

					  if (code == MK_SOT)
					  {
						  if (length != 10)
						  {
							  return -1;
						  }
						  ostream_skip(stream, -4); // restore the SOT marker
						  return CHANGE_TILE;

					  }
					  else if (code ==  MK_SOP)
					  {
						  if (length != 4)
						  {
							  return -1;
						  }
						  // to do : Found the required SOP segment
					  }
				  }
				  else if (code == MK_EOC /*EOC*/)
				  {
					  byte = ostream_read_bytes(stream, 1);
					  // todo : to support EOC
					  // 					  if (!codestream->resilient)
					  // 					  {
					  // 						  source->putback(byte);
					  // 						  source->terminate_prematurely();
					  // 						  length = 0; code = 0;
					  // 						  return false;
					  // 					  }
					  // 					  else
					  {
						  return CODE_STREAM_END;
					  }

				  }
				  else if ((code >= MK_SKIP_MIN) && (code <= MK_SKIP_MAX))
				  { // These codes are all to be skipped.
					  force_skip = true;
				  }
				  else if (code == MK_COM)	// comment only, skip
				  {
					  force_skip = true;
					  length = ostream_read_bytes(stream, 2);
					  ostream_skip(stream, length-2);
				  }
				  else	if (  (code == MK_SOD /*SOD*/) ||code == MK_EPH /*EPH*/)
				  {
					  break;
				  }
				  else
				  {
					  // data section
					  valid_code = 0;
					  ostream_skip(stream, -2);
				  }
			  }
			  else
			  {
				  valid_code = 0;
				  ostream_skip(stream, -1);
				  break;
			  }
		  } while (force_skip);

		  if ( valid_code)
		  {
			  if (  (code == MK_SOC /*SOC*/) || (code == MK_SOD /*SOD*/) ||
			  		 (code == MK_EOC /*EOC*/) || (code == MK_EPH /*EPH*/)) 
				  
			  {
				  break; // Delimiter found. There is no marker segment.
			  }

			  if (length < 2)	return -1;
			  ostream_skip(stream, length-2);
		  }
		  else
		  {
			  break;
		  }

	  }
  }
  
  if(s_opt->conf_sop) 
  {                         // sop
    type    = ostream_read_bytes(stream, 2);
    len     = ostream_read_bytes(stream, 2);
    content = ostream_read_bytes(stream, 2);
    if(type != MK_SOP)    return -1;
  }

  empty = !ostream_get_bit(stream);

  // empty packet header need to be byte aligned too
  if(empty) {
    ostream_byte_align(stream);
    if(s_opt->conf_eph) {                       // eph
      type = ostream_read_bytes(stream, 2);
      if(type != MK_EPH)    return -1;
    }
    return 0;
  }

  // 3 bands if not first resolution
  dec_prec(s_opt, stream, prec1, layer);
  if(resolution) {
    dec_prec(s_opt, stream, prec2, layer);
    dec_prec(s_opt, stream, prec3, layer);
  }

  ostream_byte_align(stream);         // byte align packet header

  if(s_opt->conf_eph) {                         // eph
    type = ostream_read_bytes(stream, 2);
    if(type != MK_EPH)    return -1;
  }

  // restore coding pass data from stream, 3 bands if not first resolution
  dec_restore_coding_pass(stream, prec1);
  if(resolution) {
    dec_restore_coding_pass(stream, prec2);
    dec_restore_coding_pass(stream, prec3);
  }

  return 0;
}

/**************************************************************************************/
/* static                                dec_lrcp                                     */
/**************************************************************************************/
static int dec_lrcp(stru_dec_param *dec_param, stru_opt *s_opt, stru_stream *stream, int poc_index )
{
  int num_component = s_opt->num_component;
  int  l, c, r, pw, ph;
  short pkt_idx = -1;
  int ret,	skip_to_next_component = 0;

  stru_tile_component *cur_comp;
  stru_subband *band1, *band2, *band3;
  stru_precinct *prec1, *prec2, *prec3;


  int  layer_max = dec_param->layer;
  int  res_min = 0, res_max = dec_param->dwt_level;
  int  comp_min = 0, comp_max = num_component;

	if (poc_index != -1)
	{
		stru_pipe *s_pipe = (stru_pipe *)dec_param->mempool[0];
    stru_jpc_poc *s_m_poc = &s_pipe->s_jp2file.s_jp2stream.s_m_poc;
    stru_jpc_poc *s_t_poc = &s_pipe->s_jp2file.s_jp2stream.s_t_poc;

		if (s_t_poc->num_poc != 0){
		  layer_max = s_t_poc->LYEpoc[poc_index] > layer_max  ? layer_max   : s_t_poc->LYEpoc[poc_index];
		  res_min   = s_t_poc->RSpoc [poc_index];
		  res_max   = s_t_poc->REpoc [poc_index] > res_max    ? res_max + 1 : s_t_poc->REpoc[poc_index];
		  comp_min  = s_t_poc->CSpoc [poc_index];
		  comp_max  = s_t_poc->CEpoc [poc_index] > comp_max   ? comp_max    : s_t_poc->CEpoc[poc_index];
	    }
	  else {
		  layer_max = s_m_poc->LYEpoc[poc_index] > layer_max  ? layer_max   : s_m_poc->LYEpoc[poc_index];
		  res_min   = s_m_poc->RSpoc [poc_index];
		  res_max   = s_m_poc->REpoc [poc_index] > res_max    ? res_max + 1 : s_m_poc->REpoc[poc_index];
		  comp_min  = s_m_poc->CSpoc [poc_index];
		  comp_max  = s_m_poc->CEpoc [poc_index] > comp_max   ? comp_max    : s_m_poc->CEpoc[poc_index];
	  }
	}    

	if (s_opt->t2_scan_resolution > res_min)
	{
		res_min = s_opt->t2_scan_resolution;
	}
	if (s_opt->t2_scan_component > comp_min)
	{
		comp_min = s_opt->t2_scan_component;
	}

  for(l = 0; l < layer_max; ++l) {
    for(r = res_min; r <= res_max; ++r) {
		for(c = comp_min; c < comp_max; ++c) {
		  param_switch(dec_param, c);
		  cur_comp = &s_opt->component[c];
		  skip_to_next_component = c;
        if(r == 0) {
          band1 = cur_comp->band;
          band2 = cur_comp->band;
          band3 = cur_comp->band;
        } else {
          band1 = cur_comp->band + (r - 1) * 3 + 1;
          band2 = cur_comp->band + (r - 1) * 3 + 2;
          band3 = cur_comp->band + (r - 1) * 3 + 3;
        }

        for(ph = 0; ph < band1->nprec_bh; ++ph) {
          for(pw = 0; pw < band1->nprec_bw; ++pw) {
            prec1 = band1->precinct + pw + ph * band1->nprec_bw;
            prec2 = band2->precinct + pw + ph * band2->nprec_bw;
            prec3 = band3->precinct + pw + ph * band3->nprec_bw;
// 			printf("layer %d", l);
// 			printf(" Comp id %d", c);
// 			printf(" dwt level %d subband %d [%d:%d]\n", r, band1->subband, band1->width, band1->height);
// 
// 
// 			printf("Code block stripe [%d:%d] of [%d:%d]\n",pw + 1, ph + 1, band1->nprec_bw,band1->nprec_bh);

// 			DEBUG_PRINT(("  == Layer %d resolution %d component %d\n", l, r, c));
// 			DEBUG_PAUSE(c!= 0);
			ret = dec_read_packet(s_opt, stream, prec1, prec2, prec3, r, l, ++pkt_idx,   skip_to_next_component);
			if (ret )
			{
				// remember current scanning
				s_opt->t2_scan_component = c;
				s_opt->t2_scan_resolution = r;
				s_opt->t2_scan_layer = l;
				return ret; 

			}
          }
        }
      }
    }
  }
  return 0;
}

/**************************************************************************************/
/* static                                dec_rlcp                                     */
/**************************************************************************************/
static int dec_rlcp(stru_dec_param *dec_param, stru_opt *s_opt, stru_stream *stream, int poc_index)
{
  int num_component = s_opt->num_component;
  int l, r, c, pw, ph;
  short pkt_idx = -1;
  int ret,	skip_to_next_component = 0;

  stru_tile_component *cur_comp;
  stru_subband *band1, *band2, *band3;
  stru_precinct *prec1, *prec2, *prec3;

  for(r = 0; r <= dec_param->dwt_level; ++r) {
    for(l = 0; l < dec_param->layer; ++l) {
      for(cur_comp = s_opt->component, c = 0; c < num_component; ++c, ++cur_comp) {
		  param_switch(dec_param, c);
		  skip_to_next_component = c;
        if(r == 0) {
          band1 = cur_comp->band;
          band2 = cur_comp->band;
          band3 = cur_comp->band;
        } else {
          band1 = cur_comp->band + (r - 1) * 3 + 1;
          band2 = cur_comp->band + (r - 1) * 3 + 2;
          band3 = cur_comp->band + (r - 1) * 3 + 3;
        }

        for(ph = 0; ph < band1->nprec_bh; ++ph) {
          for(pw = 0; pw < band1->nprec_bw; ++pw) {
            prec1 = band1->precinct + pw + ph * band1->nprec_bw;
            prec2 = band2->precinct + pw + ph * band2->nprec_bw;
            prec3 = band3->precinct + pw + ph * band3->nprec_bw;

			ret = dec_read_packet(s_opt, stream, prec1, prec2, prec3, r, l, ++pkt_idx,   skip_to_next_component);
			if (ret == -1) return -1;
          }
        }
      }
    }
  }

  return 0;
}

/**************************************************************************************/
/* static                                dec_rpcl                                     */
/**************************************************************************************/
static int dec_rpcl(stru_dec_param *dec_param, stru_opt *s_opt, stru_stream *stream, int poc_index)
{
  int num_component = s_opt->num_component;
  int x, y, log_pw, log_ph, lv, pw_lv, ph_lv;
  int l, r, c, pw, ph;
  short pkt_idx = -1;
  int ret,	skip_to_next_component = 0;

  stru_tile_component *cur_comp;
  stru_subband *band1, *band2, *band3;
  stru_precinct *prec1, *prec2, *prec3;


  for(r = 0; r <= dec_param->dwt_level; ++r) {
    for(y = s_opt->ty0; y < s_opt->ty1; y += dec_param->prec_height - (y % dec_param->prec_height)) {
      for(x = s_opt->tx0; x < s_opt->tx1; x += dec_param->prec_width - (x % dec_param->prec_width)) {
        for(cur_comp = s_opt->component, c = 0; c < num_component; ++c, ++cur_comp) {
          param_switch(dec_param, c);
			skip_to_next_component = c;

          if(r == 0) {
            band1 = cur_comp->band;
            band2 = cur_comp->band;
            band3 = cur_comp->band;

            log_pw = int_log2(dec_param->prec_width) - 1;
            log_ph = int_log2(dec_param->prec_height) - 1;
          } else {
            band1 = cur_comp->band + (r - 1) * 3 + 1;
            band2 = cur_comp->band + (r - 1) * 3 + 2;
            band3 = cur_comp->band + (r - 1) * 3 + 3;

            log_pw = int_log2(dec_param->prec_width);
            log_ph = int_log2(dec_param->prec_height);
          }

          lv = dec_param->dwt_level - r;
          pw_lv = log_pw + lv;
          ph_lv = log_ph + lv;

          if((!(y % (s_opt->YRsiz[c] << ph_lv)) || ((y == s_opt->ty0) && ((band1->try0 << lv) % (1 << ph_lv)))) && 
             (!(x % (s_opt->XRsiz[c] << pw_lv)) || ((x == s_opt->tx0) && ((band1->trx0 << lv) % (1 << pw_lv))))
            ) {
            pw = floor_ratio(ceil_ratio(x, (s_opt->XRsiz[c] << lv)), (1 << log_pw)) - floor_ratio(band1->trx0, (1 << log_pw));
            ph = floor_ratio(ceil_ratio(y, (s_opt->YRsiz[c] << lv)), (1 << log_ph)) - floor_ratio(band1->try0, (1 << log_ph));

            for(l = 0; l < dec_param->layer; ++l) {
              prec1 = band1->precinct + pw + ph * band1->nprec_bw;
              prec2 = band2->precinct + pw + ph * band2->nprec_bw;
              prec3 = band3->precinct + pw + ph * band3->nprec_bw;

			  ret = dec_read_packet(s_opt, stream, prec1, prec2, prec3, r, l, ++pkt_idx,   skip_to_next_component);
			  if (ret == -1) return -1;

            }
          }
        }
      }
    }
#if defined(B_FPGA_TB)
    if(s_opt->res_align)  ostream_read_bytes(stream, ((4 - (stream->cur - stream->start) & 0x3) & 0x3));
#endif
  }

  return 0;
}

/**************************************************************************************/
/* static                                dec_pcrl                                     */
/**************************************************************************************/
static int dec_pcrl(stru_dec_param *dec_param, stru_opt *s_opt, stru_stream *stream, int poc_index)
{
  int num_component = s_opt->num_component;
  int x, y, log_pw, log_ph, lv, pw_lv, ph_lv;
  int l, r, c, pw, ph;
  short pkt_idx = -1;
  int ret, skip_to_next_component;

  stru_tile_component *cur_comp;
  stru_subband *band1, *band2, *band3;
  stru_precinct *prec1, *prec2, *prec3;

  for(y = s_opt->ty0; y < s_opt->ty1; y += dec_param->prec_height - (y % dec_param->prec_height)) {
    for(x = s_opt->tx0; x < s_opt->tx1; x += dec_param->prec_width - (x % dec_param->prec_width)) {
      for(cur_comp = s_opt->component, c = 0; c < num_component; ++c, ++cur_comp) {
		  param_switch(dec_param, c);
		  skip_to_next_component = c;
        for(r = 0; r <= dec_param->dwt_level; ++r) {
          if(r == 0) {
            band1 = cur_comp->band;
            band2 = cur_comp->band;
            band3 = cur_comp->band;

            log_pw = int_log2(dec_param->prec_width) - 1;
            log_ph = int_log2(dec_param->prec_height) - 1;
          } else {
            band1 = cur_comp->band + (r - 1) * 3 + 1;
            band2 = cur_comp->band + (r - 1) * 3 + 2;
            band3 = cur_comp->band + (r - 1) * 3 + 3;

            log_pw = int_log2(dec_param->prec_width);
            log_ph = int_log2(dec_param->prec_height);
          }

          lv = dec_param->dwt_level - r;
          pw_lv = log_pw + lv;
          ph_lv = log_ph + lv;

          if((!(y % (s_opt->YRsiz[c] << ph_lv)) || ((y == s_opt->ty0) && ((band1->try0 << lv) % (1 << ph_lv)))) && 
             (!(x % (s_opt->XRsiz[c] << pw_lv)) || ((x == s_opt->tx0) && ((band1->trx0 << lv) % (1 << pw_lv))))
            ) {
            pw = floor_ratio(ceil_ratio(x, (s_opt->XRsiz[c] << lv)), (1 << log_pw)) - floor_ratio(band1->trx0, (1 << log_pw));
            ph = floor_ratio(ceil_ratio(y, (s_opt->YRsiz[c] << lv)), (1 << log_ph)) - floor_ratio(band1->try0, (1 << log_ph));

            for(l = 0; l < dec_param->layer; ++l) {
              prec1 = band1->precinct + pw + ph * band1->nprec_bw;
              prec2 = band2->precinct + pw + ph * band2->nprec_bw;
              prec3 = band3->precinct + pw + ph * band3->nprec_bw;

			  ret = dec_read_packet(s_opt, stream, prec1, prec2, prec3, r, l, ++pkt_idx,   skip_to_next_component);
			  if (ret == -1) return -1;
            }
          }
        }
      }
    }
  }

  return 0;
}

/**************************************************************************************/
/* static                                dec_cprl                                     */
/**************************************************************************************/
static int dec_cprl(stru_dec_param *dec_param, stru_opt *s_opt, stru_stream *stream, int poc_index)
{
  int num_component = s_opt->num_component;
  int x, y, log_pw, log_ph, lv, pw_lv, ph_lv;
  int l, r, /*c,*/ pw, ph;
  short pkt_idx = -1;
  int ret, skip_to_next_component = 0;

  stru_tile_component *cur_comp;
  stru_subband *band1, *band2, *band3;
  stru_precinct *prec1, *prec2, *prec3;
//   stru_pipe * s_pipe = (stru_pipe *)(dec_param->mempool[0]);

  for(; s_opt->t2_scan_component < num_component; ++s_opt->t2_scan_component) {
	  int c = s_opt->t2_scan_component;
	  cur_comp = &s_opt->component[c];
	  param_switch(dec_param, c);
	  skip_to_next_component = c;
    for(y = s_opt->ty0; y < s_opt->ty1; y += dec_param->prec_height - (y % dec_param->prec_height)) {
      for(x = s_opt->tx0; x < s_opt->tx1; x += dec_param->prec_width - (x % dec_param->prec_width)) {
        for(r = 0; r <= dec_param->dwt_level; ++r) {
          if(r == 0) {
            band1 = cur_comp->band;
            band2 = cur_comp->band;
            band3 = cur_comp->band;

            log_pw = int_log2(dec_param->prec_width) - 1;
            log_ph = int_log2(dec_param->prec_height) - 1;
          } else {
            band1 = cur_comp->band + (r - 1) * 3 + 1;
            band2 = cur_comp->band + (r - 1) * 3 + 2;
            band3 = cur_comp->band + (r - 1) * 3 + 3;

            log_pw = int_log2(dec_param->prec_width);
            log_ph = int_log2(dec_param->prec_height);
          }

          lv = dec_param->dwt_level - r;
          pw_lv = log_pw + lv;
          ph_lv = log_ph + lv;

          if((!(y % (s_opt->YRsiz[c] << ph_lv)) || ((y == s_opt->ty0) && ((band1->try0 << lv) % (1 << ph_lv)))) && 
             (!(x % (s_opt->XRsiz[c] << pw_lv)) || ((x == s_opt->tx0) && ((band1->trx0 << lv) % (1 << pw_lv))))
            ) {
            pw = floor_ratio(ceil_ratio(x, (s_opt->XRsiz[c] << lv)), (1 << log_pw)) - floor_ratio(band1->trx0, (1 << log_pw));
            ph = floor_ratio(ceil_ratio(y, (s_opt->YRsiz[c] << lv)), (1 << log_ph)) - floor_ratio(band1->try0, (1 << log_ph));

            for(l = 0; l < dec_param->layer; ++l) {
              prec1 = band1->precinct + pw + ph * band1->nprec_bw;
              prec2 = band2->precinct + pw + ph * band2->nprec_bw;
              prec3 = band3->precinct + pw + ph * band3->nprec_bw;


			  ret = dec_read_packet(s_opt, stream, prec1, prec2, prec3, r, l, ++pkt_idx,   skip_to_next_component);
			  if (ret) 
			  {
				  return ret;
			  }
            }
          }
        }
      }
    }
  }

  return 0;
}

/**************************************************************************************/
/*                                dec_progression                                     */
/**************************************************************************************/
int dec_progression(stru_dec_param *dec_param, stru_opt *s_opt, stru_stream *stream)
{
  int rc, poc_index = 0;
  unsigned char poc_number = 0;
  int progression = dec_param->progression;
  tagtree_init(/*dec_param, */s_opt);
  
  stru_pipe *s_pipe = (stru_pipe *)dec_param->mempool[0];
  stru_jpc_cod *s_cod = s_pipe->s_jp2file.s_jp2stream.s_cod;
  stru_jpc_poc *s_m_poc = &s_pipe->s_jp2file.s_jp2stream.s_m_poc;
  stru_jpc_poc *s_t_poc = &s_pipe->s_jp2file.s_jp2stream.s_t_poc;

  do
  {
	  if (s_t_poc->num_poc != 0) {
		progression = s_t_poc->Ppoc[poc_index];
		poc_number	= s_t_poc->num_poc;
		}	  
	  else if (s_m_poc->num_poc != 0) {
		progression = s_m_poc->Ppoc[poc_index];
		poc_number	= s_m_poc->num_poc;
		}	  
	  else{
		progression = s_cod[0].progression;
		poc_index	= -1;
	  }

	  switch(progression) {
	case LRCP :   rc = dec_lrcp(dec_param, s_opt, stream, poc_index );    break;
	case RLCP :   rc = dec_rlcp(dec_param, s_opt, stream, poc_index );    break;
	case RPCL :   rc = dec_rpcl(dec_param, s_opt, stream, poc_index );    break;
	case PCRL :   rc = dec_pcrl(dec_param, s_opt, stream, poc_index );    break;
	case CPRL :   rc = dec_cprl(dec_param, s_opt, stream, poc_index );    break;
	default   :       
		{
			char msg[100];
			SPRINTF_S((msg, 100, "unknown   progressing [%d]", progression));
			BMI_ASSERT_MSG(0, msg); // unknown return code
		}
		break;
	  }

	  if (rc)	// found 0xff90
	  {
		return rc;
	  }

	  ++poc_index;
  }while(poc_index < poc_number );

  ostream_byte_align(stream);

  return 0;
}


static void param_switch(stru_dec_param *dec_param, int comp_id)
{  
	int cur_level;
    stru_pipe *s_pipe           = (stru_pipe *)dec_param->mempool[0];
    stru_jp2_syntax *s_jp2file  = &s_pipe->s_jp2file;

    stru_jpc_siz *s_siz   = &s_jp2file->s_jp2stream.s_siz;
    stru_jpc_cod *s_cod = s_jp2file->s_jp2stream.s_cod;
    stru_jpc_rgn *s_rgn   = &s_jp2file->s_jp2stream.s_rgn;  
    
    // fill dec_param
    switch(s_cod[comp_id].progression) {
      case 0  :   dec_param->progression = LRCP;    break;
      case 1  :   dec_param->progression = RLCP;    break;
      case 2  :   dec_param->progression = RPCL;    break;
      case 3  :   dec_param->progression = PCRL;    break;
      case 4  :   dec_param->progression = CPRL;    break;
      default :   dec_param->progression = UNKNOWN_PROGRESSION;      break;
    }

    dec_param->width        = s_siz->width;
    dec_param->height       = s_siz->height;
    dec_param->bitdepth     = s_siz->bitdepth[comp_id];
    dec_param->tile_width   = s_siz->tile_width;
    dec_param->tile_height  = s_siz->tile_height;

    dec_param->layer        = s_cod[comp_id].layer;
    dec_param->dwt_level    = s_cod[comp_id].dwt_level;
    cur_level			    = s_cod[comp_id].dwt_level;
	dec_param->roi_shift    = s_rgn->roi_shift[comp_id];

    dec_param->cb_width     = s_cod[comp_id].cb_width;
    dec_param->cb_height    = s_cod[comp_id].cb_height;
    dec_param->prec_width   = s_cod[comp_id].prec_width[cur_level];
    dec_param->prec_height  = s_cod[comp_id].prec_height[cur_level];


    if(s_cod[comp_id].bypass)           dec_param->configuration |= CONF_BYPASS;
    if(s_cod[comp_id].reset)            dec_param->configuration |= CONF_RESET;
    if(s_cod[comp_id].termall)          dec_param->configuration |= CONF_TERMALL;
    if(s_cod[comp_id].causal)           dec_param->configuration |= CONF_CAUSAL;
    if(s_cod[comp_id].pterm)            dec_param->configuration |= CONF_PTERM;
    if(s_cod[comp_id].segsym)           dec_param->configuration |= CONF_SEGSYM;
    if(s_cod[comp_id].sop)              dec_param->configuration |= CONF_SOP;
    if(s_cod[comp_id].eph)              dec_param->configuration |= CONF_EPH;
    if(s_cod[comp_id].dwt97)            dec_param->configuration |= CONF_DWT97;
    if(s_cod[comp_id].mct)              dec_param->configuration |= CONF_MCT;
    if(s_cod[comp_id].dwt_i53)          dec_param->configuration |= CONF_QUANT;

}