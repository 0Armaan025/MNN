//
//  StrassenMatmulComputor.hpp
//  MNN
//
//  Created by MNN on 2019/02/11.
//  Copyright © 2018, Alibaba Group Holding Limited
//

#ifndef StrassenMatmulComputor_hpp
#define StrassenMatmulComputor_hpp

#include "Backend.hpp"
#include "Execution.hpp"
namespace MNN {
class StrassenMatrixComputor : public Execution {
public:
    StrassenMatrixComputor(Backend* bn, int maxDepth = 5, bool cacheB = false);
    virtual ~StrassenMatrixComputor();
    /*
     It's assume that:
     A is a matrix where each element is a (4,1) vector
     B is a matrix where each element is a (4,4) matrix
     inputs[0] is the transpose of A: AT, inputs[1] is the transpose of B: BT
     outputs[0] is the transpose of C: CT

     Let a be one element of A, b be one element of B,
     then a * b = c is a (4, 1) vector.
     So C is a matrix where each element is a (4,1) vector, the same as A
     */
    virtual ErrorCode onResize(const std::vector<Tensor*>& inputs, const std::vector<Tensor*>& outputs) override;
    virtual ErrorCode onExecute(const std::vector<Tensor*>& inputs, const std::vector<Tensor*>& outputs) override;

private:
    class AddTensor;
    ErrorCode _generateMatMul(const Tensor* AT, const Tensor* BT, const Tensor* CT);
    ErrorCode _generateMatMulConstB(const Tensor* AT, const Tensor* BT, const Tensor* CT);
    ErrorCode _generateTrivalMatMul(const Tensor* AT, const Tensor* BT, const Tensor* CT);

    std::vector<std::function<void()>> mFunctions;
    std::vector<std::shared_ptr<AddTensor>> mConstTensor;
    int mMaxDepth;
    int mCurrentDepth = 0;
    bool mCacheB;
};
} // namespace MNN

#endif /* StrassenMatmulComputor_hpp */
