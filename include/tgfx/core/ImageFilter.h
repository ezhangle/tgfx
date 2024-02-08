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

#include "tgfx/core/Filter.h"

namespace tgfx {
/**
 * ImageFilter is the base class for all image filters. It accepts various Image types as its input
 * and rasterizes the input Image to a texture before applying the filter. The rasterized Image is
 * then altered by the ImageFilter, potentially changing its bounds.
 */
class ImageFilter : public Filter {
 public:
  /**
   * Create a filter that blurs its input by the separate X and Y blurriness. The provided tile mode
   * is used when the blur kernel goes outside the input image.
   * @param blurrinessX  The Gaussian sigma value for blurring along the X axis.
   * @param blurrinessY  The Gaussian sigma value for blurring along the Y axis.
   * @param tileMode     The tile mode applied at edges.
   */
  static std::shared_ptr<ImageFilter> Blur(float blurrinessX, float blurrinessY,
                                           TileMode tileMode = TileMode::Decal);

  /**
   * Create a filter that draws a drop shadow under the input content. This filter produces an image
   * that includes the inputs' content.
   * @param dx            The X offset of the shadow.
   * @param dy            The Y offset of the shadow.
   * @param blurrinessX   The blur radius for the shadow, along the X axis.
   * @param blurrinessY   The blur radius for the shadow, along the Y axis.
   * @param color         The color of the drop shadow.
   */
  static std::shared_ptr<ImageFilter> DropShadow(float dx, float dy, float blurrinessX,
                                                 float blurrinessY, const Color& color);

  /**
   * Create a filter that renders a drop shadow, in exactly the same manner as the DropShadow(),
   * except that the resulting image does not include the input content.
   * @param dx            The X offset of the shadow.
   * @param dy            The Y offset of the shadow.
   * @param blurrinessX   The blur radius for the shadow, along the X axis.
   * @param blurrinessY   The blur radius for the shadow, along the Y axis.
   * @param color         The color of the drop shadow.
   */
  static std::shared_ptr<ImageFilter> DropShadowOnly(float dx, float dy, float blurrinessX,
                                                     float blurrinessY, const Color& color);

  Rect filterBounds(const Rect& rect) const override;

 protected:
  std::unique_ptr<DrawOp> onMakeDrawOp(std::shared_ptr<Image> source, const DrawArgs& args,
                                       const Matrix* localMatrix, TileMode tileModeX,
                                       TileMode tileModeY) const override;

  virtual Rect onFilterBounds(const Rect& srcRect) const;

  bool applyCropRect(const Rect& srcRect, Rect* dstRect, const Rect* clipBounds = nullptr) const;

  friend class DropShadowImageFilter;
};
}  // namespace tgfx
