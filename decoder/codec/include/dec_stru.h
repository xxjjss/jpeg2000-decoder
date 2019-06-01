/***************************************************************************************
 *
 * Copyright (C) 2004 Broad Motion, Inc.
 *
 * All rights reserved.
 *
 * http://www.broadmotion.com
 *
 **************************************************************************************/

#if !defined(DEC_STRU_H)
#define DEC_STRU_H

#include "platform.h"
#include "bmi_jp2dec.h"

//#define B_STAT
//#define B_FPGA_TB

//#define B_FPGA_MQ_TB

//#define USE_MARCO_T1
//#define USE_MARCO_MQ
#define BMI_SLOPE_LOG

#if defined(ASIC_T1) || defined(ASIC_DWT_T1)
  #define ASIC
#endif

#if defined(ASIC)
  #define BMI_IDENTIFIER  "BroadMotion Lyra ASIC Decoder V3.00a"
#elif defined(WIN32)
  #define BMI_IDENTIFIER  "BroadMotion Lyra Desktop Decoder V3.00a"
#else
  #define BMI_IDENTIFIER  "BroadMotion Lyra Embedded Decoder V3.00a"
#endif

#if defined(TI_DSP)
  #include <std.h>
  #include <sys.h>
  #include <clk.h>
  #include <log.h>
  #include <sem.h>
  #include <tsk.h>

  #include <csl.h>
  #include <csl_dat.h>
  #include <csl_cache.h>

  extern far LOG_Obj trace;
#endif

#if defined(WIN32)
  #define restrict
  #define inline                __inline

  typedef void*                 SEM_Handle;
  typedef void*                 TSK_Handle;
#endif

  #define DAT_1D2D              0
  #define DAT_2D1D              1
  #define DAT_2D2D              2
  #define DAT_XFRID_WAITNONE    0

  #define CACHE_WAIT            0
  #define CACHE_L2_LINESIZE     128

#if defined(ALTERA_NIOS2)
  #include <stddef.h>
  #include "includes.h"
  #include "sys/alt_timestamp.h"

  #define restrict
  #define inline                __inline

  typedef OS_EVENT*             SEM_Handle;
  typedef void*                 TSK_Handle;

  #define CACHE_WAIT            -1

  #define DAT_1D2D              0
  #define DAT_2D1D              1
  #define DAT_2D2D              2
  #define DAT_XFRID_WAITNONE    0

  #define CACHE_L2_LINESIZE     128
#endif

#define CNT_S_OPT             3             // number of s_opt
#define CNT_PERSIS_BUF        2             // number of persis_buffer

#define MAX_BITDEPTH          32
#define MAX_CODINGPASS        ((MAX_BITDEPTH - 1) * 3 - 2)
#define	MAX_NUM_POC			 128

#define MIN_DWT_NROWS         4   // have to be multiply of 4?
#define LUT_VMSE_SIZE         (4 * (MAX_DWT_LEVEL + 1))
#define MAX_BDWT_WIDTH        (1024 - 8)    // width of block dwt, have to be multiply of 8?

#define MQ_FIFO_SIZE          256
#define MQ_FPGA_SIZE          2048
#define RDS_STAT_SIZE         4096

#define SHORT_MAX             0x7FFF
#define USHORT_MAX            0xFFFF
#define DSP_ALIGN             (CACHE_L2_LINESIZE - 1)

#define CTX_RUN               17
#define CTX_UNIFORM           18

// ctx flag definition
#define MASK_CTX              0x00FF
#define MASK_SIGN             0x0100
#define MASK_SIG              0x0200
#define MASK_P2               0x0400
#define MASK_VISIT            0x0800

#define MK_SOC                0xFF4F        // delimiting markers and marker segments
#define MK_SOT                0xFF90
#define MK_SOD                0xFF93
#define MK_EOC                0xFFD9
#define MK_SIZ                0xFF51        // fixed information marker segments
#define MK_COD                0xFF52        // functional marker segments
#define MK_COC                0xFF53
#define MK_RGN                0xFF5E
#define MK_QCD                0xFF5C
#define MK_QCC                0xFF5D
#define MK_POC                0xFF5F
#define MK_TLM                0xFF55        // pointer marker segments
#define MK_PLM                0xFF57
#define MK_PLT                0xFF58
#define MK_PPM                0xFF60
#define MK_PPT                0xFF61
#define MK_SOP                0xFF91        // in-bit-stream markers and marker segments
#define MK_EPH                0xFF92
#define MK_CRG                0xFF63        // informational marker segments
#define MK_COM                0xFF64
#define MK_SKIP_MIN			  0xFF30
#define MK_SKIP_MAX			  0xFF3F


// return code for parse packet:
#define  CHANGE_TILE		1
#define  CODE_STREAM_END	2


enum {                    // have to use this order to match FPGA configuration
  BAND_LL,                // left top
  BAND_HL,                // right top
  BAND_LH,                // left bottom
  BAND_HH                 // right bottom
};

typedef struct tag_stru_mqd {

  unsigned int a;                   // a register
  unsigned int c;                   // c register
  unsigned char ct;                 // counter
  unsigned char *buffer;            // buffer pointer
  unsigned char cx_states[19][2];

} stru_mqd;

typedef struct tag_stru_stream {

  unsigned char *start;             //start of buffer
  unsigned char *end;               //end of buffer
  unsigned char *cur;               //current location in buffer

  unsigned char bits;               //bits used in byts

} stru_stream;

typedef struct tag_stru_tagtree_node {

  unsigned int value;
  unsigned int low;

  struct tag_stru_tagtree_node *parent;

} stru_tagtree_node;

typedef struct tag_stru_tagtree {

  short width;                      // horizontal size at lowest level
  short height;                     // vertical size at lowest level

  short num_node;                   // number of tagtree node
  stru_tagtree_node *node;          // tagtree node buffer

} stru_tagtree;

typedef struct tag_stru_coding_pass {

  unsigned short cumlen;            // actual accumulated data size in this coding pass
  unsigned char *buffer;            // starting pointer to pass buffer

  char layer;                       // which layer this coding pass belongs to
  char num_t2layer;                 // number of coding pass in current layer

} stru_coding_pass;

typedef struct tag_stru_code_block {

  short x0;                         // x of left top coner
  short y0;                         // y of left top coner
  short width;                      // rect width
  short height;                     // rect height

  unsigned char lblock;             // state variable for codeword segment
  unsigned char missing_bit_plan;   // adjusted missing bit plan, sign: bit[31]

  unsigned char has_t2pass;         // ever had output coding pass
  unsigned char idx_t2pass;         // index of output coding pass
  unsigned char num_t2pass;         // number of output coding pass
  unsigned char num_pass;           // number of coding pass

  stru_coding_pass *pass;           // pointer to first coding pass

} stru_code_block;

typedef struct tag_stru_precinct {

  char resolution;                  // resolution index
  char subband;                     // subband index

  short ncb_pw;                     // number of code block at horizontal direction
  short ncb_ph;                     // number of code block at vertical direction

  short ncb_bw;                     // number of code block in subband at horizontal direction
  short ncb_bw0;                    // idx of first code block in subband domain
  short ncb_bh0;                    // idx of first code block in subband domain
  stru_code_block *codeblock;       // pointer to first code block;

  short num_node;                   // number of tagtree node
  stru_tagtree_node *inclnode;      // tagtree node buffer
  stru_tagtree_node *imsbnode;      // tagtree node buffer

  stru_tagtree *incltree;           // the inclusion tagtree
  stru_tagtree *imsbtree;           // the insignifcant msb tagtree

} stru_precinct;

typedef struct tag_stru_subband {

  char resolution;                  // resolution index
  char subband;                     // subband index
  int  cnt;

  int numbps;
  short expn;                       // exponent, eb
  short mant;                       // mantissa, ub
  int stepsize;                     // (1 << 13) of real stepsize (delta)

  short ncb_bw;                     // number of code block at horizontal direction
  short ncb_bh;                     // number of code block at vertical direction
#if SOFTWARE_MULTI_THREAD
  int	width;						//	subband width & height in pixels
  int	height;
#endif	// SOFTWARE_MULTI_THREAD
  stru_code_block *codeblock;       // pointer to first code block;

  short trx0;                       // x of left top coner of the resolution this subband belongs to
  short try0;                       // y of left top coner of the resolution this subband belongs to
  short nprec_bw;                   // number of precinct at horizontal direction
  short nprec_bh;                   // number of precinct at vertical direction
  stru_precinct *precinct;          // pointer to first precinct;

  short num_node;                   // number of tagtree node
  stru_tagtree_node *inclnode;      // tagtree node buffer
  stru_tagtree_node *imsbnode;      // tagtree node buffer

  stru_tagtree *incltree;           // the inclusion tagtree
  stru_tagtree *imsbtree;           // the insignifcant msb tagtree

} stru_subband;

typedef struct tag_stru_tile_component {

  short width[MAX_DWT_LEVEL];       // component width, end with LL
  short height[MAX_DWT_LEVEL];      // component height, end with LL

// pointers can be used to direct structure access
  int num_pass;                     // number of coding pass
  stru_coding_pass *pass;           // pointer to first coding pass, , start with LL

  int num_cb;                     // number of code block at vertical direction
  stru_code_block *codeblock;       // pointer to first code block, start with LL;

  short num_node;                   // number of tagtree node
  stru_tagtree_node *node;          // tagtree node buffer, , start with LL

  short num_tree;                   // number of tagtree node
  stru_tagtree *tree;               // the tagtree

  short num_prec;                   // number of precinct
  stru_precinct *precinct;          // precinct buffer, , start with LL

  char num_band;                    // number of subband
  char num_roi_band;                // number of roi rect per subband
  stru_subband *band;               // subband buffer, , start with LL

} stru_tile_component;

typedef struct tag_stru_opt {

  void *pointer_holder;             // pointer place holder

  short tx0;                        // x of left top coner of a tile
  short ty0;                        // y of left top coner of a tile
  short tx1;                        // x of right bottom coner of a tile
  short ty1;                        // y of right bottom coner of a tile
  short tile_idx;

#if defined(B_FPGA_TB)
  unsigned int res_align;
  unsigned int max_num_pass;
#endif

//   char qcd_guard_bit;
  char qcc_guard_bit[MAX_COMPONENT];
  char roi_shift;
  char reso_thumb;
  char num_component;

  unsigned int prec_width[1 + MAX_DWT_LEVEL];
  unsigned int prec_height[1 + MAX_DWT_LEVEL];
//   unsigned int qcd_quant_step[1 + 3 * MAX_DWT_LEVEL];
  unsigned int qcc_quant_step[MAX_COMPONENT][1 + 3 * MAX_DWT_LEVEL];

  char XRsiz[MAX_COMPONENT];
  char YRsiz[MAX_COMPONENT];

  char conf_bypass;
  char conf_reset;
  char conf_termall;
  char conf_causal;

  char conf_pterm;
  char conf_segsym;
  char conf_sop;
  char conf_eph;

  char conf_dwt97;
  char conf_mct;
  char conf_quant;

  char conf_jp2file;

  char conf_vhdltb;

  char conf_dwti53;

  stru_dec_param tile_dec_param;                         // param for individual tile
  stru_tile_component component[MAX_COMPONENT];     // end with LL
  
  unsigned char t2_scan_component;
  unsigned char t2_scan_layer;
  unsigned char t2_scan_resolution;

} stru_opt;

typedef struct tag_stru_persis_buffer {
  // all persistent buffers used by codec

  int *dwt_buffer[MAX_COMPONENT]; // dwt output buffer, used by dwt/t1

} stru_persis_buffer;

typedef struct tag_stru_scratch_buffer {

#if !SOFTWARE_MULTI_THREAD
  // off chip, used by dwt
  int *mct_g_buffer;              // mct tmp g buffer, used in RGB mode
  int *mct_r_buffer;              // mct tmp r buffer, used in RGB mode

  int *dwt_tmp_buffer;            // dwt tmp/ll buffer

#if defined(ASIC)
  // off chip, used by t1
  unsigned int *pass_fpga_buffer;   // pass tmp buffer for dma in t1 fpga

  // on chip, used by t1
  short *cb_fpga[2];                // cb buffer for dma in t1 fpga
#else
  // off chip, used by t1
  unsigned char *pass_buffer;       // mq input buffer

  // on chip, used by t1
  unsigned short *cb_ctx;           // code block contex
  unsigned int *cb_data;            // code block data
#endif

  // on chip, used by dwt
  int *dwt_proc_buffer;             // dwt processing buffer
  int dwt_proc_size;                // total size of dwt processing buffer

#else	//SOFTWARE_MULTI_THREAD

	unsigned char *pass_buffer;       // mq input buffer
	int size_pass_buf;
#if SOFTWARE_MULTI_THREAD
	int_32 *cb_data;          			// code block data
	int_32 *cb_ctx;						// code block contex
#else
	unsigned int *cb_data;            // code block data
	unsigned short *cb_ctx;           // code block contex
#endif
	int size_data_buf;
	int size_ctx_buf;

#endif	//SOFTWARE_MULTI_THREAD
} stru_scratch_buffer;

#if !SOFTWARE_MULTI_THREAD
typedef struct tag_stru_dspbios {

  SEM_Handle sem_t2_start, sem_dwt_start, sem_t1_cb_start, sem_t1_mq_start;
  SEM_Handle sem_t2_done,  sem_dwt_done,  sem_t1_cb_done,  sem_t1_mq_done;

  SEM_Handle sem_fpga_cb, sem_fpga_mq;

  TSK_Handle tsk_dec_t2, tsk_dec_dwt, tsk_dec_t1_cb, tsk_dec_t1_mq;

} stru_dspbios;
#endif
typedef struct tag_stru_jpc_siz {

  short width;
  short height;
  short tile_width;
  short tile_height;

  short num_component;
  char bitdepth[MAX_COMPONENT];
  char XRsiz[MAX_COMPONENT];
  char YRsiz[MAX_COMPONENT];

} stru_jpc_siz;

typedef struct tag_stru_jpc_cod {

  char siz_prec;                              // not in dec_param or s_opt
  unsigned int prec_width[1 + MAX_DWT_LEVEL];         // not in dec_param, in s_opt
  unsigned int prec_height[1 + MAX_DWT_LEVEL];        // not in dec_param, in s_opt

  char sop;
  char eph;

  char progression;
  char layer;
  char mct;

  char dwt_level;
  unsigned short cb_width;
  unsigned short cb_height;
  char dwt97;
  char dwt_i53;

  char bypass;
  char reset;
  char termall;
  char causal;
  char pterm;
  char segsym;

} stru_jpc_cod;


typedef struct tag_stru_jpc_qcd {

  unsigned char enable;
  int guard_bit;
  unsigned int quant_step[1 + 3 * MAX_DWT_LEVEL];     // not in dec_param, in s_opt

} stru_jpc_qcd;

typedef struct tag_stru_jpc_rgn {

  unsigned char enable[MAX_COMPONENT];
  char roi_shift[MAX_COMPONENT];

} stru_jpc_rgn;


typedef struct tag_stru_jpc_poc {

	unsigned char num_poc;

	unsigned	char	        RSpoc[MAX_NUM_POC];		// 0 - 32
	unsigned	short	        CSpoc[MAX_NUM_POC];		// 0 -255 (number_components < 257) or 0 - 383
	unsigned	short			LYEpoc[MAX_NUM_POC];	// 0 - 65535
	unsigned	char	        REpoc[MAX_NUM_POC];		// RSpoc + 1 - 33
	unsigned	short	        CEpoc[MAX_NUM_POC];		// CSpoc + 1 - 255 (number_components < 257) or 0 - 383
	unsigned	char			Ppoc[MAX_NUM_POC];

} stru_jpc_poc;

typedef struct tag_stru_jpc_sot {

  unsigned int tile_offset;
  unsigned int tile_len;
  unsigned short tile_idx;

  unsigned char tilep_num;
  unsigned char tilep_idx;

} stru_jpc_sot;

//*************************************************************************
  typedef struct tag_stru_jpc_pp_header {

    unsigned char f_ppm, f_ppt;								      // PPM and PPT exist flags
 
    unsigned int num_of_ppm;                   
    unsigned int ppm_offset[MAX_TILE];              // stream offset for each ppm section

    unsigned int num_of_ppt;                   
    unsigned int ppt_offset[MAX_PACKAGE];            // stream offset for each ppt section

    unsigned char non_first_tile;                   // first tile in component flag	
  } stru_jpc_pp_header;   

//*************************************************************************

typedef struct tag_stru_jpc_syntax {

  stru_jpc_siz        s_siz;                    // image and tile size

  stru_jpc_cod        s_cod[MAX_COMPONENT];   // coding style components in first tile header
      
  stru_jpc_qcd        s_qcd[MAX_COMPONENT];   // quantization for each component in main header

  stru_jpc_rgn        s_rgn;                  // ROI info in main header
  
  stru_jpc_poc        s_m_poc;
  stru_jpc_poc        s_t_poc;  
  stru_jpc_sot        s_sot;                // start of tile-part
  
  stru_jpc_pp_header  s_pp_header;          // PPM and PPT section

} stru_jpc_syntax;

typedef struct tag_stru_jp2_colr {

  int meth;
  int enmu_colr_space;

} stru_jp2_colr;

typedef struct tag_stru_jp2_jp2h {

  stru_jp2_colr s_colr;             // color specification

} stru_jp2_jp2h;

typedef struct tag_stru_jp2_syntax {

  char exists_jp2;
  stru_jp2_jp2h s_jp2h;             // jp2 header

  stru_jpc_syntax s_jp2stream;      // jpc header

} stru_jp2_syntax;

typedef struct tag_stru_pipe {


  // file format & code stream format
  stru_jp2_syntax s_jp2file;

  // multiple copies of s_opt/persis_buffer are for pipeline version that
  // implemented for DSP/FPGA, ([1]/[2]) are not used in non-pipeline version

#if !SOFTWARE_MULTI_THREAD
  stru_dspbios s_dspbios;
  stru_persis_buffer persis_buffer[CNT_PERSIS_BUF];
	stru_opt s_opt[CNT_S_OPT];
#else
  stru_opt *s_opt;
#endif	// SOFTWARE_MULTI_THREAD
  stru_scratch_buffer scratch_buffer;

  stru_stream stream;

  int idx_opt_dwt;                  // active s_opt for dwt/t1/t2
  int idx_opt_t1;
  int idx_opt_t2;

  int idx_persis_dwt;               // active persis_buffer for dwt/t1/t2
  int idx_persis_t1;
  int idx_persis_t2;

  char pipe_pause;
  char pipe_use;

} stru_pipe;

void tsk_dec_t2(void *arg);
void tsk_dec_dwt(void *arg);
void dec_t1_cb(void *arg);
void tsk_dec_t1_mq(void *arg);

#if !SOFTWARE_MULTI_THREAD
int dec_dwt(stru_pipe *s_pipe);
int dec_t1(stru_pipe *s_pipe);
int dec_dwt_53(stru_dec_param *dec_param, stru_opt *s_opt,
			   stru_persis_buffer *persis_buffer, stru_scratch_buffer *scratch_buffer,
			   int comp_num);
int dec_dwt_i53(stru_dec_param *dec_param, stru_opt *s_opt,
				stru_persis_buffer *persis_buffer, stru_scratch_buffer *scratch_buffer,
				int comp_num);

int dec_t1_comp(stru_dec_param *dec_param, stru_opt *s_opt,
				stru_persis_buffer *persis_buffer,
				stru_scratch_buffer *scratch_buffer, int comp_num);
int dec_init_pipe(stru_dec_param *dec_param);

int dec_info(stru_dec_param *dec_param, int pipe_use, int tx0, int ty0,
			 int *siz_s_opt, int *siz_persis_buffer, int *siz_extra);

int dec_t2(stru_pipe *s_pipe,int use_j2k_header);

#else	// !SOFTWARE_MULTI_THREAD
#if GPU_T1_TESTING
int	cpy_cb_passes( uint_8 * pass_buf, short numBps, stru_code_block *cur_cb, unsigned int& length);
#endif
int  dec_cb_stripe(stru_pipe * s_pipe, int tileId, int comp_num, short band_num, stru_code_block *cur_cb, int number_in_stripe, unsigned char * lut_ctx, int thread_index, unsigned char * result_buf, int result_buf_pitch);
int dec_init_pipe(stru_dec_param *dec_param, int threadNum);
int dec_info(stru_dec_param *dec_param, int pipe_use, int tx0, int ty0,
			 int *siz_s_opt, int *siz_persis_buffer, int *siz_extra, int thread_num);
int dec_t2(stru_dec_param *dec_param, int resolution, int use_j2k_header);

#endif	// !SOFTWARE_MULTI_THREAD
/*static*/ int dec_reset_opt(stru_dec_param *dec_param, stru_opt *s_opt,
						 stru_jp2_syntax *s_jp2file, int resolution,
						 int tx0, int ty0, int width, int height, int tile_idx);


int dec_init_opt(stru_dec_param *dec_param, stru_opt *s_opt, int tx0, int ty0);


void dec_get_context_table(const unsigned char ** lllh, const unsigned char ** hl, const unsigned char ** hh);

#if GPU_T1_TESTING
void dec_get_all_context_table(const unsigned char ** lllh, const unsigned char ** hl, const unsigned char ** hh, const unsigned char ** sign);
void dec_get_trans_table(const unsigned char ** table, unsigned int& size);
#endif

const unsigned char * dec_get_sign_lut();	

int dec_t1_p1(stru_mqd *mqd, unsigned int *cb_data, unsigned short *cb_ctx,
              int cb_width, int cb_height, int mbp, char causal, unsigned char *lut_ctx);
int dec_t1_p2(stru_mqd *mqd, unsigned int *cb_data, unsigned short *cb_ctx,
              int cb_width, int cb_height, int mbp);
int dec_t1_p3(stru_mqd *mqd, unsigned int *cb_data, unsigned short *cb_ctx,
              int cb_width, int cb_height, int mbp, char causal, unsigned char *lut_ctx);
int dec_t1_p1_raw(stru_mqd *mqd, unsigned int *cb_data, unsigned short *cb_ctx,
                  int cb_width, int cb_height, int mbp, char causal);
int dec_t1_p2_raw(stru_mqd *mqd, unsigned int *cb_data, unsigned short *cb_ctx,
                  int cb_width, int cb_height, int mbp);

int mqd_reset(stru_mqd *mqd);
int mqd_init(stru_mqd *mqd, unsigned char *buffer);
int mqd_flush(stru_mqd *mqd, int term, int pterm);

int mqd_init_raw(stru_mqd *mqd, unsigned char *buffer);
int mqd_flush_raw(stru_mqd *mqd, int term);


int dec_progression(stru_dec_param *dec_param, stru_opt *s_opt, stru_stream *stream);

int dec_read_header(stru_dec_param *dec_param, stru_jp2_syntax *s_jp2file,
                    stru_stream *stream, int use_j2k_header);
int dec_read_tile_header(stru_jp2_syntax *s_jp2file, stru_stream *stream/*, int tile_idx*/);

int tagtree_num_node(int width, int height);
int tagtree_init(/*stru_dec_param *dec_param, */stru_opt *s_opt);
int tagtree_dec(stru_stream *stream, stru_tagtree_node *node,
                unsigned int threshold, int *included);

int ostream_init(stru_stream *stream, unsigned char *buffer, int buf_size);
int ostream_tell(stru_stream *stream);
int ostream_seek(stru_stream *stream, int offset);
int ostream_skip(stru_stream *stream, int offset);
int ostream_byte_align(stru_stream *stream);

int ostream_get_bit(stru_stream *stream);
int ostream_get_int(stru_stream *stream, int bits);
unsigned char * ostream_restore_bytes(stru_stream *stream, unsigned int len);
int ostream_read_bytes(stru_stream *stream, unsigned int byts);

//int ceil_ratio(int num, int den);
#define ceil_ratio(num, den)		(((num) + (den) - 1 ) / (den))
// int floor_ratio(int num, int den);
#define floor_ratio(num, den)		((num)/(den))
int count_bits(unsigned int v);
int int_log2(unsigned int v);

void touch(const void *start, int bytes);
unsigned int bmi_DAT_copy_my(void *src, void *dst, unsigned short byteCnt);
unsigned int bmi_DAT_copy2d_my(unsigned int type, void *src, void *dst,
                               unsigned short lineLen, unsigned short lineCnt,
                               unsigned short linePitch);

void bmi_CACHE_wbL2_my(void *blockPtr, unsigned int byteCnt, int wait);
void bmi_CACHE_invL2_my(void *blockPtr, unsigned int byteCnt, int wait);
void bmi_CACHE_wbInvL2_my(void *blockPtr, unsigned int byteCnt, int wait);
void bmi_CACHE_wbInvAllL2_my(int wait);

unsigned int bmi_CLK_countspms();
unsigned int bmi_CLK_gethtime();

#if !SOFTWARE_MULTI_THREAD
SEM_Handle bmi_SEM_create(int count);
void bmi_SEM_post(SEM_Handle sem);
void bmi_SEM_ipost(SEM_Handle sem);
void bmi_SEM_pend(SEM_Handle sem);
void bmi_SEM_delete(SEM_Handle sem);

void bmi_TSK_delete(TSK_Handle task);
TSK_Handle bmi_TSK_create(void (*task)(void *arg), void *arg, int priority, unsigned int stacksize);
#endif

void bmi_CACHE_wbL2(void *blockPtr, unsigned int byteCnt, int wait);
void bmi_CACHE_invL2(void *blockPtr, unsigned int byteCnt, int wait);
void bmi_CACHE_wbInvL2(void *blockPtr, unsigned int byteCnt, int wait);
void bmi_CACHE_wbInvAllL2(int wait);

void bmi_DAT_close();
void bmi_DAT_wait(unsigned int id);
unsigned int bmi_DAT_open(int chaNum, int priority, unsigned int flags);
unsigned int bmi_DAT_fill(void *dst, unsigned short byteCnt, unsigned int *fillValue);
unsigned int bmi_DAT_copy(void *src, void *dst, unsigned short byteCnt);
unsigned int bmi_DAT_copy2d(unsigned int type, void *src, void *dst,
                            unsigned short lineLen, unsigned short lineCnt,
                            unsigned short linePitch);


#if !defined(USE_MARCO_MQ)
  void mqd_dec(stru_mqd *mqd, unsigned short *bit, unsigned char ctx);
  void mqd_dec_raw(stru_mqd *mqd, unsigned short *bit);
#else
  extern const unsigned int table_mq[47][4];

  #define mqd_bytein(mqd)                         \
  {                                               \
    if(*mqd->buffer == 0xff) {                    \
      if(*(mqd->buffer + 1) > 0x8f) {             \
        mqd->ct =  8;                             \
      } else {                                    \
        ++mqd->buffer;                            \
        mqd->c  += 0xfe00 - (*mqd->buffer << 9);  \
        mqd->ct =  7;                             \
      }                                           \
    } else {                                      \
      ++mqd->buffer;                              \
      mqd->c  +=   (0xff - *mqd->buffer) << 8;    \
      mqd->ct =    8;                             \
    }                                             \
  }

  #define mqd_renorme(mqd)              \
  {                                     \
    do {                                \
      if(!mqd->ct)    mqd_bytein(mqd);  \
      mqd->a <<= 1;                     \
      mqd->c <<= 1;                     \
      --mqd->ct;                        \
    } while(!(mqd->a & 0x8000));        \
  }

  #define mqd_dec(mqd, bit, ctx)                            \
  {                                                         \
    unsigned int idx;                                       \
                                                            \
    idx   = mqd->cx_states[ctx][0];                         \
    *bit  = mqd->cx_states[ctx][1];                         \
                                                            \
    mqd->a -= table_mq[idx][0];                             \
                                                            \
    if((mqd->c >> 16) < mqd->a) {                           \
      if(!(mqd->a & 0x8000)) {                              \
        if(mqd->a < table_mq[idx][0]) {                     \
          *bit ^= 1;                                        \
          mqd->cx_states[ctx][1] ^= table_mq[idx][3];       \
          mqd->cx_states[ctx][0] = table_mq[idx][2];        \
        } else {                                            \
          mqd->cx_states[ctx][0] = table_mq[idx][1];        \
        }                                                   \
                                                            \
        mqd_renorme(mqd);                                   \
      }                                                     \
    } else {                                                \
      mqd->c -= mqd->a << 16;                               \
                                                            \
      if(mqd->a < table_mq[idx][0]) {                       \
        mqd->cx_states[ctx][0] = table_mq[idx][1];          \
      } else {                                              \
        *bit ^=  1;                                         \
        mqd->cx_states[ctx][1] ^= table_mq[idx][3];         \
        mqd->cx_states[ctx][0] = table_mq[idx][2];          \
      }                                                     \
                                                            \
      mqd->a = table_mq[idx][0];                            \
      mqd_renorme(mqd);                                     \
    }                                                       \
  }

  #define mqd_dec_raw(mqd, bit)         \
  {                                     \
    if(!mqd->ct) {                      \
      if(mqd->c == 0xFF) {              \
        mqd->ct = 7;                    \
        mqd->c = *mqd->buffer++;        \
        if(mqd->c == 0xFF) {            \
          mqd->ct = 8;                  \
          --mqd->buffer;                \
        }                               \
      } else {                          \
        mqd->ct = 8;                    \
        mqd->c = *mqd->buffer++;        \
      }                                 \
    }                                   \
                                        \
    --mqd->ct;                          \
    *bit = (mqd->c >> mqd->ct) & 0x1;   \
  }
#endif

#if defined(B_FPGA_TB)
  int fpga_tb_config(stru_opt *s_opt);
  int fpga_tb_open(stru_dec_param *dec_param, stru_opt *s_opt);
  int fpga_tb_dump_img(stru_dec_param *dec_param, stru_opt *s_opt);
  int fpga_tb_dump_dwt(stru_dec_param *dec_param, stru_opt *s_opt, stru_persis_buffer *persis_buffer);
  int fpga_tb_dump_t1_dec(stru_dec_param *dec_param, stru_opt *s_opt, stru_persis_buffer *persis_buffer);
  int fpga_tb_close(stru_pipe *s_pipe);
#endif

#endif // DEC_STRU_H
