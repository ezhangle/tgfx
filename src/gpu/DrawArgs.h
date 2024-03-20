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

#include "tgfx/core/SamplingOptions.h"
#include "tgfx/gpu/Surface.h"

namespace tgfx {
class DrawArgs {
 public:
  DrawArgs() = default;

  DrawArgs(Context* context, uint32_t renderFlags, const Rect& drawRect,
           const Matrix& viewMatrix = Matrix::I())
      : context(context), renderFlags(renderFlags), drawRect(drawRect), viewMatrix(viewMatrix) {
  }

  bool empty() const {
    return context == nullptr || drawRect.isEmpty();
  }

  Context* context = nullptr;
  uint32_t renderFlags = 0;
  Rect drawRect = Rect::MakeEmpty();
  Matrix viewMatrix = Matrix::I();
};
}  // namespace tgfx
