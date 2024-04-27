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

#include <vector>
#include "tgfx/core/Mask.h"
#include "tgfx/core/PathEffect.h"
#include "tgfx/gpu/Surface.h"
#include "utils/TestUtils.h"
#include "vectors/freetype/FTMask.h"

namespace tgfx {

TGFX_TEST(FilterTest, ColorMatrixFilter) {
  auto device = DevicePool::Make();
  ASSERT_TRUE(device != nullptr);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto image = MakeImage("resources/apitest/test_timestretch.png");
  ASSERT_TRUE(image != nullptr);
  auto surface = Surface::Make(context, image->width(), image->height());
  auto canvas = surface->getCanvas();
  Paint paint;
  std::array<float, 20> matrix = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0};
  paint.setColorFilter(ColorFilter::Matrix(matrix));
  canvas->drawImage(image, &paint);
  EXPECT_TRUE(Baseline::Compare(surface, "FilterTest/identityMatrix"));
  canvas->clear();
  std::array<float, 20> greyColorMatrix = {0.21f, 0.72f, 0.07f, 0.41f, 0,  // red
                                           0.21f, 0.72f, 0.07f, 0.41f, 0,  // green
                                           0.21f, 0.72f, 0.07f, 0.41f, 0,  // blue
                                           0,     0,     0,     1.0f,  0};
  paint.setColorFilter(ColorFilter::Matrix(greyColorMatrix));
  canvas->drawImage(image, &paint);
  EXPECT_TRUE(Baseline::Compare(surface, "FilterTest/greyColorMatrix"));
  device->unlock();
}

TGFX_TEST(FilterTest, ShaderMaskFilter) {
  auto device = DevicePool::Make();
  ASSERT_TRUE(device != nullptr);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto mask = MakeImage("resources/apitest/test_timestretch.png");
  ASSERT_TRUE(mask != nullptr);
  auto shader = Shader::MakeImageShader(mask);
  ASSERT_TRUE(shader != nullptr);
  shader = shader->makeWithColorFilter(ColorFilter::Luma());
  ASSERT_TRUE(shader != nullptr);
  auto maskFilter = MaskFilter::MakeShader(shader);
  auto image = MakeImage("resources/apitest/rotation.jpg");
  image = image->makeOriented(Orientation::LeftBottom);
  image = image->makeMipmapped(true);
  image = image->makeRasterized(0.25f);
  ASSERT_TRUE(image != nullptr);
  auto surface = Surface::Make(context, image->width(), image->height());
  auto canvas = surface->getCanvas();
  Paint paint;
  paint.setMaskFilter(maskFilter);
  canvas->drawImage(image, &paint);
  EXPECT_TRUE(Baseline::Compare(surface, "FilterTest/shaderMaskFilter"));
  device->unlock();
}

TGFX_TEST(FilterTest, Blur) {
  auto device = DevicePool::Make();
  ASSERT_TRUE(device != nullptr);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto image = MakeImage("resources/apitest/rotation.jpg");
  ASSERT_TRUE(image != nullptr);
  auto imageMatrix = Matrix::MakeScale(0.2f, 0.2f);
  auto bounds = Rect::MakeWH(image->width(), image->height());
  imageMatrix.mapRect(&bounds);
  auto imageWidth = static_cast<float>(bounds.width());
  auto imageHeight = static_cast<float>(bounds.height());
  auto padding = 30.f;
  Paint paint;
  auto surface = Surface::Make(context, static_cast<int>(imageWidth * 2.f + padding * 3.f),
                               static_cast<int>(imageHeight * 2.f + padding * 3.f));
  auto canvas = surface->getCanvas();
  canvas->concat(Matrix::MakeTrans(padding, padding));
  canvas->save();
  canvas->concat(imageMatrix);
  canvas->drawImage(image, &paint);
  canvas->restore();
  Path path;
  path.addRect(Rect::MakeWH(imageWidth, imageHeight));
  Stroke stroke(1.f);
  PathEffect::MakeStroke(&stroke)->applyTo(&path);
  paint.setImageFilter(nullptr);
  paint.setColor(Color{1.f, 0.f, 0.f, 1.f});
  canvas->drawPath(path, paint);

  canvas->concat(Matrix::MakeTrans(imageWidth + padding, 0));
  canvas->save();
  canvas->concat(imageMatrix);
  paint.setImageFilter(ImageFilter::Blur(130, 130, TileMode::Decal));
  canvas->drawImage(image, &paint);
  canvas->restore();
  paint.setImageFilter(nullptr);
  canvas->drawPath(path, paint);
  paint.setImageFilter(nullptr);

  canvas->concat(Matrix::MakeTrans(-imageWidth - padding, imageHeight + padding));
  canvas->save();
  canvas->concat(imageMatrix);
  Point filterOffset = Point::Zero();
  auto cropRect = Rect::MakeXYWH(0, 0, image->width(), image->height());
  auto filterImage = image->makeWithFilter(ImageFilter::Blur(130, 130, TileMode::Repeat),
                                           &filterOffset, &cropRect);
  ASSERT_TRUE(filterImage != nullptr);
  EXPECT_EQ(filterImage->width(), image->width());
  EXPECT_EQ(filterImage->height(), image->height());
  EXPECT_EQ(filterOffset.x, 0.0f);
  EXPECT_EQ(filterOffset.y, 0.0f);
  canvas->drawImage(filterImage, &paint);
  canvas->restore();
  paint.setImageFilter(nullptr);
  canvas->drawPath(path, paint);

  canvas->concat(Matrix::MakeTrans(imageWidth + padding, 0));
  canvas->save();
  canvas->concat(imageMatrix);
  auto filter = ImageFilter::Blur(130, 130, TileMode::Clamp);
  cropRect = Rect::MakeLTRB(2000, -100, 3124, 2000);
  filterImage = image->makeWithFilter(filter, &filterOffset, &cropRect);
  canvas->drawImage(filterImage, 2000, -100, &paint);
  cropRect = Rect::MakeXYWH(1000, 1000, 1000, 1000);
  filterImage = image->makeWithFilter(filter, &filterOffset, &cropRect);
  canvas->drawImage(filterImage, 1000, 1000, &paint);
  cropRect = Rect::MakeXYWH(1000, 2000, 1000, 1000);
  filterImage = image->makeWithFilter(filter, &filterOffset, &cropRect);
  canvas->drawImage(filterImage, 1000, 2000, &paint);
  canvas->restore();
  canvas->drawPath(path, paint);

  EXPECT_TRUE(Baseline::Compare(surface, "FilterTest/blur"));
  device->unlock();
}

TGFX_TEST(FilterTest, DropShadow) {
  auto device = DevicePool::Make();
  ASSERT_TRUE(device != nullptr);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto image = MakeImage("resources/apitest/image_as_mask.png");
  ASSERT_TRUE(image != nullptr);
  auto imageWidth = static_cast<float>(image->width());
  auto imageHeight = static_cast<float>(image->height());
  auto padding = 30.f;
  Paint paint;
  auto surface = Surface::Make(context, static_cast<int>(imageWidth * 2.f + padding * 3.f),
                               static_cast<int>(imageHeight * 2.f + padding * 3.f));
  auto canvas = surface->getCanvas();
  canvas->concat(Matrix::MakeTrans(padding, padding));
  paint.setImageFilter(ImageFilter::Blur(15, 15));
  canvas->drawImage(image, &paint);

  canvas->concat(Matrix::MakeTrans(imageWidth + padding, 0));
  paint.setImageFilter(ImageFilter::DropShadowOnly(0, 0, 15, 15, Color::White()));
  canvas->drawImage(image, &paint);

  canvas->concat(Matrix::MakeTrans(-imageWidth - padding, imageWidth + padding));
  paint.setImageFilter(ImageFilter::DropShadow(0, 0, 15, 15, Color::White()));
  canvas->drawImage(image, &paint);

  canvas->concat(Matrix::MakeTrans(imageWidth + padding, 0));
  auto filter = ImageFilter::DropShadow(3, 3, 0, 0, Color::White());
  paint.setImageFilter(filter);
  canvas->drawImage(image, &paint);

  EXPECT_TRUE(Baseline::Compare(surface, "FilterTest/dropShadow"));
  device->unlock();

  auto src = Rect::MakeXYWH(10, 10, 10, 10);
  auto bounds = filter->filterBounds(src);
  EXPECT_EQ(bounds, Rect::MakeXYWH(10, 10, 13, 13));
  bounds = ImageFilter::DropShadowOnly(3, 3, 0, 0, Color::White())->filterBounds(src);
  EXPECT_EQ(bounds, Rect::MakeXYWH(13, 13, 10, 10));
}

TGFX_TEST(FilterTest, ImageFilterShader) {
  auto device = DevicePool::Make();
  ASSERT_TRUE(device != nullptr);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto image = MakeImage("resources/assets/bridge.jpg");
  ASSERT_TRUE(image != nullptr);
  auto surface = Surface::Make(context, 720, 720);
  auto canvas = surface->getCanvas();
  image = image->makeMipmapped(true);
  auto filter = tgfx::ImageFilter::DropShadow(0, 0, 300, 300, tgfx::Color::Black());
  image = image->makeWithFilter(std::move(filter));
  auto imageSize = 480.0f;
  auto imageScale = imageSize / static_cast<float>(image->width());
  SamplingOptions sampling(tgfx::FilterMode::Linear, tgfx::MipmapMode::Linear);
  auto shader = Shader::MakeImageShader(image, TileMode::Repeat, TileMode::Repeat, sampling);
  auto matrix = Matrix::MakeScale(imageScale);
  matrix.postTranslate(120, 120);
  shader = shader->makeWithMatrix(matrix);
  Paint paint = {};
  paint.setShader(std::move(shader));
  canvas->drawRect(Rect::MakeWH(720, 720), paint);
  EXPECT_TRUE(Baseline::Compare(surface, "FilterTest/ImageFilterShader"));
  device->unlock();
}

}  // namespace tgfx
