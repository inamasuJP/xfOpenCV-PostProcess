/***************************************************************************
Copyright (c) 2019, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, 
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, 
this list of conditions and the following disclaimer in the documentation 
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors 
may be used to endorse or promote products derived from this software 
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, 
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/
/*  specify the filter height and filter width  */


#define  FILTER_HEIGHT  15 // change by inamasu from "3" to "15"
#define  FILTER_WIDTH  	15 // change by inamasu from "3" to "15"


/*  set the optimization type  */


#define NO  1  	// Normal Operation
#define RO  0  	// Resource Optimized

/* set the output type */


#define 	OUT_8U  	1
#define 	OUT_16S 	0

#define GRAY 0
#define RGBA 1

/* copy from xfopencv/examples/arithm/xf_config_params.h */
/*  set the input types  */

#define T_8U  1  // Input type of 8U
#define T_16S 0  // Input type of 16S

#define ARRAY	1
#define SCALAR	0
//macros for accel
#define FUNCT_NAME add // change from mutiply to add by inamasu
//#define EXTRA_ARG  0.05 // comment out by inamasu
#define EXTRA_PARM XF_CONVERT_POLICY_SATURATE

//OpenCV reference macros
#define CV_FUNCT_NAME add // change from mutiply to add
#define CV_EXTRA_ARG  0.05

