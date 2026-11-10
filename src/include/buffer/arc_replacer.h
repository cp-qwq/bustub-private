
//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// arc_replacer.h
//
// Identification: src/include/buffer/arc_replacer.h
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <cstdint>
#include <list>
#include <memory>
#include <mutex>  // NOLINT
#include <optional>
#include <unordered_map>

#include "common/config.h"
#include "common/macros.h"

namespace bustub {

enum class AccessType { Unknown = 0, Lookup, Scan, Index };

// 淘汰策略类型
enum class ArcStatus { MRU, MFU, MRU_GHOST, MFU_GHOST };

// TODO(student): You can modify or remove this struct as you like.
struct FrameStatus {
  page_id_t page_id_;
  frame_id_t frame_id_;
  bool evictable_;
  ArcStatus arc_status_;
  std::list<int32_t>::iterator pos_;
  FrameStatus(page_id_t pid, frame_id_t fid, bool ev, ArcStatus st, std::list<int32_t>::iterator pos)
      : page_id_(pid), frame_id_(fid), evictable_(ev), arc_status_(st), pos_(pos) {}
};

/**
 * ArcReplacer implements the ARC replacement policy.
 */
class ArcReplacer {
 public:
  explicit ArcReplacer(size_t num_frames);

  DISALLOW_COPY_AND_MOVE(ArcReplacer);

  /**
   * TODO(P1): Add implementation
   *
   * @brief Destroys the LRUReplacer.
   */
  ~ArcReplacer() = default;

  auto Evict() -> std::optional<frame_id_t>;
  void RecordAccess(frame_id_t frame_id, page_id_t page_id, AccessType access_type = AccessType::Unknown);
  void SetEvictable(frame_id_t frame_id, bool set_evictable);
  void Remove(frame_id_t frame_id);
  auto Size() -> size_t;

 private:
  // TODO(student): implement me! You can replace or remove these member variables as you like.
  std::list<frame_id_t> mru_;       // 最近最常使用列表
  std::list<frame_id_t> mfu_;       // 最近访问次数最多的列表
  std::list<page_id_t> mru_ghost_;  // 最近从 mru_ 淘汰的页面
  std::list<page_id_t> mfu_ghost_;  // 最近从 mfu_ 淘汰的页面

  /* record entries in mru_ and mfu_
   * this uses frame_id_t to guarantee no duplicate records for the same
   * frame when they are alive */
  std::unordered_map<frame_id_t, std::shared_ptr<FrameStatus>> alive_map_;
  /* record entries in mru_ghost_ and mfu_ghost_
   * this uses page_id_t but not frame_id_t because page_id is the unique
   * identifier in ghost lists */
  std::unordered_map<page_id_t, std::shared_ptr<FrameStatus>> ghost_map_;

  /* alive, evictable entries count */
  [[maybe_unused]] size_t curr_size_{0};
  /* p as in original paper */
  [[maybe_unused]] size_t mru_target_size_{0};  // ARC 希望给 mru_ 分配多少缓存容量（和实际大小是不同的）
  /* c as in original paper */
  [[maybe_unused]] size_t replacer_size_;  // 缓存的最大容量
  std::mutex latch_;

  // TODO(student): You can add member variables / functions as you like.
};

}  // namespace bustub
