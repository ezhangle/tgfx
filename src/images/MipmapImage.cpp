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

#include "MipmapImage.h"
#include "utils/Log.h"
#include "utils/UniqueID.h"

namespace tgfx {
static BytesKey MakeMipmapBytesKey() {
  auto mipmapFlag = UniqueID::Next();
  BytesKey bytesKey(1);
  bytesKey.write(mipmapFlag);
  return bytesKey;
}

std::shared_ptr<Image> MipmapImage::MakeFrom(std::shared_ptr<ResourceImage> source) {
  if (source == nullptr) {
    return nullptr;
  }
  DEBUG_ASSERT(!source->hasMipmaps());
  static const auto MipmapBytesKey = MakeMipmapBytesKey();
  auto uniqueKey = UniqueKey::Combine(source->uniqueKey, MipmapBytesKey);
  auto image =
      std::shared_ptr<MipmapImage>(new MipmapImage(std::move(uniqueKey), std::move(source)));
  image->weakThis = image;
  return image;
}

MipmapImage::MipmapImage(UniqueKey uniqueKey, std::shared_ptr<ResourceImage> source)
    : ResourceImage(std::move(uniqueKey)), source(std::move(source)) {
}

std::shared_ptr<Image> MipmapImage::makeRasterized(float rasterizationScale,
                                                   SamplingOptions sampling) const {
  if (rasterizationScale == 1.0f) {
    return weakThis.lock();
  }
  auto newSource =
      std::static_pointer_cast<ResourceImage>(source->makeRasterized(rasterizationScale, sampling));
  if (newSource != nullptr) {
    return newSource->makeMipmapped(true);
  }
  return newSource;
}

std::shared_ptr<Image> MipmapImage::onMakeDecoded(Context* context, bool) const {
  auto newSource = std::static_pointer_cast<ResourceImage>(source->onMakeDecoded(context, false));
  if (newSource == nullptr) {
    return nullptr;
  }
  auto newImage = std::shared_ptr<MipmapImage>(new MipmapImage(uniqueKey, std::move(newSource)));
  newImage->weakThis = newImage;
  return newImage;
}

std::shared_ptr<Image> MipmapImage::onMakeMipmapped(bool enabled) const {
  return enabled ? weakThis.lock() : source;
}

std::shared_ptr<TextureProxy> MipmapImage::onLockTextureProxy(Context* context,
                                                              const UniqueKey& key, bool,
                                                              uint32_t renderFlags) const {
  return source->onLockTextureProxy(context, key, true, renderFlags);
}
}  // namespace tgfx
