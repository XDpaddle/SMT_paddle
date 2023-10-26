// Copyright (c) 2020 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "core/general-server/op/general_clas_op.h"
#include "core/predictor/framework/infer.h"
#include "core/predictor/framework/memory.h"
#include "core/predictor/framework/resource.h"
#include "core/util/include/timer.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>

namespace baidu {
namespace paddle_serving {
namespace serving {

using baidu::paddle_serving::Timer;
using baidu::paddle_serving::predictor::MempoolWrapper;
using baidu::paddle_serving::predictor::general_model::Tensor;
using baidu::paddle_serving::predictor::general_model::Response;
using baidu::paddle_serving::predictor::general_model::Request;
using baidu::paddle_serving::predictor::InferManager;
using baidu::paddle_serving::predictor::PaddleGeneralModelConfig;

int GeneralClasOp::inference() {
  VLOG(2) << "Going to run inference";
  const std::vector<std::string> pre_node_names = pre_names();
  if (pre_node_names.size() != 1) {
    LOG(ERROR) << "This op(" << op_name()
               << ") can only have one predecessor op, but received "
               << pre_node_names.size();
    return -1;
  }
  const std::string pre_name = pre_node_names[0];

  const GeneralBlob *input_blob = get_depend_argument<GeneralBlob>(pre_name);
  if (!input_blob) {
    LOG(ERROR) << "input_blob is nullptr,error";
    return -1;
  }
  uint64_t log_id = input_blob->GetLogId();
  VLOG(2) << "(logid=" << log_id << ") Get precedent op name: " << pre_name;

  GeneralBlob *output_blob = mutable_data<GeneralBlob>();
  if (!output_blob) {
    LOG(ERROR) << "output_blob is nullptr,error";
    return -1;
  }
  output_blob->SetLogId(log_id);

  if (!input_blob) {
    LOG(ERROR) << "(logid=" << log_id
               << ") Failed mutable depended argument, op:" << pre_name;
    return -1;
  }

  const TensorVector *in = &input_blob->tensor_vector;
  TensorVector *out = &output_blob->tensor_vector;

  int batch_size = input_blob->_batch_size;
  output_blob->_batch_size = batch_size;
  VLOG(2) << "(logid=" << log_id << ") infer batch size: " << batch_size;

  Timer timeline;
  int64_t start = timeline.TimeStampUS();
  timeline.Start();

  // only support string type

  char *total_input_ptr = static_cast<char *>(in->at(0).data.data());
  std::string base64str = total_input_ptr;

  cv::Mat img = Base2Mat(base64str);

  // RGB2BGR
  cv::cvtColor(img, img, cv::COLOR_BGR2RGB);

  // Resize
  cv::Mat resize_img;
  resize_op_.Run(img, resize_img, resize_short_size_);

  // CenterCrop
  crop_op_.Run(resize_img, crop_size_);

  // Normalize
  normalize_op_.Run(&resize_img, mean_, scale_, is_scale_);

  // Permute
  std::vector<float> input(1 * 3 * resize_img.rows * resize_img.cols, 0.0f);
  permute_op_.Run(&resize_img, input.data());
  float maxValue = *max_element(input.begin(), input.end());
  float minValue = *min_element(input.begin(), input.end());

  TensorVector *real_in = new TensorVector();
  if (!real_in) {
    LOG(ERROR) << "real_in is nullptr,error";
    return -1;
  }

  std::vector<int> input_shape;
  int in_num = 0;
  void *databuf_data = NULL;
  char *databuf_char = NULL;
  size_t databuf_size = 0;

  input_shape = {1, 3, resize_img.rows, resize_img.cols};
  in_num = std::accumulate(input_shape.begin(), input_shape.end(), 1,
                           std::multiplies<int>());

  databuf_size = in_num * sizeof(float);
  databuf_data = MempoolWrapper::instance().malloc(databuf_size);
  if (!databuf_data) {
    LOG(ERROR) << "Malloc failed, size: " << databuf_size;
    return -1;
  }

  memcpy(databuf_data, input.data(), databuf_size);
  databuf_char = reinterpret_cast<char *>(databuf_data);
  paddle::PaddleBuf paddleBuf(databuf_char, databuf_size);
  paddle::PaddleTensor tensor_in;
  tensor_in.name = in->at(0).name;
  tensor_in.dtype = paddle::PaddleDType::FLOAT32;
  tensor_in.shape = {1, 3, resize_img.rows, resize_img.cols};
  tensor_in.lod = in->at(0).lod;
  tensor_in.data = paddleBuf;
  real_in->push_back(tensor_in);

  if (InferManager::instance().infer(engine_name().c_str(), real_in, out,
                                     batch_size)) {
    LOG(ERROR) << "(logid=" << log_id
               << ") Failed do infer in fluid model: " << engine_name().c_str();
    return -1;
  }

  int64_t end = timeline.TimeStampUS();
  CopyBlobInfo(input_blob, output_blob);
  AddBlobInfo(output_blob, start);
  AddBlobInfo(output_blob, end);
  return 0;
}

cv::Mat GeneralClasOp::Base2Mat(std::string &base64_data) {
  cv::Mat img;
  std::string s_mat;
  s_mat = base64Decode(base64_data.data(), base64_data.size());
  std::vector<char> base64_img(s_mat.begin(), s_mat.end());
  img = cv::imdecode(base64_img, cv::IMREAD_COLOR); // CV_LOAD_IMAGE_COLOR
  return img;
}

std::string GeneralClasOp::base64Decode(const char *Data, int DataByte) {
  const char DecodeTable[] = {
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,
      62, // '+'
      0,  0,  0,
      63,                                     // '/'
      52, 53, 54, 55, 56, 57, 58, 59, 60, 61, // '0'-'9'
      0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
      10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, // 'A'-'Z'
      0,  0,  0,  0,  0,  0,  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
      37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, // 'a'-'z'
  };

  std::string strDecode;
  int nValue;
  int i = 0;
  while (i < DataByte) {
    if (*Data != '\r' && *Data != '\n') {
      nValue = DecodeTable[*Data++] << 18;
      nValue += DecodeTable[*Data++] << 12;
      strDecode += (nValue & 0x00FF0000) >> 16;
      if (*Data != '=') {
        nValue += DecodeTable[*Data++] << 6;
        strDecode += (nValue & 0x0000FF00) >> 8;
        if (*Data != '=') {
          nValue += DecodeTable[*Data++];
          strDecode += nValue & 0x000000FF;
        }
      }
      i += 4;
    } else // 回车换行,跳过
    {
      Data++;
      i++;
    }
  }
  return strDecode;
}
DEFINE_OP(GeneralClasOp);
} // namespace serving
} // namespace paddle_serving
} // namespace baidu
