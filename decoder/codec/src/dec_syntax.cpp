/***************************************************************************************
 *
 * Copyright (C) 2004 Broad Motion, Inc.
 *
 * All rights reserved.
 *
 * http://www.broadmotion.com
 *
 **************************************************************************************/

#include "dec_stru.h"
#include "debug.h"
#include "codec_datatype.h"

#define MK_JP2S               0x6A502020    // 'jP\040\040'
#define MK_JP2SIG             0x0D0A870A    // '<CR><LF><0x87><LF>'
#define MK_FTYP               0x66747970
#define MK_JP2                0x6A703220    // 'jp2\040'
#define MK_JP2H               0x6A703268
#define MK_IHDR               0x69686472
#define MK_BPCC               0x62706363
#define MK_COLR               0x636F6C72
#define MK_PCLR               0x70636C72
#define MK_CMAP               0x636D6170
#define MK_CDEF               0x63646566
#define MK_RES                0x72657320    // 'res\040'
#define MK_RESC               0x72657363
#define MK_RESD               0x72657364
#define MK_JP2C               0x6A703263
#define MK_JP2I               0x6A703269
#define MK_XML                0x786D6C20    // 'xml\040'
#define MK_UUID               0x75756964
#define MK_UINF               0x75696E66
#define MK_ULST               0x75637374
#define MK_URL                0x75726C20    // 'url\040'


static int dec_read_jp2_header( stru_dec_param *dec_param, stru_jp2_syntax *s_jp2file,
                                stru_stream *stream);
static int dec_read_jpc_header(stru_jp2_syntax *s_jp2file, stru_stream *stream);

/**************************************************************************************/
/* static                      dec_read_jp2_header                                    */
/**************************************************************************************/
static int dec_read_jp2_header( stru_dec_param *dec_param, stru_jp2_syntax *s_jp2file,
                                stru_stream *stream)
{
  int len, slen, type, content;
  int b_jp2s = 0, b_ftyp = 0, b_jp2h = 0;

  stru_jp2_colr *s_colr = &s_jp2file->s_jp2h.s_colr;

  while(1) {
    len = ostream_read_bytes(stream, 4);
    if(!len)  len = dec_param->in_size - ostream_tell(stream);
    type = ostream_read_bytes(stream, 4);
    if(!(len -= 8))                         return -1;
// 	DEBUG_PRINT(("...%x...", type));

    switch(type) {
    case MK_JP2I  :
    case MK_XML   :
    case MK_UUID  :
    case MK_UINF  :
      if(!b_jp2h)                           return -1;
      ostream_skip(stream, len);
//      ostream_seek(stream, ostream_tell(stream) + 5);     // error resilience for Prodrive xml length
      break;

    case MK_JP2S  :                             // jp2s
      content = ostream_read_bytes(stream, 4);
      if(content != MK_JP2SIG || len != 4)  return -1;
      b_jp2s = 1;
      break;

    case MK_FTYP  :                             // ftyp
      content = ostream_read_bytes(stream, 4);
      if(content != MK_JP2 || !b_jp2s)      return -1;
      ostream_skip(stream, len - 4);
      b_ftyp = 1;
      break;

    case MK_JP2C  :                             // jp2c
      if(!b_jp2h)                           return -1;
      else                                  return 0;

    case MK_JP2H  :                             // jp2h
      if(!b_ftyp)                           return -1;
       slen = len;
      while(slen) {
        len = ostream_read_bytes(stream, 4);
        if(!len)  len = dec_param->in_size - ostream_tell(stream);
        type = ostream_read_bytes(stream, 4);
// 		DEBUG_PRINT(("...%x...", type));

        slen -= len;
        if(!(len -= 8))                     return -1;

        switch(type) {
        case MK_IHDR  :
        case MK_BPCC  :
        case MK_PCLR  :
        case MK_CMAP  :
        case MK_CDEF  :
        case MK_RES   :
          ostream_skip(stream, len);
          break;

        case MK_COLR  :                         // colr
          s_colr->meth              = ostream_read_bytes(stream, 1);
          content                   = ostream_read_bytes(stream, 1);
          content                   = ostream_read_bytes(stream, 1);
          if(s_colr->meth == 1)
            s_colr->enmu_colr_space = ostream_read_bytes(stream, 4);
          else
            ostream_skip(stream, len - 3);
          break;
        default :                           return -1;
        }
      }
      b_jp2h = 1;
      break;

    default :                               return -1;
    }
  }

  return -1;
}

/**************************************************************************************/
/* static                       dec_read_jpc_header                                   */
/**************************************************************************************/
static int dec_read_jpc_header(stru_jp2_syntax *s_jp2file, stru_stream *stream)
{ 
  int n, cfg, comp_num;
  int len, type, content, comp_id;
  int b_siz = 0, b_qcd = 0, b_cod = 0;

  stru_jpc_siz *s_siz   = &s_jp2file->s_jp2stream.s_siz;
  stru_jpc_poc *s_m_poc = &s_jp2file->s_jp2stream.s_m_poc;
  stru_jpc_rgn *s_rgn   = &s_jp2file->s_jp2stream.s_rgn;

  stru_jpc_cod *s_cod = s_jp2file->s_jp2stream.s_cod;  
  stru_jpc_qcd *s_qcd = s_jp2file->s_jp2stream.s_qcd;

  stru_jpc_pp_header *s_pp_header = &s_jp2file->s_jp2stream.s_pp_header;

  type = ostream_read_bytes(stream, 2);         // soc

  if(type != MK_SOC)        return -1;

  s_pp_header   ->num_of_ppm    = 0;      // reset counter 
  s_pp_header   ->f_ppm         = 0;
  s_pp_header   ->non_first_tile= 0;                               
                               
  while(1) {
    type  = ostream_read_bytes(stream, 2);
    len   = ostream_read_bytes(stream, 2);

    switch(type) {
    //**************----------***************
    case MK_SIZ :                               // siz
      content                     = ostream_read_bytes(stream, 2);
      s_siz->width                = ostream_read_bytes(stream, 4);
      s_siz->height               = ostream_read_bytes(stream, 4);
      content                     = ostream_read_bytes(stream, 4);
      content                     = ostream_read_bytes(stream, 4);
      s_siz->tile_width           = ostream_read_bytes(stream, 4);
      s_siz->tile_height          = ostream_read_bytes(stream, 4);
      content                     = ostream_read_bytes(stream, 4);
      content                     = ostream_read_bytes(stream, 4);
      s_siz->num_component        = ostream_read_bytes(stream, 2);

      for(comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
        s_siz->bitdepth[comp_num] = ostream_read_bytes(stream, 1) + 1;
        s_siz->XRsiz[comp_num]    = ostream_read_bytes(stream, 1);
        s_siz->YRsiz[comp_num]    = ostream_read_bytes(stream, 1);
      }
      b_siz = 1;
      break;
    //**************----------***************
    case MK_COD :                               // cod
      if(!b_siz)            return -1;
      cfg                 = ostream_read_bytes(stream, 1);                        //Scod
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
        s_cod[comp_num].siz_prec   = (cfg & 0x1) >> 0;
        s_cod[comp_num].sop        = (cfg & 0x2) >> 1;
        s_cod[comp_num].eph        = (cfg & 0x4) >> 2;
//        s_cod[comp_num].enable     = 1;
        }

      cfg                 = ostream_read_bytes(stream, 1);                        //SGcod
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
          s_cod[comp_num].progression = cfg;                                       
        }
      cfg                 = ostream_read_bytes(stream, 2);
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
          s_cod[comp_num].layer = cfg;                                       
        }
      cfg                 = ostream_read_bytes(stream, 1);
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
          s_cod[comp_num].mct = cfg;                                       
        }

      cfg                 = ostream_read_bytes(stream, 1);                        //SPcod
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
          s_cod[comp_num].dwt_level = cfg;                                       
        }
      cfg                 = ostream_read_bytes(stream, 1);                        
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
          s_cod[comp_num].cb_width = 1 << (cfg + 2);                                       
        }
      cfg                 = ostream_read_bytes(stream, 1);                        
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
          s_cod[comp_num].cb_height = 1 << (cfg + 2);                                       
        }
      cfg                 = ostream_read_bytes(stream, 1);
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
          s_cod[comp_num].bypass     = (cfg & 0x01) >> 0;
          s_cod[comp_num].reset      = (cfg & 0x02) >> 1;
          s_cod[comp_num].termall    = (cfg & 0x04) >> 2;
          s_cod[comp_num].causal     = (cfg & 0x08) >> 3;
          s_cod[comp_num].pterm      = (cfg & 0x10) >> 4;
          s_cod[comp_num].segsym     = (cfg & 0x20) >> 5;
        }
      content             = ostream_read_bytes(stream, 1);
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
          s_cod[comp_num].dwt97      = (content == 0);
          s_cod[comp_num].dwt_i53    = (content == 2);
        }
      
        
      if(s_cod[0].siz_prec) {
        content = ostream_read_bytes(stream, 1);
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
          s_cod[comp_num].prec_width[0]   = 1 << ((content & 0xf) + 1);
          s_cod[comp_num].prec_height[0]  = 1 << (((content & 0xf0) >> 4) + 1);
        }  
        for(n = 1; n <= s_cod[0].dwt_level; ++n) {
          content = ostream_read_bytes(stream, 1);
          for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
            s_cod[comp_num].prec_width[n]   = 1 << (content & 0xf);
            s_cod[comp_num].prec_height[n]  = 1 << ((content & 0xf0) >> 4);
          }
        }
      } else {
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
          for(n = 0; n <= s_cod[comp_num].dwt_level; ++n) {
            s_cod[comp_num].prec_width[n]  = 32768;
            s_cod[comp_num].prec_height[n] = 32768;
          }
        }
	    }
	    b_cod = 1;
      break;
    //**************----------***************
	  case MK_COC :                                                 // COC prameters, if exist, overwrite COD related information
      if(!b_siz)            return -1;
      comp_id                  = ostream_read_bytes(stream, 1);        // Ccoc index, not used
      cfg                      = ostream_read_bytes(stream, 1);        // Scoc
      s_cod[comp_id].siz_prec  = (cfg & 0x1) >> 0;                     // overwrite siz_prec from COD
      s_cod[comp_id].dwt_level = ostream_read_bytes(stream, 1);
      s_cod[comp_id].cb_width  = 1 << (ostream_read_bytes(stream, 1) + 2);
      s_cod[comp_id].cb_height = 1 << (ostream_read_bytes(stream, 1) + 2);

      cfg                      = ostream_read_bytes(stream, 1);            // Code_Bloack Style 
      s_cod[comp_id].bypass    = (cfg & 0x01) >> 0;
      s_cod[comp_id].reset     = (cfg & 0x02) >> 1;
      s_cod[comp_id].termall   = (cfg & 0x04) >> 2;
      s_cod[comp_id].causal    = (cfg & 0x08) >> 3;
      s_cod[comp_id].pterm     = (cfg & 0x10) >> 4;
      s_cod[comp_id].segsym    = (cfg & 0x20) >> 5;

      content             = ostream_read_bytes(stream, 1);            // Transformation
      s_cod[comp_id].dwt97     = (content == 0);
      s_cod[comp_id].dwt_i53   = (content == 2);

      if(s_cod[comp_id].siz_prec) {
        content = ostream_read_bytes(stream, 1);
        s_cod[comp_id].prec_width[0]  = 1 << ((content & 0xf) + 1);
        s_cod[comp_id].prec_height[0] = 1 << (((content & 0xf0) >> 4) + 1);
        for(n = 1; n <= s_cod[comp_id].dwt_level; ++n) {
          content = ostream_read_bytes(stream, 1);
          s_cod[comp_id].prec_width[n]   = 1 << (content & 0xf);
          s_cod[comp_id].prec_height[n]  = 1 << ((content & 0xf0) >> 4);
        }
      } else {
        for(n = 0; n <= s_cod[comp_id].dwt_level; ++n) {
          s_cod[comp_id].prec_width[n]   = 32768;
          s_cod[comp_id].prec_height[n]  = 32768;
        }
	    }
      break;
    //**************----------***************        
    case MK_QCD :                               // qcd
			if(!b_siz)            return -1;
			content              = ostream_read_bytes(stream, 1);
      for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
			  s_qcd[comp_num].guard_bit  = content >> 5;
      }          
			content              = content & 0x1f;
			
			for(n = 0; n <= 3 * s_cod[0].dwt_level; ++n) {
			  if(content) {
			    cfg = ostream_read_bytes(stream, 2);
			    for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
			  	  s_qcd[comp_num].quant_step[n]  = cfg;
			  	}
			  } else {
			    cfg = ostream_read_bytes(stream, 1);
			    for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
			  	  s_qcd[comp_num].quant_step[n]  = cfg >> 3;
			  	}
			  }
			}
			b_qcd = 1;
      break;
    //**************----------***************
	  case MK_QCC :
	    if(!b_siz)            return -1;
      comp_id               = ostream_read_bytes(stream, 1);        // component index
      content               = ostream_read_bytes(stream, 1);
			s_qcd[comp_id].guard_bit  = content >> 5;
			content              = content & 0x1f;

			for(n = 0; n <= 3 * s_cod[comp_id].dwt_level; ++n) {
				if(content) {
					s_qcd[comp_id].quant_step[n]  = ostream_read_bytes(stream, 2);
				} else {
					s_qcd[comp_id].quant_step[n]  = ostream_read_bytes(stream, 1) >> 3;
				}
			}
      break;
  //**************----------***************
    case MK_RGN :
      if(!b_siz)            return -1;
      comp_id                     = ostream_read_bytes(stream, 1);
      content                     = ostream_read_bytes(stream, 1);
      s_rgn ->roi_shift[comp_id]  = ostream_read_bytes(stream, 1); 
      s_rgn ->enable[comp_id]     = 1;
      break;
    //**************----------***************
	  case MK_POC :
		{
			int l_poc = 7;
			if (s_siz->num_component >= 256)
			{
				l_poc = 9;
			}
			BMI_ASSERT((len - 2)%l_poc == 0);
			s_m_poc->num_poc = (len - 2)/l_poc;
			BMI_ASSERT(s_m_poc->num_poc <= MAX_NUM_POC);
			for(n = 0; n < s_m_poc->num_poc; ++n)
			{
				s_m_poc->RSpoc[n] = (unsigned char)ostream_read_bytes(stream, 1);
				if (l_poc == 7)
				{
					s_m_poc->CSpoc[n] = (unsigned short)ostream_read_bytes(stream, 1);
				}
				else
				{
					s_m_poc->CSpoc[n] = (unsigned short)ostream_read_bytes(stream, 2);
				}
				s_m_poc->LYEpoc[n] = (unsigned short)ostream_read_bytes(stream, 2);
				s_m_poc->REpoc[n] = (unsigned char)ostream_read_bytes(stream, 1);
				if (l_poc == 7)
				{
					s_m_poc->CEpoc[n] = (unsigned short)ostream_read_bytes(stream, 1);
				}
				else
				{
					s_m_poc->CEpoc[n] = (unsigned short)ostream_read_bytes(stream, 2);
				}
				s_m_poc->Ppoc[n] = (unsigned char)ostream_read_bytes(stream, 1);
			}
		}
		break;
    //**************----------***************  
    case MK_PPM :
      if(!b_siz)            return -1;  
      s_pp_header->ppm_offset[s_pp_header->num_of_ppm]   = ostream_tell(stream);      // record offset in sttream
      s_pp_header->num_of_ppm++;
      s_pp_header ->f_ppm = 1;
      ostream_skip(stream, len - 2);
      break;
    //**************----------***************
    case MK_TLM :               // tile length section, no use here
      if(!b_siz)            return -1;
      ostream_skip(stream, len - 2);
      break;
    //**************----------***************  
    case MK_PLM :
      if(!b_siz)            return -1;  
      ostream_skip(stream, len - 2);
      break;
    //**************----------***************  
    case MK_CRG :
      if(!b_siz)            return -1;
      ostream_skip(stream, len - 2);
      break;
    //**************----------***************  
    case MK_COM :
      if(!b_siz)            return -1;
      ostream_skip(stream, len - 2);
      break;
    //**************----------***************
    case MK_SOT :                               // SOT means Main Header finifhed
      if(!b_qcd || !b_cod)            return -1;
      ostream_skip(stream, - 4); 
      return 0;
    //**************----------***************
    default :               return -1;
    }
  }

  return -1;
}

/**************************************************************************************/
/*                                 dec_read_tile_header                               */
/**************************************************************************************/
int dec_read_tile_header(stru_jp2_syntax *s_jp2file, stru_stream *stream/*, int tile_idx*/)
{
  int n, cfg, content, comp_id, comp_num;
  int len, type;
  int b_sot = 0;

  stru_jpc_siz *s_siz     = &s_jp2file->s_jp2stream.s_siz;
  stru_jpc_sot *s_sot     = &s_jp2file->s_jp2stream.s_sot;
  stru_jpc_rgn *s_rgn     = &s_jp2file->s_jp2stream.s_rgn;  

  stru_jpc_cod *s_cod     = s_jp2file->s_jp2stream.s_cod;  
  stru_jpc_qcd *s_qcd     = s_jp2file->s_jp2stream.s_qcd;

  stru_jpc_poc *s_t_poc     = &s_jp2file->s_jp2stream.s_t_poc;

  stru_jpc_pp_header *s_pp_header = &s_jp2file->s_jp2stream.s_pp_header;

  s_pp_header ->num_of_ppt  = 0;      // reset counter 
  s_pp_header ->f_ppt       = 0;

//  if (!s_pp_header ->non_first_tile){
//  }   

  while(1) {
    if ((type  = ostream_read_bytes(stream, 2)) == MK_EOC)
	{
		  return CODE_STREAM_END;
	}

    len   = ostream_read_bytes(stream, 2);
    
    switch(type) {
    //**************----------***************
    case MK_SOT :                               // sot
      s_sot->tile_offset  = ostream_tell(stream) - 4;
      s_sot->tile_idx     = ostream_read_bytes(stream, 2);
      s_sot->tile_len     = ostream_read_bytes(stream, 4);
      s_sot->tilep_idx    = ostream_read_bytes(stream, 1);
      s_sot->tilep_num    = ostream_read_bytes(stream, 1);
      b_sot = 1;
      break;
	  //**************----------***************  
	  case MK_COD :
//      if(!b_sot || s_pp_header ->non_first_tile == 1)   return -1;
	  if(!b_sot || s_pp_header ->non_first_tile == 1){
	    ostream_skip(stream, -3);	
	    break;
	  }  
      cfg                 = ostream_read_bytes(stream, 1);                        //Scod
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
        s_cod[comp_num].siz_prec   = (cfg & 0x1) >> 0;
        s_cod[comp_num].sop        = (cfg & 0x2) >> 1;
        s_cod[comp_num].eph        = (cfg & 0x4) >> 2;
//        s_cod[comp_num].enable     = 1;
        }

      cfg                 = ostream_read_bytes(stream, 1);                        //SGcod
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
          s_cod[comp_num].progression = cfg;                                       
        }
      cfg                 = ostream_read_bytes(stream, 2);
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
          s_cod[comp_num].layer = cfg;                                       
        }
      cfg                 = ostream_read_bytes(stream, 1);
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
          s_cod[comp_num].mct = cfg;                                       
        }

      cfg                 = ostream_read_bytes(stream, 1);                        //SPcod
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
          s_cod[comp_num].dwt_level = cfg;                                       
        }
      cfg                 = ostream_read_bytes(stream, 1);                        
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
          s_cod[comp_num].cb_width = 1 << (cfg + 2);                                       
        }
      cfg                 = ostream_read_bytes(stream, 1);                        
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
          s_cod[comp_num].cb_height = 1 << (cfg + 2);                                       
        }
      cfg                 = ostream_read_bytes(stream, 1);
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
          s_cod[comp_num].bypass     = (cfg & 0x01) >> 0;
          s_cod[comp_num].reset      = (cfg & 0x02) >> 1;
          s_cod[comp_num].termall    = (cfg & 0x04) >> 2;
          s_cod[comp_num].causal     = (cfg & 0x08) >> 3;
          s_cod[comp_num].pterm      = (cfg & 0x10) >> 4;
          s_cod[comp_num].segsym     = (cfg & 0x20) >> 5;
        }
      content             = ostream_read_bytes(stream, 1);
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
          s_cod[comp_num].dwt97      = (content == 0);
          s_cod[comp_num].dwt_i53    = (content == 2);
        }
      
        
      if(s_cod[0].siz_prec) {
        content = ostream_read_bytes(stream, 1);
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
          s_cod[comp_num].prec_width[0]   = 1 << ((content & 0xf) + 1);
          s_cod[comp_num].prec_height[0]  = 1 << (((content & 0xf0) >> 4) + 1);
        }  
        for(n = 1; n <= s_cod[0].dwt_level; ++n) {
          content = ostream_read_bytes(stream, 1);
          for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
            s_cod[comp_num].prec_width[n]   = 1 << (content & 0xf);
            s_cod[comp_num].prec_height[n]  = 1 << ((content & 0xf0) >> 4);
          }
        }
      } else {
        for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
          for(n = 0; n <= s_cod[comp_num].dwt_level; ++n) {
            s_cod[comp_num].prec_width[n]  = 32768;
            s_cod[comp_num].prec_height[n] = 32768;
          }
        }
	    }
      break;
    //**************----------***************
    case MK_COC :
      //if(!b_sot || s_pp_header ->non_first_tile == 1)   return -1;
      if(!b_sot || s_pp_header ->non_first_tile == 1){
        ostream_skip(stream, -3);
        break;
      }
	    comp_id                  = ostream_read_bytes(stream, 1);        // Ccoc index, not used
      cfg                      = ostream_read_bytes(stream, 1);        // Scoc
      s_cod[comp_id].siz_prec  = (cfg & 0x1) >> 0;                     // overwrite siz_prec from COD
      s_cod[comp_id].dwt_level = ostream_read_bytes(stream, 1);
      s_cod[comp_id].cb_width  = 1 << (ostream_read_bytes(stream, 1) + 2);
      s_cod[comp_id].cb_height = 1 << (ostream_read_bytes(stream, 1) + 2);

      cfg                      = ostream_read_bytes(stream, 1);            // Code_Bloack Style 
      s_cod[comp_id].bypass    = (cfg & 0x01) >> 0;
      s_cod[comp_id].reset     = (cfg & 0x02) >> 1;
      s_cod[comp_id].termall   = (cfg & 0x04) >> 2;
      s_cod[comp_id].causal    = (cfg & 0x08) >> 3;
      s_cod[comp_id].pterm     = (cfg & 0x10) >> 4;
      s_cod[comp_id].segsym    = (cfg & 0x20) >> 5;

      content             = ostream_read_bytes(stream, 1);            // Transformation
      s_cod[comp_id].dwt97     = (content == 0);
      s_cod[comp_id].dwt_i53   = (content == 2);

      if(s_cod[comp_id].siz_prec) {
        content = ostream_read_bytes(stream, 1);
        s_cod[comp_id].prec_width[0]  = 1 << ((content & 0xf) + 1);
        s_cod[comp_id].prec_height[0] = 1 << (((content & 0xf0) >> 4) + 1);
        for(n = 1; n <= s_cod[comp_id].dwt_level; ++n) {
          content = ostream_read_bytes(stream, 1);
          s_cod[comp_id].prec_width[n]   = 1 << (content & 0xf);
          s_cod[comp_id].prec_height[n]  = 1 << ((content & 0xf0) >> 4);
        }
      } else {
        for(n = 0; n <= s_cod[comp_id].dwt_level; ++n) {
          s_cod[comp_id].prec_width[n]   = 32768;
          s_cod[comp_id].prec_height[n]  = 32768;
        }
	    }
      break;
    //**************----------***************  
    case MK_QCD :
      //if(!b_sot || s_pp_header ->non_first_tile == 1) return -1;
	  if(!b_sot || s_pp_header ->non_first_tile == 1){
	    ostream_skip(stream, -3);	
	    break;
	    }
		content              = ostream_read_bytes(stream, 1);
      for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
			  s_qcd[comp_num].guard_bit  = content >> 5;
      }          
			content              = content & 0x1f;
			
			for(n = 0; n <= 3 * s_cod[0].dwt_level; ++n) {
			  if(content) {
			    cfg = ostream_read_bytes(stream, 2);
			    for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
			  	  s_qcd[comp_num].quant_step[n]  = cfg;
			  	}
			  } else {
			    cfg = ostream_read_bytes(stream, 1);
			    for (comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
			  	  s_qcd[comp_num].quant_step[n]  = cfg >> 3;
			  	}
			  }
			}
      break;
    //**************----------***************
	  case MK_QCC :
	    //if(!b_sot || s_pp_header ->non_first_tile == 1)     return -1;
	  if(!b_sot || s_pp_header ->non_first_tile == 1){         
	    ostream_skip(stream, -3);
	    break;
	  }
      comp_id               = ostream_read_bytes(stream, 1);        // component index
      content               = ostream_read_bytes(stream, 1);
			s_qcd[comp_id].guard_bit  = content >> 5;
			content              = content & 0x1f;

			for(n = 0; n <= 3 * s_cod[comp_id].dwt_level; ++n) {
				if(content) {
					s_qcd[comp_id].quant_step[n]  = ostream_read_bytes(stream, 2);
				} else {
					s_qcd[comp_id].quant_step[n]  = ostream_read_bytes(stream, 1) >> 3;
				}
			}
      break;
    //**************----------***************  
    case MK_RGN :
      //if(!b_sot || s_pp_header ->non_first_tile == 1)         return -1;
	  if(!b_sot || s_pp_header ->non_first_tile == 1){
	    ostream_skip(stream, -3);
	    break;
	  }
      comp_id                     = ostream_read_bytes(stream, 1);
      content                     = ostream_read_bytes(stream, 1);
      s_rgn->roi_shift[comp_id]   = ostream_read_bytes(stream, 1);
      s_rgn->enable[comp_id]      = 1;
      break;
   //**************----------***************
	  case MK_POC :
		{
			int l_poc = 7;
			//if(!b_sot)      return -1;
			if(!b_sot) {
			  ostream_skip(stream, -3);
			  break;
			}
			if (s_siz->num_component >= 256)
			{
				l_poc = 9;
			}
			{
				if ((len - 2)%l_poc != 0 || s_t_poc->num_poc > MAX_NUM_POC)
				{
					char msg[256];
					SPRINTF_S((msg, 256, "len = [%d], l_poc = [%d] s_t_poc->num_poc = [%d]  MAX_NUM_POC = [%d]", len, l_poc, s_t_poc->num_poc, MAX_NUM_POC));
					BMI_ASSERT_MSG(0,msg);
					return -1;
				}
				s_t_poc->num_poc = (len - 2)/l_poc;
			}
			for(n = 0; n < s_t_poc->num_poc; ++n)
			{
				s_t_poc->RSpoc[n] = (unsigned char)ostream_read_bytes(stream, 1);
				if (l_poc == 7)
				{
					s_t_poc->CSpoc[n] = (unsigned short)ostream_read_bytes(stream, 1);
				}
				else
				{
					s_t_poc->CSpoc[n] = (unsigned short)ostream_read_bytes(stream, 2);
				}
				s_t_poc->LYEpoc[n] = (unsigned short)ostream_read_bytes(stream, 2);
				s_t_poc->REpoc[n] = (unsigned char)ostream_read_bytes(stream, 1);
				if (l_poc == 7)
				{
					s_t_poc->CEpoc[n] = (unsigned short)ostream_read_bytes(stream, 1);
				}
				else
				{
					s_t_poc->CEpoc[n] = (unsigned short)ostream_read_bytes(stream, 2);
				}
				s_t_poc->Ppoc[n] = (unsigned char)ostream_read_bytes(stream, 1);
			}
		}
		break;
    //**************----------***************  
    case MK_PPT :
      //if(!b_sot || s_pp_header ->f_ppm == 1)            return -1;
		if(!b_sot) {
		  if (s_pp_header ->f_ppm == 1)		return -1;
		  ostream_skip(stream, -3);
		  break;
		}
      
	    s_pp_header->ppt_offset[s_pp_header->num_of_ppt]   = ostream_tell(stream);      // record offset in stream 
      s_pp_header->num_of_ppt++;
      s_pp_header ->f_ppt = 1;                       // ppt exist flag in tile header
      ostream_skip(stream, len - 2);
      break;
    //**************----------***************  
    case MK_PLT :
      //if(!b_sot)            return -1;
      if(!b_sot){	
        ostream_skip(stream, -3);
        break;
      }
	    ostream_skip(stream, len - 2);
      break;
    //**************----------***************
    case MK_COM :
      //if(!b_sot)            return -1;
      if(!b_sot){
        ostream_skip(stream, -3);
        break;
      }
      ostream_skip(stream, len - 2);
      break;
    //**************----------***************
    case MK_SOD :                               // sod, end of tile header
      //if(!b_sot)            return -1;
      if(!b_sot){
        ostream_skip(stream, -3);
        break;
      }
      s_pp_header ->non_first_tile= 1;          // clear first tile flag
      ostream_skip(stream, -2);
      return 0;
	  //**************----------***************
	  case MK_EOC:
	  	return CODE_STREAM_END;
    //**************----------***************
    default :                                   // error resilience, seek to next SOT
      ostream_skip(stream, -3);
      if (stream->cur >= stream->end)   return -1;
      
    }
  }

  return -1;
}

/**************************************************************************************/
/*                                    dec_read_header                                 */
/**************************************************************************************/
int dec_read_header(stru_dec_param *dec_param, stru_jp2_syntax *s_jp2file,
                    stru_stream *stream, int use_j2k_header)
{
  int rc = 0;
  
  int n, comp_num;
  int log_pw = int_log2(dec_param->prec_width);
  int log_ph = int_log2(dec_param->prec_height);
  int siz_prec = (log_pw == 15 && log_ph == 15) ? 0 : 1 + dec_param->dwt_level;


  stru_jpc_siz *s_siz = &s_jp2file->s_jp2stream.s_siz;
  stru_jpc_cod *s_cod = s_jp2file->s_jp2stream.s_cod;
  stru_jpc_qcd *s_qcd = s_jp2file->s_jp2stream.s_qcd;

  ostream_init(stream, (unsigned char *)dec_param->input, dec_param->in_size);

  if(use_j2k_header) {
	  rc = ostream_read_bytes(stream, 4);
    ostream_skip(stream, -4);

    // parse file header
    if(rc == 12) {
      s_jp2file->exists_jp2 = 1;
      rc = dec_read_jp2_header(dec_param, s_jp2file, stream);
      if(rc)    return -1;    // error in file header
    }
    // parse main header
    rc = dec_read_jpc_header(s_jp2file, stream);
  } else {
      s_siz->width                = dec_param->width;
      s_siz->height               = dec_param->height;
      s_siz->tile_width           = dec_param->tile_width;
      s_siz->tile_height          = dec_param->tile_height;
      s_siz->num_component        = ( dec_param->format == RAW)       ? 1 :
                                    ( dec_param->format == RGB ||
                                      dec_param->format == YUV444 ||
                                      dec_param->format == YUV422 ||
                                      dec_param->format == YUV420 ||
                                      dec_param->format == BAYERGRB)  ? 3 : MAX_COMPONENT;
      
      for(comp_num = 0; comp_num < s_siz->num_component; ++comp_num) {
        s_siz->bitdepth[comp_num] = dec_param->bitdepth;
        s_siz->YRsiz[comp_num]    = ((dec_param->format == YUV420 || dec_param->format == BAYERGRB) && comp_num) ? 2 : 1;
        s_siz->XRsiz[comp_num]    = ( (dec_param->format == YUV422 || dec_param->format == YUV420) && comp_num) ? 2 : 1;
      
      
        s_cod[comp_num].siz_prec     = !!siz_prec;
        s_cod[comp_num].sop          = !!(dec_param->configuration & CONF_SOP);
        s_cod[comp_num].eph          = !!(dec_param->configuration & CONF_EPH);
        
        s_cod[comp_num].progression  = dec_param->progression;
        s_cod[comp_num].layer        = dec_param->layer;
        s_cod[comp_num].mct          = !!(dec_param->configuration & CONF_MCT);
        s_cod[comp_num].dwt_level    = dec_param->dwt_level;
        s_cod[comp_num].cb_width     = dec_param->cb_width;
        s_cod[comp_num].cb_height    = dec_param->cb_height;
        
        s_cod[comp_num].bypass       = !!(dec_param->configuration & CONF_BYPASS);
        s_cod[comp_num].reset        = !!(dec_param->configuration & CONF_RESET);
        s_cod[comp_num].termall      = !!(dec_param->configuration & CONF_TERMALL);
        s_cod[comp_num].causal       = !!(dec_param->configuration & CONF_CAUSAL);
        s_cod[comp_num].pterm        = !!(dec_param->configuration & CONF_PTERM);
        s_cod[comp_num].segsym       = !!(dec_param->configuration & CONF_SEGSYM);
        
        s_cod[comp_num].dwt97        = !!(dec_param->configuration & CONF_DWT97);
        s_cod[comp_num].dwt_i53      = (!!(dec_param->configuration & CONF_QUANT)) && (!s_cod[comp_num].dwt97);
      
        if(s_cod[comp_num].siz_prec) {
          s_cod[comp_num].prec_width[0] = 1 << log_pw;
          s_cod[comp_num].prec_height[0] = 1 << log_ph;
          for(n = 1; n <= s_cod[comp_num].dwt_level; ++n) {
            s_cod[comp_num].prec_width[n] = 1 << log_pw;
            s_cod[comp_num].prec_height[n] = 1 << log_ph;
          }
        } else {
          for(n = 0; n <= s_cod[comp_num].dwt_level; ++n) {
            s_cod[comp_num].prec_width[n]  = 32768;
            s_cod[comp_num].prec_height[n] = 32768;
          }
        }
      
        s_qcd[comp_num].guard_bit        = dec_param->guard_bit;
        for(n = 0; n <= 3 * s_cod[comp_num].dwt_level; ++n) {
          s_qcd[comp_num].quant_step[n]  = dec_param->quant_step[n];
        }
      }
    }   
  return rc;
}
