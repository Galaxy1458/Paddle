/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#pragma once

#include <chrono>  // NOLINT
#include <memory>
#include <string>
#include <vector>

#include "paddle/fluid/framework/op_registry.h"
#include "paddle/phi/core/tensor_utils.h"
#include "paddle/phi/kernels/split_kernel.h"
namespace paddle {
namespace operators {
static inline std::vector<phi::DDim> UpdateOutsDims(
    const bool is_runtime,
    const bool each_section_is_known,
    const phi::DDim in_dims,
    const size_t num,
    std::vector<int> sections,
    const size_t axis,
    const int outs_number) {
  std::vector<phi::DDim> outs_dims(outs_number, in_dims);
  int64_t input_axis_dim = in_dims[axis];
  if (num > 0) {
    if (is_runtime || input_axis_dim > 0) {
      PADDLE_ENFORCE_EQ(
          input_axis_dim % num,
          0,
          phi::errors::InvalidArgument(
              "The input's size along the split dimension "
              "must be evenly divisible by Attr(num_or_sections). "
              "But received Attr(num_or_sections) "
              "= %d, input(X)'s shape = [%s], Attr(dim) = %d.",
              num,
              in_dims,
              axis));
      size_t out_axis_dim = input_axis_dim / num;

      for (auto& out_dim : outs_dims) {
        out_dim[axis] = out_axis_dim;
      }
    } else {
      for (auto& out_dim : outs_dims) {
        out_dim[axis] = -1;
      }
    }
  } else if (sections.size() > 0) {
    if (is_runtime || input_axis_dim > 0) {
      const int unk_dim_val = -1;
      int unk_dim_idx = -1, num_of_unk = 0;
      int sum_of_section = 0;
      for (size_t i = 0; i < sections.size(); ++i) {
        if (sections[i] == unk_dim_val) {
          num_of_unk++;
          unk_dim_idx = i;
        } else {
          sum_of_section += sections[i];
        }
      }

      if (each_section_is_known) {
        PADDLE_ENFORCE_LE(
            num_of_unk,
            1,
            phi::errors::InvalidArgument(
                "Only one dimension value of Attr(num_or_sections) "
                "in SplitOp can be -1. "
                "But received Attr(num_or_sections) = [%s].",
                common::make_ddim(sections)));
      }

      if (unk_dim_idx != -1) {
        // for example, input shape = [4 ,5], axis = 1, sections = [2, 3, -1].
        // input_axis_dim = 5, sum_of_sections = 5.
        // the following check will fail.
        PADDLE_ENFORCE_LT(
            sum_of_section,
            input_axis_dim,
            phi::errors::InvalidArgument(
                "Sum of Attr(num_or_sections) other than unknown section "
                "must be less than the input's "
                "size "
                "along the split dimension. But received Attr(num_or_sections) "
                "= [%s], input(X)'s shape = [%s], Attr(dim) = %d.",
                common::make_ddim(sections),
                in_dims,
                axis));
        if (each_section_is_known) {
          sections[unk_dim_idx] = input_axis_dim - sum_of_section;
        }
      } else {
        PADDLE_ENFORCE_EQ(
            sum_of_section,
            input_axis_dim,
            phi::errors::InvalidArgument(
                "Sum of Attr(num_or_sections) must be equal to the input's "
                "size "
                "along the split dimension. But received Attr(num_or_sections)"
                " = [%s], input(X)'s shape = [%s], Attr(dim) = %d.",
                common::make_ddim(sections),
                in_dims,
                axis));
      }
    }
    for (int i = 0; i < outs_number; ++i) {
      outs_dims[i][axis] = sections[i];
    }
  }
  return outs_dims;
}

template <typename T>
class SplitGradMaker : public framework::SingleGradOpMaker<T> {
 public:
  using framework::SingleGradOpMaker<T>::SingleGradOpMaker;

 protected:
  void Apply(GradOpPtr<T> op) const override {
    op->SetType("concat");
    op->SetInput("X", this->OutputGrad("Out"));
    if (this->HasInput("AxisTensor")) {
      op->SetInput("AxisTensor", this->Input("AxisTensor"));
    }
    op->SetOutput("Out", this->InputGrad("X"));
    op->SetAttrMap(this->Attrs());
  }
};

}  // namespace operators
}  // namespace paddle
