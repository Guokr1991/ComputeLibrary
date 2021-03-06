/*
 * Copyright (c) 2017 ARM Limited.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "arm_compute/core/TensorShape.h"
#include "arm_compute/core/Types.h"
#include "tests/AssetsLibrary.h"
#include "tests/Globals.h"
#include "tests/IAccessor.h"
#include "tests/framework/Asserts.h"
#include "tests/framework/Fixture.h"
#include "tests/validation/CPP/ConvolutionLayer.h"
#include "tests/validation/Helpers.h"
#include "tests/validation/fixtures/ConvolutionLayerFixture.h"

#include <random>

namespace arm_compute
{
namespace test
{
namespace validation
{
template <typename TensorType, typename AccessorType, typename FunctionType, typename T>
class DirectConvolutionValidationFixedPointFixture : public framework::Fixture
{
public:
    template <typename...>
    void setup(TensorShape input_shape, int stride_x, int stride_y, int pad_x, int pad_y, unsigned int kernel_size, unsigned int num_kernels, DataType data_type, int fractional_bits)
    {
        _fractional_bits = fractional_bits;
        _data_type       = data_type;

        const TensorShape   weights_shape(kernel_size, kernel_size, input_shape.z(), num_kernels);
        const TensorShape   bias_shape(num_kernels);
        const PadStrideInfo info(stride_x, stride_y, pad_x, pad_y, DimensionRoundingType::FLOOR);
        const TensorShape   output_shape = get_output_shape(input_shape, weights_shape, info);

        _target    = compute_target(input_shape, weights_shape, bias_shape, output_shape, info, data_type, fractional_bits);
        _reference = compute_reference(input_shape, weights_shape, bias_shape, output_shape, info, data_type, fractional_bits);
    }

protected:
    template <typename U>
    void fill(U &&tensor, int i)
    {
        switch(tensor.data_type())
        {
            case DataType::F16:
            case DataType::F32:
            {
                std::uniform_real_distribution<> distribution(-1.0f, 1.0f);
                library->fill(tensor, distribution, i);
                break;
            }
            default:
                library->fill_tensor_uniform(tensor, i);
        }
    }

    TensorType compute_target(const TensorShape &input_shape, const TensorShape &weights_shape, const TensorShape &bias_shape, const TensorShape &output_shape, const PadStrideInfo &info,
                              DataType data_type, int fixed_point_position)
    {
        // Create tensors
        TensorType src     = create_tensor<TensorType>(input_shape, data_type, 1, fixed_point_position);
        TensorType weights = create_tensor<TensorType>(weights_shape, data_type, 1, fixed_point_position);
        TensorType bias    = create_tensor<TensorType>(bias_shape, data_type, 1, fixed_point_position);
        TensorType dst     = create_tensor<TensorType>(output_shape, data_type, 1, fixed_point_position);

        // Create and configure function
        FunctionType conv;
        conv.configure(&src, &weights, &bias, &dst, info);

        ARM_COMPUTE_EXPECT(src.info()->is_resizable(), framework::LogLevel::ERRORS);
        ARM_COMPUTE_EXPECT(weights.info()->is_resizable(), framework::LogLevel::ERRORS);
        ARM_COMPUTE_EXPECT(bias.info()->is_resizable(), framework::LogLevel::ERRORS);
        ARM_COMPUTE_EXPECT(dst.info()->is_resizable(), framework::LogLevel::ERRORS);

        // Allocate tensors
        src.allocator()->allocate();
        weights.allocator()->allocate();
        bias.allocator()->allocate();
        dst.allocator()->allocate();

        ARM_COMPUTE_EXPECT(!src.info()->is_resizable(), framework::LogLevel::ERRORS);
        ARM_COMPUTE_EXPECT(!weights.info()->is_resizable(), framework::LogLevel::ERRORS);
        ARM_COMPUTE_EXPECT(!bias.info()->is_resizable(), framework::LogLevel::ERRORS);
        ARM_COMPUTE_EXPECT(!dst.info()->is_resizable(), framework::LogLevel::ERRORS);

        // Fill tensors
        fill(AccessorType(src), 0);
        fill(AccessorType(weights), 1);
        fill(AccessorType(bias), 2);

        // Compute NEConvolutionLayer function
        conv.run();

        return dst;
    }

    SimpleTensor<T> compute_reference(const TensorShape &input_shape, const TensorShape &weights_shape, const TensorShape &bias_shape, const TensorShape &output_shape, const PadStrideInfo &info,
                                      DataType data_type, int fixed_point_position)
    {
        // Create reference
        SimpleTensor<T> src{ input_shape, data_type, 1, fixed_point_position };
        SimpleTensor<T> weights{ weights_shape, data_type, 1, fixed_point_position };
        SimpleTensor<T> bias{ bias_shape, data_type, 1, fixed_point_position };

        // Fill reference
        fill(src, 0);
        fill(weights, 1);
        fill(bias, 2);

        return reference::convolution_layer<T>(src, weights, bias, output_shape, info);
    }

    TensorType      _target{};
    SimpleTensor<T> _reference{};
    int             _fractional_bits{};
    DataType        _data_type{};

private:
    TensorShape get_output_shape(TensorShape in_shape, TensorShape kernel_shape, const PadStrideInfo &info)
    {
        TensorShape out_shape(in_shape);
        const std::pair<unsigned int, unsigned int> scaled_dims = scaled_dimensions(in_shape.x(),
                                                                                    in_shape.y(),
                                                                                    kernel_shape.x(),
                                                                                    kernel_shape.y(),
                                                                                    info);
        out_shape.set(0, scaled_dims.first);
        out_shape.set(1, scaled_dims.second);
        out_shape.set(2, kernel_shape[3]);
        return out_shape;
    }
};

template <typename TensorType, typename AccessorType, typename FunctionType, typename T>
class DirectConvolutionValidationFixture : public DirectConvolutionValidationFixedPointFixture<TensorType, AccessorType, FunctionType, T>
{
public:
    template <typename...>
    void setup(TensorShape input_shape, int stride_x, int stride_y, int pad_x, int pad_y, unsigned int kernel_size, unsigned int num_kernels, DataType data_type)
    {
        DirectConvolutionValidationFixedPointFixture<TensorType, AccessorType, FunctionType, T>::setup(input_shape, stride_x, stride_y, pad_x, pad_y, kernel_size, num_kernels, data_type, 0);
    }
};
} // namespace validation
} // namespace test
} // namespace arm_compute
