// maboroutu.mutex_wrapper のテスト。
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

import maboroutu.mutex_wrapper;

namespace {

using maboroutu::mutex_wrapper;
using maboroutu::null_mutex;

TEST(MaboroutuMutexWrapper, Types) {
   using W = mutex_wrapper<int>;
   static_assert(std::is_same_v<W::value_type, int>);
   static_assert(std::is_same_v<W::mutex_type, maboroutu::mutex>);
   static_assert(std::is_same_v<mutex_wrapper<int, maboroutu::shared_mutex>::mutex_type,
                                maboroutu::shared_mutex>);
}

// 既定構築では保持値が値初期化される。
TEST(MaboroutuMutexWrapper, DefaultConstructedValueIsZero) {
   mutex_wrapper<int> w;
   EXPECT_EQ(w.lock([](int const &v) { return v; }), 0);
}

// コンストラクタ引数で保持値を初期化できる。
TEST(MaboroutuMutexWrapper, ConstructWithArguments) {
   mutex_wrapper<int> a(7);
   EXPECT_EQ(a.lock([](int const &v) { return v; }), 7);
   struct pair_t {
      pair_t(int a, double b) : a(a), b(b) {}
      int a;
      double b;
   };
   mutex_wrapper<pair_t> b(1, 2.5);
   EXPECT_EQ(b.lock([](pair_t const &v) { return v.a; }), 1);
   EXPECT_EQ(b.lock([](pair_t const &v) { return v.b; }), 2.5);
}

// 非const左辺値からのコピーがコンストラクタテンプレートに奪われない。
TEST(MaboroutuMutexWrapper, MoveConstructible) {
   mutex_wrapper<int, null_mutex> a(4);
   mutex_wrapper<int, null_mutex> b(std::move(a));
   EXPECT_EQ(b.lock([](int const &v) { return v; }), 4);
}

// 同一型の引数も保持値のコンストラクタへ転送される。
TEST(MaboroutuMutexWrapper, ConstructWithSameTypeArguments) {
   mutex_wrapper<std::vector<int>> w(3, 1);
   EXPECT_EQ(w.lock([](auto const &v) { return v.size(); }), 3u);
}

TEST(MaboroutuMutexWrapper, CopyFromNonConstLvalue) {
   mutex_wrapper<int, null_mutex> a(4);
   mutex_wrapper<int, null_mutex> b(a);
   EXPECT_EQ(b.lock([](int const &v) { return v; }), 4);
}

TEST(MaboroutuMutexWrapper, WriteAndRead) {
   mutex_wrapper<int> w;
   w.lock([](int &v) { v = 5; });
   EXPECT_EQ(w.lock([](int const &v) { return v; }), 5);
   EXPECT_EQ(w.lock([](int &v) { return ++v; }), 6);
}

TEST(MaboroutuMutexWrapper, ReturnsReferenceAsIs) {
   mutex_wrapper<int> w;
   int &r = w.lock([](int &v) -> int & { return v; });
   r = 9;
   EXPECT_EQ(w.lock([](int const &v) { return v; }), 9);
}

TEST(MaboroutuMutexWrapper, NoexceptPropagation) {
   mutex_wrapper<int> w;
   auto f = [](int &) noexcept {};
   auto g = [](int &) {};
   static_assert(noexcept(w.lock(f)));
   static_assert(!noexcept(w.lock(g)));
}

TEST(MaboroutuMutexWrapper, NullMutex) {
   mutex_wrapper<int, null_mutex> w;
   w.lock([](int &v) { v = 3; });
   EXPECT_EQ(w.lock([](int const &v) { return v; }), 3);
}

template <class M> void counts_exactly() {
   mutex_wrapper<long, M> w;
   constexpr int threads = 4;
   constexpr int loops = 5000;
   std::vector<std::thread> ts;
   for (int i = 0; i < threads; ++i) {
      ts.emplace_back([&] {
         for (int j = 0; j < loops; ++j) {
            w.lock([](long &v) { ++v; });
         }
      });
   }
   for (auto &t : ts) {
      t.join();
   }
   EXPECT_EQ(w.lock([](long const &v) { return v; }), threads * loops);
}

TEST(MaboroutuMutexWrapper, ConcurrentWriteMutex) {
   counts_exactly<maboroutu::mutex>();
}
TEST(MaboroutuMutexWrapper, ConcurrentWriteSharedMutex) {
   counts_exactly<maboroutu::shared_mutex>();
}

// 書き込み用ロック（非const引数）は排他になる。
TEST(MaboroutuMutexWrapper, WriteIsExclusive) {
   mutex_wrapper<int, maboroutu::shared_mutex> w;
   std::atomic<int> inside{0};
   std::atomic<int> max_inside{0};
   std::vector<std::thread> ts;
   for (int i = 0; i < 4; ++i) {
      ts.emplace_back([&] {
         for (int j = 0; j < 200; ++j) {
            w.lock([&](int &) {
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

// 読み取り用ロック（const引数）はshared_mutexで同時に保持できる。
// 2スレッドが同時にロック内へ入れなければ（排他ロックなら）待ちが解消せず失敗する。
TEST(MaboroutuMutexWrapper, ReadIsShared) {
   mutex_wrapper<int, maboroutu::shared_mutex> w;
   std::mutex m;
   std::condition_variable cv;
   int inside = 0;
   bool both = false;
   auto reader = [&] {
      w.lock([&](int const &) {
         std::unique_lock lk(m);
         ++inside;
         cv.notify_all();
         both = cv.wait_for(lk, std::chrono::seconds(5),
                            [&] { return inside == 2; });
      });
   };
   std::thread a(reader);
   std::thread b(reader);
   a.join();
   b.join();
   EXPECT_TRUE(both);
}

} // namespace
