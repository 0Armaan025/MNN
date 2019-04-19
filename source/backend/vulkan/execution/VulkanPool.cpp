//
//  VulkanPool.cpp
//  MNN
//
//  Created by MNN on 2019/01/31.
//  Copyright © 2018, Alibaba Group Holding Limited
//

#include "VulkanPool.hpp"
#include "Macro.h"
namespace MNN {
struct ConstBuffer {
    ivec4 inputSize;
    ivec4 outputSize;
    ivec2 pad;
    ivec2 kernelSize;
    ivec2 stride;
};

VulkanPool::VulkanPool(const Op* op, Backend* bn) : VulkanBasicExecution(bn) {
    mCommon    = op->main_as_Pool();
    auto extra = static_cast<VulkanBackend*>(bn);

    std::vector<VkDescriptorType> types{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER};
    switch (mCommon->type()) {
        case PoolType_MAXPOOL:
            mPoolPipeline =
                extra->getPipeline("glsl_maxpool_comp", /*glsl_maxpool_comp, glsl_maxpool_comp_len,*/ types);
            break;
        case PoolType_AVEPOOL:
            mPoolPipeline =
                extra->getPipeline("glsl_avgpool_comp", /*glsl_avgpool_comp, glsl_avgpool_comp_len,*/ types);
            break;
        default:
            break;
    }
    mConstBuffer.reset(new VulkanBuffer(extra->getMemoryPool(), false, sizeof(ConstBuffer), nullptr,
                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
}
VulkanPool::~VulkanPool() {
}

ErrorCode VulkanPool::onEncode(const std::vector<Tensor*>& inputs, const std::vector<Tensor*>& outputs,
                               const VulkanCommandPool::Buffer* cmdBuffer) {
    auto input  = inputs[0];
    auto output = outputs[0];
    int iw      = input->width();
    int ih      = input->height();
    int ow      = output->width();
    int oh      = output->height();
    int icDiv4  = UP_DIV(input->channel(), 4);
    int ocDiv4  = UP_DIV(output->channel(), 4);
    auto extra  = (VulkanBackend*)backend();
    // Set Const Buffer
    {
        auto pool = (ConstBuffer*)mConstBuffer->map();
        ::memset(pool, 0, sizeof(ConstBuffer));
        pool->inputSize[0]  = input->width();
        pool->inputSize[1]  = input->height();
        pool->inputSize[2]  = icDiv4 * input->batch();
        pool->outputSize[0] = ow;
        pool->outputSize[1] = oh;
        pool->outputSize[2] = ocDiv4 * output->batch();

        int stride_width  = mCommon->strideX();
        int stride_height = mCommon->strideY();
        int pad_width     = mCommon->padX();
        int pad_height    = mCommon->padY();

        // edit const if global
        int kernel_width  = std::min(mCommon->kernelX(), iw);
        int kernel_height = std::min(mCommon->kernelY(), ih);
        if (mCommon->isGlobal()) {
            kernel_width  = iw;
            kernel_height = ih;
            stride_width  = iw;
            stride_height = ih;
            pad_width     = 0;
            pad_height    = 0;
        }
        pool->pad[0]        = pad_width;
        pool->pad[1]        = pad_height;
        pool->stride[0]     = stride_width;
        pool->stride[1]     = stride_height;
        pool->kernelSize[0] = kernel_width;
        pool->kernelSize[1] = kernel_height;
        mConstBuffer->flush(true, 0, sizeof(ConstBuffer));
        mConstBuffer->unmap();
    }

    // Set Command Buffer
    {
        mDescriptorSet.reset(mPoolPipeline->createSet());
        mDescriptorSet->writeImage((VkImageView)output->deviceId(), extra->getCommonSampler()->get(),
                                   VK_IMAGE_LAYOUT_GENERAL, 0);
        mDescriptorSet->writeImage((VkImageView)input->deviceId(), extra->getCommonSampler()->get(),
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
        mDescriptorSet->writeBuffer(mConstBuffer->buffer(), 2, mConstBuffer->size());
        mPoolPipeline->bind(cmdBuffer->get(), mDescriptorSet->get());
        vkCmdDispatch(cmdBuffer->get(), UP_DIV(ow, 8), UP_DIV(oh, 8), UP_DIV(ocDiv4 * output->batch(), 1));
    }
    return NO_ERROR;
}

class VulkanPoolCreator : public VulkanBackend::Creator {
public:
    virtual Execution* onCreate(const std::vector<Tensor*>& inputs, const MNN::Op* op,
                                Backend* backend) const override {
        return new VulkanPool(op, backend);
    }
};

static bool gResistor = []() {
    VulkanBackend::addCreator(OpType_Pooling, new VulkanPoolCreator);
    return true;
}();

} // namespace MNN
