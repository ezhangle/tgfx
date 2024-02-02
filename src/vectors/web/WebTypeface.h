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

#include <emscripten/val.h>
#include <unordered_map>
#include <vector>
#include "tgfx/core/Font.h"
#include "tgfx/core/Typeface.h"

namespace tgfx {
class WebTypeface : public Typeface {
 public:
  static std::shared_ptr<WebTypeface> Make(const std::string& name, const std::string& style = "");

  uint32_t uniqueID() const override {
    return _uniqueID;
  }

  std::string fontFamily() const override {
    return name;
  }

  std::string fontStyle() const override {
    return style;
  }

  size_t glyphsCount() const override {
    return 1;  // Returns a non-zero value to indicate that we are not empty.
  }

  int unitsPerEm() const override {
    return 0;
  }

  bool hasColor() const override;

  std::string getText(GlyphID glyphID) const;

  GlyphID getGlyphID(Unichar unichar) const override;

  std::shared_ptr<Data> getBytes() const override;

  std::shared_ptr<Data> copyTableData(FontTableTag) const override {
    return nullptr;
  }

 private:
  explicit WebTypeface(std::string name, std::string style);

  uint32_t _uniqueID;
  emscripten::val scalerContextClass = emscripten::val::null();
  std::string name;
  std::string style;
  std::string webFontFamily;
};
}  // namespace tgfx
