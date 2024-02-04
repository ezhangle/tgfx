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

#include "DecoderImage.h"
#include "BufferImage.h"
#include "gpu/ProxyProvider.h"

namespace tgfx {
std::shared_ptr<Image> DecoderImage::MakeFrom(ResourceKey resourceKey,
                                              std::shared_ptr<ImageDecoder> decoder) {
  if (decoder == nullptr) {
    return nullptr;
  }
  auto image =
      std::shared_ptr<DecoderImage>(new DecoderImage(std::move(resourceKey), std::move(decoder)));
  image->weakThis = image;
  return image;
}

DecoderImage::DecoderImage(ResourceKey resourceKey, std::shared_ptr<ImageDecoder> decoder)
    : TextureImage(std::move(resourceKey)), decoder(std::move(decoder)) {
}

std::shared_ptr<TextureProxy> DecoderImage::onLockTextureProxy(Context* context,
                                                               const ResourceKey& key,
                                                               bool mipmapped,
                                                               uint32_t renderFlags) const {
  return context->proxyProvider()->createTextureProxy(key, decoder, mipmapped, renderFlags);
}
}  // namespace tgfx
