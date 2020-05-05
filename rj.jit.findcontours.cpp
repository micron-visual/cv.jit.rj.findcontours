/**
	 @file
	 rj.jit.findcontours - opencv findcontours function
	 + finds contours into a binary image
	 + approximates contours

	 @ingroup	rj

	 Rajan Craveri rajancraveri@gmail.com
*/

#include <opencv2/core/core.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <math.h>
#include <iostream>

#include "jit.common.h" // include this after opencv!

using namespace cv;
using namespace std;

// Our Jitter object instance data
typedef struct _rj_jit_findcontours {
	t_object	ob;
	double		level; // contorni ....
	double		epsilon;	// approssimazione contorni
} t_rj_jit_findcontours;


// prototypes
BEGIN_USING_C_LINKAGE
t_jit_err		rj_jit_findcontours_init				(void);
t_rj_jit_findcontours	*rj_jit_findcontours_new				(void);
void			rj_jit_findcontours_free				(t_rj_jit_findcontours *x);
t_jit_err		rj_jit_findcontours_matrix_calc		(t_rj_jit_findcontours *x, void *inputs, void *outputs);
void			rj_jit_findcontours_calculate_ndim	(t_rj_jit_findcontours *x, long dim, long *dimsize, long planecount, t_jit_matrix_info *in_minfo, char *bip, t_jit_matrix_info *out_minfo, char *bop);
END_USING_C_LINKAGE


// globals
static void *s_rj_jit_findcontours_class = NULL;


/************************************************************************************/

t_jit_err rj_jit_findcontours_init(void)
{
	long			attrflags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_USURP_LOW;
	t_jit_object	*attr;
	t_jit_object	*mop;

	s_rj_jit_findcontours_class = jit_class_new("rj_jit_findcontours", (method)rj_jit_findcontours_new, (method)rj_jit_findcontours_free, sizeof(t_rj_jit_findcontours), 0);

	// add matrix operator (mop)
	mop = (t_jit_object *)jit_object_new(_jit_sym_jit_mop, 1, 2); // args are  num inputs and num outputs
	jit_class_addadornment(s_rj_jit_findcontours_class, mop);

	// add method(s)
	jit_class_addmethod(s_rj_jit_findcontours_class, (method)rj_jit_findcontours_matrix_calc, "matrix_calc", A_CANT, 0);

	attr = (t_jit_object*)jit_object_new(_jit_sym_jit_attr_offset,
		"level",
		_jit_sym_float64,
		attrflags,
		(method)NULL, (method)NULL,
		calcoffset(t_rj_jit_findcontours, level));
	jit_class_addattr(s_rj_jit_findcontours_class, attr);

	// CRASH IF NEGATIVE HOW CAN WE LIMIT THIS RANGE ?
	attr = (t_jit_object*)jit_object_new(_jit_sym_jit_attr_offset,
		"epsilon",
		_jit_sym_float64,
		attrflags,
		(method)NULL, (method)NULL,
		calcoffset(t_rj_jit_findcontours, epsilon));
	jit_class_addattr(s_rj_jit_findcontours_class, attr);

	// finalize class
	jit_class_register(s_rj_jit_findcontours_class);
	return JIT_ERR_NONE;
}


/************************************************************************************/
// Object Life Cycle

t_rj_jit_findcontours *rj_jit_findcontours_new(void)
{
	t_rj_jit_findcontours	*x = NULL;

	x = (t_rj_jit_findcontours *)jit_object_alloc(s_rj_jit_findcontours_class);
	if (x) {
		x->level = -2;
		x->epsilon = 3;
	}
	return x;
}


void rj_jit_findcontours_free(t_rj_jit_findcontours *x)
{
	;	// nothing to free for our simple object
}


/************************************************************************************/
// Methods bound to input/inlets

t_jit_err rj_jit_findcontours_matrix_calc(t_rj_jit_findcontours *x, void *inputs, void *outputs)
{
	t_jit_err			err = JIT_ERR_NONE;
	long				in_savelock;
	long				out_savelock;
	t_jit_matrix_info	in_minfo;
	t_jit_matrix_info	out_minfo, out2_minfo;
	char				*in_bp;
	char				*out_bp;
	char				*out2_bp;
	long				i;
	long				dimcount;
	long				planecount;
	long				dim[JIT_MATRIX_MAX_DIMCOUNT];
	void				*in_matrix;
	void				*out_matrix;
	void* out2_matrix;
	Mat					image;
	Mat					cnt_img;

	char* m_data;

	in_matrix 	= jit_object_method(inputs,_jit_sym_getindex,0);
	out_matrix 	= jit_object_method(outputs,_jit_sym_getindex,0);
	out2_matrix = jit_object_method(outputs, _jit_sym_getindex, 1);

	if (x && in_matrix && out_matrix) {
		in_savelock = (long) jit_object_method(in_matrix, _jit_sym_lock, 1);
		out_savelock = (long) jit_object_method(out_matrix, _jit_sym_lock, 1);
		// LOCK FOR THE SECOND OUTPUT MATRIX ?

		jit_object_method(in_matrix, _jit_sym_getinfo, &in_minfo);
		jit_object_method(out_matrix, _jit_sym_getinfo, &out_minfo);
		jit_object_method(out2_matrix, _jit_sym_getinfo, &out2_minfo);

		jit_object_method(in_matrix, _jit_sym_getdata, &in_bp);
		jit_object_method(out_matrix, _jit_sym_getdata, &out_bp);
		jit_object_method(out_matrix, _jit_sym_getdata, &out2_bp);

		if (!in_bp) {
			err=JIT_ERR_INVALID_INPUT;
			goto out;
		}
		if (!out_bp) {
			err=JIT_ERR_INVALID_OUTPUT;
			goto out;
		}
		if (in_minfo.type != out_minfo.type) {
			err = JIT_ERR_MISMATCH_TYPE;
			goto out;
		}

		//get dimensions/planecount
		dimcount   = out_minfo.dimcount;
		planecount = out_minfo.planecount;

		for (i=0; i<dimcount; i++) {
			//if dimsize is 1, treat as infinite domain across that dimension.
			//otherwise truncate if less than the output dimsize
			dim[i] = out_minfo.dim[i];
			if ((in_minfo.dim[i]<dim[i]) && in_minfo.dim[i]>1) {
				dim[i] = in_minfo.dim[i];
			}
		}

		//RAJAN START

		// WHAT IS THIS ?
		vector<vector<Point> > contours;

		// WHAT IS THIS ?
		vector<Vec4i> hierarchy;

		//DEBUG
		/*
		post("IN_MINFO:");
		post("#####################");
		object_post((t_object*)x, "in_minfo.dim[0]: %ld", in_minfo.dim[0]);
		object_post((t_object*)x, "in_minfo.dim[1]: %ld", in_minfo.dim[1]);
		object_post((t_object*)x, "in_minfo.dimstride[0]: %ld", in_minfo.dimstride[0]);
		object_post((t_object*)x, "in_minfo.dimstride[1]: %ld", in_minfo.dimstride[1]);
		object_post((t_object*)x, "in_minfo.dimcount: %ld", in_minfo.dimcount);
		object_post((t_object*)x, "in_minfo.type: %ld", in_minfo.type);
		object_post((t_object*)x, "in_minfo.size: %ld", in_minfo.size);
		post("#####################");
		*/

		//CREATE A OPENCV MAT FROM INPUT JITTER MATRIX DATA 
		image = Mat(dim[1], dim[0], CV_8U, in_bp, in_minfo.dimstride[1]);
		
		// WHAT IS THIS ?
		vector<vector<Point> > contours0;
		
		// WHAT ARE RETR_TREE AND CHAIN_APPROX_SIMPLE ?
		findContours(image, contours0, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);
		
		//object_post((t_object*)x, "NUMBER OF CONTOURS: %ld", contours0.size());
		
		// WHAT IS THIS ?
		contours.resize(contours0.size());
		
		// SIMPLIFY CONTOURS
		for (size_t k = 0; k < contours0.size(); k++)
			approxPolyDP(Mat(contours0[k]), contours[k], x->epsilon, true);
		
		//CREATE TWO OPENCV WINDOWS FOR DEBUG
		//namedWindow("source", 1);
		//namedWindow("contours", 1);
		
		// CREATE THE MAT FOR CONTOURS AND SET THE OUTPUT JITTER DATA BLOCK **THIS WAS THE PROBLEM out_bp here fixed the output issue
		cnt_img = Mat(dim[1], dim[0], CV_8U, out_bp, out_minfo.dimstride[1]);
		
		// CLEAN THE MAT
		cnt_img = cnt_img.zeros(dim[1], dim[0], CV_8U);

		// DRAW CONTOURNS
		drawContours(cnt_img, contours, x->level, Scalar(255), 1, LINE_AA, hierarchy, std::abs(x->level));

		//USE THE OPENCV WINDOWS FOR DEBUG
		//imshow("source", image);
		//imshow("contours", cnt_img);
		
		//DEBUG
		/*post("CNT_IMG:");
		post("#####################");
		object_post((t_object*)x, "cnt_img.cols: %ld", cnt_img.cols);
		object_post((t_object*)x, "cnt_img.rows: %ld", cnt_img.rows);
		object_post((t_object*)x, "cnt_img.step[0]: %ld", cnt_img.step[0]);
		object_post((t_object*)x, "cnt_img.step[1]: %ld", cnt_img.step[1]);
		object_post((t_object*)x, "cnt_img.dims: %ld", cnt_img.dims);
		object_post((t_object*)x, "cnt_img.size[0]: %ld", cnt_img.size[0]);
		object_post((t_object*)x, "cnt_img.size[1]: %ld", cnt_img.size[1]);
		post("#####################");

		post("OUT_MINFO BEFORE:");
		post("#####################");
		object_post((t_object*)x, "out_minfo.dim[0]: %ld", out_minfo.dim[0]);
		object_post((t_object*)x, "out_minfo.dim[1]: %ld", out_minfo.dim[1]);
		object_post((t_object*)x, "out_minfo.dimstride[0]: %ld", out_minfo.dimstride[0]);
		object_post((t_object*)x, "out_minfo.dimstride[1]: %ld", out_minfo.dimstride[1]);
		object_post((t_object*)x, "out_minfo.dimcount: %ld", out_minfo.dimcount);
		object_post((t_object*)x, "out_minfo.type: %ld", out_minfo.type);
		object_post((t_object*)x, "out_minfo.size: %ld", out_minfo.size);
		post("#####################");
		*/

		// PREPARE THE OUTPUT MATRIX
		out_minfo.dimcount = 2;
		out_minfo.planecount = 1;
		out_minfo.dim[0] = cnt_img.cols;
		out_minfo.dim[1] = cnt_img.rows;
		out_minfo.type = _jit_sym_char;
		out_minfo.dimstride[0] = sizeof(char);
		out_minfo.dimstride[1] = cnt_img.step;
		out_minfo.size = cnt_img.step * cnt_img.rows;
		
		/*post("OUT_MINFO AFTER:");
		post("#####################");
		object_post((t_object*)x, "out_minfo.dim[0]: %ld", out_minfo.dim[0]);
		object_post((t_object*)x, "out_minfo.dim[1]: %ld", out_minfo.dim[1]);
		object_post((t_object*)x, "out_minfo.dimstride[0]: %ld", out_minfo.dimstride[0]);
		object_post((t_object*)x, "out_minfo.dimstride[1]: %ld", out_minfo.dimstride[1]);
		object_post((t_object*)x, "out_minfo.dimcount: %ld", out_minfo.dimcount);
		object_post((t_object*)x, "out_minfo.type: %ld", out_minfo.type);
		object_post((t_object*)x, "out_minfo.size: %ld", out_minfo.size);
		post("#####################");
		*/

		/*
		long width = out_minfo.dim[0];
		long height = out_minfo.dim[1];
		char* array = new char[out_minfo.size];
		int index = 0;
		int i, j;
		for (i = 0; i < height; i++) {
			for(j = 0; j < width; j++) {
				array[index++] = ((float)j/width) * 255;
			}
			for( ; j < out_minfo.dimstride[1]; j++) {
				array[index++] = 0; // pad out with zeros based on our row alignment
			}
		}
		jit_object_method(out_matrix, _jit_sym_setinfo_ex, &out_minfo); // set the matrix as described in out_minfo
		jit_object_method(out_matrix, _jit_sym_data, array);
		*/
		//int index = 0;
		int total = 0;
		int i;
		for (i = 0; i < contours.size(); i++) {
			//for (int j = 0; j < contours[i].size(); j++) {
			//	index++;
			//out2_bp[j] = contours[i][j].x;
			total = total + contours[i].size();
			//}

		}
		
		//total = total*2;
		long* array = new long[total*3];
		int ind = 0;
		int j, k;
		for (i = 0; i < contours.size(); i++) {
			for (j = 0; j < contours[i].size(); j++) {

				//object_post((t_object*)x, "contours: %ld", contours[i][j]);

				// iterate the planes HARDCODED 2
				//for (k = 0; k < 2; k++) {
					//if (k = 0) {
						array[ind++] = contours[i][j].x;
					//}
					//if (k = 1) {
						array[ind++] = contours[i][j].y;
						array[ind++] = i;
						
					//}
				//}
			}
			//for (; j < out2_minfo.dimstride[1]; j++) {
				//array[ind++] = 0; // pad out with zeros based on our row alignment
			//}
		}


		out2_minfo.dim[1] = 1;
		out2_minfo.dim[0] = total;
		out2_minfo.type = _jit_sym_long;
		out2_minfo.planecount = 3;

		jit_object_method(out2_matrix, _jit_sym_setinfo_ex, &out2_minfo);
		jit_object_method(out2_matrix, _jit_sym_data, array);

		//for (int i = 0; i < contours.size(); i++) {
		//	for (int j = 0; j < contours[i].size(); j++) {
			//	
				//out2_bp[j] = contours[i][j].x;

			//}
		//}

		

		out_minfo.flags = JIT_MATRIX_DATA_REFERENCE | JIT_MATRIX_DATA_FLAGS_USE;
		
		jit_object_method(out_matrix, _jit_sym_setinfo_ex, &out_minfo); // set the matrix as described in out_minfo
		jit_object_method(out_matrix, _jit_sym_data, cnt_img.data);

		
		

		//RAJAN END
		
		//jit_parallel_ndim_simplecalc2((method)rj_jit_findcontours_calculate_ndim, x, dimcount, dim, planecount, &in_minfo, in_bp, &out_minfo, out_bp, 0 /* flags1 */, 0 /* flags2 */);
	}
	else
		return JIT_ERR_INVALID_PTR;

out:
	jit_object_method(out_matrix,_jit_sym_lock,out_savelock);
	jit_object_method(in_matrix,_jit_sym_lock,in_savelock);
	return err;
}