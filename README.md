# IterCpp

A single file C++20/23 compatible lazy iterator library implementing Rust-style iterators for a FP oriented paradigm.

**Note:** This does not use `std::ranges` or range-v3, though it implements many range-like functionalities from scratch.

I have used CRTP and expression-template-inspired techniques to build the lazy evaluation iterators.

**Note:** I initially considered implementing runtime-checked affine types to implement Rust like borrow checking rules but discarded the idea because I wanted to preserve C++'s zero-cost abstraction philosophy.

# How to use

As I have used C++20 concepts so the project needs to be **C++20** compatible.

It is a single file library just include this [header](/include/itercpp/itercpp.hpp)

For running the tests use CMake and vcpkg(for google tests).

# Docs

Examples available [here](/examples/)

## Creating an Iterator

```cpp
auto view = itercpp::iter(container);
```

Works with any iterable type supporting `std::begin()` and `std::end()`.

---

## Adapter Operations

All adapters are lazy and return a new iterator pipeline.

### `map`

```cpp
.map(func)
```

Transform each element.

### `filter`

```cpp
.filter(pred)
```

Keep elements satisfying a predicate.

### `take`

```cpp
.take(n)
```

Take the first `n` elements.

### `skip`

```cpp
.skip(n)
```

Skip the first `n` elements.

### `step_by`

```cpp
.step_by(step)
```

Yield every `step`-th element.

### `take_while`

```cpp
.take_while(pred)
```

Yield elements while predicate remains true.

### `skip_while`

```cpp
.skip_while(pred)
```

Skip elements while predicate remains true.

### `enumerate`

```cpp
.enumerate(start = 0)
```

Yield `(index, value)` pairs.

### `zip`

```cpp
.zip(other)
```

Combine two iterators into pairs.

### `chain`

```cpp
.chain(other)
```

Concatenate two iterators.

---

## Consumers

Consumers terminate the pipeline and produce a result.

### `collect`

```cpp
.collect()
.collect<Container>()
```

Collect elements into a container.

Examples:

```cpp
auto vec = itercpp::iter(data)
    .map(...)
    .collect();

auto set = itercpp::iter(data)
    .collect<std::set<int>>();
```

### `fold`

```cpp
.fold(init, op)
```

Fold elements into an accumulator.

### `reduce`

```cpp
.reduce(op)
```

Reduce elements into a single value.

Returns:

```cpp
std::optional<T>
```

### `find`

```cpp
.find(pred)
```

Returns the first matching element.

Returns:

```cpp
std::optional<T>
```

### `position`

```cpp
.position(pred)
```

Returns the index of the first matching element.

Returns:

```cpp
std::optional<std::size_t>
```

### `count`

```cpp
.count()
```

Count elements.

### `any`

```cpp
.any(pred)
```

Returns whether any element satisfies the predicate.

### `all`

```cpp
.all(pred)
```

Returns whether all elements satisfy the predicate.

### `for_each`

```cpp
.for_each(func)
```

Invoke a function for every element.

---

# To Use

These operations are designed to be chained seamlessly. The library uses reference-collapsing and type traits to safely handle both lvalues (by reference) and rvalues (by taking ownership), eliminating dangling reference bugs while maintaining zero-cost abstractions.
