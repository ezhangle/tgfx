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

#include "tgfx/core/Image.h"
#include "gpu/Texture.h"
#include "gpu/processors/TiledTextureEffect.h"
#include "images/ImageSource.h"
#include "images/RGBAAAImage.h"
#include "images/RasterBuffer.h"
#include "images/RasterGenerator.h"
#include "images/SubsetImage.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/gpu/Surface.h"

namespace tgfx {
std::shared_ptr<Image> Image::MakeFromFile(const std::string& filePath) {
  auto codec = ImageCodec::MakeFrom(filePath);
  if (codec == nullptr) {
    return nullptr;
  }
  auto source = ImageSource::MakeFrom(codec);
  return MakeFrom(source, codec->origin());
}

std::shared_ptr<Image> Image::MakeFromEncoded(std::shared_ptr<Data> encodedData) {
  auto codec = ImageCodec::MakeFrom(std::move(encodedData));
  if (codec == nullptr) {
    return nullptr;
  }
  auto source = ImageSource::MakeFrom(codec);
  return MakeFrom(source, codec->origin());
}

std::shared_ptr<Image> Image::MakeFrom(NativeImageRef nativeImage) {
  auto codec = ImageCodec::MakeFrom(nativeImage);
  return MakeFrom(std::move(codec));
}

std::shared_ptr<Image> Image::MakeFrom(std::shared_ptr<ImageGenerator> generator) {
  auto source = ImageSource::MakeFrom(std::move(generator));
  return MakeFrom(std::move(source));
}

std::shared_ptr<Image> Image::MakeFrom(const ImageInfo& info, std::shared_ptr<Data> pixels) {
  auto imageBuffer = RasterBuffer::MakeFrom(info, pixels);
  if (imageBuffer != nullptr) {
    return MakeFrom(std::move(imageBuffer));
  }
  auto imageGenerator = RasterGenerator::MakeFrom(info, std::move(pixels));
  return MakeFrom(std::move(imageGenerator));
}

std::shared_ptr<Image> Image::MakeFrom(const Bitmap& bitmap) {
  return MakeFrom(bitmap.makeBuffer());
}

std::shared_ptr<Image> Image::MakeFrom(HardwareBufferRef hardwareBuffer, YUVColorSpace colorSpace) {
  auto buffer = ImageBuffer::MakeFrom(hardwareBuffer, colorSpace);
  return MakeFrom(std::move(buffer));
}

std::shared_ptr<Image> Image::MakeI420(std::shared_ptr<YUVData> yuvData, YUVColorSpace colorSpace) {
  auto buffer = ImageBuffer::MakeI420(std::move(yuvData), colorSpace);
  return MakeFrom(std::move(buffer));
}

std::shared_ptr<Image> Image::MakeNV12(std::shared_ptr<YUVData> yuvData, YUVColorSpace colorSpace) {
  auto buffer = ImageBuffer::MakeNV12(std::move(yuvData), colorSpace);
  return MakeFrom(std::move(buffer));
}

std::shared_ptr<Image> Image::MakeFrom(std::shared_ptr<ImageBuffer> imageBuffer) {
  auto source = ImageSource::MakeFrom(std::move(imageBuffer));
  return MakeFrom(std::move(source));
}

std::shared_ptr<Image> Image::MakeFrom(Context* context, const BackendTexture& backendTexture,
                                       ImageOrigin origin) {
  if (context == nullptr) {
    return nullptr;
  }
  auto textureProxy = context->proxyProvider()->wrapBackendTexture(backendTexture, origin, false);
  return MakeFrom(std::move(textureProxy));
}

std::shared_ptr<Image> Image::MakeAdopted(Context* context, const BackendTexture& backendTexture,
                                          ImageOrigin origin) {
  if (context == nullptr) {
    return nullptr;
  }
  auto textureProxy = context->proxyProvider()->wrapBackendTexture(backendTexture, origin, true);
  return MakeFrom(std::move(textureProxy));
}

std::shared_ptr<Image> Image::MakeFrom(std::shared_ptr<TextureProxy> textureProxy) {
  auto source = ImageSource::MakeFrom(std::move(textureProxy));
  return MakeFrom(std::move(source));
}

std::shared_ptr<Image> Image::MakeFrom(std::shared_ptr<ImageSource> source, EncodedOrigin origin) {
  if (source == nullptr) {
    return nullptr;
  }
  std::shared_ptr<Image> image = nullptr;
  if (origin != EncodedOrigin::TopLeft) {
    image = std::make_shared<SubsetImage>(std::move(source), origin);
  } else {
    image = std::shared_ptr<Image>(new Image(std::move(source)));
  }
  image->weakThis = image;
  return image;
}

Image::Image(std::shared_ptr<ImageSource> source) : source(std::move(source)) {
}

int Image::width() const {
  return source->width();
}

int Image::height() const {
  return source->height();
}

bool Image::hasMipmaps() const {
  return source->hasMipmaps();
}

bool Image::isAlphaOnly() const {
  return source->isAlphaOnly();
}

bool Image::isLazyGenerated() const {
  return source->isLazyGenerated();
}

bool Image::isTextureBacked() const {
  return source->isTextureBacked();
}

BackendTexture Image::getBackendTexture(Context* context) const {
  return source->getBackendTexture(context);
}

std::shared_ptr<Image> Image::makeTextureImage(Context* context) const {
  auto textureSource = onMakeTextureSource(context);
  if (textureSource == source) {
    return weakThis.lock();
  }
  if (textureSource != nullptr) {
    return cloneWithSource(std::move(textureSource));
  }
  auto surface = Surface::Make(context, width(), height(), isAlphaOnly(), 1, hasMipmaps());
  if (surface == nullptr) {
    return nullptr;
  }
  auto canvas = surface->getCanvas();
  canvas->drawImage(weakThis.lock(), 0, 0);
  return surface->makeImageSnapshot();
}

std::shared_ptr<ImageSource> Image::onMakeTextureSource(Context* context) const {
  return source->makeTextureSource(context);
}

std::shared_ptr<Image> Image::onCloneWith(std::shared_ptr<ImageSource> newSource) const {
  return std::shared_ptr<Image>(new Image(std::move(newSource)));
}

std::shared_ptr<Image> Image::makeSubset(const Rect& subset) const {
  auto rect = subset;
  rect.round();
  auto bounds = Rect::MakeWH(width(), height());
  if (bounds == rect) {
    return weakThis.lock();
  }
  if (!bounds.contains(rect)) {
    return nullptr;
  }
  auto subsetImage = onMakeSubset(rect);
  subsetImage->weakThis = subsetImage;
  return subsetImage;
}

std::shared_ptr<Image> Image::onMakeSubset(const Rect& subset) const {
  return std::make_shared<SubsetImage>(source, subset);
}

std::shared_ptr<Image> Image::makeDecoded(Context* context) const {
  auto decodedSource = source->makeDecoded(context);
  if (decodedSource == source) {
    return weakThis.lock();
  }
  return cloneWithSource(std::move(decodedSource));
}

std::shared_ptr<Image> Image::makeMipMapped() const {
  auto mipMappedSource = source->makeMipMapped();
  if (mipMappedSource == source) {
    return weakThis.lock();
  }
  return cloneWithSource(std::move(mipMappedSource));
}

std::shared_ptr<Image> Image::makeRGBAAA(int displayWidth, int displayHeight, int alphaStartX,
                                         int alphaStartY) {
  if (alphaStartX == 0 && alphaStartY == 0) {
    return makeSubset(Rect::MakeWH(displayWidth, displayHeight));
  }
  auto image = onMakeRGBAAA(displayWidth, displayHeight, alphaStartX, alphaStartY);
  if (image != nullptr) {
    image->weakThis = image;
  }
  return image;
}

std::shared_ptr<Image> Image::onMakeRGBAAA(int displayWidth, int displayHeight, int alphaStartX,
                                           int alphaStartY) const {
  if (isAlphaOnly() || alphaStartX + displayWidth > source->width() ||
      alphaStartY + displayHeight > source->height()) {
    return nullptr;
  }
  return std::shared_ptr<Image>(
      new RGBAAAImage(source, displayWidth, displayHeight, alphaStartX, alphaStartY));
}

std::shared_ptr<Image> Image::applyOrigin(EncodedOrigin origin) const {
  if (origin == EncodedOrigin::TopLeft) {
    return weakThis.lock();
  }
  auto image = onApplyOrigin(origin);
  if (image != nullptr) {
    image->weakThis = image;
  }
  return image;
}

std::shared_ptr<Image> Image::onApplyOrigin(EncodedOrigin encodedOrigin) const {
  return std::make_shared<SubsetImage>(std::move(source), encodedOrigin);
}

std::unique_ptr<FragmentProcessor> Image::asFragmentProcessor(
    Context* context, uint32_t renderFlags, TileMode tileModeX, TileMode tileModeY,
    const SamplingOptions& sampling, const Matrix* localMatrix) {
  return TiledTextureEffect::Make(source->lockTextureProxy(context, renderFlags), tileModeX,
                                  tileModeY, sampling, localMatrix);
}

std::unique_ptr<FragmentProcessor> Image::asFragmentProcessor(Context* context,
                                                              uint32_t renderFlags,
                                                              const SamplingOptions& sampling) {
  return asFragmentProcessor(context, renderFlags, TileMode::Clamp, TileMode::Clamp, sampling);
}

std::shared_ptr<Image> Image::cloneWithSource(std::shared_ptr<ImageSource> newSource) const {
  auto newImage = onCloneWith(std::move(newSource));
  newImage->weakThis = newImage;
  return newImage;
}
}  // namespace tgfx
