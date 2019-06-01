/***************************************************************************************
 *
 * Copyright (C) 2004 Broad Motion, Inc.
 *
 * All rights reserved.
 *
 * http://www.broadmotion.com
 *
 **************************************************************************************/

#if !defined(PLATFORM_H)
#define PLATFORM_H

// cpu processor used
#define PLATFORM_SOFTWARE 1
#define PLATFORM_ASIC     0

#if (PLATFORM_SOFTWARE == 1) && !defined(WIN32)
  #define WIN32
#elif (PLATFORM_SOFTWARE == 2)
  #define TI_DSP
#elif (PLATFORM_SOFTWARE == 3)
  #define ALTERA_NIOS2
#endif

// asic accelerator used
#if (PLATFORM_ASIC == 1)
  #define ASIC_T1
#elif (PLATFORM_ASIC == 2)
  #define ASIC_DWT_T1
#endif

#endif // PLATFORM_H
