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

#define TAGTREE_MAX_LEVEL     32

static inline int tagtree_build(stru_tagtree *tree, stru_tagtree_node *node,
                                int width, int height);

/**************************************************************************************/
/*                                 tagtree_num_node                                   */
/**************************************************************************************/
int tagtree_num_node(int width, int height)
{
  int num_node = 0;

  if(width && height) {
    while(1) {
      num_node += width * height;
      if(width == 1 && height == 1)   break;
      width = (width + 1) >> 1;       height = (height + 1) >> 1;
    }
  }

  return num_node;
}

/**************************************************************************************/
/* static                            tagtree_build                                    */
/**************************************************************************************/
static inline int tagtree_build(stru_tagtree *tree, stru_tagtree_node *node,
                                int width, int height)
{
  int w, h, i, j;
  stru_tagtree_node *last, *parent;

  tree->width = width;
  tree->height = height;
  tree->node = node;

  while(1) {
    last = node + width * height;
    w = (width + 1) >> 1, h = (height + 1) >> 1;
    for(i = 0; i < height; ++i) {
      for(parent = last + (i >> 1) * w, j = 0; j < width; ++j) {
        node->low = 0;
        node->value = 0;
        node->parent = parent;
        ++node;  parent += (j & 1);
      }
    }

    if (width == 1 && height == 1)  break;
    width = w;  height = h;
  }

  tree->num_node = (short)(node - tree->node);

  --node;
  node->low = 0;
  node->value = 0;
  node->parent = 0;

  return 0;
}

/**************************************************************************************/
/*                                    tagtree_init                                    */
/**************************************************************************************/
int tagtree_init(/*stru_dec_param *dec_param,*/ stru_opt *s_opt)
{
  int comp_num, band_num, prec_num;
  int num_component = s_opt->num_component;
  stru_tagtree_node *inclnode, *imsbnode;
  stru_tile_component *cur_comp;
  stru_precinct *cur_prec;
  stru_subband *cur_band;

  for(cur_comp = s_opt->component, comp_num = num_component;
      comp_num; --comp_num, ++cur_comp) {
    for(cur_band = cur_comp->band, band_num = cur_comp->num_band;
        band_num; --band_num, ++cur_band) {
      for(cur_prec = cur_band->precinct, prec_num = cur_band->nprec_bw * cur_band->nprec_bh;
          prec_num; --prec_num, ++cur_prec) {

        if(!cur_prec->ncb_pw || !cur_prec->ncb_ph)   continue;

        // build tagtree structure
        inclnode = cur_prec->inclnode;
        imsbnode = cur_prec->imsbnode;
        tagtree_build(cur_prec->incltree, inclnode, cur_prec->ncb_pw, cur_prec->ncb_ph);
        tagtree_build(cur_prec->imsbtree, imsbnode, cur_prec->ncb_pw, cur_prec->ncb_ph);
      }
    }
  }

  return 0;
}

/**************************************************************************************/
/*                                    tagtree_dec                                     */
/**************************************************************************************/
int tagtree_dec(stru_stream *stream, stru_tagtree_node *node,
                unsigned int threshold, int *included)
{
  stru_tagtree_node *stk[TAGTREE_MAX_LEVEL - 1], *nod = node;
  stru_tagtree_node **stkptr = stk;
  unsigned int low = 0;
  int rc = 0;



  // traverse to the root of the tree, recording the path taken.
  while(nod->parent) {
    *stkptr++ = nod;
    nod = nod->parent;
  }

  while(1) {
    if(nod->low < low)  nod->value = nod->low = low;

    while(nod->low == nod->value && nod->low < threshold) {
      rc = ostream_get_bit(stream);
      if(!rc)   ++nod->value;
      ++nod->low;
    }
    low = nod->value;

    if(stkptr == stk)   break;
    nod = *--stkptr;
  }

  *included = (nod->low != nod->value);

  return 0;
}
