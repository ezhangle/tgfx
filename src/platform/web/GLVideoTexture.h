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

#pragma once

#include "gpu/Texture.h"

namespace tgfx {
class GLVideoTexture : public Texture {
 public:
  static std::shared_ptr<GLVideoTexture> Make(Context* context, int width, int height,
                                              bool mipmapped = false);

  size_t memoryUsage() const override;

  const TextureSampler* getSampler() const override {
    return sampler.get();
  }

  Point getTextureCoord(float x, float y) const override;

  BackendTexture getBackendTexture() const override;

 protected:
  void onReleaseGPU() override;

 private:
  std::unique_ptr<TextureSampler> sampler = {};
  int textureWidth = 0;
  int textureHeight = 0;

  GLVideoTexture(std::unique_ptr<TextureSampler> sampler, int width, int height);
};
}  // namespace tgfx
