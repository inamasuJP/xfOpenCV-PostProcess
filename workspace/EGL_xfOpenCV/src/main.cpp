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

#include "shader.h"
#include "egl.h"
#include "texture.h"


#define SAMPLE_COUNT 5 // 5 or 15

bool enable_fpga = false;
bool enable_gpu = false;
int interval = 0;

int loopNum = 100;

cv::Mat in_img, out_img, ocv_ref, diff, filter, bright_img, blur_img;

void gpu_loop(Display *xdisplay, EGLDisplay display, EGLSurface surface, std::string pngFile,bool verbose,int mode)
{

	eglSwapInterval(display,interval); //Beyond 60fps
    in_img = cv::imread(pngFile,1);
    cv::flip(in_img , in_img, 0);

    GLuint program0 = load_shader_from_file("bright.vert", "bright.frag");
    GLuint program1 = load_shader_from_file("normal.vert", "normal.frag");
    GLuint program2 = load_shader_from_file("bloom.vert", "bloom.frag");
    GLuint program3 = load_shader_from_file("result.vert", "result.frag");

    const float vertices[] = {-1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f};

    //positionLocation 0~3
    GLuint position0 = glGetAttribLocation(program0, "position");
    glEnableVertexAttribArray(position0);
    glVertexAttribPointer(position0, 3, GL_FLOAT, GL_FALSE, 0, vertices);

    GLuint position1 = glGetAttribLocation(program1, "position");
    glEnableVertexAttribArray(position1);
    glVertexAttribPointer(position1, 3, GL_FLOAT, GL_FALSE, 0, vertices);

    GLuint position2 = glGetAttribLocation(program2, "position");
    glEnableVertexAttribArray(position2);
    glVertexAttribPointer(position2, 3, GL_FLOAT, GL_FALSE, 0, vertices);

    GLuint position3 = glGetAttribLocation(program3, "position");
    glEnableVertexAttribArray(position3);
    glVertexAttribPointer(position3, 3, GL_FLOAT, GL_FALSE, 0, vertices);

    glBindBuffer(GL_ARRAY_BUFFER, 0);//NULL

    //textureCoord0~2
    const float texcoords[] = {0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    GLuint texcoord0 = glGetAttribLocation(program0, "textureCoord");
    glEnableVertexAttribArray(texcoord0);
    glVertexAttribPointer(texcoord0, 2, GL_FLOAT, GL_FALSE, 0, texcoords);

    GLuint texcoord1 = glGetAttribLocation(program1, "textureCoord");
    glEnableVertexAttribArray(texcoord1);
    glVertexAttribPointer(texcoord1, 2, GL_FLOAT, GL_FALSE, 0, texcoords);

    GLuint texcoord2 = glGetAttribLocation(program2, "textureCoord");
    glEnableVertexAttribArray(texcoord2);
    glVertexAttribPointer(texcoord2, 2, GL_FLOAT, GL_FALSE, 0, texcoords);
    glBindBuffer(GL_ARRAY_BUFFER, 0); //NULL

    //const float texcoords2[] = {0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    const float texcoords2[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    GLuint texcoord3 = glGetAttribLocation(program3, "textureCoord");
    glEnableVertexAttribArray(texcoord3);
    glVertexAttribPointer(texcoord3, 2, GL_FLOAT, GL_FALSE, 0, texcoords2);
    glBindBuffer(GL_ARRAY_BUFFER, 0); //NULL

    // textureLocation 0~4
    GLuint textures[] = {
        static_cast<GLuint>(glGetUniformLocation(program0, "texture")) ,
        static_cast<GLuint>(glGetUniformLocation(program1, "texture")) ,
        static_cast<GLuint>(glGetUniformLocation(program2, "texture")) ,
        static_cast<GLuint>(glGetUniformLocation(program3, "originalTexture")) ,
        static_cast<GLuint>(glGetUniformLocation(program3, "bloomTexture")) ,
        };

    GLuint offsetsLocationH = static_cast<GLuint>(glGetUniformLocation(program2, "offsetsH"));
    GLuint weightsLocationH = static_cast<GLuint>(glGetUniformLocation(program2, "weightsH"));
    GLuint offsetsLocationV = static_cast<GLuint>(glGetUniformLocation(program2, "offsetsV"));
    GLuint weightsLocationV = static_cast<GLuint>(glGetUniformLocation(program2, "weightsV"));
    GLuint isVerticalLocation = static_cast<GLuint>(glGetUniformLocation(program2, "isVertical"));
    GLuint toneScaleLocation = static_cast<GLuint>(glGetUniformLocation(program3, "toneScale"));

    GLuint pngTexture; // webGLの例だと"texture"
    glGenTextures(1,&pngTexture);
    glBindTexture(GL_TEXTURE_2D, pngTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    //glTexImage2D(GL_TEXTURE_2D, 0, png.has_alpha ? GL_RGBA : GL_RGB, png.width, png.height,
    //             0, png.has_alpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, png.data);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, in_img.cols, in_img.rows, 0 , GL_RGB , GL_UNSIGNED_BYTE, in_img.data); // test

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D,0); //NULL

    GLuint originalScreen;
    glGenTextures(1,&originalScreen);
    glBindTexture(GL_TEXTURE_2D,originalScreen);
    //glTexImage2D(GL_TEXTURE_2D, 0, png.has_alpha ? GL_RGBA : GL_RGB, png.width, png.height, 0, png.has_alpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,in_img.cols, in_img.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D,0); //NULL

    // --------------change framebuffer to texture ----------------------------------------------------------------------------------------------------
    GLuint mainBuffer;
    glGenTextures(1,&mainBuffer);
    glBindTexture(GL_TEXTURE_2D,mainBuffer);
    //glTexImage2D(GL_TEXTURE_2D, 0, png.has_alpha ? GL_RGBA : GL_RGB, png.width, png.height, 0, png.has_alpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,in_img.cols, in_img.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D,0); //NULL

    GLuint collectBrightBuffer;
    glGenTextures(1,&collectBrightBuffer);
    glBindTexture(GL_TEXTURE_2D,collectBrightBuffer);
    //glTexImage2D(GL_TEXTURE_2D, 0, png.has_alpha ? GL_RGBA : GL_RGB, png.width, png.height, 0, png.has_alpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,in_img.cols, in_img.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D,0); //NULL

    GLuint bloomBuffer;
    glGenTextures(1,&bloomBuffer);
    glBindTexture(GL_TEXTURE_2D,bloomBuffer);
    //glTexImage2D(GL_TEXTURE_2D, 0, png.has_alpha ? GL_RGBA : GL_RGB, png.width, png.height, 0, png.has_alpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,in_img.cols, in_img.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D,0); //NULL

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
	GLfloat weightVH[SAMPLE_COUNT * SAMPLE_COUNT];
	GLfloat offsetVH[SAMPLE_COUNT * SAMPLE_COUNT * 2];
	for(int i = 0;i < SAMPLE_COUNT; i++){
		for(int j = 0; j < SAMPLE_COUNT;j++){
			weightVH[i * SAMPLE_COUNT + j] = filter.at<GLfloat>(j,i);
			offsetVH[(i * SAMPLE_COUNT + j)*2] = offsetTmp[j];
			offsetVH[(i * SAMPLE_COUNT + j)*2 + 1] = offsetTmp[i];
		}
	}


    std::chrono::milliseconds sec(1000);
    auto lastTime = std::chrono::high_resolution_clock::now();
    int nbFrames = 0;

    bool mode_1 = false;
    bool mode_2 = false;
    bool mode_3 = false;
    bool mode_4 = false;
    for(int i = 0; i < 4; i++){
    	switch (mode % 10){
    		case 1:
    			mode_1 = true;
    			break;
    		case 2:
    			mode_2 = true;
    			break;
    		case 3:
    		 	mode_3 = true;
    		    break;
    		case 4:
    		    mode_4 = true;
    		    break;
    		default:
    			break;
    	}
    	mode /= 10;
    }
    std::cout << "mode_1 : " << std::boolalpha << mode_1
    		<< "mode_2 : " << std::boolalpha << mode_2
			<< "mode_3 : " << std::boolalpha << mode_3
			<< "mode_4 : " << std::boolalpha << mode_4 << std::endl;
    bool while_loop = true;
    int loop_count = 0;
    long int average_tmp = 0;

    while (while_loop)
     {
    	auto startTime = std::chrono::high_resolution_clock::now();
    	XPending(xdisplay);
    	if(mode_1){ //load in_img (program1)
			glUseProgram(program1);
			glBindFramebuffer(GL_FRAMEBUFFER, 0); //0を指定すると描画する
			//glClearColor(0.0f, 0.0f, 0.0f, 0.1f);
			//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, pngTexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, in_img.cols, in_img.rows, 0 , GL_RGB , GL_UNSIGNED_BYTE, in_img.data); // test add
			glUniform1i(textures[1], 0);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    	}
		if(mode_2){ //LUT (program0)
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D,originalScreen);
			glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,0,0,in_img.cols, in_img.rows,0);
			glUseProgram(program0);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
          //glClearColor(0.0f, 0.0f, 0.0f, 0.1f);
          //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D,originalScreen);
			glUniform1i(textures[0], 1);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}
		if(mode_3){//Filter2D (program2)
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D,collectBrightBuffer);
			glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,0,0,in_img.cols, in_img.rows,0);
			glUseProgram(program2);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			//glClearColor(0.0f, 0.0f, 0.0f, 0.1f);
			//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D,collectBrightBuffer);
			glUniform1i(textures[2],0);
			glUniform1i(isVerticalLocation,true);
			glUniform2fv(offsetsLocationV, SAMPLE_COUNT * SAMPLE_COUNT * 2 ,offsetVH);
			glUniform1fv(weightsLocationV, SAMPLE_COUNT * SAMPLE_COUNT ,weightVH);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}
		if(mode_4){ //Add (program3)
			glActiveTexture(GL_TEXTURE1); //add
			glBindTexture(GL_TEXTURE_2D,collectBrightBuffer);//add
			glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,0,0,in_img.cols, in_img.rows,0);
			glUseProgram(program3);
			glBindFramebuffer(GL_FRAMEBUFFER,0);
			//glClearColor(0.0f, 0.0f, 0.0f, 0.1f);
			//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D,originalScreen);
			glUniform1i(textures[3],0);
			//glUniform1f(toneScaleLocation,0.7);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D,collectBrightBuffer);
			glUniform1i(textures[4],1);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}
		eglSwapBuffers(display, surface);

		auto curerntTime = std::chrono::high_resolution_clock::now();
		nbFrames++;
		if (curerntTime - lastTime >= sec) {
			loop_count ++;
			average_tmp += std::chrono::duration_cast< std::chrono::microseconds >(curerntTime - startTime).count() ;
        	std::cout << "Elapsed Time(µs) : "<<std::chrono::duration_cast< std::chrono::microseconds >(curerntTime - startTime).count() << std::endl;
			printf("%d fps \n",nbFrames);
			nbFrames = 0;
			lastTime = std::chrono::high_resolution_clock::now();
           if(loop_count > 9) while_loop = false;
		}
		if(verbose){
			GLenum error_code;
		   error_code = glGetError();
		   if( error_code != GL_NO_ERROR ) std::cout << "glGetError Code: " << error_code <<std::endl;
		}
     }
    std::cout << "10 times Average(µs) : " << average_tmp / 10 << std::endl;

    deleteShaderProgram(program0);
    deleteShaderProgram(program1);
    deleteShaderProgram(program2);
    deleteShaderProgram(program3);
}

void fpga_loop(Display *xdisplay, EGLDisplay display, EGLSurface surface, std::string pngFile,bool verbose,int mode)
{
	eglSwapInterval(display,interval); //Beyond 60fps
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

	// EGL
	GLuint program1 = load_shader_from_file("normal.vert", "normal.frag");
	const float vertices[] = {-1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f};
	GLuint position1 = glGetAttribLocation(program1, "position");
	glEnableVertexAttribArray(position1);
	glVertexAttribPointer(position1, 3, GL_FLOAT, GL_FALSE, 0, vertices);
	glBindBuffer(GL_ARRAY_BUFFER, 0);//NULL

	const float texcoords[] = {0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f};
	GLuint texcoord1 = glGetAttribLocation(program1, "textureCoord");
	glEnableVertexAttribArray(texcoord1);
	glVertexAttribPointer(texcoord1, 2, GL_FLOAT, GL_FALSE, 0, texcoords);

	GLuint textures[] = {
			static_cast<GLuint>(glGetUniformLocation(program1, "texture"))
	};
	 GLuint cvTexture;
	 glGenTextures(1,&cvTexture);
	 glBindTexture(GL_TEXTURE_2D, cvTexture);
	 glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	 //glTexImage2D(GL_TEXTURE_2D, 0, png.has_alpha ? GL_RGBA : GL_RGB, png.width, png.height, 0, png.has_alpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, png.data);
	 glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	 glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	 //glBindTexture(GL_TEXTURE_2D,0); //NULL

	 // Put out for while loop
	 glUseProgram(program1);
	 glActiveTexture(GL_TEXTURE0);
	 glUniform1i(textures[0], 0);



	std::chrono::milliseconds sec(1000);
	auto lastTime = std::chrono::high_resolution_clock::now();
	int nbFrames = 0;
	for (int i = 0; i < loopNum; i++)
	{
		 auto loadTime = std::chrono::high_resolution_clock::now();
		 XPending(xdisplay);

		 imgInput.copyTo(in_img.data);

		 auto startTime = std::chrono::high_resolution_clock::now();
		 lut_accel(imgInput,imgBright,lut_ptr);
		 auto lutTime = std::chrono::high_resolution_clock::now();
		 Filter2d_accel(imgBright,imgFilter,filter_ptr,shift);
		 auto filter2DTime = std::chrono::high_resolution_clock::now();
		 arithm_accel(imgFilter,imgInput,imgOutput);
		 auto addTime = std::chrono::high_resolution_clock::now();

        //glUseProgram(program1);
        //glBindFramebuffer(GL_FRAMEBUFFER, 0); //0を指定すると描画する
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, in_img.cols,in_img.rows , 0, GL_RGB, GL_UNSIGNED_BYTE, imgOutput.data);
        //glClearColor(0.0f, 0.0f, 0.0f, 0.1f);
        //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //glActiveTexture(GL_TEXTURE0);
        //glBindTexture(GL_TEXTURE_2D, cvTexture);
        //glUniform1i(textures[0], 0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        eglSwapBuffers(display, surface);

        auto currentTime = std::chrono::high_resolution_clock::now();

		nbFrames++;
		if (currentTime - lastTime >= sec) {
			//std::cout << std::chrono::duration_cast< std::chrono::microseconds >(currentTime - startTime).count() << " (microsecond)" << std::endl;
			std::cout << "  convert to xf::Mat from cv::Mat Time(µs) : " <<std::chrono::duration_cast< std::chrono::microseconds >(startTime - loadTime).count() << "\n"
					<< "  lut_accel Time(µs) : " <<std::chrono::duration_cast< std::chrono::microseconds >(lutTime - startTime).count() << "\n"
					<< "  Filter2d_accel Time(µs) : " <<std::chrono::duration_cast< std::chrono::microseconds >(filter2DTime - lutTime).count() << "\n"
					<< "  arithm_accel Time(µs) : " <<std::chrono::duration_cast< std::chrono::microseconds >(addTime - filter2DTime).count() << "\n"
					<< "  GPU Load and Draw Time(µs) : " <<std::chrono::duration_cast< std::chrono::microseconds >(currentTime - addTime).count() <<"\n"
					<< "  Total Time (µs) : " << std::chrono::duration_cast< std::chrono::microseconds >(currentTime - loadTime).count() << std::endl;
			printf("%d fps \n",nbFrames);
			nbFrames = 0;
			lastTime = std::chrono::high_resolution_clock::now();
		}
	}

    ocv_ref.~Mat();
	diff.~Mat();
	filter.~Mat();
	bright_img.~Mat();
	blur_img.~Mat();
	in_img.~Mat();
	out_img.~Mat();
}


int main(int argc, char *argv[])
{
    int opt;
    opterr = 0; //getopt()のエラーメッセージを無効にする。

    bool verbose = false;
    int mode = 4;
    std::string pngFile = "Torus.png";

    while ((opt = getopt(argc, argv, "vm:p:fgn:i:")) != -1) {
        switch (opt){
            case 'v':
                verbose = true;
                break;

            case 'm':
                mode = atoi(optarg);
                if(mode < 1 || 1234 < mode) {
                    mode = 1234;
                    std::cout << "[Error] mode(-m) argument is only 1 ~ 4 number." << std::endl;
                }
                break;

            case 'p':
                pngFile = optarg;
                break;

            case 'f':
                enable_fpga = true;
                break;

            case 'g':
                enable_gpu = true;
                break;

            case 'n':
            	loopNum = atoi(optarg);
            	break;

            case 'i'://eglSwapInterval
            	interval = atoi(optarg);
            	break;

            default:
                std::cout << "Usage:\t" << argv[0] << " [-f -n number] [-g] [-v] [-m number(1~4)] [-p png-file-name] [-i number(0~10)]\n\n"
                    << "Options:\n"
                    << "\t -f : Enable FPGA acceleration / -g : Enable GPU acceleration \n"
					  << "\t -n : Exection time at FPGA mode \t(default : 100)\n"
                    << "\t -v : Enable verbose \t(default : false)\n"
                    << "\t -m : Change mode at GPU \t(default : 1234)\n"
                    << "\t -p : Set png file \t(default : Torus.png)\n"
					   << "\t -i : Set eglSwapInterval() 0:No Limit, 1:60fps 2:30fps 3:20fps ... \t(default : 0)"
					<< std::endl;
                return 0;
                break;
        }
    }
    if(enable_fpga && !enable_gpu) std::cout << "Enable FPGA acceleration" << std::endl;
    else if(enable_gpu && !enable_fpga) std::cout << "Enable GPU acceleration" << std::endl;
    else if(!enable_gpu && !enable_fpga) std::cout << "Disable Hardware acceleration. Use CPU. Enable HW option is '-f' or '-g'.  " << std::endl;
    else {
        std::cout << "FPGA / GPU are exclusive. Allow option is only '-f' or '-g'. " << std::endl;
        return -1;
    }
    std::cout << "Run Mode:" << mode << " Load Png:" << pngFile << " verbose:" << std::boolalpha << verbose << std::endl;
    if(interval == 0) std::cout << "Frame Limit : No Limit" << std::endl;
    else std::cout << "Frame Limit : " << 60/interval << std::endl;
    //auto png = loadPng(pngFile);

#if GRAY
	in_img = cv::imread(pngFile,0); // reading in the gray image
#else
	in_img = cv::imread(pngFile,1); // reading in the gray image
#endif
    if (in_img.data == NULL)
	{
    	std::cerr << "Cannot open image at" << pngFile << std::endl;
		return 0;
	}

    Display *xdisplay = XOpenDisplay(nullptr);
    if (xdisplay == nullptr)
    {
        std::cerr << "Error XOpenDisplay." << std::endl;
        exit(EXIT_FAILURE);
    }

    Window xwindow = XCreateSimpleWindow(xdisplay, DefaultRootWindow(xdisplay), 100, 100, in_img.cols, in_img.rows, 1, BlackPixel(xdisplay, 0), WhitePixel(xdisplay, 0));
    XMapWindow(xdisplay, xwindow);

    EGLDisplay display = nullptr;
    EGLContext context = nullptr;
    EGLSurface surface = nullptr;
    if (initializeEGL(xdisplay, xwindow, display, context, surface,verbose) < 0)
    {
        std::cerr << "Error initializeEGL." << std::endl;
        exit(EXIT_FAILURE);
    }
    if(enable_gpu) gpu_loop(xdisplay, display, surface,pngFile,verbose,mode);
    else if(enable_fpga) fpga_loop(xdisplay, display, surface,pngFile,verbose,mode);
    else {
        std::cout << "Use CPU mode.(Unimplemented)" << std::endl;
    }

    //deletePng(png);
    destroyEGL(display, context, surface);
    XDestroyWindow(xdisplay, xwindow);
    XCloseDisplay(xdisplay);

    return 0;
}
