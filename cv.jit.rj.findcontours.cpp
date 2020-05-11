/*
cv.jit.blur
	

Copyright 2010, Jean-Marc Pelletier
jmp@jmpelletier.com

This file is part of cv.jit.

cv.jit is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published 
by the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

cv.jit is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with cv.jit.  If not, see <http://www.gnu.org/licenses/>.

*/


#include <opencv2/core/core.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <math.h>
#include <iostream>

#include "c74_jitter.h"

#include "cvjit.h"
#include "jitOpenCV.h"

using namespace cv;
using namespace std;
using namespace c74::max;

typedef struct _cv_jit_rj_findcontours {
	t_object ob;
	
	float		level; // contorni ....
	float		epsilon;	// approssimazione contorni
} t_cv_jit_rj_findcontours;

void *_cv_jit_rj_findcontours_class;

t_jit_err cv_jit_rj_findcontours_init(void);
t_cv_jit_rj_findcontours *cv_jit_rj_findcontours_new(void);
void cv_jit_rj_findcontours_free(t_cv_jit_rj_findcontours *x);
t_jit_err cv_jit_rj_findcontours_matrix_calc(t_cv_jit_rj_findcontours *x, void *inputs, void *outputs);
	
t_jit_err cv_jit_rj_findcontours_init(void) 
{
	t_jit_object *mop;
	
	_cv_jit_rj_findcontours_class = jit_class_new("cv_jit_rj_findcontours",(method)cv_jit_rj_findcontours_new,(method)cv_jit_rj_findcontours_free,sizeof(t_cv_jit_rj_findcontours),0L);

	// add mop
	mop = (t_jit_object*)jit_object_new(_jit_sym_jit_mop,1,2); // args are  num inputs and num outputs
	jit_class_addadornment(_cv_jit_rj_findcontours_class,mop);
	
	// add methods
	jit_class_addmethod(_cv_jit_rj_findcontours_class, (method)cv_jit_rj_findcontours_matrix_calc, "matrix_calc", A_CANT, 0L);

	// add attributes
	cvjit::AttributeManager attributes(_cv_jit_rj_findcontours_class);

	attributes.add<float>("level", -2.f, CVJIT_CALCOFFSET(&t_cv_jit_rj_findcontours::level));
	attributes.add<float>("epsilon", 0.f, CVJIT_CALCOFFSET(&t_cv_jit_rj_findcontours::epsilon));
	
	jit_class_register(_cv_jit_rj_findcontours_class);

	return JIT_ERR_NONE;
}


t_jit_err cv_jit_rj_findcontours_matrix_calc(t_cv_jit_rj_findcontours *x, void *inputs, void *outputs)
{
	t_object * in_matrix = (t_object *)jit_object_method(inputs, _jit_sym_getindex, 0);
	t_object * out_matrix = (t_object *)jit_object_method(outputs, _jit_sym_getindex, 0);
	
	t_jit_matrix_info	out2_minfo;
	char*				out2_bp;
	void*				out2_matrix;
	out2_matrix = jit_object_method(outputs, _jit_sym_getindex, 1);

	if (x && in_matrix && out_matrix) {

		cvjit::Savelock savelocks[] = {in_matrix, out_matrix};
		
		cvjit::JitterMatrix src(in_matrix);
		cvjit::JitterMatrix dst(out_matrix);
		
		jit_object_method(out2_matrix, _jit_sym_getinfo, &out2_minfo);
		jit_object_method(out2_matrix, _jit_sym_getdata, &out2_bp);

		// long matrices are not supported, and input must have 2 dimensions.
		t_jit_err err = cvjit::Validate(x, src)
			.dimcount(2)
			.type(_jit_sym_char, _jit_sym_float32, _jit_sym_float64);
		
		if (JIT_ERR_NONE == err) {
			
			vector<vector<Point> > contours;
			vector<Vec4i> hierarchy;

			cv::Mat src_mat = src;

			vector<vector<Point> > contours0;
			findContours(src_mat, contours0, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);
			contours.resize(contours0.size());

			// SIMPLIFY CONTOURS
			for (size_t k = 0; k < contours0.size(); k++)
				approxPolyDP(Mat(contours0[k]), contours[k], x->epsilon, true);

			dst.clear();

			cv::Mat dst_mat = dst;

			drawContours(dst_mat, contours, x->level, Scalar(255), 1, LINE_AA, hierarchy, std::abs(x->level));

			// END FIRST MATRIX OUT

			int total = 0;
			int i;
			for (i = 0; i < contours.size(); i++) {
				total = total + contours[i].size();
			}

			long* array = new long[total * 3];
			int ind = 0;
			int j, k;
			for (i = 0; i < contours.size(); i++) {
				for (j = 0; j < contours[i].size(); j++) {
					array[ind++] = contours[i][j].x;
					array[ind++] = contours[i][j].y;
					array[ind++] = i;
				}
			}

			out2_minfo.dim[1] = 1;
			out2_minfo.dim[0] = total;
			out2_minfo.type = _jit_sym_long;
			out2_minfo.planecount = 3;

			jit_object_method(out2_matrix, _jit_sym_setinfo_ex, &out2_minfo);
			jit_object_method(out2_matrix, _jit_sym_data, array);

			// Calculate
			try {
				//cv::GaussianBlur(src_mat, dst_mat, cv::Size(kernel_size, kernel_size), x->sigma, x->sigma);
			}
			catch (cv::Exception &exception) {
				object_error((t_object *)x, "OpenCV error: %s", exception.what());
			}
		}
		
		return err;

	} else {
		return JIT_ERR_INVALID_PTR;
	}
}

t_cv_jit_rj_findcontours *cv_jit_rj_findcontours_new(void)
{
	t_cv_jit_rj_findcontours *x;
		
	if ((x=(t_cv_jit_rj_findcontours *)jit_object_alloc(_cv_jit_rj_findcontours_class))) 
	{
		x->level = -2.f;
		x->epsilon = 3.f;
	} 
	else 
	{
		x = NULL;
	}	
	return x;
}

void cv_jit_rj_findcontours_free(t_cv_jit_rj_findcontours *x)
{
	// Nothing
}
