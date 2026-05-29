#include <iostream>
#include <set>
#include <vector>

#include <itercpp/itercpp.hpp>

int main() {
  std::vector<int> nums{1, 2, 3, 4, 5};

  auto out = itercpp::iter(nums).enumerate().map(
      [](auto p) { return p.first * p.second; });

  for (auto v : out) {
    std::cout << v << ' ';
  }

  auto out2 = itercpp::iter(out).take(4).collect<std::set<int>>();

  std::cout << '\n';
}
