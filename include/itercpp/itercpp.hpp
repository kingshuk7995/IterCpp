#pragma once

#include <cstddef>
#include <functional>
#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace itercpp {
namespace detail {

template <class T> using begin_t = decltype(std::begin(std::declval<T &>()));
template <class T> using end_t = decltype(std::end(std::declval<T &>()));
template <class T>
using range_reference_t = decltype(*std::declval<T &>().begin());
template <class T>
using range_value_t = std::remove_cvref_t<range_reference_t<T>>;

template <class T>
concept iterable = requires(T t) {
  std::begin(t);
  std::end(t);
};

template <class T>
concept sized = requires(const T &t) {
  { std::size(t) } -> std::convertible_to<std::size_t>;
};

template <class F, class... Args>
concept invocable = requires(F f, Args &&...args) {
  std::invoke(f, std::forward<Args>(args)...);
};

template <class T> struct materialize {
  using type = std::remove_cvref_t<T>;
};
template <class A, class B> struct materialize<std::pair<A, B>> {
  using type =
      std::pair<typename materialize<A>::type, typename materialize<B>::type>;
};
template <class T> using materialize_t = typename materialize<T>::type;

template <class C, class V>
concept has_push_back =
    requires(C &c, V &&v) { c.push_back(std::forward<V>(v)); };
template <class C, class V>
concept has_emplace_back =
    requires(C &c, V &&v) { c.emplace_back(std::forward<V>(v)); };
template <class C, class V>
concept has_insert = requires(C &c, V &&v) { c.insert(std::forward<V>(v)); };
template <class C, class V>
concept has_emplace = requires(C &c, V &&v) { c.emplace(std::forward<V>(v)); };
template <class C>
concept has_reserve = requires(C &c, std::size_t n) { c.reserve(n); };

template <class Container, class T>
constexpr void append_one(Container &c, T &&value) {
  if constexpr (has_push_back<Container, T>) {
    c.push_back(std::forward<T>(value));
  } else if constexpr (has_emplace_back<Container, T>) {
    c.emplace_back(std::forward<T>(value));
  } else if constexpr (has_insert<Container, T>) {
    c.insert(std::forward<T>(value));
  } else if constexpr (has_emplace<Container, T>) {
    c.emplace(std::forward<T>(value));
  } else {
    static_assert(false, "itercpp::collect: container does not support append");
  }
}

template <class R>
using iter_storage_t =
    std::conditional_t<std::is_lvalue_reference_v<R>,
                       std::remove_reference_t<R> *, std::remove_cvref_t<R>>;

template <class R> constexpr auto make_iter_storage(R &&r) {
  using raw_t = std::remove_reference_t<R>;
  if constexpr (std::is_lvalue_reference_v<R>) {
    return static_cast<raw_t *>(&r);
  } else {
    return raw_t(std::forward<R>(r));
  }
}

} // namespace detail

template <class Storage> class iter_view;
template <class Parent, class Func> class map_view;
template <class Parent, class Pred> class filter_view;
template <class Parent> class take_view;
template <class Parent> class skip_view;
template <class Parent, class Pred> class take_while_view;
template <class Parent, class Pred> class skip_while_view;
template <class Parent> class enumerate_view;
template <class Parent> class step_by_view;
template <class L, class R> class zip_view;
template <class L, class R> class chain_view;

/// @brief Creates an iterator view from a given iterable container.
template <class R> constexpr auto iter(R &&r);

template <class Derived> class iterator_trait {
protected:
  constexpr Derived &derived() noexcept {
    return static_cast<Derived &>(*this);
  }
  constexpr const Derived &derived() const noexcept {
    return static_cast<const Derived &>(*this);
  }

public:
  /// @brief Transforms each element by applying the provided function.
  template <class F> constexpr auto map(F &&f) && {
    return map_view<Derived, std::decay_t<F>>(std::move(derived()),
                                              std::forward<F>(f));
  }

  /// @brief Yields only the elements for which the predicate returns true.
  template <class Pred> constexpr auto filter(Pred &&pred) && {
    return filter_view<Derived, std::decay_t<Pred>>(std::move(derived()),
                                                    std::forward<Pred>(pred));
  }

  /// @brief Yields the first n elements.
  constexpr auto take(std::size_t n) && {
    return take_view<Derived>(std::move(derived()), n);
  }

  /// @brief Skips the first n elements.
  constexpr auto skip(std::size_t n) && {
    return skip_view<Derived>(std::move(derived()), n);
  }

  /// @brief Yields elements in steps of the given size.
  constexpr auto step_by(std::size_t step) && {
    return step_by_view<Derived>(std::move(derived()), step);
  }

  /// @brief Yields elements continuously while the predicate returns true.
  template <class Pred> constexpr auto take_while(Pred &&pred) && {
    return take_while_view<Derived, std::decay_t<Pred>>(
        std::move(derived()), std::forward<Pred>(pred));
  }

  /// @brief Skips elements continuously while the predicate returns true.
  template <class Pred> constexpr auto skip_while(Pred &&pred) && {
    return skip_while_view<Derived, std::decay_t<Pred>>(
        std::move(derived()), std::forward<Pred>(pred));
  }

  /// @brief Yields pairs of (index, element) starting from a given index.
  constexpr auto enumerate(std::size_t start = 0) && {
    return enumerate_view<Derived>(std::move(derived()), start);
  }

  /// @brief Combines two iterators into a single iterator of pairs.
  template <class Other> constexpr auto zip(Other &&other) && {
    return zip_view<Derived, decltype(iter(std::forward<Other>(other)))>(
        std::move(derived()), iter(std::forward<Other>(other)));
  }

  /// @brief Chains two iterators together to yield elements sequentially.
  template <class Other> constexpr auto chain(Other &&other) && {
    return chain_view<Derived, decltype(iter(std::forward<Other>(other)))>(
        std::move(derived()), iter(std::forward<Other>(other)));
  }

  /// @brief Consumes the iterator and collects elements into a container.
  template <class Container = void> constexpr auto collect() && {
    using actual_container_t = std::conditional_t<
        std::is_void_v<Container>,
        std::vector<detail::materialize_t<detail::range_reference_t<Derived>>>,
        Container>;
    actual_container_t out{};
    if constexpr (detail::sized<Derived> &&
                  detail::has_reserve<actual_container_t>) {
      out.reserve(static_cast<std::size_t>(std::size(derived())));
    }
    for (auto &&elem : derived()) {
      detail::append_one(out, std::forward<decltype(elem)>(elem));
    }
    return out;
  }

  /// @brief Folds every element into an accumulator by applying an operation.
  template <class Init, class Op> constexpr Init fold(Init init, Op &&op) && {
    auto acc = std::move(init);
    for (auto &&elem : derived()) {
      acc = std::invoke(op, std::move(acc), std::forward<decltype(elem)>(elem));
    }
    return acc;
  }

  /// @brief Reduces elements to a single value by applying an operation.
  template <class Op> constexpr auto reduce(Op &&op) && {
    using result_t = detail::materialize_t<detail::range_reference_t<Derived>>;
    std::optional<result_t> acc;
    for (auto &&elem : derived()) {
      if (!acc) {
        acc = std::forward<decltype(elem)>(elem);
      } else {
        acc = std::invoke(op, std::move(*acc),
                          std::forward<decltype(elem)>(elem));
      }
    }
    return acc;
  }

  /// @brief Finds the first element satisfying a predicate.
  template <class Pred> constexpr auto find(Pred &&pred) && {
    using result_t = detail::materialize_t<detail::range_reference_t<Derived>>;
    for (auto &&elem : derived()) {
      if (std::invoke(pred, elem)) {
        return std::optional<result_t>(std::forward<decltype(elem)>(elem));
      }
    }
    return std::optional<result_t>{};
  }

  /// @brief Returns the index of the first element satisfying a predicate.
  template <class Pred>
  constexpr std::optional<std::size_t> position(Pred &&pred) && {
    std::size_t idx = 0;
    for (auto &&elem : derived()) {
      if (std::invoke(pred, elem))
        return idx;
      ++idx;
    }
    return std::nullopt;
  }

  /// @brief Consumes the iterator and returns the total number of elements.
  constexpr std::size_t count() && {
    std::size_t n = 0;
    for ([[maybe_unused]] auto &&elem : derived()) {
      ++n;
    }
    return n;
  }

  /// @brief Tests whether any element matches the predicate.
  template <class Pred> constexpr bool any(Pred &&pred) && {
    for (auto &&elem : derived()) {
      if (std::invoke(pred, elem))
        return true;
    }
    return false;
  }

  /// @brief Tests whether all elements match the predicate.
  template <class Pred> constexpr bool all(Pred &&pred) && {
    for (auto &&elem : derived()) {
      if (!std::invoke(pred, elem))
        return false;
    }
    return true;
  }

  /// @brief Consumes the iterator by calling a closure on each element.
  template <class F> constexpr void for_each(F &&f) && {
    for (auto &&elem : derived()) {
      std::invoke(f, std::forward<decltype(elem)>(elem));
    }
  }
};

template <class Storage>
class iter_view : public iterator_trait<iter_view<Storage>> {
  Storage base_;

public:
  iter_view() = default;
  constexpr explicit iter_view(Storage base) : base_(std::move(base)) {}
  constexpr auto begin() {
    if constexpr (std::is_pointer_v<Storage>)
      return std::begin(*base_);
    else
      return std::begin(base_);
  }
  constexpr auto end() {
    if constexpr (std::is_pointer_v<Storage>)
      return std::end(*base_);
    else
      return std::end(base_);
  }
  constexpr auto begin() const {
    if constexpr (std::is_pointer_v<Storage>)
      return std::begin(*base_);
    else
      return std::begin(base_);
  }
  constexpr auto end() const {
    if constexpr (std::is_pointer_v<Storage>)
      return std::end(*base_);
    else
      return std::end(base_);
  }
};

template <class Parent, class Func>
class map_view : public iterator_trait<map_view<Parent, Func>> {
  Parent parent_;
  [[no_unique_address]] Func func_;

public:
  constexpr map_view(Parent parent, Func func)
      : parent_(std::move(parent)), func_(std::move(func)) {}
  class iterator {
    using parent_iter_t = decltype(std::declval<Parent &>().begin());
    parent_iter_t it_{};
    const Func *func_{};

  public:
    using difference_type = std::ptrdiff_t;
    using parent_category =
        typename std::iterator_traits<parent_iter_t>::iterator_category;
    using iterator_category = std::conditional_t<
        std::is_base_of_v<std::random_access_iterator_tag, parent_category>,
        std::random_access_iterator_tag, parent_category>;
    constexpr iterator &operator--()
      requires std::bidirectional_iterator<parent_iter_t>
    {
      --it_;
      return *this;
    }
    constexpr iterator operator--(int)
      requires std::bidirectional_iterator<parent_iter_t>
    {
      iterator tmp = *this;
      --(*this);
      return tmp;
    }
    constexpr iterator &operator+=(difference_type n)
      requires std::random_access_iterator<parent_iter_t>
    {
      it_ += n;
      return *this;
    }
    constexpr iterator operator+(difference_type n) const
      requires std::random_access_iterator<parent_iter_t>
    {
      iterator tmp = *this;
      tmp += n;
      return tmp;
    }
    friend constexpr iterator operator+(difference_type n, const iterator &i)
      requires std::random_access_iterator<parent_iter_t>
    {
      return i + n;
    }
    constexpr iterator &operator-=(difference_type n)
      requires std::random_access_iterator<parent_iter_t>
    {
      it_ -= n;
      return *this;
    }
    constexpr iterator operator-(difference_type n) const
      requires std::random_access_iterator<parent_iter_t>
    {
      iterator tmp = *this;
      tmp -= n;
      return tmp;
    }
    constexpr difference_type operator-(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ - other.it_;
    }
    constexpr auto operator[](difference_type n) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return (*func_)(it_[n]);
    }
    constexpr bool operator<(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ < other.it_;
    }
    constexpr bool operator>(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ > other.it_;
    }
    constexpr bool operator<=(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ <= other.it_;
    }
    constexpr bool operator>=(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ >= other.it_;
    }
    using reference =
        decltype(std::declval<Func>()(*std::declval<parent_iter_t>()));
    using value_type = std::remove_cvref_t<reference>;
    using pointer = void;

    iterator() = default;
    constexpr iterator(parent_iter_t it, const Func &func)
        : it_(std::move(it)), func_(&func) {}
    constexpr iterator &operator++() {
      ++it_;
      return *this;
    }
    constexpr iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }
    constexpr bool operator==(const iterator &other) const {
      return it_ == other.it_;
    }
    constexpr bool operator!=(const iterator &other) const {
      return it_ != other.it_;
    }
    constexpr decltype(auto) operator*() const { return (*func_)(*it_); }
  };
  constexpr iterator begin() { return iterator(parent_.begin(), func_); }
  constexpr iterator end() { return iterator(parent_.end(), func_); }
};

template <class Parent, class Pred>
class filter_view : public iterator_trait<filter_view<Parent, Pred>> {
  Parent parent_;
  [[no_unique_address]] Pred pred_;

public:
  constexpr filter_view(Parent parent, Pred pred)
      : parent_(std::move(parent)), pred_(std::move(pred)) {}
  class iterator {
    using parent_iter_t = decltype(std::declval<Parent &>().begin());
    parent_iter_t it_{};
    parent_iter_t end_{};
    const Pred *pred_{};
    constexpr void satisfy() {
      while (it_ != end_ && !std::invoke(*pred_, *it_))
        ++it_;
    }

  public:
    using difference_type = std::ptrdiff_t;
    using parent_category =
        typename std::iterator_traits<parent_iter_t>::iterator_category;
    using iterator_category = std::conditional_t<
        std::is_base_of_v<std::forward_iterator_tag, parent_category>,
        std::forward_iterator_tag, std::input_iterator_tag>;
    using reference = decltype(*std::declval<parent_iter_t>());
    using value_type = std::remove_cvref_t<reference>;
    using pointer = void;

    iterator() = default;
    constexpr iterator(parent_iter_t it, parent_iter_t end, const Pred &pred)
        : it_(std::move(it)), end_(std::move(end)), pred_(&pred) {
      satisfy();
    }
    constexpr iterator &operator++() {
      ++it_;
      satisfy();
      return *this;
    }
    constexpr iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }
    constexpr bool operator==(const iterator &other) const {
      return it_ == other.it_;
    }
    constexpr bool operator!=(const iterator &other) const {
      return it_ != other.it_;
    }
    constexpr decltype(auto) operator*() const { return *it_; }
  };
  constexpr iterator begin() {
    return iterator(parent_.begin(), parent_.end(), pred_);
  }
  constexpr iterator end() {
    return iterator(parent_.end(), parent_.end(), pred_);
  }
};

template <class Parent>
class take_view : public iterator_trait<take_view<Parent>> {
  Parent parent_;
  std::size_t limit_{};

public:
  constexpr take_view(Parent parent, std::size_t limit)
      : parent_(std::move(parent)), limit_(limit) {}
  class iterator {
    using parent_iter_t = decltype(std::declval<Parent &>().begin());
    parent_iter_t it_{};
    parent_iter_t end_{};
    std::size_t remaining_{};

  public:
    using difference_type = std::ptrdiff_t;
    using parent_category =
        typename std::iterator_traits<parent_iter_t>::iterator_category;
    using iterator_category = std::conditional_t<
        std::is_base_of_v<std::random_access_iterator_tag, parent_category>,
        std::random_access_iterator_tag, parent_category>;
    constexpr iterator &operator--()
      requires std::bidirectional_iterator<parent_iter_t>
    {
      --it_;
      ++remaining_;
      return *this;
    }
    constexpr iterator operator--(int)
      requires std::bidirectional_iterator<parent_iter_t>
    {
      iterator tmp = *this;
      --(*this);
      return tmp;
    }
    constexpr iterator &operator+=(difference_type n)
      requires std::random_access_iterator<parent_iter_t>
    {
      it_ += n;
      remaining_ -= n;
      return *this;
    }
    constexpr iterator operator+(difference_type n) const
      requires std::random_access_iterator<parent_iter_t>
    {
      iterator tmp = *this;
      tmp += n;
      return tmp;
    }
    friend constexpr iterator operator+(difference_type n, const iterator &i)
      requires std::random_access_iterator<parent_iter_t>
    {
      return i + n;
    }
    constexpr iterator &operator-=(difference_type n)
      requires std::random_access_iterator<parent_iter_t>
    {
      it_ -= n;
      remaining_ += n;
      return *this;
    }
    constexpr iterator operator-(difference_type n) const
      requires std::random_access_iterator<parent_iter_t>
    {
      iterator tmp = *this;
      tmp -= n;
      return tmp;
    }
    constexpr difference_type operator-(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ - other.it_;
    }
    constexpr auto operator[](difference_type n) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_[n];
    }
    constexpr bool operator<(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ < other.it_;
    }
    constexpr bool operator>(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ > other.it_;
    }
    constexpr bool operator<=(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ <= other.it_;
    }
    constexpr bool operator>=(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ >= other.it_;
    }
    using reference = decltype(*std::declval<parent_iter_t>());
    using value_type = std::remove_cvref_t<reference>;
    using pointer = void;

    iterator() = default;
    constexpr iterator(parent_iter_t it, parent_iter_t end, std::size_t n)
        : it_(std::move(it)), end_(std::move(end)), remaining_(n) {}
    constexpr iterator &operator++() {
      ++it_;
      if (remaining_ != 0)
        --remaining_;
      return *this;
    }
    constexpr iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }
    constexpr bool operator==(const iterator &other) const {
      bool this_done = (remaining_ == 0 || it_ == end_);
      bool other_done = (other.remaining_ == 0 || other.it_ == other.end_);
      if (this_done || other_done)
        return this_done == other_done;
      return it_ == other.it_;
    }
    constexpr bool operator!=(const iterator &other) const {
      return !(*this == other);
    }
    constexpr decltype(auto) operator*() const { return *it_; }
  };
  constexpr iterator begin() {
    return iterator(parent_.begin(), parent_.end(), limit_);
  }
  constexpr iterator end() { return iterator(parent_.end(), parent_.end(), 0); }
};

template <class Parent>
class skip_view : public iterator_trait<skip_view<Parent>> {
  Parent parent_;
  std::size_t n_{};

public:
  constexpr skip_view(Parent parent, std::size_t n)
      : parent_(std::move(parent)), n_(n) {}
  class iterator {
    using parent_iter_t = decltype(std::declval<Parent &>().begin());
    parent_iter_t it_{};
    parent_iter_t end_{};
    constexpr void skip_prefix(std::size_t n) {
      while (n != 0 && it_ != end_) {
        ++it_;
        --n;
      }
    }

  public:
    using difference_type = std::ptrdiff_t;
    using parent_category =
        typename std::iterator_traits<parent_iter_t>::iterator_category;
    using iterator_category = std::conditional_t<
        std::is_base_of_v<std::random_access_iterator_tag, parent_category>,
        std::random_access_iterator_tag, parent_category>;
    constexpr iterator &operator--()
      requires std::bidirectional_iterator<parent_iter_t>
    {
      --it_;
      return *this;
    }
    constexpr iterator operator--(int)
      requires std::bidirectional_iterator<parent_iter_t>
    {
      iterator tmp = *this;
      --(*this);
      return tmp;
    }
    constexpr iterator &operator+=(difference_type n)
      requires std::random_access_iterator<parent_iter_t>
    {
      it_ += n;
      return *this;
    }
    constexpr iterator operator+(difference_type n) const
      requires std::random_access_iterator<parent_iter_t>
    {
      iterator tmp = *this;
      tmp += n;
      return tmp;
    }
    friend constexpr iterator operator+(difference_type n, const iterator &i)
      requires std::random_access_iterator<parent_iter_t>
    {
      return i + n;
    }
    constexpr iterator &operator-=(difference_type n)
      requires std::random_access_iterator<parent_iter_t>
    {
      it_ -= n;
      return *this;
    }
    constexpr iterator operator-(difference_type n) const
      requires std::random_access_iterator<parent_iter_t>
    {
      iterator tmp = *this;
      tmp -= n;
      return tmp;
    }
    constexpr difference_type operator-(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ - other.it_;
    }
    constexpr auto operator[](difference_type n) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_[n];
    }
    constexpr bool operator<(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ < other.it_;
    }
    constexpr bool operator>(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ > other.it_;
    }
    constexpr bool operator<=(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ <= other.it_;
    }
    constexpr bool operator>=(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ >= other.it_;
    }
    using reference = decltype(*std::declval<parent_iter_t>());
    using value_type = std::remove_cvref_t<reference>;
    using pointer = void;

    iterator() = default;
    constexpr iterator(parent_iter_t it, parent_iter_t end, std::size_t n)
        : it_(std::move(it)), end_(std::move(end)) {
      skip_prefix(n);
    }
    constexpr iterator &operator++() {
      ++it_;
      return *this;
    }
    constexpr iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }
    constexpr bool operator==(const iterator &other) const {
      return it_ == other.it_;
    }
    constexpr bool operator!=(const iterator &other) const {
      return it_ != other.it_;
    }
    constexpr decltype(auto) operator*() const { return *it_; }
  };
  constexpr iterator begin() {
    return iterator(parent_.begin(), parent_.end(), n_);
  }
  constexpr iterator end() { return iterator(parent_.end(), parent_.end(), 0); }
};

template <class Parent>
class step_by_view : public iterator_trait<step_by_view<Parent>> {
  Parent parent_;
  std::size_t step_;

public:
  constexpr step_by_view(Parent parent, std::size_t step)
      : parent_(std::move(parent)), step_(step) {}
  class iterator {
    using parent_iter_t = decltype(std::declval<Parent &>().begin());
    parent_iter_t it_{};
    parent_iter_t end_{};
    std::size_t step_{};

  public:
    using difference_type = std::ptrdiff_t;
    using parent_category =
        typename std::iterator_traits<parent_iter_t>::iterator_category;
    using iterator_category = std::conditional_t<
        std::is_base_of_v<std::random_access_iterator_tag, parent_category>,
        std::random_access_iterator_tag, parent_category>;
    constexpr iterator &operator--()
      requires std::bidirectional_iterator<parent_iter_t>
    {
      for (std::size_t i = 0; i < step_; ++i)
        --it_;
      return *this;
    }
    constexpr iterator operator--(int)
      requires std::bidirectional_iterator<parent_iter_t>
    {
      iterator tmp = *this;
      --(*this);
      return tmp;
    }
    constexpr iterator &operator+=(difference_type n)
      requires std::random_access_iterator<parent_iter_t>
    {
      it_ += n * static_cast<difference_type>(step_);
      return *this;
    }
    constexpr iterator operator+(difference_type n) const
      requires std::random_access_iterator<parent_iter_t>
    {
      iterator tmp = *this;
      tmp += n;
      return tmp;
    }
    friend constexpr iterator operator+(difference_type n, const iterator &i)
      requires std::random_access_iterator<parent_iter_t>
    {
      return i + n;
    }
    constexpr iterator &operator-=(difference_type n)
      requires std::random_access_iterator<parent_iter_t>
    {
      it_ -= n * static_cast<difference_type>(step_);
      return *this;
    }
    constexpr iterator operator-(difference_type n) const
      requires std::random_access_iterator<parent_iter_t>
    {
      iterator tmp = *this;
      tmp -= n;
      return tmp;
    }
    constexpr difference_type operator-(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      difference_type diff = it_ - other.it_;
      if (diff > 0)
        return (diff + step_ - 1) / static_cast<difference_type>(step_);
      if (diff < 0)
        return (diff - step_ + 1) / static_cast<difference_type>(step_);
      return 0;
    }
    constexpr auto operator[](difference_type n) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_[n * static_cast<difference_type>(step_)];
    }
    constexpr bool operator<(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ < other.it_;
    }
    constexpr bool operator>(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ > other.it_;
    }
    constexpr bool operator<=(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ <= other.it_;
    }
    constexpr bool operator>=(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ >= other.it_;
    }
    using reference = decltype(*std::declval<parent_iter_t>());
    using value_type = std::remove_cvref_t<reference>;
    using pointer = void;

    iterator() = default;
    constexpr iterator(parent_iter_t it, parent_iter_t end, std::size_t step)
        : it_(std::move(it)), end_(std::move(end)), step_(step) {}
    constexpr iterator &operator++() {
      for (std::size_t i = 0; i < step_ && it_ != end_; ++i) {
        ++it_;
      }
      return *this;
    }
    constexpr iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }
    constexpr bool operator==(const iterator &other) const {
      return it_ == other.it_;
    }
    constexpr bool operator!=(const iterator &other) const {
      return it_ != other.it_;
    }
    constexpr decltype(auto) operator*() const { return *it_; }
  };
  constexpr iterator begin() {
    return iterator(parent_.begin(), parent_.end(), step_);
  }
  constexpr iterator end() {
    return iterator(parent_.end(), parent_.end(), step_);
  }
};

template <class Parent, class Pred>
class take_while_view : public iterator_trait<take_while_view<Parent, Pred>> {
  Parent parent_;
  Pred pred_;

public:
  constexpr take_while_view(Parent parent, Pred pred)
      : parent_(std::move(parent)), pred_(std::move(pred)) {}
  class iterator {
    using parent_iter_t = decltype(std::declval<Parent &>().begin());
    parent_iter_t it_{};
    parent_iter_t end_{};
    const Pred *pred_{};
    bool done_ = false;
    constexpr void check_current() {
      if (it_ == end_ || !std::invoke(*pred_, *it_))
        done_ = true;
    }

  public:
    using difference_type = std::ptrdiff_t;
    using parent_category =
        typename std::iterator_traits<parent_iter_t>::iterator_category;
    using iterator_category = std::conditional_t<
        std::is_base_of_v<std::forward_iterator_tag, parent_category>,
        std::forward_iterator_tag, std::input_iterator_tag>;
    using reference = decltype(*std::declval<parent_iter_t>());
    using value_type = std::remove_cvref_t<reference>;
    using pointer = void;

    iterator() = default;
    constexpr iterator(parent_iter_t it, parent_iter_t end, const Pred &pred)
        : it_(std::move(it)), end_(std::move(end)), pred_(&pred) {
      check_current();
    }
    constexpr iterator &operator++() {
      ++it_;
      check_current();
      return *this;
    }
    constexpr iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }
    constexpr bool operator==(const iterator &other) const {
      if (done_ || other.done_)
        return done_ == other.done_;
      return it_ == other.it_;
    }
    constexpr bool operator!=(const iterator &other) const {
      return !(*this == other);
    }
    constexpr decltype(auto) operator*() const { return *it_; }
  };
  constexpr iterator begin() {
    return iterator(parent_.begin(), parent_.end(), pred_);
  }
  constexpr iterator end() {
    return iterator(parent_.end(), parent_.end(), pred_);
  }
};

template <class Parent, class Pred>
class skip_while_view : public iterator_trait<skip_while_view<Parent, Pred>> {
  Parent parent_;
  Pred pred_;

public:
  constexpr skip_while_view(Parent parent, Pred pred)
      : parent_(std::move(parent)), pred_(std::move(pred)) {}
  class iterator {
    using parent_iter_t = decltype(std::declval<Parent &>().begin());
    parent_iter_t it_{};
    parent_iter_t end_{};
    constexpr void skip_prefix(const Pred &pred) {
      while (it_ != end_ && std::invoke(pred, *it_))
        ++it_;
    }

  public:
    using difference_type = std::ptrdiff_t;
    using parent_category =
        typename std::iterator_traits<parent_iter_t>::iterator_category;
    using iterator_category = std::conditional_t<
        std::is_base_of_v<std::forward_iterator_tag, parent_category>,
        std::forward_iterator_tag, std::input_iterator_tag>;
    using reference = decltype(*std::declval<parent_iter_t>());
    using value_type = std::remove_cvref_t<reference>;
    using pointer = void;

    iterator() = default;
    constexpr iterator(parent_iter_t it, parent_iter_t end, const Pred &pred)
        : it_(std::move(it)), end_(std::move(end)) {
      skip_prefix(pred);
    }
    constexpr iterator &operator++() {
      ++it_;
      return *this;
    }
    constexpr iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }
    constexpr bool operator==(const iterator &other) const {
      return it_ == other.it_;
    }
    constexpr bool operator!=(const iterator &other) const {
      return it_ != other.it_;
    }
    constexpr decltype(auto) operator*() const { return *it_; }
  };
  constexpr iterator begin() {
    return iterator(parent_.begin(), parent_.end(), pred_);
  }
  constexpr iterator end() {
    return iterator(parent_.end(), parent_.end(), pred_);
  }
};

template <class Parent>
class enumerate_view : public iterator_trait<enumerate_view<Parent>> {
  Parent parent_;
  std::size_t start_{};

public:
  constexpr enumerate_view(Parent parent, std::size_t start = 0)
      : parent_(std::move(parent)), start_(start) {}
  class iterator {
    using parent_iter_t = decltype(std::declval<Parent &>().begin());
    parent_iter_t it_{};
    parent_iter_t end_{};
    std::size_t index_{};

  public:
    using difference_type = std::ptrdiff_t;
    using parent_category =
        typename std::iterator_traits<parent_iter_t>::iterator_category;
    using iterator_category = std::conditional_t<
        std::is_base_of_v<std::random_access_iterator_tag, parent_category>,
        std::random_access_iterator_tag, parent_category>;
    constexpr iterator &operator--()
      requires std::bidirectional_iterator<parent_iter_t>
    {
      --it_;
      --index_;
      return *this;
    }
    constexpr iterator operator--(int)
      requires std::bidirectional_iterator<parent_iter_t>
    {
      iterator tmp = *this;
      --(*this);
      return tmp;
    }
    constexpr iterator &operator+=(difference_type n)
      requires std::random_access_iterator<parent_iter_t>
    {
      it_ += n;
      index_ += n;
      return *this;
    }
    constexpr iterator operator+(difference_type n) const
      requires std::random_access_iterator<parent_iter_t>
    {
      iterator tmp = *this;
      tmp += n;
      return tmp;
    }
    friend constexpr iterator operator+(difference_type n, const iterator &i)
      requires std::random_access_iterator<parent_iter_t>
    {
      return i + n;
    }
    constexpr iterator &operator-=(difference_type n)
      requires std::random_access_iterator<parent_iter_t>
    {
      it_ -= n;
      index_ -= n;
      return *this;
    }
    constexpr iterator operator-(difference_type n) const
      requires std::random_access_iterator<parent_iter_t>
    {
      iterator tmp = *this;
      tmp -= n;
      return tmp;
    }
    constexpr difference_type operator-(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ - other.it_;
    }
    constexpr auto operator[](difference_type n) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return std::pair<std::size_t, decltype(it_[n])>{index_ + n, it_[n]};
    }
    constexpr bool operator<(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ < other.it_;
    }
    constexpr bool operator>(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ > other.it_;
    }
    constexpr bool operator<=(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ <= other.it_;
    }
    constexpr bool operator>=(const iterator &other) const
      requires std::random_access_iterator<parent_iter_t>
    {
      return it_ >= other.it_;
    }
    using reference =
        std::pair<std::size_t, decltype(*std::declval<parent_iter_t>())>;
    using value_type = std::remove_cvref_t<reference>;
    using pointer = void;

    iterator() = default;
    constexpr iterator(parent_iter_t it, parent_iter_t end, std::size_t idx)
        : it_(std::move(it)), end_(std::move(end)), index_(idx) {}
    constexpr iterator &operator++() {
      ++it_;
      ++index_;
      return *this;
    }
    constexpr iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }
    constexpr bool operator==(const iterator &other) const {
      return it_ == other.it_;
    }
    constexpr bool operator!=(const iterator &other) const {
      return it_ != other.it_;
    }
    constexpr auto operator*() const {
      return std::pair<std::size_t, decltype(*it_)>{index_, *it_};
    }
  };
  constexpr iterator begin() {
    return iterator(parent_.begin(), parent_.end(), start_);
  }
  constexpr iterator end() { return iterator(parent_.end(), parent_.end(), 0); }
};

template <class L, class R>
class zip_view : public iterator_trait<zip_view<L, R>> {
  L left_;
  R right_;

public:
  constexpr zip_view(L left, R right)
      : left_(std::move(left)), right_(std::move(right)) {}
  class iterator {
    using left_iter_t = decltype(std::declval<L &>().begin());
    using right_iter_t = decltype(std::declval<R &>().begin());
    left_iter_t it_l_{};
    left_iter_t end_l_{};
    right_iter_t it_r_{};
    right_iter_t end_r_{};

  public:
    using difference_type = std::ptrdiff_t;
    using left_category =
        typename std::iterator_traits<left_iter_t>::iterator_category;
    using right_category =
        typename std::iterator_traits<right_iter_t>::iterator_category;
    using iterator_category = std::conditional_t<
        std::is_base_of_v<std::random_access_iterator_tag, left_category> &&
            std::is_base_of_v<std::random_access_iterator_tag, right_category>,
        std::random_access_iterator_tag,
        std::conditional_t<
            std::is_base_of_v<std::bidirectional_iterator_tag, left_category> &&
                std::is_base_of_v<std::bidirectional_iterator_tag,
                                  right_category>,
            std::bidirectional_iterator_tag, std::forward_iterator_tag>>;

    constexpr iterator &operator--()
      requires(std::bidirectional_iterator<left_iter_t> &&
               std::bidirectional_iterator<right_iter_t>)
    {
      --it_l_;
      --it_r_;
      return *this;
    }
    constexpr iterator operator--(int)
      requires(std::bidirectional_iterator<left_iter_t> &&
               std::bidirectional_iterator<right_iter_t>)
    {
      iterator tmp = *this;
      --(*this);
      return tmp;
    }
    constexpr iterator &operator+=(difference_type n)
      requires(std::random_access_iterator<left_iter_t> &&
               std::random_access_iterator<right_iter_t>)
    {
      it_l_ += n;
      it_r_ += n;
      return *this;
    }
    constexpr iterator operator+(difference_type n) const
      requires(std::random_access_iterator<left_iter_t> &&
               std::random_access_iterator<right_iter_t>)
    {
      iterator tmp = *this;
      tmp += n;
      return tmp;
    }
    friend constexpr iterator operator+(difference_type n, const iterator &i)
      requires(std::random_access_iterator<left_iter_t> &&
               std::random_access_iterator<right_iter_t>)
    {
      return i + n;
    }
    constexpr iterator &operator-=(difference_type n)
      requires(std::random_access_iterator<left_iter_t> &&
               std::random_access_iterator<right_iter_t>)
    {
      it_l_ -= n;
      it_r_ -= n;
      return *this;
    }
    constexpr iterator operator-(difference_type n) const
      requires(std::random_access_iterator<left_iter_t> &&
               std::random_access_iterator<right_iter_t>)
    {
      iterator tmp = *this;
      tmp -= n;
      return tmp;
    }
    constexpr difference_type operator-(const iterator &other) const
      requires(std::random_access_iterator<left_iter_t> &&
               std::random_access_iterator<right_iter_t>)
    {
      difference_type diff_l = it_l_ - other.it_l_;
      difference_type diff_r = it_r_ - other.it_r_;
      if (diff_l >= 0 && diff_r >= 0)
        return std::min(diff_l, diff_r);
      if (diff_l <= 0 && diff_r <= 0)
        return std::max(diff_l, diff_r);
      return diff_l;
    }
    constexpr auto operator[](difference_type n) const
      requires(std::random_access_iterator<left_iter_t> &&
               std::random_access_iterator<right_iter_t>)
    {
      return std::pair<decltype(it_l_[n]), decltype(it_r_[n])>{it_l_[n],
                                                               it_r_[n]};
    }
    constexpr bool operator<(const iterator &other) const
      requires(std::random_access_iterator<left_iter_t> &&
               std::random_access_iterator<right_iter_t>)
    {
      return (*this - other) < 0;
    }
    constexpr bool operator>(const iterator &other) const
      requires(std::random_access_iterator<left_iter_t> &&
               std::random_access_iterator<right_iter_t>)
    {
      return (*this - other) > 0;
    }
    constexpr bool operator<=(const iterator &other) const
      requires(std::random_access_iterator<left_iter_t> &&
               std::random_access_iterator<right_iter_t>)
    {
      return (*this - other) <= 0;
    }
    constexpr bool operator>=(const iterator &other) const
      requires(std::random_access_iterator<left_iter_t> &&
               std::random_access_iterator<right_iter_t>)
    {
      return (*this - other) >= 0;
    }
    using reference = std::pair<decltype(*std::declval<left_iter_t>()),
                                decltype(*std::declval<right_iter_t>())>;
    using value_type = std::remove_cvref_t<reference>;
    using pointer = void;

    iterator() = default;
    constexpr iterator(left_iter_t it_l, left_iter_t end_l, right_iter_t it_r,
                       right_iter_t end_r)
        : it_l_(std::move(it_l)), end_l_(std::move(end_l)),
          it_r_(std::move(it_r)), end_r_(std::move(end_r)) {}
    constexpr iterator &operator++() {
      ++it_l_;
      ++it_r_;
      return *this;
    }
    constexpr iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }
    constexpr bool operator==(const iterator &other) const {
      return it_l_ == other.it_l_ || it_r_ == other.it_r_;
    }
    constexpr bool operator!=(const iterator &other) const {
      return !(*this == other);
    }
    constexpr auto operator*() const {
      return std::pair<decltype(*it_l_), decltype(*it_r_)>{*it_l_, *it_r_};
    }
  };
  constexpr iterator begin() {
    return iterator(left_.begin(), left_.end(), right_.begin(), right_.end());
  }
  constexpr iterator end() {
    return iterator(left_.end(), left_.end(), right_.end(), right_.end());
  }
};

template <class L, class R>
class chain_view : public iterator_trait<chain_view<L, R>> {
  L left_;
  R right_;

public:
  constexpr chain_view(L left, R right)
      : left_(std::move(left)), right_(std::move(right)) {}
  class iterator {
    using left_iter_t = decltype(std::declval<L &>().begin());
    using right_iter_t = decltype(std::declval<R &>().begin());
    bool in_left_ = true;
    left_iter_t it_l_{};
    left_iter_t end_l_{};
    right_iter_t it_r_{};
    right_iter_t end_r_{};

  public:
    using difference_type = std::ptrdiff_t;
    using left_category =
        typename std::iterator_traits<left_iter_t>::iterator_category;
    using right_category =
        typename std::iterator_traits<right_iter_t>::iterator_category;
    using iterator_category = std::conditional_t<
        std::is_base_of_v<std::forward_iterator_tag, left_category> &&
            std::is_base_of_v<std::forward_iterator_tag, right_category>,
        std::forward_iterator_tag, std::input_iterator_tag>;
    using reference = decltype(*std::declval<left_iter_t>());
    using value_type = std::remove_cvref_t<reference>;
    using pointer = void;

    iterator() = default;
    constexpr iterator(left_iter_t it_l, left_iter_t end_l, right_iter_t it_r,
                       right_iter_t end_r)
        : in_left_(it_l != end_l), it_l_(std::move(it_l)),
          end_l_(std::move(end_l)), it_r_(std::move(it_r)),
          end_r_(std::move(end_r)) {
      if (!in_left_)
        in_left_ = false;
    }
    constexpr iterator &operator++() {
      if (in_left_) {
        ++it_l_;
        if (it_l_ == end_l_)
          in_left_ = false;
      } else {
        ++it_r_;
      }
      return *this;
    }
    constexpr iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }
    constexpr bool operator==(const iterator &other) const {
      if (in_left_ != other.in_left_)
        return false;
      return in_left_ ? it_l_ == other.it_l_ : it_r_ == other.it_r_;
    }
    constexpr bool operator!=(const iterator &other) const {
      return !(*this == other);
    }
    constexpr decltype(auto) operator*() const {
      return in_left_ ? *it_l_ : *it_r_;
    }
  };
  constexpr iterator begin() {
    return iterator(left_.begin(), left_.end(), right_.begin(), right_.end());
  }
  constexpr iterator end() {
    return iterator(left_.end(), left_.end(), right_.end(), right_.end());
  }
};

template <class R> constexpr auto iter(R &&r) {
  using storage_t = detail::iter_storage_t<R>;
  return iter_view<storage_t>(detail::make_iter_storage(std::forward<R>(r)));
}

} // namespace itercpp