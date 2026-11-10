// :bustub-keep-private:
//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// arc_replacer.cpp
//
// Identification: src/buffer/arc_replacer.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/arc_replacer.h"
#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include "common/config.h"

namespace bustub {

/**
 *
 * TODO(P1): Add implementation
 *
 * @brief a new ArcReplacer, with lists initialized to be empty and target size to 0
 * @param num_frames the maximum number of frames the ArcReplacer will be required to cache
 */
ArcReplacer::ArcReplacer(size_t num_frames) : replacer_size_(num_frames) {}

/**
 * TODO(P1): Add implementation
 *
 * @brief Performs the Replace operation as described by the writeup
 * that evicts from either mfu_ or mru_ into its corresponding ghost list
 * according to balancing policy.
 *
 * If you wish to refer to the original ARC paper, please note that there are
 * two changes in our implementation:
 * 1. When the size of mru_ equals the target size, we don't check
 * the last access as the paper did when deciding which list to evict from.
 * This is fine since the original decision is stated to be arbitrary.
 * 2. Entries that are not evictable are skipped. If all entries from the desired side
 * (mru_ / mfu_) are pinned, we instead try victimize the other side (mfu_ / mru_),
 * and move it to its corresponding ghost list (mfu_ghost_ / mru_ghost_).
 *
 * @return frame id of the evicted frame, or std::nullopt if cannot evict
 */
auto ArcReplacer::Evict() -> std::optional<frame_id_t> {
  std::lock_guard<std::mutex> lk(latch_);
  if (mru_.size() >= mru_target_size_) {
    // 优先淘汰 mru
    auto it = mru_.rbegin();
    while (it != mru_.rend()) {
      frame_id_t frame = *it;
      auto frame_status_iter = alive_map_.find(frame);
      if (frame_status_iter != alive_map_.end()) {
        auto frame_status = frame_status_iter->second;
        if (frame_status->evictable_) {
          // alive 这边清空
          mru_.erase(--it.base());
          alive_map_.erase(frame_status_iter);
          // 加入到ghost
          mru_ghost_.push_front(frame_status->page_id_);
          frame_status->evictable_ = false;
          frame_status->arc_status_ = ArcStatus::MRU_GHOST;
          frame_status->pos_ = mru_ghost_.begin();
          ghost_map_[frame_status->page_id_] = frame_status;
          curr_size_--;
          return frame_status->frame_id_;
        }
      }
      it = std::next(it);
    }
    it = mfu_.rbegin();
    while (it != mfu_.rend()) {
      frame_id_t frame = *it;
      auto frame_status_iter = alive_map_.find(frame);
      if (frame_status_iter != alive_map_.end()) {
        auto frame_status = frame_status_iter->second;
        if (frame_status->evictable_) {
          // alive 这边清空
          mfu_.erase(--it.base());
          alive_map_.erase(frame_status_iter);
          // 加入到ghost
          mfu_ghost_.push_front(frame_status->page_id_);
          frame_status->arc_status_ = ArcStatus::MFU_GHOST;
          frame_status->evictable_ = false;
          frame_status->pos_ = mfu_ghost_.begin();
          ghost_map_[frame_status->page_id_] = frame_status;
          curr_size_--;
          return frame_status->frame_id_;
        }
      }
      it = std::next(it);
    }
  } else {
    // 优先淘汰 mfu_
    auto it = mfu_.rbegin();
    while (it != mfu_.rend()) {
      frame_id_t frame = *it;
      auto frame_status_iter = alive_map_.find(frame);
      if (frame_status_iter != alive_map_.end()) {
        auto frame_status = frame_status_iter->second;
        if (frame_status->evictable_) {
          // alive 这边清空
          mfu_.erase(--it.base());
          alive_map_.erase(frame_status_iter);
          // 加入到ghost
          mfu_ghost_.push_front(frame_status->page_id_);
          frame_status->arc_status_ = ArcStatus::MFU_GHOST;
          frame_status->evictable_ = false;
          frame_status->pos_ = mfu_ghost_.begin();
          ghost_map_[frame_status->page_id_] = frame_status;
          curr_size_--;
          return frame_status->frame_id_;
        }
      }
      it = std::next(it);
    }
    it = mru_.rbegin();
    while (it != mru_.rend()) {
      frame_id_t frame = *it;
      auto frame_status_iter = alive_map_.find(frame);
      if (frame_status_iter != alive_map_.end()) {
        auto frame_status = frame_status_iter->second;
        if (frame_status->evictable_) {
          // alive 这边清空
          mru_.erase(--it.base());
          alive_map_.erase(frame_status_iter);
          // 加入到ghost
          mru_ghost_.push_front(frame_status->page_id_);
          frame_status->arc_status_ = ArcStatus::MRU_GHOST;
          frame_status->evictable_ = false;
          frame_status->pos_ = mru_ghost_.begin();
          ghost_map_[frame_status->page_id_] = frame_status;
          curr_size_--;
          return frame_status->frame_id_;
        }
      }
      it = std::next(it);
    }
  }
  return std::nullopt;
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Record access to a frame, adjusting ARC bookkeeping accordingly
 * by bring the accessed page to the front of mfu_ if it exists in any of the lists
 * or the front of mru_ if it does not.
 *
 * Performs the operations EXCEPT REPLACE described in original paper, which is
 * handled by `Evict()`.
 *
 * Consider the following four cases, handle accordingly:
 * 1. Access hits mru_ or mfu_
 * 2/3. Access hits mru_ghost_ / mfu_ghost_
 * 4. Access misses all the lists
 *
 * This routine performs all changes to the four lists as preperation
 * for `Evict()` to simply find and evict a victim into ghost lists.
 *
 * Note that frame_id is used as identifier for alive pages and
 * page_id is used as identifier for the ghost pages, since page_id is
 * the unique identifier to the page after it's dead.
 * Using page_id for alive pages should be the same since it's one to one mapping,
 * but using frame_id is slightly more intuitive.
 *
 * @param frame_id id of frame that received a new access.
 * @param page_id id of page that is mapped to the frame.
 * @param access_type type of access that was received. This parameter is only needed for
 * leaderboard tests.
 */
void ArcReplacer::RecordAccess(frame_id_t frame_id, page_id_t page_id, [[maybe_unused]] AccessType access_type) {
  std::lock_guard<std::mutex> lk(latch_);
  auto it = alive_map_.find(frame_id);
  if (it != alive_map_.end()) {
    auto frame_status = it->second;

    if (frame_status->arc_status_ == ArcStatus::MRU) {
      // mru -> mfu
      frame_status->arc_status_ = ArcStatus::MFU;
      auto pos = frame_status->pos_;
      if (pos != mru_.end()) {
        mru_.erase(pos);
      }
      mfu_.push_front(frame_status->frame_id_);
      frame_status->pos_ = mfu_.begin();
    } else {
      auto pos = frame_status->pos_;
      if (pos != mfu_.end()) {
        mfu_.erase(pos);
      }
      mfu_.push_front(frame_status->frame_id_);
      frame_status->pos_ = mfu_.begin();
    }
    return;
  }

  it = ghost_map_.find(page_id);
  if (it != ghost_map_.end()) {
    auto frame_status = it->second;
    // 命中 ghost
    if (frame_status->arc_status_ == ArcStatus::MRU_GHOST) {
      // 自适应调整 mru_ghost
      if (mru_ghost_.size() >= mfu_ghost_.size()) {
        mru_target_size_++;
      } else {
        mru_target_size_ += mfu_ghost_.size() / mru_ghost_.size();
      }
      mru_target_size_ = std::min(mru_target_size_, replacer_size_);

      // 先从 ghost 这边的list和map 删掉
      auto pos = frame_status->pos_;
      if (pos != mru_ghost_.end()) {
        mru_ghost_.erase(pos);
      }
      ghost_map_.extract(it);

      // 更新一下状态放到 alive这边去
      frame_status->arc_status_ = ArcStatus::MFU;
      frame_status->frame_id_ = frame_id;
      alive_map_[frame_id] = frame_status;
      mfu_.push_front(frame_id);
      frame_status->pos_ = mfu_.begin();
    } else {
      // 自适应调整 mfu_ghost
      if (mfu_ghost_.size() >= mru_ghost_.size()) {
        if (mru_target_size_ == 0) {
          mru_target_size_ = 0;
        } else {
          mru_target_size_--;
        }
      } else {
        if (mru_target_size_ < mru_ghost_.size() / mfu_ghost_.size()) {
          mru_target_size_ = 0;
        } else {
          mru_target_size_ -= mru_ghost_.size() / mfu_ghost_.size();
        }
      }
      mru_target_size_ = std::max(static_cast<size_t>(0), mru_target_size_);
      // 先从 ghost 这边的list和map 删掉
      auto pos = frame_status->pos_;
      if (pos != mfu_ghost_.end()) {
        mfu_ghost_.erase(pos);
      }
      ghost_map_.extract(it);

      // 更新一下状态放到 alive这边去
      frame_status->arc_status_ = ArcStatus::MFU;
      frame_status->frame_id_ = frame_id;
      alive_map_[frame_id] = frame_status;
      mfu_.push_front(frame_id);
      frame_status->pos_ = mfu_.begin();
    }
    return;
  }

  if (mru_.size() + mru_ghost_.size() == replacer_size_) {
    // 删除 MRU 影子列表中的最后一个元素，然后将该页面添加到 MRU 的前端
    if (!mru_ghost_.empty()) {
      auto lst = std::prev(mru_ghost_.end());
      if (ghost_map_.count(*lst) >= 1) {
        ghost_map_.erase(*lst);
      }
      mru_ghost_.erase(lst);
    }
  } else if (mru_.size() + mru_ghost_.size() < replacer_size_) {
    if (mru_.size() + mfu_.size() + mru_ghost_.size() + mfu_ghost_.size() == 2 * replacer_size_) {
      // 删除 MFU 影子列表中的最后一个元素，然后将该页面添加到 MRU 的前端
      if (!mfu_ghost_.empty()) {
        auto lst = std::prev(mfu_ghost_.end());
        if (ghost_map_.count(*lst) >= 1) {
          ghost_map_.erase(*lst);
        }
        mfu_ghost_.erase(lst);
      }
    }
  } 

  mru_.push_front(frame_id);
  std::shared_ptr<FrameStatus> frame_status =
      std::make_shared<FrameStatus>(page_id, frame_id, false, ArcStatus::MRU, mru_.begin());
  alive_map_[frame_id] = frame_status;
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Toggle whether a frame is evictable or non-evictable. This function also
 * controls replacer's size. Note that size is equal to number of evictable entries.
 *
 * If a frame was previously evictable and is to be set to non-evictable, then size should
 * decrement. If a frame was previously non-evictable and is to be set to evictable,
 * then size should increment.
 *
 * If frame id is invalid, throw an exception or abort the process.
 *
 * For other scenarios, this function should terminate without modifying anything.
 *
 * @param frame_id id of frame whose 'evictable' status will be modified
 * @param set_evictable whether the given frame is evictable or not
 */
void ArcReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
  std::lock_guard<std::mutex> lk(latch_);
  auto it = alive_map_.find(frame_id);
  if (it != alive_map_.end()) {
    auto frame_status = it->second;
    if (frame_status->evictable_) {
      if (!set_evictable) {
        curr_size_--;
        frame_status->evictable_ = false;
      }
    } else {
      if (set_evictable) {
        curr_size_++;
        frame_status->evictable_ = true;
      }
    }
  } else {
    throw std::invalid_argument("invalid frame id");
  }
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Remove an evictable frame from replacer.
 * This function should also decrement replacer's size if removal is successful.
 *
 * Note that this is different from evicting a frame, which always remove the frame
 * decided by the ARC algorithm.
 *
 * If Remove is called on a non-evictable frame, throw an exception or abort the
 * process.
 *
 * If specified frame is not found, directly return from this function.
 *
 * @param frame_id id of frame to be removed
 */
void ArcReplacer::Remove(frame_id_t frame_id) {
  std::lock_guard<std::mutex> lk(latch_);
  auto it = alive_map_.find(frame_id);
  if (it != alive_map_.end()) {
    auto frame_status = it->second;
    if (frame_status->evictable_) {
      if (frame_status->arc_status_ == ArcStatus::MRU) {
        mru_.erase(frame_status->pos_);
      } else {
        mfu_.erase(frame_status->pos_);
      }
      alive_map_.erase(it);
      curr_size_--;
    } else {
      throw std::invalid_argument("invalid frame id");
    }
  }
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Return replacer's size, which tracks the number of evictable frames.
 *
 * @return size_t
 */
auto ArcReplacer::Size() -> size_t {
  std::lock_guard<std::mutex> lk(latch_);
  return curr_size_;
}

}  // namespace bustub
