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

#include <atomic>
#include "utils/UniqueID.h"

namespace tgfx {
class UniqueDomain {
 public:
  UniqueDomain();

  /**
   * Returns a global unique ID for the UniqueBlock.
   */
  uint32_t uniqueID() const {
    return _uniqueID;
  }

  /**
   * Returns the total number of times the UniqueBlock has been referenced.
   */
  long useCount() const {
    return _useCount;
  }

  /**
   * Returns the number of times the UniqueBlock has been referenced strongly.
   */
  long strongCount() const {
    return _strongCount;
  }

  /**
   * Increments the number of times the UniqueBlock has been referenced.
   */
  void addReference(bool strong);

  /**
   * Decrements the number of times the UniqueBlock has been referenced.
   */
  void releaseReference(bool strong);

 private:
  uint32_t _uniqueID = 0;
  std::atomic_long _useCount = {0};
  std::atomic_long _strongCount = {0};
};
}  // namespace tgfx
