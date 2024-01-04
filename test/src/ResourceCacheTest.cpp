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
#include "tgfx/gpu/Resource.h"
#include "tgfx/utils/Task.h"
#include "utils/TestUtils.h"

namespace tgfx {
class TestResource : public Resource {
 public:
  static std::shared_ptr<const TestResource> Make(tgfx::Context* context, uint32_t id) {
    return Resource::Wrap(context, new TestResource(id));
  }

  size_t memoryUsage() const override {
    return 1;
  }

 protected:
  void computeScratchKey(BytesKey* key) const override {
    key->write(id);
  }

 private:
  TestResource(uint32_t id) : Resource(), id(id) {
  }

  void onReleaseGPU() override {
  }

  uint32_t id = 0;
};

TGFX_TEST(ResourceCacheTest, multiThreadRecycling) {
  auto device = DevicePool::Make();
  ASSERT_TRUE(device != nullptr);
  tgfx::Task::Run([device] {
    for (int i = 0; i < 100; ++i) {
      auto context = device->lockContext();
      ASSERT_TRUE(context != nullptr);
      auto resource = TestResource::Make(context, i);
      context->flush();
      context->resourceCache()->purgeUntilMemoryTo(0);
      device->unlock();
      tgfx::Task::Run([resource, device] {
        resource.get();
        device.get();
      });
    }
  });
}
}  // namespace tgfx
