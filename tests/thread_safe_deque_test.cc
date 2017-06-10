/*
Copyright 2017 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS-IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "lullaby/base/thread_safe_deque.h"

namespace lull {
namespace {

struct TestObject {
  explicit TestObject(int value) : value(value) {}
  int value;
};

typedef std::unique_ptr<TestObject> TestObjectPtr;

TEST(ThreadSafeDeque, MultiProducerSingleConsumer) {
  static constexpr int kSentinel = -1;
  static constexpr int kNumProducers = 100;
  typedef ThreadSafeDeque<TestObjectPtr> TSDeque;
  TSDeque deque;

  // Create 100 threads that insert the numbers 1-100 into the deque.
  std::vector<std::thread> producers;
  for (int i = 0; i < kNumProducers; ++i) {
    producers.emplace_back([&deque]() {
      // Wait one second just to make sure we have started the dequeing loop
      // below.
      std::this_thread::sleep_for(std::chrono::seconds(1));
      for (int j = 0; j < 100; ++j) {
        deque.PushBack(TestObjectPtr(new TestObject(j + 1)));
      }
      // Insert a -1 to mark the end of the insertion loop.
      deque.PushBack(TestObjectPtr(new TestObject(kSentinel)));
    });
  }

  int end_count = 0;
  int total_count = 0;
  while (end_count < kNumProducers) {
    TestObjectPtr obj = nullptr;
    if (deque.PopFront(&obj)) {
      if (obj->value == kSentinel) {
        ++end_count;
      } else {
        total_count += obj->value;
      }
    }
  }

  for (auto& thread : producers) {
    thread.join();
  }

  EXPECT_EQ(kNumProducers, end_count);
  EXPECT_EQ(5050 * kNumProducers, total_count);  // Sum(1..100) == 5050
  EXPECT_TRUE(deque.Empty());
}

TEST(ThreadSafeDeque, MultiProducerSingleConsumerWithWait) {
  static const int kSentinel = -1;
  static const int kNumProducers = 100;
  typedef ThreadSafeDeque<TestObjectPtr> TSDeque;
  TSDeque deque;

  // Create 100 threads that insert the numbers 1-100 into the deque.
  std::vector<std::thread> producers;
  for (int i = 0; i < kNumProducers; ++i) {
    producers.emplace_back([&deque]() {
      // Wait one second just to make sure we have started the dequeing loop
      // below.
      std::this_thread::sleep_for(std::chrono::seconds(1));
      for (int j = 0; j < 100; ++j) {
        deque.PushBack(TestObjectPtr(new TestObject(j + 1)));
      }
      // Insert a -1 to mark the end of the insertion loop.
      deque.PushBack(TestObjectPtr(new TestObject(kSentinel)));
    });
  }

  int end_count = 0;
  int total_count = 0;
  while (end_count < kNumProducers) {
    TestObjectPtr obj = deque.WaitPopFront();
    if (obj) {
      if (obj->value == kSentinel) {
        ++end_count;
      } else {
        total_count += obj->value;
      }
    }
  }

  for (auto& thread : producers) {
    thread.join();
  }

  EXPECT_EQ(kNumProducers, end_count);
  EXPECT_EQ(5050 * kNumProducers, total_count);  // Sum(1..100) == 5050
  EXPECT_TRUE(deque.Empty());
}

TEST(ThreadSafeDeque, MultiProducerMultiConsumer) {
  static const int kSentinel = -1;
  static const int kNumProducers = 100;
  static const int kNumConsumers = 20;
  typedef ThreadSafeDeque<TestObjectPtr> TSDeque;
  TSDeque deque;

  // Create 100 threads that insert the numbers 1-100 into the deque.
  std::vector<std::thread> producers;
  for (int i = 0; i < kNumProducers; ++i) {
    producers.emplace_back([&deque]() {
      // Wait one second just to make sure we have started the dequeing loop
      // below.
      std::this_thread::sleep_for(std::chrono::seconds(1));
      for (int j = 0; j < 100; ++j) {
        deque.PushBack(TestObjectPtr(new TestObject(j + 1)));
      }
      // Insert a -1 to mark the end of the insertion loop.
      deque.PushBack(TestObjectPtr(new TestObject(kSentinel)));
    });
  }

  std::mutex mutex;
  int end_count = 0;
  int total_count = 0;

  std::vector<std::thread> consumers;
  for (int i = 0; i < kNumConsumers; ++i) {
    consumers.emplace_back([&]() {
      bool done = false;
      while (!done) {
        TestObjectPtr obj = nullptr;
        if (deque.PopFront(&obj)) {
          if (obj->value == kSentinel) {
            mutex.lock();
            ++end_count;
            mutex.unlock();
          } else {
            mutex.lock();
            total_count += obj->value;
            mutex.unlock();
          }
        }
        mutex.lock();
        done = end_count >= kNumProducers;
        mutex.unlock();
      }
    });
  }

  for (auto& thread : consumers) {
    thread.join();
  }
  for (auto& thread : producers) {
    thread.join();
  }

  EXPECT_EQ(kNumProducers, end_count);
  EXPECT_EQ(5050 * kNumProducers, total_count);  // Sum(1..100) == 5050
  EXPECT_TRUE(deque.Empty());
}

TEST(ThreadSafeDeque, RemoveIf) {
  ThreadSafeDeque<TestObjectPtr> deque;

  deque.PushBack(TestObjectPtr(new TestObject(0)));
  deque.RemoveIf([](const TestObjectPtr& obj) { return obj->value == 0; });
  EXPECT_TRUE(deque.Empty());

  deque.PushBack(TestObjectPtr(new TestObject(0)));
  deque.RemoveIf([](const TestObjectPtr& obj) { return obj->value == 1; });
  EXPECT_FALSE(deque.Empty());

  TestObjectPtr ptr;
  EXPECT_TRUE(deque.PopFront(&ptr));

  deque.PushBack(TestObjectPtr(new TestObject(0)));
  deque.PushBack(TestObjectPtr(new TestObject(1)));
  deque.PushBack(TestObjectPtr(new TestObject(0)));
  deque.PushBack(TestObjectPtr(new TestObject(2)));
  deque.RemoveIf([](const TestObjectPtr& obj) { return obj->value == 0; });
  EXPECT_TRUE(deque.PopFront(&ptr));
  EXPECT_EQ(ptr->value, 1);
  EXPECT_TRUE(deque.PopFront(&ptr));
  EXPECT_EQ(ptr->value, 2);
}

}  // namespace
}  // namespace lull
