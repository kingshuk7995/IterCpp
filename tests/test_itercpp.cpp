#include <array>
#include <cctype>
#include <deque>
#include <list>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "itercpp/itercpp.hpp"

namespace {

using itercpp::iter;

// Chains
TEST(IterCpp, VectorMapFilterCollect) {
  std::vector<int> v{1, 2, 3, 4, 5};
  auto out = iter(v)
                 .map([](int x) { return x * 2; })
                 .filter([](int x) { return x > 4; })
                 .collect();
  EXPECT_EQ(out, (std::vector<int>{6, 8, 10}));
}

TEST(IterCpp, ListEnumerateAndFold) {
  std::list<int> v{10, 20, 30};
  auto out = iter(v)
                 .enumerate()
                 .map([](const auto &p) {
                   return static_cast<int>(p.first + p.second);
                 })
                 .collect();
  EXPECT_EQ(out, (std::vector<int>{10, 21, 32}));

  auto sum = iter(v).fold(0, [](int acc, int x) { return acc + x; });
  EXPECT_EQ(sum, 60);
}

// Zipping & Chaining
TEST(IterCpp, ZipStopsAtShortest) {
  std::deque<int> a{1, 2, 3, 4};
  std::deque<char> b{'a', 'b'};
  auto zipped = iter(a).zip(b).collect();
  ASSERT_EQ(zipped.size(), 2u);
  EXPECT_EQ(zipped[1], (std::pair<int, char>{2, 'b'}));
}

TEST(IterCpp, ChainDisparateContainers) {
  std::vector<int> a{1, 2};
  std::list<int> b{3, 4};
  auto out = iter(a).chain(b).collect();
  EXPECT_EQ(out, (std::vector<int>{1, 2, 3, 4}));
}

// Interval & Boundary Adaptors
TEST(IterCpp, TakeAndSkipBoundaries) {
  std::array<int, 5> a{1, 2, 3, 4, 5};

  // Exact boundaries
  EXPECT_EQ(iter(a).take(5).count(), 5u);
  EXPECT_EQ(iter(a).take(0).count(), 0u);

  // Out of bounds skipping
  EXPECT_EQ(iter(a).skip(10).count(), 0u);

  auto prefix = iter(a).skip(1).take(2).collect();
  EXPECT_EQ(prefix, (std::vector<int>{2, 3}));
}

TEST(IterCpp, TakeWhileAndSkipWhile) {
  std::vector<int> v{1, 2, 3, 4, 5, 1, 2};

  // take_while halts correctly
  auto prefix = iter(v).take_while([](int x) { return x < 4; }).collect();
  EXPECT_EQ(prefix, (std::vector<int>{1, 2, 3}));

  // skip_while resumes correctly
  auto suffix = iter(v).skip_while([](int x) { return x < 4; }).collect();
  EXPECT_EQ(suffix, (std::vector<int>{4, 5, 1, 2}));
}

TEST(IterCpp, StepByIntervals) {
  std::vector<int> v{0, 1, 2, 3, 4, 5, 6, 7};
  auto out = iter(v).step_by(3).collect();
  EXPECT_EQ(out, (std::vector<int>{0, 3, 6}));

  // Step larger than container
  auto out2 = iter(v).step_by(10).collect();
  EXPECT_EQ(out2, (std::vector<int>{0}));
}

// Custom Container Collection
TEST(IterCpp, StringCollectAndFind) {
  std::string s = "Abc";
  auto lower = iter(s)
                   .map([](char c) {
                     return static_cast<char>(
                         std::tolower(static_cast<unsigned char>(c)));
                   })
                   .collect<std::string>();
  EXPECT_EQ(lower, "abc");

  auto found = iter(s).find([](char c) { return c == 'b'; });
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(*found, 'b');
}

TEST(IterCpp, SetCollectToList) {
  std::set<int> s{4, 2, 8, 6};
  auto out = iter(s).map([](int x) { return x + 1; }).collect<std::list<int>>();

  // Validate order and type
  std::vector<int> verifier{out.begin(), out.end()};
  EXPECT_EQ(verifier, (std::vector<int>{3, 5, 7, 9}));
}

TEST(IterCpp, MapIterationCollectPairs) {
  std::map<std::string, int> m{{"a", 1}, {"b", 2}};
  auto pairs = iter(m).collect();
  ASSERT_EQ(pairs.size(), 2u);
  EXPECT_EQ(pairs[0], (std::pair<const std::string, int>{"a", 1}));
}

// Terminal Consumers & Short-Circuiting
TEST(IterCpp, ShortCircuitConsumers) {
  std::vector<int> v{10, 20, 30, 40};

  // Position
  auto pos = iter(v).position([](int x) { return x == 30; });
  ASSERT_TRUE(pos.has_value());
  EXPECT_EQ(*pos, 2u);

  // Any & All
  EXPECT_TRUE(iter(v).any([](int x) { return x > 35; }));
  EXPECT_FALSE(iter(v).all([](int x) { return x > 15; }));
}

TEST(IterCpp, ReduceConsumer) {
  std::vector<int> v{1, 2, 3, 4};
  auto prod = iter(v).reduce([](int acc, int x) { return acc * x; });
  ASSERT_TRUE(prod.has_value());
  EXPECT_EQ(*prod, 24);

  std::vector<int> empty_vec;
  auto empty_reduce =
      iter(empty_vec).reduce([](int acc, int x) { return acc + x; });
  EXPECT_FALSE(empty_reduce.has_value());
}

// Memory & Lifetime Semantics
TEST(IterCpp, LvalueMutationViaReference) {
  std::vector<int> v{1, 2, 3};

  // Lvalue container should yield mutable references
  iter(v).for_each([](int &x) { x *= 10; });

  EXPECT_EQ(v, (std::vector<int>{10, 20, 30}));
}

TEST(IterCpp, RvalueLifetimePreservation) {
  // Prvalue vector passed directly to iter()
  // make_iter_storage should move it into iter_view, preventing dangling
  // references
  auto out = iter(std::vector<int>{5, 10, 15})
                 .map([](int x) { return x + 1; })
                 .collect();

  EXPECT_EQ(out, (std::vector<int>{6, 11, 16}));
}

TEST(IterCpp, DeepAdaptorComposition) {
  std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

  auto out = iter(v)
                 .skip(2) // 3, 4, 5, 6, 7, 8, 9, 10
                 .filter([](int x) { return x % 2 != 0; }) // 3, 5, 7, 9
                 .step_by(2)                               // 3, 7
                 .map([](int x) { return x * 10; })        // 30, 70
                 .enumerate(1)                             // (1, 30), (2, 70)
                 .fold(0, [](int acc, const auto &pair) {
                   return acc + pair.first + pair.second;
                 }); // 0 + 1 + 30 + 2 + 70

  EXPECT_EQ(out, 103);
}

TEST(IterCpp, RobustEdgeCases) {
  std::vector<int> empty;

  // Fold on empty
  auto f = iter(empty).fold(10, [](int acc, int x) { return acc + x; });
  EXPECT_EQ(f, 10);

  // Any/All on empty
  EXPECT_FALSE(iter(empty).any([](int x) { return x > 0; }));
  EXPECT_TRUE(iter(empty).all([](int x) { return x > 0; }));

  // Find and position on empty
  EXPECT_FALSE(iter(empty).find([](int) { return true; }).has_value());
  EXPECT_FALSE(iter(empty).position([](int) { return true; }).has_value());

  // Chain empty with empty
  auto chained = iter(empty).chain(empty).collect();
  EXPECT_TRUE(chained.empty());

  // Zipping empty with something
  std::vector<int> full{1, 2, 3};
  auto zipped1 = iter(empty).zip(full).collect();
  EXPECT_TRUE(zipped1.empty());

  auto zipped2 = iter(full).zip(empty).collect();
  EXPECT_TRUE(zipped2.empty());

  // Taking more than size
  EXPECT_EQ(iter(full).take(10).count(), 3u);

  // Skipping more than size
  EXPECT_EQ(iter(full).skip(10).count(), 0u);

  // Iterator standard compatibility traits
  auto it = iter(full);
  auto b = it.begin();
  auto e = it.end();
  EXPECT_TRUE(b != e);
  EXPECT_TRUE(b == b);
  auto b_copy = b;
  b_copy++; // post-increment test
  EXPECT_TRUE(b_copy != b);

  // Take-while that never starts
  auto tw = iter(full).take_while([](int) { return false; }).collect();
  EXPECT_TRUE(tw.empty());

  // Skip-while that skips everything
  auto sw = iter(full).skip_while([](int) { return true; }).collect();
  EXPECT_TRUE(sw.empty());
}

} // namespace
