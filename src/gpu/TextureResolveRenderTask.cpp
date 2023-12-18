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

#include "TextureResolveRenderTask.h"
#include "Gpu.h"
#include "RenderTarget.h"

namespace tgfx {
TextureResolveRenderTask::TextureResolveRenderTask(std::shared_ptr<RenderTarget> renderTarget,
                                                   std::shared_ptr<Texture> texture)
    : RenderTask(std::move(renderTarget)), texture(std::move(texture)) {
}

bool TextureResolveRenderTask::execute(Gpu* gpu) {
  if (renderTarget->sampleCount() > 1) {
    gpu->resolveRenderTarget(renderTarget.get());
  }
  if (texture != nullptr && texture->getSampler()->hasMipmaps()) {
    gpu->regenerateMipMapLevels(texture->getSampler());
  }
  return true;
}
}  // namespace tgfx
