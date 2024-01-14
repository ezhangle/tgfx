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

#include "RenderTargetCopyTask.h"

namespace tgfx {
RenderTargetCopyTask::RenderTargetCopyTask(std::shared_ptr<RenderTargetProxy> source,
                                           std::shared_ptr<TextureProxy> dest, Rect srcRect,
                                           Point dstPoint)
    : RenderTask(std::move(source)), dest(std::move(dest)), srcRect(std::move(srcRect)),
      dstPoint(std::move(dstPoint)) {
}

bool RenderTargetCopyTask::execute(Gpu* gpu) {
  auto renderTarget = renderTargetProxy->getRenderTarget();
  if (renderTarget == nullptr) {
    LOGE("RenderTargetCopyTask::execute() Failed to get the source render target!");
    return false;
  }
  auto texture = dest->getTexture();
  if (texture == nullptr) {
    LOGE("RenderTargetCopyTask::execute() Failed to get the dest texture!");
    return false;
  }
  gpu->copyRenderTargetToTexture(renderTarget.get(), texture.get(), srcRect, dstPoint);
  return true;
}
}  // namespace tgfx