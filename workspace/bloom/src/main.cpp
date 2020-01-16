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

#include "xf_headers.h"
#include "xf_custom_convolution_config.h"
#include "xf_lut_config.h"
#include "xf_arithm_config.h"

#include <chrono>


int main(int argc, char** argv)
{
	if (argc != 3)
	{
		fprintf(stderr,"Invalid Number of Arguments!\nUsage:\n");
		fprintf(stderr,"<Executable Name> <input image path> <Number of executions> \n");
		return -1;
	}

	cv::Mat in_img, out_img, ocv_ref, diff, filter, bright_img, blur_img;


#if GRAY
	in_img = cv::imread(argv[1],0); // reading in the gray image
#else
	in_img = cv::imread(argv[1],1); // reading in the gray image
#endif


	if (in_img.data == NULL)
	{
		fprintf(stderr,"Cannot open image at %s\n",argv[1]);
		return 0;
	}


	unsigned char shift = SHIFT;

	//////////////////  Creating the kernel ////////////////
	filter.create(FILTER_HEIGHT,FILTER_WIDTH,CV_32F);

	/////////////////		create gaussian filter	add by inamasu	/////////////////
	cv::Mat weight = cv::Mat::zeros(1,FILTER_WIDTH, CV_32F);
	float offsetTmp[FILTER_WIDTH];
	float total = 0.0f;

	for (int i =0; i < FILTER_WIDTH; i++){
		float p = (i - (FILTER_WIDTH -1) * 0.5) * 0.0018;
		offsetTmp[i] = p;
		weight.at<float>(0,i) = std::exp(-p * p /2) / std::sqrt(M_PI * 2);
		total += weight.at<float>(0,i);
	}
	for (int i = 0; i< FILTER_WIDTH; i++) {
		weight.at<float>(0,i) /= total;
	}
	filter = weight.t() * weight;

#if __SDSCC__
	short int *filter_ptr=(short int*)sds_alloc_non_cacheable(FILTER_WIDTH*FILTER_HEIGHT*sizeof(short int));
#else
	short int *filter_ptr=(short int*)malloc(FILTER_WIDTH*FILTER_HEIGHT*sizeof(short int));
#endif
	for(int i = 0; i < FILTER_HEIGHT; i++)
	{
		for(int j = 0; j < FILTER_WIDTH; j++)
		{
			filter_ptr[i*FILTER_WIDTH+j] = (short int) (filter.at<float>(j,i) * std::pow(2,SHIFT) );
		}
	}

	/////////////////    create LUT table	add by inamasu   /////////////////
	float minBright = 0.6;
	int int_minBright = (int)(255 * minBright);
	uchar lut[256];
	for(int i = 0; i < int_minBright; i++) lut[i] = 0;
	for(int i = int_minBright; i < 256; i++) lut[i] = (i - int_minBright) / ((255.0 - int_minBright)/255.0) * 0.7;

#if __SDSCC__
	uchar_t*lut_ptr=(uchar_t*)sds_alloc_non_cacheable(256 * sizeof(uchar_t));
#else
	uchar_t*lut_ptr=(uchar_t*)malloc(256*sizeof(uchar_t));
#endif
	for(int i=0;i<256;i++) {
		lut_ptr[i]=lut[i];
	}


	/////////////////    OpenCV reference   /////////////////
#if GRAY
#if   OUT_8U
	out_img.create(in_img.rows,in_img.cols,CV_8UC1); // create memory for output image
	diff.create(in_img.rows,in_img.cols,CV_8UC1);    // create memory for difference image
#elif OUT_16S
	out_img.create(in_img.rows,in_img.cols,CV_16SC1); // create memory for output image
	diff.create(in_img.rows,in_img.cols,CV_16SC1);	  // create memory for difference image
#endif
#else
#if   OUT_8U
	out_img.create(in_img.rows,in_img.cols,CV_8UC3); // create memory for output image
	diff.create(in_img.rows,in_img.cols,CV_8UC3);    // create memory for difference image
#elif OUT_16S
	out_img.create(in_img.rows,in_img.cols,CV_16SC3); // create memory for output image
	diff.create(in_img.rows,in_img.cols,CV_16SC3);	  // create memory for difference image
#endif
#endif

	cv::Point anchor = cv::Point( -1, -1 );

#if __SDSCC__
	perf_counter hw_ctr_lut;
	hw_ctr_lut.start();
#endif
	cv::LUT(in_img, cv::Mat(1,256,CV_8UC1, lut) , bright_img);
#if __SDSCC__
	hw_ctr_lut.stop();
	uint64_t hw_cycles_lut = hw_ctr_lut.avg_cpu_cycles();
#endif

#if __SDSCC__
	perf_counter hw_ctr_filter;
	hw_ctr_filter.start();
#endif
	cv::filter2D(bright_img, blur_img, CV_8U , filter , anchor , 0 , cv::BORDER_CONSTANT);
#if __SDSCC__
	hw_ctr_filter.stop();
	uint64_t hw_cycles_filter = hw_ctr_filter.avg_cpu_cycles();
#endif

#if __SDSCC__
	perf_counter hw_ctr_add;
	hw_ctr_add.start();
#endif
	cv::add(blur_img,in_img,ocv_ref);
#if __SDSCC__
	hw_ctr_add.stop();
	uint64_t hw_cycles_add = hw_ctr_add.avg_cpu_cycles();
#endif
	imwrite("ref_img.jpg", ocv_ref);  // reference image


	static xf::Mat<INTYPE, HEIGHT, WIDTH, NPC_T> imgInput(in_img.rows,in_img.cols);
	static xf::Mat<INTYPE, HEIGHT, WIDTH, NPC_T> imgBright(in_img.rows,in_img.cols);
	static xf::Mat<INTYPE, HEIGHT, WIDTH, NPC_T> imgFilter(in_img.rows,in_img.cols);
	static xf::Mat<OUTTYPE, HEIGHT, WIDTH, NPC_T> imgOutput(in_img.rows,in_img.cols);

	imgInput.copyTo(in_img.data);

#if __SDSCC__
	perf_counter hw_ctr_lut_accel; hw_ctr_lut_accel.start();
#endif
	lut_accel(imgInput,imgBright,lut_ptr);
#if __SDSCC__
	hw_ctr_lut_accel.stop();
	uint64_t hw_cycles_lut_accel = hw_ctr_lut_accel.avg_cpu_cycles();
#endif

#if __SDSCC__
	perf_counter hw_ctr_filter_accel; hw_ctr_filter_accel.start();
#endif
	Filter2d_accel(imgBright,imgFilter,filter_ptr,shift);
#if __SDSCC__
	hw_ctr_filter_accel.stop();
	uint64_t hw_cycles_filter_accel = hw_ctr_filter_accel.avg_cpu_cycles();
#endif

#if __SDSCC__
	perf_counter hw_ctr_add_accel; hw_ctr_add_accel.start();
#endif
	arithm_accel(imgFilter,imgInput,imgOutput);
#if __SDSCC__
	hw_ctr_add_accel.stop();
	uint64_t hw_cycles_add_accel = hw_ctr_add_accel.avg_cpu_cycles();
#endif

	xf::imwrite("hls_imgBright.jpg",imgBright);
	xf::imwrite("hls_imgFilter.jpg",imgFilter);
	xf::imwrite("hls_imgOutput.jpg",imgOutput);

	xf::absDiff(ocv_ref,imgOutput,diff);    // Compute absolute difference image

	float err_per;
	xf::analyzeDiff(diff,2,err_per);
	cv::imwrite("diff_img.jpg",diff);

	ocv_ref.~Mat();
	diff.~Mat();
	filter.~Mat();
	bright_img.~Mat();
	blur_img.~Mat();
	in_img.~Mat();
	out_img.~Mat();

	std::chrono::milliseconds sec(1000);
	auto lastTime = std::chrono::high_resolution_clock::now();
	int nbFrames = 0;
	int loopNum = atoi(argv[2]);
	for (int i = 0; i < loopNum; i++)
	{
		auto startTime = std::chrono::high_resolution_clock::now();

		lut_accel(imgInput,imgBright,lut_ptr);
		Filter2d_accel(imgBright,imgFilter,filter_ptr,shift);
		arithm_accel(imgFilter,imgInput,imgOutput);

		auto currentTime = std::chrono::high_resolution_clock::now();
		nbFrames++;
		if (currentTime - lastTime >= sec) {
			std::cout << std::chrono::duration_cast< std::chrono::microseconds >(currentTime - startTime).count() << " microsecond" << std::endl;
			printf("%d fps \n",nbFrames);
			nbFrames = 0;
			lastTime = std::chrono::high_resolution_clock::now();
		}
	}

	if(err_per > 0.0f)
		return 1;

	return 0;
}

