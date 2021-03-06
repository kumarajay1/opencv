/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                           License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2017, Intel Corporation, all rights reserved.
// Copyright (c) 2016-2017 Fabian David Tschopp, all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of the copyright holders may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

#ifndef _OPENCV_LIBDNN_HPP_
#define _OPENCV_LIBDNN_HPP_
#include "../../precomp.hpp"
#include <iomanip>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "common.hpp"

namespace cv { namespace dnn { namespace ocl4dnn {
#ifdef HAVE_OPENCL

struct OCL4DNNConvConfig
{
    OCL4DNNConvConfig() :
        kernel(1, 1),
        pad(0, 0),
        stride(1, 1),
        dilation(1, 1),
        group(1),
        bias_term(false)
    {}
    MatShape in_shape;
    MatShape out_shape;
    Size kernel;
    Size pad;
    Size stride;
    Size dilation;
    int group; // = 1;
    bool bias_term; // = false;
};


template<typename Dtype>
class OCL4DNNConvSpatial
{
    public:
        explicit OCL4DNNConvSpatial(OCL4DNNConvConfig config);
        ~OCL4DNNConvSpatial();
        bool Forward(const UMat& bottom_data, const UMat& weight,
                     const UMat& bias,
                     UMat& top_data, int32_t batch_size);

    private:
        struct kernelConfig
        {
            std::string kernelName;
            float executionTime;
            size_t local_work_size[3];
            size_t global_work_size[3];
            int32_t workItem_output[3];
            bool verified;
            bool tested;
            bool swizzle_weights;
            bool use_null_local;
            int32_t kernelType;

            kernelConfig()
            {}

            kernelConfig(const std::string& name, const size_t* global_size, const size_t* local_size,
                         const int32_t* workItem,
                         bool swizzle,
                         int32_t type = 0)
                : executionTime(0)
            {
                kernelName = name;
                for (int32_t x = 0; x < 3; x++)
                {
                    local_work_size[x] = local_size ? local_size[x] : 1;
                    global_work_size[x] = global_size[x];
                    workItem_output[x] = workItem[x];
                }
                swizzle_weights = swizzle;
                use_null_local = local_size == NULL;
                verified = false;
                tested = false;
                kernelType = type;
            }
        };

        struct tunerParam
        {
           int kernelType;
           int blockWidth;
           int blockHeight;
           int blockDepth;

           tunerParam(int type, int w, int h, int d)
           {
               kernelType = type;
               blockWidth = w;
               blockHeight= h;
               blockDepth = d;
           }
        };

        inline void addDef(const char* name)
        {
            options_ << " -D " << name;
        }

        inline void addDef(const char* name, const int value)
        {
            options_ << " -D " << name << "=" << value;
        }

        inline void addDef(const char* name, const float value)
        {
            options_ << " -D " << name << "=(float)" << value;
        }

        inline void addDef(const char* name, const double value)
        {
            options_ << " -D " << name << "=(double)" << value;
        }

        inline void addDef(const char* name, const char* value)
        {
            options_ << " -D " << name << "=" << value;
        }

        void useFirstAvailable(const UMat &bottom,
                               UMat &top,
                               const UMat &weight,
                               const UMat &bias,
                               int32_t numImages,
                               UMat &verifyTop);
        void setupKernel();
        void collectCommonInformation();
        void setupKernelDetails(int32_t kernelType,
                                int32_t blockM,
                                int32_t blockK,
                                int32_t blockN);

        ocl::Program compileKernel();
        typedef std::map<std::string, ocl::Program> phash_t;
        phash_t phash;
        void calculateBenchmark(const UMat &bottom, UMat &verifyTop,
                                const UMat &weight, const UMat &bias,
                                int32_t numImages);


        void setupConvolution(const UMat &bottom,
                              UMat &top,
                              const UMat &weight,
                              const UMat &bias,
                              int32_t numImags,
                              UMat &verifyTop);
        bool createConvolutionKernel(int32_t kernelType,
                                     int32_t blockWidth,
                                     int32_t blockHeight,
                                     int32_t blockDepth);
        bool setupIDLF(int32_t blockWidth,
                       int32_t blockHeight,
                       int32_t blockDepth);
        bool createBasicKernel(int32_t blockWidth,
                               int32_t blockHeight,
                               int32_t blockDepth);
        bool createGEMMLikeConvKernel(int32_t blockWidth,
                                      int32_t blockHeight,
                                      int32_t blockDepth);
        void CreateSubBuffer(const UMat& buffer, UMat& sub_buffer,
                             int32_t offset, int32_t size, bool write_only);
        bool convolve(const UMat &bottom, UMat &top,
                      const UMat &weight, const UMat &bias,
                      int32_t numImages,
                      kernelConfig* config,
                      const cv::ocl::Queue& queue);
        float timedConvolve(const UMat &bottom, UMat &top,
                            const UMat &weight, const UMat &bias,
                            int32_t numImages, kernelConfig* config);

        bool verifyResult(const UMat &bottom,
                          UMat &top,
                          const UMat &weight,
                          const UMat &bias,
                          int32_t numImages,
                          kernelConfig* config,
                          UMat &verifyTop);

        bool swizzleWeight(const UMat &weight,
                           int32_t swizzled_factor,
                           bool interleave = false);

        void generateKey();
        std::string generateSpecificKey(int32_t type, int32_t blockWidth,
                                          int32_t blockHeight,
                                          int32_t blockDepth);
        void cacheTunedConfig();
        bool loadTunedConfig();

        void saveTunedConfig();
        bool loadCachedConfig();

        void unloadProgram(const std::string& kernelName);
        void prepareKernel(const UMat &bottom, UMat &top,
                           const UMat &weight, const UMat &bias,
                           int32_t numImages);
        bool setupKernelByConfig(int x, int y, int z, int type,
                                 int lx, int ly, int lz,
                                 bool swizzle, bool nullLocal);
        void generateTunerItems(std::vector< cv::Ptr<tunerParam> > &tunerItems);

        int32_t group_;
        bool bias_term_;
        UMat swizzled_weights_umat;

        int32_t bottom_index_;
        int32_t output_h_;
        int32_t output_w_;
        int32_t kernel_h_;
        int32_t kernel_w_;
        int32_t height_;
        int32_t width_;
        int32_t pad_h_;
        int32_t pad_w_;
        int32_t stride_h_;
        int32_t stride_w_;
        int32_t dilation_h_;
        int32_t dilation_w_;

        /// M_ is the channel dimension of the output for a single group, which is the
        /// leading dimension of the filter matrix.
        int32_t M_;

        bool tuned_;
        std::string key_, key_sanitized_;
        std::string short_key_;
        std::string kernel_name_;
        std::string cache_path_;
        bool use_cache_path_; // true if cache_path_ directory exists
        bool force_auto_tuning_;
        int32_t kernel_index_;
        std::vector< cv::Ptr<kernelConfig> > kernelQueue;
        cv::Ptr<kernelConfig> bestKernelConfig;

        int32_t bottom_dim_;
        int32_t top_dim_;
        int32_t num_;
        int32_t channels_;
        int32_t num_output_;

        int32_t kernelType_;
        int32_t blockM_;
        int32_t blockK_;
        int32_t blockN_;
        std::stringstream options_;
        cv::ocl::ProgramSource src_;
        int32_t prev_kernel_type_;
};

typedef enum {
    LIBDNN_POOLING_METHOD_MAX                 = 0,
    LIBDNN_POOLING_METHOD_AVE                 = 1,
    LIBDNN_POOLING_METHOD_STO                 = 2
} ocl4dnnPoolingMethod_t;

struct OCL4DNNPoolConfig
{
    OCL4DNNPoolConfig() :
        kernel(1, 1),
        pad(0, 0),
        stride(1, 1),
        dilation(1, 1),
        channels(0),
        pool_method(LIBDNN_POOLING_METHOD_MAX),
        global_pooling(false)
    {}
    MatShape in_shape;
    MatShape out_shape;
    Size kernel;
    Size pad;
    Size stride;
    Size dilation;

    int channels;
    ocl4dnnPoolingMethod_t pool_method; // = LIBDNN_POOLING_METHOD_MAX;
    bool global_pooling; // = false;
};

template<typename Dtype>
class OCL4DNNPool
{
    public:
        explicit OCL4DNNPool(OCL4DNNPoolConfig config);
        ~OCL4DNNPool();
        bool Forward(const UMat& bottom_data,
                     UMat& top_data,
                     UMat& top_mask);
    private:
        UMat mask_idx_;

        // Pooling parameters
        std::vector<int32_t> pad_;
        std::vector<int32_t> stride_;
        std::vector<int32_t> kernel_shape_;
        std::vector<int32_t> im_in_shape_;
        std::vector<int32_t> im_out_shape_;

        ocl4dnnPoolingMethod_t pool_method_;
        int32_t count_;
        int32_t batch_size_;
        int32_t channels_;
        int32_t kernel_h_;
        int32_t kernel_w_;
        int32_t stride_h_;
        int32_t stride_w_;
        int32_t pad_h_;
        int32_t pad_w_;
        int32_t height_;
        int32_t width_;
        int32_t pooled_height_;
        int32_t pooled_width_;
};

struct OCL4DNNInnerProductConfig
{
    OCL4DNNInnerProductConfig() :
        num_output(0), M(0), K(0),
        bias_term(false), transpose(false), phase_test(true)
    {}
    int num_output;
    int M;
    int K;
    bool bias_term;
    bool transpose; // = false;
    bool phase_test; // = true;
};

template<typename Dtype>
class OCL4DNNInnerProduct
{
    public:
        explicit OCL4DNNInnerProduct(OCL4DNNInnerProductConfig config);
        ~OCL4DNNInnerProduct();
        bool Forward(const UMat& bottom_data,
                     const UMat& weight,
                     const UMat& bias,
                     UMat& top_data);
    private:
        OCL4DNNInnerProductConfig config_;
        int32_t axis_;
        int32_t num_output_;
        int32_t M_;
        int32_t N_;
        int32_t K_;
        bool bias_term_;
        bool transpose_;
        bool image_copied_;
        bool phase_test_;
};

typedef enum {
    LRNParameter_NormRegion_ACROSS_CHANNELS = 0,
    LRNParameter_NormRegion_WITHIN_CHANNEL = 1
} LRNParameter_NormRegion_WITHIN_CHANNEL_t;

struct OCL4DNNLRNConfig
{
    OCL4DNNLRNConfig() :
        phase_test(true)
    {}
    MatShape in_shape;
    LRNParameter_NormRegion_WITHIN_CHANNEL_t lrn_type;
    bool phase_test; // = true;
    int local_size;
    float alpha;
    float beta;
    float k;
    bool norm_by_size;
    int32_t batch_size;
    int32_t channels;
    int32_t height;
    int32_t width;
};

template<typename Dtype>
class OCL4DNNLRN
{
    public:
        explicit OCL4DNNLRN(OCL4DNNLRNConfig config);
        bool Forward(const UMat& bottom_data, UMat& top_data);

    private:
        bool crossChannelForward(const UMat& bottom_data, UMat& top_data);
        LRNParameter_NormRegion_WITHIN_CHANNEL_t lrn_type_;
        bool phase_test_;
        int32_t size_;
        Dtype alpha_;
        Dtype beta_;
        Dtype k_;
        int32_t num_;
        int32_t channels_;
        int32_t height_;
        int32_t width_;
        bool norm_by_size_;
};

struct OCL4DNNSoftmaxConfig
{
    OCL4DNNSoftmaxConfig()
    {}
    MatShape in_shape;
    int axis;
    int channels;
};

template<typename Dtype>
class OCL4DNNSoftmax
{
    public:
        explicit OCL4DNNSoftmax(OCL4DNNSoftmaxConfig config);
        ~OCL4DNNSoftmax();
        bool Forward(const UMat& bottom_data, UMat& top_data);

    private:
        int32_t softmax_axis_;
        int32_t inner_num_;
        int32_t outer_num_;
        int32_t channels_;
        int32_t count_;
        bool use_slm_;
        UMat scale_data_;
};
#endif // HAVE_OPENCL
} // namespace ocl4dnn
} // namespace dnn
} // namespce cv
#endif
