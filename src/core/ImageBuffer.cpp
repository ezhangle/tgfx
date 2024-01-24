/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
//  in compliance with the License. You may obtain a copy of the License at
//
//      https://opensource.org/licenses/BSD-3-Clause
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "tgfx/core/ImageBuffer.h"
#include "core/RasterBuffer.h"
#include "core/RasterGenerator.h"
#include "core/RasterYUVBuffer.h"

namespace tgfx {
std::shared_ptr<ImageBuffer> ImageBuffer::MakeFrom(const ImageInfo& info,
                                                   std::shared_ptr<Data> pixels) {
  auto buffer = RasterBuffer::MakeFrom(info, pixels);
  if (buffer != nullptr) {
    return buffer;
  }
  auto generator = RasterGenerator::MakeFrom(info, std::move(pixels));
  return generator->makeBuffer();
}

std::shared_ptr<ImageBuffer> ImageBuffer::MakeI420(std::shared_ptr<YUVData> yuvData,
                                                   YUVColorSpace colorSpace) {
  if (yuvData == nullptr || yuvData->planeCount() != YUVData::I420_PLANE_COUNT) {
    return nullptr;
  }
  return std::make_shared<RasterYUVBuffer>(std::move(yuvData), YUVPixelFormat::I420, colorSpace);
}

std::shared_ptr<ImageBuffer> ImageBuffer::MakeNV12(std::shared_ptr<YUVData> yuvData,
                                                   YUVColorSpace colorSpace) {
  if (yuvData == nullptr || yuvData->planeCount() != YUVData::NV12_PLANE_COUNT) {
    return nullptr;
  }
  return std::make_shared<RasterYUVBuffer>(std::move(yuvData), YUVPixelFormat::NV12, colorSpace);
}
}  // namespace tgfx
