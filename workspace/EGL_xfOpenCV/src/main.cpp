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


#define SAMPLE_COUNT 15

bool enable_fpga = false;
bool enable_gpu = false;

int loopNum = 100;

cv::Mat in_img, out_img, ocv_ref, diff, filter, bright_img, blur_img;

void gpu_loop(Display *xdisplay, EGLDisplay display, EGLSurface surface, std::string pngFile,bool verbose,int mode)
{

	eglSwapInterval(display,0); //Beyond 60fps
    //auto png = loadPng(pngFile);
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
    // ------------------------------------------------------------------------------------------------------------------

    //samplingあたりの部分
    /*

    GLfloat weightH[SAMPLE_COUNT];
    GLfloat offsetTmp[SAMPLE_COUNT];
    GLfloat total = 0.0f;

    for(int i = 0; i < SAMPLE_COUNT; i++) {
        GLfloat p = (i - (SAMPLE_COUNT -1) * 0.5) * 0.0018;
        offsetTmp[i] = p;
        weightH[i] = std::exp(-p * p /2) / std::sqrt(M_PI * 2);
        total += weightH[i];
    }
    for (int i = 0; i< SAMPLE_COUNT; i++) {
        weightH[i] /= total;
    }
    std::vector<GLfloat> tmp;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        tmp.insert(tmp.end(),{offsetTmp[i] , (GLfloat)0});
    }
    GLfloat offsetH[SAMPLE_COUNT * 2];
    for (int i = 0; i < SAMPLE_COUNT *2; i++) {
        offsetH[i] = tmp[i];
    }

    GLfloat weightV[SAMPLE_COUNT];
    total = 0.0f;
    for(int i = 0; i < SAMPLE_COUNT; i++) {
        GLfloat p = (i - (SAMPLE_COUNT -1) * 0.5) * 0.0018;
        offsetTmp[i] = p;
        weightV[i] = std::exp(-p * p /2) / std::sqrt(M_PI * 2);
        total += weightV[i];
    }
    for (int i = 0; i< SAMPLE_COUNT; i++) {
        weightV[i] /= total;
    }
    std::vector<GLfloat> tmp2;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        //tmp2.insert(tmp2.end(),{offsetTmp[i] , (GLfloat)0});
    	tmp2.insert(tmp2.end(),{(GLfloat)0, offsetTmp[i] });
    }
    GLfloat offsetV[SAMPLE_COUNT * 2];
    for (int i = 0; i < SAMPLE_COUNT *2; i++) {
        offsetV[i] = tmp2[i];
    }
*/


    //------------------------Copy from fpga-loop (tmp)---------------------------------------------------------------
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
    //while (false)
    //{
    /*
        XPending(xdisplay);
        //if(mode >= 0) {
        	auto startTime_0 = std::chrono::high_resolution_clock::now();

            glUseProgram(program1);
            glBindFramebuffer(GL_FRAMEBUFFER, 0); //0を指定すると描画する
            //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, originalScreen, 0); // Error Code: 1282
            glClearColor(0.0f, 0.0f, 0.0f, 0.1f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glActiveTexture(GL_TEXTURE0);

            auto clearTime_0 = std::chrono::high_resolution_clock::now();

            glBindTexture(GL_TEXTURE_2D, pngTexture);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, in_img.cols, in_img.rows, 0 , GL_RGB , GL_UNSIGNED_BYTE, in_img.data); // test add


            auto BindTextureTime_0 = std::chrono::high_resolution_clock::now();

            glUniform1i(textures[1], 0);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            auto drawTime_0 = std::chrono::high_resolution_clock::now();
        //}
        //if (mode >= 1) {
        	auto startTime_1 = std::chrono::high_resolution_clock::now();

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D,originalScreen);

            auto BindTextureTime_1 = std::chrono::high_resolution_clock::now();

            //glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,0,0,png.width,png.height,0);
            glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,0,0,in_img.cols, in_img.rows,0);

            auto CopyTexImageTime_1 = std::chrono::high_resolution_clock::now();

            glUseProgram(program0);
            //glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)collectBrightBuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClearColor(0.0f, 0.0f, 0.0f, 0.1f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            auto clearTime_1 = std::chrono::high_resolution_clock::now();

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D,originalScreen);
            glUniform1i(textures[0], 1);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            auto drawTime_1 = std::chrono::high_resolution_clock::now();

        //}
        //if(mode >= 2) {
        	auto startTime_2 = std::chrono::high_resolution_clock::now();

            glActiveTexture(GL_TEXTURE1); //add
            glBindTexture(GL_TEXTURE_2D,collectBrightBuffer); //add
            //glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,0,0,png.width,png.height,0); //add

            auto BindTextureTime_2 = std::chrono::high_resolution_clock::now();

            glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,0,0,in_img.cols, in_img.rows,0);

            auto CopyTexImageTime_2 = std::chrono::high_resolution_clock::now();

            glUseProgram(program2);
            //glBindFramebuffer(GL_FRAMEBUFFER,bloomBuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, 0); // add
            glClearColor(0.0f, 0.0f, 0.0f, 0.1f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            auto clearTime_2 = std::chrono::high_resolution_clock::now();

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D,collectBrightBuffer);
            glUniform1i(textures[2],0);
            glUniform1i(isVerticalLocation,true);
            glUniform2fv(offsetsLocationV, SAMPLE_COUNT * SAMPLE_COUNT * 2 ,offsetVH);
            glUniform1fv(weightsLocationV, SAMPLE_COUNT * SAMPLE_COUNT ,weightVH);
            //glUniform2fv(offsetsLocationV, SAMPLE_COUNT * 2 ,offsetV);
            //glUniform1fv(weightsLocationV, SAMPLE_COUNT ,weightV);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            auto drawTime_2 = std::chrono::high_resolution_clock::now();
        //}
        //if (mode >= 3) {

        	auto startTime_3 = std::chrono::high_resolution_clock::now();

            glActiveTexture(GL_TEXTURE1); //add
            glBindTexture(GL_TEXTURE_2D,bloomBuffer); //add

            auto BindTextureTime_3 = std::chrono::high_resolution_clock::now();

            //glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,0,0,png.width,png.height,0); //add
            glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,0,0,in_img.cols, in_img.rows,0);

            auto CopyTexImageTime_3 = std::chrono::high_resolution_clock::now();

            glUseProgram(program2);
            //glBindFramebuffer(GL_FRAMEBUFFER,collectBrightBuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, 0); // add
            glClearColor(0.0f, 0.0f, 0.0f, 0.1f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            auto clearTime_3 = std::chrono::high_resolution_clock::now();

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D,bloomBuffer);
            glUniform1i(textures[2],0);
            glUniform1i(isVerticalLocation,false);
            glUniform2fv(offsetsLocationH, SAMPLE_COUNT * 2 ,offsetH);
            glUniform1fv(weightsLocationH, SAMPLE_COUNT ,weightH);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            auto drawTime_3 = std::chrono::high_resolution_clock::now();

        //}
        //if (mode >= 4) {
        	auto startTime_4 = std::chrono::high_resolution_clock::now();

            glActiveTexture(GL_TEXTURE1); //add
            glBindTexture(GL_TEXTURE_2D,collectBrightBuffer); //add

            auto BindTextureTime_4 = std::chrono::high_resolution_clock::now();

            //glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,0,0,png.width,png.height,0); //add
            glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,0,0,in_img.cols, in_img.rows,0);

            auto CopyTexImageTime_4 = std::chrono::high_resolution_clock::now();

            glUseProgram(program3);
            glBindFramebuffer(GL_FRAMEBUFFER,0);
            glClearColor(0.0f, 0.0f, 0.0f, 0.1f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            auto clearTime_4 = std::chrono::high_resolution_clock::now();

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D,originalScreen);
            glUniform1i(textures[3],0);
            glUniform1f(toneScaleLocation,0.7);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D,collectBrightBuffer);
            glUniform1i(textures[4],1);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            auto drawTime_4 = std::chrono::high_resolution_clock::now();
        //}
        //glFlush();
        eglSwapBuffers(display, surface);
        */
/*
        auto curerntTime = std::chrono::high_resolution_clock::now();
        nbFrames++;
        if (curerntTime - lastTime >= sec) {
        	std::cout << "mode0 init Time(ms) "<<std::chrono::duration_cast< std::chrono::microseconds >(clearTime_0 - startTime_0).count() << "\n"
        			<< "mode0 glBindTextureTime(ms) "<<std::chrono::duration_cast< std::chrono::microseconds >(BindTextureTime_0 - clearTime_0).count() << "\n"
					<< "mode0 drawTime(ms) "<<std::chrono::duration_cast< std::chrono::microseconds >(drawTime_0 - BindTextureTime_0).count() << "\n"

					<< "mode1 glBindTextureTime Time(ms) "<<std::chrono::duration_cast< std::chrono::microseconds >(BindTextureTime_1 - startTime_1).count() << "\n"
					<< "mode1 CopyTexImageTime(ms) "<<std::chrono::duration_cast< std::chrono::microseconds >(CopyTexImageTime_1 - BindTextureTime_1).count() << "\n"
					<< "mode1 Clear Time(ms) "<<std::chrono::duration_cast< std::chrono::microseconds >(clearTime_1 - CopyTexImageTime_1).count() << "\n"
					<< "mode1 draw Time(ms) "<<std::chrono::duration_cast< std::chrono::microseconds >(drawTime_1 - clearTime_1).count() << "\n"

					<< "mode2 glBindTextureTime Time(ms) "<<std::chrono::duration_cast< std::chrono::microseconds >(BindTextureTime_2 - startTime_2).count() << "\n"
					<< "mode2 CopyTexImageTime(ms) "<<std::chrono::duration_cast< std::chrono::microseconds >(CopyTexImageTime_2 - BindTextureTime_2).count() << "\n"
					<< "mode2 Clear Time(ms) "<<std::chrono::duration_cast< std::chrono::microseconds >(clearTime_2 - CopyTexImageTime_2).count() << "\n"
					<< "mode2 draw Time(ms) "<<std::chrono::duration_cast< std::chrono::microseconds >(drawTime_2 - clearTime_2).count() << "\n"
					//<< "mode3 glBindTextureTime Time(ms) "<<std::chrono::duration_cast< std::chrono::microseconds >(BindTextureTime_3 - startTime_3).count() << "\n"
					//<< "mode3 CopyTexImageTime(ms) "<<std::chrono::duration_cast< std::chrono::microseconds >(CopyTexImageTime_3 - BindTextureTime_3).count() << "\n"
					//<< "mode3 Clear Time(ms) "<<std::chrono::duration_cast< std::chrono::microseconds >(clearTime_3 - CopyTexImageTime_3).count() << "\n"
					//<< "mode3 draw Time(ms) "<<std::chrono::duration_cast< std::chrono::microseconds >(drawTime_3 - clearTime_3).count() << "\n"
					<< "mode4 glBindTextureTime Time(ms) "<<std::chrono::duration_cast< std::chrono::microseconds >(BindTextureTime_4 - startTime_4).count() << "\n"
					<< "mode4 CopyTexImageTime(ms) "<<std::chrono::duration_cast< std::chrono::microseconds >(CopyTexImageTime_4 - BindTextureTime_4).count() << "\n"
					<< "mode4 Clear Time(ms) "<<std::chrono::duration_cast< std::chrono::microseconds >(clearTime_4 - CopyTexImageTime_4).count() << "\n"
					<< "mode4 draw Time(ms) "<<std::chrono::duration_cast< std::chrono::microseconds >(drawTime_4 - clearTime_4).count() << std::endl;
            printf("%d fps \n",nbFrames);
			nbFrames = 0;
			lastTime = std::chrono::high_resolution_clock::now();
        }
*/

    //}



    while (true)
     {
    	auto startTime = std::chrono::high_resolution_clock::now();
    	XPending(xdisplay);
    	if(mode == 0 || mode == 1){ //load in_img (program1)
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
		if(mode == 0 || mode == 2){ //LUT (program0)
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
		if(mode == 0 || mode == 3){//Filter2D (program2)
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
		if(mode == 0 || mode == 4){ //Add (program3)
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
        	std::cout << "Elapsed Time(µs) : "<<std::chrono::duration_cast< std::chrono::microseconds >(curerntTime - startTime).count() << std::endl;
			printf("%d fps \n",nbFrames);
			nbFrames = 0;
			lastTime = std::chrono::high_resolution_clock::now();
		}
		if(verbose){
			GLenum error_code;
		   error_code = glGetError();
		   if( error_code != GL_NO_ERROR ) std::cout << "glGetError Code: " << error_code <<std::endl;
		}
     }

    //deletePng(png);
    deleteShaderProgram(program0);
    deleteShaderProgram(program1);
    deleteShaderProgram(program2);
    deleteShaderProgram(program3);
}

void fpga_loop(Display *xdisplay, EGLDisplay display, EGLSurface surface, std::string pngFile,bool verbose,int mode)
{
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
	//int loopNum = atoi(argv[2]);
	for (int i = 0; i < loopNum; i++)
	{
		 XPending(xdisplay);
		 auto loadTime = std::chrono::high_resolution_clock::now();

		 imgInput.copyTo(in_img.data);

		 auto startTime = std::chrono::high_resolution_clock::now();
		 lut_accel(imgInput,imgBright,lut_ptr);
		 auto lutTime = std::chrono::high_resolution_clock::now();
		 Filter2d_accel(imgBright,imgFilter,filter_ptr,shift);
		 auto filter2DTime = std::chrono::high_resolution_clock::now();
		 arithm_accel(imgFilter,imgInput,imgOutput);
		 auto addTime = std::chrono::high_resolution_clock::now();

        // EGL output
        //glUseProgram(program1);
        //glBindFramebuffer(GL_FRAMEBUFFER, 0); //0を指定すると描画する
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, in_img.cols,in_img.rows , 0, GL_RGB, GL_UNSIGNED_BYTE, imgOutput.data);

        auto texTime = std::chrono::high_resolution_clock::now();
        //glClearColor(0.0f, 0.0f, 0.0f, 0.1f);
        //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //glActiveTexture(GL_TEXTURE0);
        //glBindTexture(GL_TEXTURE_2D, cvTexture);
        //glUniform1i(textures[0], 0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        auto currentTime = std::chrono::high_resolution_clock::now();

        eglSwapBuffers(display, surface);
		nbFrames++;
		if (currentTime - lastTime >= sec) {
			//std::cout << std::chrono::duration_cast< std::chrono::microseconds >(currentTime - startTime).count() << " (microsecond)" << std::endl;
			std::cout << "  convert to xf::Mat from cv::Mat Time(µs) : " <<std::chrono::duration_cast< std::chrono::microseconds >(startTime - loadTime).count() << "\n"
					<< "  lut_accel Time(µs) : " <<std::chrono::duration_cast< std::chrono::microseconds >(lutTime - startTime).count() << "\n"
					<< "  Filter2d_accel Time(µs) : " <<std::chrono::duration_cast< std::chrono::microseconds >(filter2DTime - lutTime).count() << "\n"
					<< "  arithm_accel Time(µs) : " <<std::chrono::duration_cast< std::chrono::microseconds >(addTime - filter2DTime).count() << "\n"
					<< "  glTexImage2D() Time(µs) : " <<std::chrono::duration_cast< std::chrono::microseconds >(texTime - addTime).count() << "\n"
					<< "  glDrawArrays() Time(µs) : " <<std::chrono::duration_cast< std::chrono::microseconds >(currentTime - texTime).count() << std::endl;
			/*
			std::cout << "FPGA time : " << std::chrono::duration_cast< std::chrono::microseconds >(fpgaTime - startTime).count()
					<< " GPU time : " << std::chrono::duration_cast< std::chrono::microseconds >(currentTime - fpgaTime).count()
					<< " (microsecond)" << std::endl;
			std::cout << "glTexImage2D() Time : " << std::chrono::duration_cast< std::chrono::microseconds >(texTime - fpgaTime).count()
					<< " glDrawArrays() Time : " << std::chrono::duration_cast< std::chrono::microseconds >(currentTime - texTime).count()
					<< " (microsecond)" << std::endl;
					*/
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
/*
	if(err_per > 0.0f)
		return 1;

	return 0;
	*/
}


int main(int argc, char *argv[])
{
    int opt;
    opterr = 0; //getopt()のエラーメッセージを無効にする。

    bool verbose = false;
    int mode = 0;
    std::string pngFile = "Torus.png";

    while ((opt = getopt(argc, argv, "vm:p:fgn:")) != -1) {
        switch (opt){
            case 'v':
                verbose = true;
                break;

            case 'm':
                mode = atoi(optarg);
                if(mode < 0 || 4 < mode) {
                    mode = 0;
                    std::cout << "[Error] mode(-m) argument is only 0 ~ 4 number." << std::endl;
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


            default:
                std::cout << "Usage:\t" << argv[0] << " [-f -n number] [-g] [-v] [-m number(1~4)] [-p png-file-name] \n\n"
                    << "Options:\n"
                    << "\t -f : Enable FPGA acceleration / -g : Enable GPU acceleration \n"
					  << "\t -n : Exection time at FPGA mode \t(default : 100)\n"
                    << "\t -v : Enable verbose \t(default : false)\n"
                    << "\t -m : Change mode \t(default : 4)\n"
                    << "\t -p : Set png file \t(default : Torus.png)\n" << std::endl;
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
    //auto png = loadPng(pngFile);

#if GRAY
	in_img = cv::imread(pngFile,0); // reading in the gray image
#else
	in_img = cv::imread(pngFile,1); // reading in the gray image
#endif
    if (in_img.data == NULL)
	{
		//fprintf(stderr,"Cannot open image at %s\n",pngFile);
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
        std::cout << "Use CPU mode." << std::endl;
    }

    //deletePng(png);
    destroyEGL(display, context, surface);
    XDestroyWindow(xdisplay, xwindow);
    XCloseDisplay(xdisplay);

    return 0;
}
