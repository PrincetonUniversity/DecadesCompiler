#pragma once

#include "DECADES.h"
#include "assert.h"
#include <algorithm>
#include <math.h>
#include <numeric>
#include <stdio.h>

//#DEFINE KERNEL_ASSERTS

////////////////////////////
// NN activations
// - relu (accelerator and software)
// - relu_grad (software)
// - prelu (accelerator and software)
// - tanh (accelerator and software)
// - tanh_grad: tanh gradient (software)
// - sigmoid (accelerator and software)
// - sigmoid_grad: sigmoid gradient (software)
// - elu: exponential linear unit (software)
// - elu_grad (software)
// - softmax (software)
// - softmax_grad (software)
// - asinh (software)
// - asinh_grad (software)
// - selu (software)
// - selu_grad (software)
// - leaky_relu (software)
// - leaky_relu_grad (software)
////////////////////////////

// relu
// - software only
//     - for standalone accelerator execution use decadesTF_sdp
//     - for fused accelerator execution use conv2d_layer or dense_layer
__attribute__((noinline))
extern "C"
void decadesTF_relu(
    int size,
    float *in, float *out, 
    int tid, int num_threads)
{
    int i;

    for (i = tid; i < size; i+=num_threads) {
	out[i] = (in[i] < 0) ? 0 : in[i];
    }
}

// relu gradient
// - software only
// TODO: can go on SDP: ReLU + mul
__attribute__((noinline))
extern "C"
void decadesTF_relugrad(
    int size,
    float *out, float *d_out, float *d_in,
    int tid, int num_threads)
{
    int i;

    for (i = tid; i < size; i+=num_threads) {
	d_in[i] = d_out[i] * (out[i] > 0 ? 1.0 : 0.0);
    }
}

// prelu
// - software only
//     - for fused accelerator execution use conv2d_layer or dense_layer
__attribute__((noinline))
extern "C"
void decadesTF_prelu(
    int size,
    float *in, float *filters, float *out,
    int tid, int num_threads)
{
	int i;

	for(i = tid; i < size; i+=num_threads) {
	    out[i] = (in[i] < 0) ? (filters[i] * in[i]) : in[i];
	}
}

// prelu gradient
// - software only
__attribute__((noinline))
extern "C"
void decadesTF_prelugrad(
    int size,
    float *out, float *d_out, float *d_in,
    int tid, int num_threads)
{
    int i;

    for (i = tid; i < size; i+=num_threads) {
	d_in[i] = d_out[i] * (out[i] > 0 ? 1.0 : 0.0);
    }
}

// tanh
// - software only
//     - for fused accelerator execution use conv2d_layer or dense_layer
__attribute__((noinline))
extern "C"
void decadesTF_tanh(
    int size,
    float *in, float *out,
    int tid, int num_threads)
{
	int i;

	for(i = tid; i < size; i+=num_threads) {
	    out[i] = tanh(in[i]);
	}
}

// tanh gradient
// - software only
__attribute__((noinline))
extern "C"
void decadesTF_tanhgrad(
    int size,
    float *out, float *d_out, float *d_in,
    int tid, int num_threads)
{
    int i;

    for (i = tid; i < size; i+=num_threads) {
	d_in[i] = d_out[i] * (1 - out[i] * out[i]);
    }
}

// sigmoid
// - software only
//     - for fused accelerator execution use conv2d_layer or dense_layer
__attribute__((noinline))
extern "C"
void decadesTF_sigmoid(
    int size,
    float *in, float *out,
    int tid, int num_threads)
{
	int i;
	float exp_value;

	for(i = tid; i < size; i+=num_threads) {
	    exp_value = exp(-1.0 * in[i]);
	    out[i] = 1.0 / (1.0 + exp_value);
	}
}

// sigmoid gradient
// - software only
__attribute__((noinline))
extern "C"
void decadesTF_sigmoidgrad(
    int size,
    float *out, float *d_out, float *d_in,
    int tid, int num_threads)
{
    // the default for alpha_ is 1.0
 
    int i;

    for (i = tid; i < size; i+=num_threads) {
	d_in[i] = d_out[i] * out[i] * (1.0 - out[i]);
    }
}

// elu
// - software only
__attribute__((noinline))
extern "C"
void decadesTF_elu(
    int size, float alpha_,
    float *in, float *out,
    int tid, int num_threads)
{
    int i;

    for(i = tid; i < size; i+=num_threads) {
	out[i] = (in[i] < 0.0) ? (alpha_ * (exp(in[i]) - 1)) : in[i];
    }
}

// elu gradient
// - software only
__attribute__((noinline))
extern "C"
void decadesTF_elugrad(
    int size, float alpha_,
    float *out, float *d_out, float *d_in,
    int tid, int num_threads)
{
    // the default for alpha_ is 1.0

    int i;

    for (i = tid; i < size; i+=num_threads) {
	d_in[i] = d_out[i] * ((out[i] > 0.0) ? 1.0 : (alpha_ + out[i]));
    }
}

// softmax
// - software only
// TODO. Implemented only for fully connected layers, i.e. 1D input
void decadesTF_softmax(
		       int batch, int size,
		       float *in, float *out,
		       int tid, int num_threads)
{
  int i, b, base = 0;

  for(b = tid; b < batch; b+=num_threads) {
    float max = in[0], sum = 0.0;
    base = b*size;
    for(i = base; i < size+base; i++) {
      if (in[i] > max)
	max = in[i];
    }
    
    for(i = base; i < size+base; i++) {
      out[i] = exp(in[i] - max);
      sum += out[i];
    }
    
    for(i = base; i < size+base; i++) {
      out[i] = out[i] / sum;
    }
    base += size;
  }
}


// softmax gradient
// - software only
__attribute__((noinline))
extern "C"
void decadesTF_softmaxgrad(
    int size,
    float *out, float *d_out, float *d_in,
    int tid, int num_threads)
{
  if (tid == 0) { 
    int i, j;
    float grad = 0.0, sum = 0.0;
    
    for (i = 0; i < size; i++) {
      for (j = 0; j < size; j++) {
	grad = (j == i) ? out[i] * (1.0 - out[i]) : -out[j] * out[i];
	sum += grad * d_out[j];
      }
      d_in[i] = sum;
    }
  }
}

// asinh
// - software only
__attribute__((noinline))
extern "C"
void decadesTF_asinh(
    int size,
    float *in, float *out,
    int tid, int num_threads)
{
    int i;

    for (i = tid; i < size; i+=num_threads) {
	out[i] = asinh(in[i]);
    }
}

// asinh gradient
// - software only
// TODO use accelerator SDP: div
__attribute__((noinline))
extern "C"
void decadesTF_asinhgrad(
    int size,
    float *out, float *d_out, float *d_in,
    int tid, int num_threads)
{
    int i;

    for (i = tid; i < size; i+=num_threads) {
	d_in[i] = d_out[i] / cosh(out[i]);
    }
}

// selu
// - software only
__attribute__((noinline))
extern "C"
void decadesTF_selu(
    int size, float alpha_, float lambda_,
    float *in, float *out,
    int tid, int num_threads)
{
    int i;

    for(i = tid; i < size; i+=num_threads) {
	out[i] = lambda_ * ((in[i] > 0.0) ? in[i] : (alpha_ * (exp(in[i]) - 1)));
    }
}

// selu gradient
// - software only
__attribute__((noinline))
extern "C"
void decadesTF_selugrad(
    int size, float alpha_, float lambda_,
    float *out, float *d_out, float *d_in,
    int tid, int num_threads)
{
    // the default for alpha_ is 1.0

    int i;

    for (i = tid; i < size; i+=num_threads) {
	d_in[i] = d_out[i] * lambda_ * ((out[i] > 0.0) ? 1.0 : (alpha_ * exp(out[i])));
    }
}

// leaky_relu
// - software only
__attribute__((noinline))
extern "C"
void decadesTF_leakyrelu(
    int size, float epsilon_,
    float *in, float *out,
    int tid, int num_threads)
{
    int i;

    for(i = tid; i < size; i+=num_threads) {
	out[i] = (in[i] > 0.0) ? in[i] : (epsilon_ * in[i]);
    }
}

// leaky_relu gradient
// - software only
__attribute__((noinline))
extern "C"
void decadesTF_leakyrelugrad(
    int size, float epsilon_,
    float *out, float *d_out, float *d_in,
    int tid, int num_threads)
{
    int i;

    for (i = tid; i < size; i+=num_threads) {
	d_in[i] = d_out[i] * ((out[i] > 0.0) ? 1.0 : epsilon_);
    }
}

////////////////////////////
// Linear algebra
// - sdp: single data point accelerator (accelerator only)
// - matmul: matrix multiplication (accelerator and software)
// - add: tensor element-wise addition (accelerator and software)
// - sub: tensor element-wise subtract (accelerator and software)
// - mul: tensor element-wise multiply (accelerator and software)
// - div: tensor element-wise divide (accelerator and software)
// - scalar_mul: tensor and scalar multiply (accelerator and software)
// TODO. TF functions for add_scalar, sub_scalar, div_scalar?
////////////////////////////

// matmul
// - software and hardware
__attribute__((noinline))
extern "C"
void decadesTF_matmul(
    volatile int rowsA, volatile int colsA, 
    volatile int rowsB, volatile int colsB, 
    volatile int batch,
    float *A, float *B, float *out,
    int tid, int num_threads)
{
#ifdef KERNEL_ASSERTS
    assert(colsA == rowsB);
#endif
    int i, j, k, b;

    int A_mtx_size = rowsA * colsA;
    int B_mtx_size = rowsB * colsB;
    int out_mtx_size = rowsA * colsB;

    int A_base = 0;
    int B_base = 0;
    int out_base = 0;

    for (b = 0; b < batch; b++) {
	for (i = tid; i < rowsA; i++) {
	    for (j = 0; j < colsB; j++) {
		out[out_base + i * colsB + j] = 0;
		for (k = 0; k < colsA; k++) {
		      out[out_base + i * colsB + j] +=
			  A[A_base + i * colsA + k] * B[B_base + k * colsB + j];
		}
	    }
	}

	A_base += A_mtx_size;
	out_base += out_mtx_size;
    }
}

__attribute__((noinline))
extern "C"
void decadesTF_add(
    int sizeA, int sizeB,
    float *A, float *B, float *out,
    int tid, int num_threads)
{
#ifdef KERNEL_ASSERTS
  assert(sizeA == sizeB);
#endif
  int i;

  for (i = tid; i < sizeA; i+=num_threads) {
    out[i] = A[i] + B[i];
  }
}

__attribute__((noinline))
extern "C"
void decadesTF_sub(
    int sizeA, int sizeB,
    float *A, float *B, float *out,
    int tid, int num_threads)
{
#ifdef KERNEL_ASSERTS
  assert(sizeA == sizeB);
#endif

  int i;

  for (i = tid; i < sizeA; i+=num_threads) {
    out[i] = A[i] - B[i];
  }
}

__attribute__((noinline))
extern "C"
void decadesTF_mul(
    int sizeA, int sizeB,
    float *A, float *B, float *out,
    int tid, int num_threads)
{
#ifdef KERNEL_ASSERTS
  assert(sizeA == sizeB); 
#endif

  int i;

  for (i = tid; i < sizeA; i+=num_threads) {
    out[i] = A[i] * B[i];
  }
}

__attribute__((noinline))
extern "C"
void decadesTF_div(
    int sizeA, int sizeB,
    float *A, float *B, float *out,
    int tid, int num_threads)
{
#ifdef KERNEL_ASSERTS
  assert(sizeA == sizeB); 
#endif

  int i;

  for (i = tid; i < sizeA; i+=num_threads) {
    out[i] = A[i] / B[i];
  }
}

__attribute__((noinline))
extern "C"
void decadesTF_scalar_add(
    volatile int sizeA, volatile int sizeB,
    float *A, float *B, float *out,
    int tid, int num_threads)
{
#ifdef KERNEL_ASSERTS
  assert(sizeB == 1); 
#endif

  int i;

  for (i = tid; i < sizeA; i+=num_threads) {
    out[i] = A[i] + (*B);
  }
}

__attribute__((noinline))
extern "C"
void decadesTF_scalar_sub(
    volatile int sizeA, volatile int sizeB,
    float *A, float *B, float *out,
    int tid, int num_threads)
{
#ifdef KERNEL_ASSERTS
  assert(sizeB == 1); 
#endif

  int i;

  for (i = tid; i < sizeA; i+=num_threads) {
    out[i] = A[i] - (*B);
  }
}

__attribute__((noinline))
extern "C"
void decadesTF_scalar_mul(
    volatile int sizeA, volatile int sizeB,
    float *A, float *B, float *out,
    int tid, int num_threads)
{
#ifdef KERNEL_ASSERTS
  assert(sizeB == 1); 
#endif
  int i;

  for (i = tid; i < sizeA; i+=num_threads) {
    out[i] = A[i] * (*B);
  }
}

__attribute__((noinline))
extern "C"
void decadesTF_scalar_div(
    volatile int sizeA, volatile int sizeB,
    float *A, float *B, float *out,
    int tid, int num_threads)
{
#ifdef KERNEL_ASSERTS
  assert(sizeB == 1); 
#endif

  int i;

  for (i = tid; i < sizeA; i+=num_threads) {
    out[i] = A[i] / (*B);
  }
}

// sdp
// - accelerator only
//
// working modes:
// 0: matrix element-wise add
// 1: matrix element-wise subtract
// 2: matrix element-wise multiply
// 3: matrix element-wise divide
// [!] 4: matrix add scalar
// [!] 5: matrix subtract scalar
// 6: matrix multiply by scalar
// [!] 7: matrix divide by scalar
// 8: ReLU
__attribute__((noinline))
extern "C"
void decadesTF_sdp(
    volatile int working_mode, volatile int size, 
    float *A, float *B, float *out,
    int tid, int num_threads)
{
    if (working_mode == 0) // add
	decadesTF_add(size, size, A, B, out, tid, num_threads);
    else if (working_mode == 1) // sub
	decadesTF_sub(size, size, A, B, out, tid, num_threads);
    else if (working_mode == 2) // mul
	decadesTF_mul(size, size, A, B, out, tid, num_threads);
    else if (working_mode == 3) // div
	decadesTF_div(size, size, A, B, out, tid, num_threads);
    else if (working_mode == 4) // scalar add
        decadesTF_scalar_add(size, size, A, B, out, tid, num_threads);
    else if (working_mode == 5) // scalar sub
        decadesTF_scalar_sub(size, size, A, B, out, tid, num_threads);
    else if (working_mode == 6) // scalar mul
	decadesTF_scalar_mul(size, size, A, B, out, tid, num_threads);
    else if (working_mode == 7) // scalar div
        decadesTF_scalar_div(size, size, A, B, out, tid, num_threads);
    else if (working_mode == 8) // relu
	decadesTF_relu(size, A, out, tid, num_threads);
}

////////////////////////////
// NN pooling
// - max_pool: max pooling (accelerator and software)
// - max_pool_grad (software)
// - avg_pool: average pooling (accelerator and software)
// - avg_pool_grad (software)
// TODO. Is there a TF function for minimum pooling?
////////////////////////////

// max pooling
// - software only
//     - for fused accelerator execution use decadesTF_conv2d_layer
// TODO: support more than just 2x2 pooling
// input: in [batch, in_channels, in_height, in_width]
// output: out [batch, out_channels, out_height, out_width]
__attribute__((noinline))
extern "C"
void decadesTF_maxpool(
    int batch, int in_height,  int in_width, int in_channels,
    int pool_height, int pool_width,
    float *in, float *out,
    int tid, int num_threads)
{
  if (tid == 0) {
    unsigned i, j, k, b;
    int channel_size = in_height * in_width;
    int j_off, j_off_p1, k_in_off, k_out_off;

    int in_chan_size = in_height * in_width;
    int out_chan_size = in_height/2 * in_width/2; // TODO better
    int in_size = in_chan_size * in_channels;
    int out_size = out_chan_size * in_channels;

    int in_base = 0;
    int out_base = 0;

    for (b = 0; b < batch; b++) {

	for(k = tid; k < in_channels; k+=num_threads) {
	    k_in_off = in_base + in_chan_size * k;
	    k_out_off = out_base + out_chan_size * k;

	    for(j = 0; j < in_height; j += 2) {
		j_off = in_width * j;
		j_off_p1 = in_width * (j + 1);

		for(i = 0; i < in_width; i += 2) {
		    out[k_out_off + (j_off/4) + (i/2)] =
			std::max(std::max(in[k_in_off + j_off + i],
					  in[k_in_off + j_off_p1 + i]),
				 std::max(in[k_in_off + j_off + (i+1)],
					  in[k_in_off + j_off_p1 + (i+1)]));
		}
	    }
	}

	in_base += in_size;
	out_base += out_size;
    }
  }
}

// max pooling gradient
// - software only
//     - for fused accelerator execution use decadesTF_conv2d_layer
// TODO: support more than just 2x2 pooling
// input: in [batch, in_channels, in_height, in_width],
//        out & d_out [batch, out_channels, out_height, out_width]
// output: d_in [batch, in_channels, in_height, in_width]
__attribute__((noinline))
extern "C"
void decadesTF_maxpoolgrad(
    int batch, int in_height,  int in_width, int in_channels,
    int pool_height, int pool_width,
    float *in, float *out, float *d_out, float *d_in,
    int tid, int num_threads)
{
    unsigned i, j, k, b, m;
    int channel_size = in_height * in_width;
    int j_off, j_off_p1, k_in_off, k_out_off;

    int in_chan_size = in_height * in_width;
    int out_chan_size = in_height/2 * in_width/2; // TODO better
    int in_size = in_chan_size * in_channels;
    int out_size = out_chan_size * in_channels;

    int in_base = 0;
    int out_base = 0;

    for (b = 0; b < batch; b++) {

	for(k = tid; k < in_channels; k+=num_threads) {
	    k_in_off = in_base + in_chan_size * k;
	    k_out_off = out_base + out_chan_size * k;

	    for(j = 0; j < in_height; j += 2) {
		j_off = in_width * j;
		j_off_p1 = in_width * (j + 1);

		for(i = 0; i < in_width; i += 2) {
		    int in_idx[4], out_idx;

		    in_idx[0] = k_in_off + j_off + i;
		    in_idx[1] = k_in_off + j_off_p1 + i;
		    in_idx[2] = k_in_off + j_off + (i+1);
		    in_idx[3] = k_in_off + j_off_p1 + (i+1);
		    out_idx = k_out_off + (j_off/4) + (i/2);

		    for (m = 0; m < 4; m++) {
			if (in[in_idx[m]] == out[out_idx])
			    d_in[in_idx[m]] = d_out[out_idx];
			else
			    d_in[in_idx[m]] = 0.0;
		    }
		}
	    }
	}
	in_base += in_size;
	out_base += out_size;
    }
}

// average pooling
// - software only
//     - for fused accelerator execution use decadesTF_conv2d_layer
// TODO: support more than just 2x2 pooling
// input: in [batch, in_channels, in_height, in_width]
// output: out [batch, out_channels, out_height, out_width]
__attribute__((noinline))
extern "C"
void decadesTF_avgpool(
    int batch, int in_height,  int in_width, int in_channels,
    int pool_height, int pool_width,
    float *in, float *out,
    int tid, int num_threads)
{
    unsigned i, j, k, b;
    int channel_size = in_height * in_width;
    int j_off, j_off_p1, k_in_off, k_out_off;
    
    int in_chan_size = in_height * in_width;
    int out_chan_size = in_height/2 * in_width/2; // TODO better
    int in_size = in_chan_size * in_channels;
    int out_size = out_chan_size * in_channels;

    int in_base = 0;
    int out_base = 0;

    for (b = 0; b < batch; b++) {

	for(k = tid; k < in_channels; k+=num_threads) {
	    k_in_off = in_base + in_chan_size * k;
	    k_out_off = out_base + out_chan_size * k;

	    for(j = 0; j < in_height; j += 2) {
		j_off = in_width * j;
		j_off_p1 = in_width * (j + 1);

		for(i = 0; i < in_width; i += 2) {
		    out[k_out_off + (j_off/4) + (i/2)] =
			(in[k_in_off + j_off + i] +
			 in[k_in_off + j_off_p1 + i] +
			 in[k_in_off + j_off + (i+1)] +
			 in[k_in_off + j_off_p1 + (i+1)]) / 4;
		}
	    }
	}
	in_base += in_size;
	out_base += out_size;
    }
}

// avg pooling gradient
// - software only
//     - for fused accelerator execution use decadesTF_conv2d_layer
// TODO: support more than just 2x2 pooling
// input: in [batch, in_channels, in_height, in_width],
//        out & d_out [batch, out_channels, out_height, out_width]
// output: d_in [batch, in_channels, in_height, in_width]
__attribute__((noinline))
extern "C"
void decadesTF_avgpoolgrad(
    int batch, int in_height,  int in_width, int in_channels,
    int pool_height, int pool_width,
    float *d_out, float *d_in,
    int tid, int num_threads)
{
  if (tid > 0) {
    unsigned i, j, k, b, m;
    int channel_size = in_height * in_width;
    int j_off, j_off_p1, k_in_off, k_out_off;

    int in_chan_size = in_height * in_width;
    int out_chan_size = in_height/2 * in_width/2; // TODO better
    int in_size = in_chan_size * in_channels;
    int out_size = out_chan_size * in_channels;

    int in_base = 0;
    int out_base = 0;

    for (b = 0; b < batch; b++) {

	for(k = tid; k < in_channels; k+=num_threads) {
	    k_in_off = in_base + in_chan_size * k;
	    k_out_off = out_base + out_chan_size * k;

	    for(j = 0; j < in_height; j += 2) {
		j_off = in_width * j;
		j_off_p1 = in_width * (j + 1);

		for(i = 0; i < in_width; i += 2) {
		    int in_idx[4], out_idx;
		    float avg;
		    
		    in_idx[0] = k_in_off + j_off + i;
		    in_idx[1] = k_in_off + j_off_p1 + i;
		    in_idx[2] = k_in_off + j_off + (i+1);
		    in_idx[3] = k_in_off + j_off_p1 + (i+1);
		    out_idx = k_out_off + (j_off/4) + (i/2);

		    avg = d_out[out_idx] / 4;
		    for (m = 0; m < 4; m++)
			d_in[in_idx[m]] = avg;
		}
	    }
	}
	in_base += in_size;
	out_base += out_size;
    }
  }
}

////////////////////////////
// NN others
// - bias_add (accelerator and software)
// - bias_add_grad: bias add gradient (software)
// - batch_norm: batch normalization (accelerator and software)
// - lrn: local response normalization (accelerator and software)
////////////////////////////

/* bias add */
/* NOTE: this is just an element-wise addition, so use sdp for it */
/* - software only */
/*     - for standalone accelerator execution use decadesTF_sdp (add) */
/*     - for fused accelerator execution use conv2d_layer or dense_layer */
__attribute__((noinline))
extern "C"
void decadesTF_bias_add(int batch, int size_channel,
			float *in, float *biases, float *out,
			int tid, int num_threads)
{
  unsigned i, j;
  
  for(i = tid; i < batch; i+=num_threads)
    for(j = 0; j < size_channel; j++)
      out[i * size_channel + j] = in[i * size_channel + j] + biases[j];
}

// bias add grad
// - software only
// input: d_out [(batch), in_height, in_width, in_channels]
// output : d_bias [(batch), in_channels]
__attribute__((noinline))
extern "C"
void decadesTF_biasaddgrad(
    int batch, int out_height, int out_width, int out_channels,
    float *d_out, float *d_bias,
    int tid, int num_threads)
{
    int idx_lo = 0, idx_hi = 0;
    int out_chan_size = out_height * out_width;

    for (size_t bc = 0; bc < out_channels * batch; bc++) {

	idx_hi = idx_lo + out_chan_size;

	const float *d_out_lo  = &d_out[idx_lo];
	const float *d_out_hi = &d_out[idx_hi];

	d_bias[bc] += std::accumulate(d_out_lo, d_out_hi, 0);

	idx_lo = idx_hi;
    }
}

// batch normalization
// - software only
//     - for fused accelerator execution use conv2d_layer or dense_layer
// TODO: implement software version 
__attribute__((noinline))
extern "C"
void decadesTF_batchnorm(
    int in_channel_size, int in_channels, float eps, float momentum,
    float *in, float *mean, float *variance, float *out,
    int tid, int num_threads)
{
    // TODO implement
}

// local response normalization
// - software only
//     - for fused accelerator execution use conv2d_layer or dense_layer
// TODO. Assuming 'in' is 4D and 'channels' is the 4th dimension.
__attribute__((noinline))
extern "C"
void decadesTF_lrn(
    int size, int depth_radius, int bias, int alpha, int beta,
    float *in, float *out,
    int tid, int num_threads)
{
    int i, j, i_off;
    float sqr_sum;
    int iter = size / (depth_radius * 2);

    for (i = tid; i < iter; i+=num_threads) {
	sqr_sum = 0.0;
	i_off = i * depth_radius * 2;

	for (j = 0; j < depth_radius * 2; j++) {
	    sqr_sum += pow(in[i_off + j], 2);
	}

	for (j = 0; j < depth_radius * 2; j++) {
	    out[i_off + j] = in[i_off + j] / pow(bias + alpha + sqr_sum, beta);
	}
    }
}

////////////////////////////
// NN layers
// - conv2d: convolution (accelerator and software)
// - conv2d_backprop_input: conv2d gradients w.r.t input (software only)
// - conv2d_backprop_filter: conv2d gradients w.r.t filters (software only)
// - conv2d_layer: convolutional layer (accelerator and software)
// - dense: matrix multiplication of fully connected layer (accelerator and software)
// - dense_layer: fully connected layer (accelerator and software)
// TODO. What are the other kernels used for training?
////////////////////////////

// conv2d
// - software only
//     - for accelerator execution use conv2d_layer
// TODO. Fixed values for input arguments (see comments below)
__attribute__((noinline))
extern "C"
void decadesTF_conv2d(
    int batch, int in_height,
    int in_width, int in_channels,
    int filter_height, int filter_width,
    int out_channels, bool padding,
    int vert_conv_stride, int horiz_conv_stride,
    float *in, float *filters,  float *out,
    int tid, int num_threads)
{
    // The current implementation has some fixed parameters:
    // - filter_height = 3, filter_width = 3
    // - vertical_conv_stride = 1, horizontal_conv_stride = 1
    // - batch = 1
    unsigned i, j, k, h, b;
    unsigned in_base, out_base;
    unsigned height_size = padding ? (in_height-2) : (in_height);
    unsigned width_size = padding ? (in_width-2) : (in_width);
    unsigned filter_size = filter_height * filter_width;
    unsigned channel_size = height_size * width_size;
    unsigned in_batch_size = in_height * in_width * in_channels;
    unsigned out_batch_size = height_size * width_size * out_channels;

    float zeropad[height_size + 2][width_size + 2]; //need to initialize this properly
    float sum;
    
    for (i = 0; i < height_size + 2; i++) {
        for (j = 0; j < width_size + 2; j++) {
            zeropad[i][j] = 0;
        }
    }

    in_base = 0;
    out_base = 0;

    for (b = 0; b < batch; b++) { // for each batch element
	for(h = tid; h < out_channels; h+=num_threads) { // for each filter
	    for(k = 0; k < in_channels; k++) { // for each in channel

		// pad if necessary
		for (i = 0; i < height_size; i++) {
		    for (j = 0; j < width_size; j++) {
			zeropad[i + 1][j + 1] = in[in_base + k*channel_size + i * width_size + j];
		    }
		}

		// convolution of one filter with one 2D matrix
		for (i = 0; i < height_size; i++) {
		    for (j = 0; j < width_size; j++) {
		      sum = zeropad[i    ][j    ] * filters[h*filter_size + 2 * 3 + 2] +
			    zeropad[i    ][j + 1] * filters[h*filter_size + 2 * 3 + 1] +
			    zeropad[i    ][j + 2] * filters[h*filter_size + 2 * 3 + 0] +
			    zeropad[i + 1][j    ] * filters[h*filter_size + 1 * 3 + 2] +
			    zeropad[i + 1][j + 1] * filters[h*filter_size + 1 * 3 + 1] +
			    zeropad[i + 1][j + 2] * filters[h*filter_size + 1 * 3 + 0] +
			    zeropad[i + 2][j    ] * filters[h*filter_size + 0 * 3 + 2] +
			    zeropad[i + 2][j + 1] * filters[h*filter_size + 0 * 3 + 1] +
			    zeropad[i + 2][j + 2] * filters[h*filter_size + 0 * 3 + 0];

			out[out_base + h * channel_size + i * width_size + j] += sum;
		    }
		}
	    }
	}
	in_base += in_batch_size;
	out_base += out_batch_size;
    }
}

// conv2d_layer
// - hardware only
__attribute__((noinline))
extern "C"
void decadesTF_conv2d_layer(
    volatile int batch, volatile int in_height,
    volatile int in_width, volatile int in_channels,
    volatile int filter_height, volatile int filter_width, volatile int out_channels, 
    volatile bool zero_pad, volatile int vert_conv_stride, volatile int horiz_conv_stride,
    volatile bool bias_add, 
    volatile bool activation, volatile int activation_type,
    volatile bool pooling, volatile int pooling_type, volatile int pool_height, volatile int pool_width, volatile int vertical_pool_stride, volatile int horizontal_pool_stride,
    volatile bool lrn, volatile int lrn_radius, volatile int lrn_bias, volatile int lrn_alpha, volatile int lrn_beta, 
    float *in, float *filters, float *bias, /* float *prelu_filters,*/ float *out,
    int tid, int num_threads)
{
  // Fusing possibilities:
    // - conv2d + bias_add + activation + (optional) pooling + (optional) lrn
    unsigned height_padded = zero_pad ? (in_height-2) : (in_height);
    unsigned width_padded = zero_pad ? (in_width-2) : (in_width);
    int out_channel_padded = height_padded * width_padded;
    int out_size_padded = out_channel_padded * out_channels;

    // conv2d
    decadesTF_conv2d(batch, in_height, in_width, in_channels, filter_height,
    		     filter_width, out_channels, zero_pad, vert_conv_stride,
    		     horiz_conv_stride, in, filters, out, tid, num_threads);

    // bias add
    if (bias_add) {
    	decadesTF_bias_add(out_channel_padded, out_channels, out, bias, out, tid, num_threads);
    }

    // activation
    if (activation) {
    	if (activation_type == 0) // relu
    	    decadesTF_relu(out_size_padded, out, out, tid, num_threads);
    	//if (activation_type == 1) // prelu
    	//    decadesTF_prelu(out_size, out, prelu_filters, out, tid, num_threads);
    	if (activation_type == 1) // tanh
    	    decadesTF_tanh(out_size_padded, out, out, tid, num_threads);
    	if (activation_type == 2) // sigmoid
    	    decadesTF_sigmoid(out_size_padded, out, out, tid, num_threads);
    	/* if (activation_type == 4) // batch_norm */
    	/*     decadesTF_batch_norm(out_size, out, out, tid, num_threads); */
    }

    /* // pooling */
    if (pooling) {
    	if (pooling_type == 0) // max_pool
    	    decadesTF_maxpool(batch, height_padded, width_padded, out_channels, pool_height,
    			       pool_width, out, out, tid, num_threads);
    	else //(pooling_type == 1) -> avg_pool
    	    decadesTF_avgpool(batch, height_padded, width_padded, out_channels, pool_height,
    			       pool_width, out, out, tid, num_threads);
    }

    /* // lrn */
    /* if (lrn) { */
    /* 	decadesTF_lrn(out_size_padded, lrn_radius, lrn_bias, lrn_alpha, lrn_beta, */
    /* 		      out, out, tid, num_threads); */
    /* } */
}

// conv2d backprop input
// - software only
// TODO: now conv stride is fixed to 1, filter sizes fixed to 3, padding = true
// intput: d_out [batch,  in_channels, in_height, in_width] 
// intput: filters [in_channels, out_channels, filter_height, filter_width] 
// output: d_in [batch, in_channels, in_height, in_width] 
__attribute__((noinline))
extern "C"
void decadesTF_conv2dbackpropinput(
    int filter_height, int filter_width, int in_channels,
    int batch, int out_height, int out_width, int out_channels,
    bool padding, int vert_conv_stride, int horiz_conv_stride,
    float *filters, float *d_out, float *d_in,
    int tid, int num_threads)
{
  if (tid == 0) { 
  size_t idx, idx_d_in, idx_d_out, idx_filters;

    // fixed: assuming conv_stride = 1
    unsigned padded_height = padding ? (out_height + 2) : out_height;
    unsigned padded_width = padding ? (out_width + 2) : out_width;
    unsigned filter_size = filter_height * filter_width;
    unsigned in_channel_size = padded_height * padded_width;
    unsigned out_channel_size = out_height * out_width;

    // propagate delta to previous layer
    idx_d_in = 0;
    idx_d_out = 0;

    for (size_t b = 0; b < batch; b++) {
	idx_filters = 0;
	for (size_t inc = 0; inc < in_channels; inc++) {
	    idx_d_out = 0;
	    for (size_t outc = 0; outc < out_channels; outc++) {

		float_t *pdelta_dst = &d_in[idx_d_in];
		const float_t *pdelta_src = &d_out[idx_d_out];
		const float_t *pw = &filters[idx_filters];

		for (size_t y = 0; y < out_height; y++) {
		    for (size_t x = 0; x < out_width; x++) {
			idx = y * out_width + x;
			const float_t ppdelta_src = pdelta_src[idx];
			float_t *ppdelta_dst = pdelta_dst + y * padded_width + x;
			const float_t *ppw = pw;

			for (size_t wy = 0; wy < filter_height; wy++) {
			    for (size_t wx = 0; wx < filter_width; wx++) {
				idx = wy * padded_width + wx;
				ppdelta_dst[idx] += *ppw++ * ppdelta_src;
			    }
			}
		    }
		}
		idx_d_out += out_channel_size;
		idx_filters += filter_size;
	    }
	    idx_d_in += in_channel_size;
	}
    }
  }
}

// conv2d backprop filter
// - software only
// TODO: now conv stride is fixed to 1, filter sizes fixed to 3, padding = true
// intput: in [batch,  in_channels, in_height, in_width] 
// output: d_out [batch, out_channels, out_height, out_width] 
// output: d_filters [in_channels, out_channels, filter_height, filter_width] 
__attribute__((noinline))
extern "C"
void decadesTF_conv2dbackpropfilter(
    int in_channels,
    int filter_height, int filter_width,
    int batch, int out_height, int out_width, int out_channels, 
    bool padding, int vert_conv_stride, int horiz_conv_stride,
    float *in, float *d_out, float *d_filters,
    int tid, int num_threads)
{
  if (tid > 0) {
    size_t idx, idx_in, idx_d_out, idx_d_filters;

    // fixed: assuming conv_stride = 1
    unsigned padded_height = padding ? (out_height + 2) : out_height;
    unsigned padded_width = padding ? (out_width + 2) : out_width;
    unsigned filter_size = filter_height * filter_width;
    unsigned in_channel_size = padded_height * padded_width;
    unsigned out_channel_size = out_height * out_width;

    // accumulate dw
    idx_in = 0;
    idx_d_out = 0;
    float dst;
    for (size_t sample = 0; sample < batch; sample++) {
	idx_d_filters = 0;
	for (size_t inc = 0; inc < in_channels; inc++) {
	    idx_d_out = 0;
	    for (size_t outc = 0; outc < out_channels; outc++) {
		for (size_t wy = 0; wy < filter_height; wy++) {
		    for (size_t wx = 0; wx < filter_width; wx++) {

			const float_t *prevo = &in[idx_in];
			const float_t *delta = &d_out[idx_d_out];

			dst = 0.0;
			for (size_t y = 0; y < out_height; y++) {
			    for (size_t z = 0; z < out_width; z++) {

				dst += *(prevo + y * padded_width + z) *
				    *(delta + y * out_width + z);
			    }
			}

			d_filters[idx_d_filters++] += dst;
		    }
		}
		idx_d_out += out_channel_size;
	    }
	    idx_in += in_channel_size;
	}
    }
  }
#pragma omp barrier
}

// dense
// - software only
//     - for accelerator execution use conv2d_dense
// TODO. Assuming 1D input tensor.
__attribute__((noinline))
extern "C"
void decadesTF_dense(
    int batch, int in_channels, int out_channels,
    float *in, float *filters, float *out,
    int tid, int num_threads)
{
    unsigned b, k, h;
    float sum;
    unsigned in_base = 0, out_base = 0;

    for (b = tid; b < batch; b++) {
	for(h = 0; h < out_channels; h++){ // for each output channel
	    sum = 0.0;
	    for(k = 0; k < in_channels; k++){ // for each input channel
		sum += in[in_base + k] * filters[h * in_channels + k];
	    }
	    out[out_base + h] = sum;
	}
	in_base += in_channels;
	out_base += out_channels;
    }
}

// dense_layer
// - hardware only
__attribute__((noinline))
extern "C"
void decadesTF_dense_layer(
    volatile int batch, volatile int in_channels, volatile int out_channels,
    volatile bool activation, volatile int activation_type,
    float *in, float *filters, float *out,
    int tid, int num_threads)
{
  int out_size = batch*out_channels;
  // matmul
  decadesTF_dense(batch, in_channels, out_channels, in, filters, out,
		  tid, num_threads);
  
  // activation
  if (activation) {
    if (activation_type == 0) // relu
      decadesTF_relu(out_size, out, out, tid, num_threads);
    if (activation_type == 1) // tanh
      decadesTF_tanh(out_size, out, out, tid, num_threads);
    if (activation_type == 2) // sigmoid
      decadesTF_sigmoid(out_size, out, out, tid, num_threads);
    
    /* if (activation_type == 4) // batch_norm */
    /*     decadesTF_batch_norm(out_size, out, out, tid, num_threads); */
  }
}

////////////////////////////
// Losses
// - log_loss (software)
// - reduction (software)
// TODO implement log_loss, reduction.
// Do we see these functions in the call trace?
////////////////////////////




