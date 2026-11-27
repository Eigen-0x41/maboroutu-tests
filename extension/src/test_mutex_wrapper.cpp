// maboroutu.mutex_wrapper のテスト。
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

import maboroutu.core;
import maboroutu.mutex_wrapper;

namespace {

using maboroutu::mutex;
using maboroutu::mutex_wrapper;
using maboroutu::null_mutex;
using maboroutu::shared_mutex;

template <class W, class F>
concept can_read_lock = requires(W &w, F &&f) { w.read_lock(std::forward<F>(f)); };
template <class W, class F>
concept can_write_lock = requires(W &w, F &&f) { w.write_lock(std::forward<F>(f)); };

TEST(MaboroutuMutexWrapper, Types) {
   static_assert(std::is_same_v<mutex_wrapper<int, mutex>::value_type, int>);
   static_assert(std::is_same_v<mutex_wrapper<int, mutex>::mutex_type, mutex>);
   static_assert(
       std::is_same_v<mutex_wrapper<int, shared_mutex>::mutex_type, shared_mutex>);
   static_assert(
       std::is_same_v<mutex_wrapper<int, null_mutex>::mutex_type, null_mutex>);
}

// 既定構築では保持値が値初期化される。
TEST(MaboroutuMutexWrapper, DefaultConstructedValueIsZero) {
   mutex_wrapper<int, mutex> w;
   EXPECT_EQ(w.read_lock([](int const &v) { return v; }), 0);
}

TEST(MaboroutuMutexWrapper, ConstructWithArguments) {
   mutex_wrapper<int, mutex> a(7);
   EXPECT_EQ(a.read_lock([](int const &v) { return v; }), 7);
   struct pair_t {
      pair_t(int a, double b) : a(a), b(b) {}
      int a;
      double b;
   };
   mutex_wrapper<pair_t, mutex> b(1, 2.5);
   EXPECT_EQ(b.read_lock([](pair_t const &v) { return v.a; }), 1);
   EXPECT_EQ(b.read_lock([](pair_t const &v) { return v.b; }), 2.5);
}

TEST(MaboroutuMutexWrapper, ConstructWithSameTypeArguments) {
   mutex_wrapper<std::vector<int>, mutex> w(3, 1);
   EXPECT_EQ(w.read_lock([](auto const &v) { return v.size(); }), 3u);
}

TEST(MaboroutuMutexWrapper, CopyAndMoveConstruct) {
   mutex_wrapper<int, null_mutex> a(4);
   mutex_wrapper<int, null_mutex> b(std::move(a));
   EXPECT_EQ(b.read_lock([](int const &v) { return v; }), 4);
}

TEST(MaboroutuMutexWrapper, WriteAndRead) {
   mutex_wrapper<int, mutex> w;
   w.write_lock([](int &v) { v = 5; });
   EXPECT_EQ(w.read_lock([](int const &v) { return v; }), 5);
   EXPECT_EQ(w.write_lock([](int &v) { return ++v; }), 6);
}

TEST(MaboroutuMutexWrapper, NullMutex) {
   mutex_wrapper<int, null_mutex> w;
   w.write_lock([](int &v) { v = 3; });
   EXPECT_EQ(w.read_lock([](int const &v) { return v; }), 3);
}

// read_lock は常に const の参照を渡す（非constのラッパーでも）。
TEST(MaboroutuMutexWrapper, ReadLockPassesConstReference) {
   mutex_wrapper<int, shared_mutex> w(1);
   int const &r = w.read_lock([](auto &&v) -> decltype(auto) { return v; });
   EXPECT_EQ(r, 1);
   static_assert(
       std::is_same_v<decltype(w.read_lock([](auto &&v) -> decltype(auto) { return v; })),
                      int const &>);
}

TEST(MaboroutuMutexWrapper, WriteLockPassesMutableReference) {
   mutex_wrapper<int, mutex> w(1);
   int &r = w.write_lock([](int &v) -> int & { return v; });
   r = 9;
   EXPECT_EQ(w.read_lock([](int const &v) { return v; }), 9);
}

// const のラッパーでは read_lock のみ使える。
TEST(MaboroutuMutexWrapper, ConstWrapper) {
   mutex_wrapper<int, shared_mutex> const w(5);
   EXPECT_EQ(w.read_lock([](int const &v) { return v; }), 5);
   using W = mutex_wrapper<int, shared_mutex> const;
   static_assert(can_read_lock<W, int (*)(int const &)>);
   static_assert(!can_write_lock<W, int (*)(int &)>);
}

// 引数に std::invoke 規則を使う（メンバポインタ）。
TEST(MaboroutuMutexWrapper, MemberPointer) {
   struct S {
      int a = 3;
   };
   mutex_wrapper<S, mutex> w;
   EXPECT_EQ(w.read_lock(&S::a), 3);
   w.write_lock([](S &s) { s.a = 4; });
   EXPECT_EQ(w.read_lock(&S::a), 4);
}

TEST(MaboroutuMutexWrapper, NoexceptPropagation) {
   mutex_wrapper<int, mutex> w;
   auto rf = [](int const &) noexcept {};
   auto rg = [](int const &) {};
   auto wf = [](int &) noexcept {};
   auto wg = [](int &) {};
   static_assert(noexcept(w.read_lock(rf)));
   static_assert(!noexcept(w.read_lock(rg)));
   static_assert(noexcept(w.write_lock(wf)));
   static_assert(!noexcept(w.write_lock(wg)));
}

// 右辺値・左辺値の関数オブジェクトの別が保たれる。
struct category_probe {
   int operator()(int &) & { return 1; }
   int operator()(int &) && { return 2; }
   int operator()(int const &) & { return 3; }
   int operator()(int const &) && { return 4; }
};

TEST(MaboroutuMutexWrapper, ForwardsCallableValueCategory) {
   mutex_wrapper<int, mutex> w;
   category_probe f;
   EXPECT_EQ(w.write_lock(f), 1);
   EXPECT_EQ(w.write_lock(std::move(f)), 2);
   EXPECT_EQ(w.read_lock(f), 3);
   EXPECT_EQ(w.read_lock(std::move(f)), 4);
}

template <class M> void counts_exactly() {
   mutex_wrapper<long, M> w;
   constexpr int threads = 4;
   constexpr int loops = 5000;
   std::vector<std::thread> ts;
   for (int i = 0; i < threads; ++i) {
      ts.emplace_back([&] {
         for (int j = 0; j < loops; ++j) {
            w.write_lock([](long &v) { ++v; });
         }
      });
   }
   for (auto &t : ts) {
      t.join();
   }
   EXPECT_EQ(w.read_lock([](long const &v) { return v; }), threads * loops);
}

TEST(MaboroutuMutexWrapper, ConcurrentWriteMutex) { counts_exactly<mutex>(); }
TEST(MaboroutuMutexWrapper, ConcurrentWriteSharedMutex) {
   counts_exactly<shared_mutex>();
}

TEST(MaboroutuMutexWrapper, WriteIsExclusive) {
   mutex_wrapper<int, shared_mutex> w;
   std::atomic<int> inside{0};
   std::atomic<int> max_inside{0};
   std::vector<std::thread> ts;
   for (int i = 0; i < 4; ++i) {
      ts.emplace_back([&] {
         for (int j = 0; j < 200; ++j) {
            w.write_lock([&](int &) {
               int n = ++inside;
               int m = max_inside.load();
               while (n > m && !max_inside.compare_exchange_weak(m, n)) {
               }
               std::this_thread::yield();
               --inside;
            });
         }
      });
   }
   for (auto &t : ts) {
      t.join();
   }
   EXPECT_EQ(max_inside.load(), 1);
}

// read_lock は shared_mutex で同時に保持できる（2スレッドが同時にロック内へ入る）。
TEST(MaboroutuMutexWrapper, ReadIsShared) {
   mutex_wrapper<int, shared_mutex> w;
   std::mutex m;
   std::condition_variable cv;
   int inside = 0;
   int satisfied = 0;
   auto reader = [&] {
      w.read_lock([&](int const &) {
         std::unique_lock lk(m);
         ++inside;
         cv.notify_all();
         if (cv.wait_for(lk, std::chrono::seconds(5),
                         [&] { return inside == 2; })) {
            ++satisfied;
         }
      });
   };
   std::thread a(reader);
   std::thread b(reader);
   a.join();
   b.join();
   EXPECT_EQ(satisfied, 2);
}

// 書き込みロックは読み取りロックと排他になる。
TEST(MaboroutuMutexWrapper, WriteExcludesRead) {
   mutex_wrapper<int, shared_mutex> w;
   std::atomic<bool> writing{false};
   std::atomic<bool> violated{false};
   std::atomic<bool> stop{false};
   std::thread reader([&] {
      while (!stop) {
         w.read_lock([&](int const &) {
            if (writing) {
               violated = true;
            }
         });
      }
   });
   for (int i = 0; i < 2000; ++i) {
      w.write_lock([&](int &) {
         writing = true;
         std::this_thread::yield();
         writing = false;
      });
   }
   stop = true;
   reader.join();
   EXPECT_FALSE(violated);
}

} // namespace
