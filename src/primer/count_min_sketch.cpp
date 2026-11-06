//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// count_min_sketch.cpp
//
// Identification: src/primer/count_min_sketch.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "primer/count_min_sketch.h"

#include <cstdint>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

namespace bustub {

/**
 * Constructor for the count-min sketch.
 *
 * @param width The width of the sketch matrix.
 * @param depth The depth of the sketch matrix.
 * @throws std::invalid_argument if width or depth are zero.
 */
template <typename KeyType>
CountMinSketch<KeyType>::CountMinSketch(uint32_t width, uint32_t depth) : width_(width), depth_(depth) {
  /** @TODO(student) Implement this function! */
  if (width_ == 0 || depth_ == 0) {
    throw std::invalid_argument("width or depth can't be zero");
  }
  matrix_.resize(depth_, std::vector<uint32_t>(width_, 0));

  /** @fall2025 PLEASE DO NOT MODIFY THE FOLLOWING */
  // Initialize seeded hash functions
  hash_functions_.reserve(depth_);
  for (size_t i = 0; i < depth_; i++) {
    hash_functions_.push_back(this->HashFunction(i));
  }
}

template <typename KeyType>
CountMinSketch<KeyType>::CountMinSketch(CountMinSketch &&other) noexcept : width_(other.width_), depth_(other.depth_) {
  /** @TODO(student) Implement this function! */
  matrix_ = std::move(other.matrix_);
  hash_functions_ = std::move(other.hash_functions_);
  other.matrix_.clear();
  other.hash_functions_.clear();
}

template <typename KeyType>
auto CountMinSketch<KeyType>::operator=(CountMinSketch &&other) noexcept -> CountMinSketch & {
  /** @TODO(student) Implement this function! */
  width_ = other.width_;
  depth_ = other.depth_;
  matrix_ = std::move(other.matrix_);
  hash_functions_ = std::move(other.hash_functions_);
  other.matrix_.clear();
  other.hash_functions_.clear();
  return *this;
}

template <typename KeyType>
void CountMinSketch<KeyType>::Insert(const KeyType &item) {
  /** @TODO(student) Implement this function! */
  for (size_t i = 0; i < depth_; i++) {
    auto hash_func = hash_functions_[i];
    auto hash = hash_func(item);
    std::lock_guard<std::mutex> lock(mtx_);
    matrix_[i][hash % width_]++;
  }
}

template <typename KeyType>
void CountMinSketch<KeyType>::Merge(const CountMinSketch<KeyType> &other) {
  if (width_ != other.width_ || depth_ != other.depth_) {
    throw std::invalid_argument("Incompatible CountMinSketch dimensions for merge.");
  }
  /** @TODO(student) Implement this function! */
  for (size_t i = 0; i < depth_; i++) {
    for (size_t j = 0; j < width_; j++) {
      matrix_[i][j] += other.matrix_[i][j];
    }
  }
}

template <typename KeyType>
auto CountMinSketch<KeyType>::Count(const KeyType &item) const -> uint32_t {
  uint32_t res = 2e9;
  for (size_t i = 0; i < depth_; i++) {
    auto hash_func = hash_functions_[i];
    auto hash = hash_func(item);
    res = std::min(res, matrix_[i][hash % width_]);
  }
  return (res == 2e9 ? 0 : res);
}

template <typename KeyType>
void CountMinSketch<KeyType>::Clear() {
  /** @TODO(student) Implement this function! */
  for (size_t i = 0; i < depth_; i++) {
    for (size_t j = 0; j < width_; j++) {
      matrix_[i][j] = 0;
    }
  }
}

template <typename KeyType>
auto CountMinSketch<KeyType>::TopK(uint16_t k, const std::vector<KeyType> &candidates)
    -> std::vector<std::pair<KeyType, uint32_t>> {
  /** @TODO(student) Implement this function! */

  std::priority_queue<std::pair<uint32_t, KeyType>> pq;
  std::vector<std::pair<KeyType, uint32_t>> res;
  for (const auto &candidate : candidates) {
    auto count = Count(candidate);
    pq.emplace(count, candidate);
  }
  while (!pq.empty() && res.size() < k) {
    auto [count, candidate] = pq.top();
    pq.pop();
    res.emplace_back(candidate, count);
  }
  return res;
}

// Explicit instantiations for all types used in tests
template class CountMinSketch<std::string>;
template class CountMinSketch<int64_t>;  // For int64_t tests
template class CountMinSketch<int>;      // This covers both int and int32_t
}  // namespace bustub
