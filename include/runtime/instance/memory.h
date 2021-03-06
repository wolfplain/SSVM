// SPDX-License-Identifier: Apache-2.0
//===-- ssvm/runtime/instance/memory.h - Memory Instance definition --=----===//
//
// Part of the SSVM Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the memory instance definition in store manager.
///
//===----------------------------------------------------------------------===//
#pragma once

#include "common/ast/type.h"
#include "common/errcode.h"
#include "common/value.h"
#include "support/casting.h"
#include "support/log.h"
#include "support/span.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <utility>

#include <linux/mman.h>
#include <sys/mman.h>

namespace SSVM {
namespace Runtime {
namespace Instance {

class MemoryInstance {
public:
  static inline constexpr const uint64_t kPageSize = UINT64_C(65536);
  static inline constexpr const uint64_t k4G = UINT64_C(0x100000000);
  static inline constexpr const uint64_t k8G = UINT64_C(0x200000000);
  MemoryInstance() = delete;
  MemoryInstance(const AST::Limit &Lim)
      : HasMaxPage(Lim.hasMax()), MinPage(Lim.getMin()), MaxPage(Lim.getMax()),
        CurrPage(Lim.getMin()) {
    const auto Pages = static_cast<uint8_t *>(
        mmap(nullptr, k8G, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_UNINITIALIZED, -1, 0));
    mprotect(Pages + CurrPage * kPageSize, k8G - CurrPage * kPageSize,
             PROT_NONE);
    DataPtr = Pages;
  }
  ~MemoryInstance() noexcept { munmap(DataPtr, k8G); }

  /// Get page size of memory.data
  uint32_t getDataPageSize() const noexcept { return CurrPage; }

  /// Getter of limit definition.
  bool getHasMax() const noexcept { return HasMaxPage; }

  /// Getter of limit definition.
  uint32_t getMin() const noexcept { return MinPage; }

  /// Getter of limit definition.
  uint32_t getMax() const noexcept { return MaxPage; }

  /// Check access size is valid.
  bool checkAccessBound(uint32_t Offset, uint32_t Length) const noexcept {
    const uint64_t AccessLen =
        static_cast<uint64_t>(Offset) + static_cast<uint64_t>(Length);
    return AccessLen <= CurrPage * kPageSize;
  }

  /// Get boundary index.
  uint32_t getBoundIdx() const noexcept {
    return CurrPage > 0 ? CurrPage * kPageSize - 1 : 0;
  }

  /// Grow page
  bool growPage(const uint32_t Count) {
    /// Maximum pages count, 65536
    uint32_t MaxPageCaped = k4G / kPageSize;
    if (HasMaxPage) {
      MaxPageCaped = std::min(MaxPage, MaxPageCaped);
    }
    if (Count + CurrPage > MaxPageCaped) {
      return false;
    }
    mprotect(DataPtr + CurrPage * kPageSize, Count * kPageSize,
             PROT_READ | PROT_WRITE);
    CurrPage += Count;
    return true;
  }

  /// Get slice of Data[Offset : Offset + Length - 1]
  Expect<Span<Byte>> getBytes(const uint32_t Offset, const uint32_t Length) {
    /// Check memory boundary.
    if (!checkAccessBound(Offset, Length)) {
      LOG(ERROR) << ErrCode::MemoryOutOfBounds;
      LOG(ERROR) << ErrInfo::InfoBoundary(Offset, Length, getBoundIdx());
      return Unexpect(ErrCode::MemoryOutOfBounds);
    }
    return Span<Byte>(&DataPtr[Offset], Length);
  }

  /// Replace the bytes of Data[Offset :] by Slice[Start : Start + Legnth - 1]
  Expect<void> setBytes(Span<const Byte> Slice, const uint32_t Offset,
                        const uint32_t Start, const uint32_t Length) {
    /// Check memory boundary.
    if (!checkAccessBound(Offset, Length)) {
      LOG(ERROR) << ErrCode::MemoryOutOfBounds;
      LOG(ERROR) << ErrInfo::InfoBoundary(Offset, Length, getBoundIdx());
      return Unexpect(ErrCode::MemoryOutOfBounds);
    }

    /// Check input data validation.
    if ((Slice.size() > 0 && Start >= Slice.size()) ||
        Start + Length > Slice.size()) {
      LOG(ERROR) << ErrCode::MemoryOutOfBounds;
      LOG(ERROR) << ErrInfo::InfoBoundary(Start, Length, Slice.size() - 1);
      return Unexpect(ErrCode::MemoryOutOfBounds);
    }

    /// Copy data.
    if (Length > 0) {
      std::copy(Slice.begin() + Start, Slice.begin() + Start + Length,
                DataPtr + Offset);
    }
    return {};
  }

  /// Get an uint8 array from Data[Offset : Offset + Length - 1]
  Expect<void> getArray(uint8_t *Arr, const uint32_t Offset,
                        const uint32_t Length, const bool IsReverse = false) {
    /// Check memory boundary.
    if (!checkAccessBound(Offset, Length)) {
      LOG(ERROR) << ErrCode::MemoryOutOfBounds;
      LOG(ERROR) << ErrInfo::InfoBoundary(Offset, Length, getBoundIdx());
      return Unexpect(ErrCode::MemoryOutOfBounds);
    }
    if (Length > 0) {
      /// Copy data.
      if (IsReverse) {
        std::reverse_copy(DataPtr + Offset, DataPtr + Offset + Length, Arr);
      } else {
        std::copy(DataPtr + Offset, DataPtr + Offset + Length, Arr);
      }
    }
    return {};
  }

  /// Replace Data[Offset : Offset + Length - 1] to an uint8 array
  Expect<void> setArray(const uint8_t *Arr, const uint32_t Offset,
                        const uint32_t Length, const bool IsReverse = false) {
    /// Check memory boundary.
    if (!checkAccessBound(Offset, Length)) {
      LOG(ERROR) << ErrCode::MemoryOutOfBounds;
      LOG(ERROR) << ErrInfo::InfoBoundary(Offset, Length, getBoundIdx());
      return Unexpect(ErrCode::MemoryOutOfBounds);
    }
    if (Length > 0) {
      /// Copy data.
      if (IsReverse) {
        std::reverse_copy(Arr, Arr + Length, DataPtr + Offset);
      } else {
        std::copy(Arr, Arr + Length, DataPtr + Offset);
      }
    }
    return {};
  }

  /// Get pointer to specific offset of memory or null.
  template <typename T>
  typename std::enable_if_t<std::is_pointer_v<T>, T>
  getPointerOrNull(const uint32_t Offset) {
    if (Offset == 0 ||
        !checkAccessBound(Offset, sizeof(std::remove_pointer_t<T>))) {
      return nullptr;
    }
    return reinterpret_cast<T>(&DataPtr[Offset]);
  }

  /// Get pointer to specific offset of memory.
  template <typename T>
  typename std::enable_if_t<std::is_pointer_v<T>, T>
  getPointer(const uint32_t Offset, const uint32_t Size = 1) {
    using Type = std::remove_pointer_t<T>;
    size_t ByteSize = sizeof(Type) * Size;
    if (!checkAccessBound(Offset, ByteSize)) {
      return nullptr;
    }
    return reinterpret_cast<T>(&DataPtr[Offset]);
  }

  /// Template of loading bytes and convert to a value.
  ///
  /// Load the length of vector and construct into a value.
  /// Only output value of int32, uint32, int64, uint64, float, and double are
  /// allowed.
  ///
  /// \param Value the constructed output value.
  /// \param Offset the start offset in data array.
  /// \param Length the load length from data. Need to <= sizeof(T).
  ///
  /// \returns void when success, ErrCode when failed.
  template <typename T>
  typename std::enable_if_t<Support::IsWasmTypeV<T>, Expect<void>>
  loadValue(T &Value, const uint32_t Offset, const uint32_t Length) {
    /// Check data boundary.
    if (Length > sizeof(T)) {
      LOG(ERROR) << ErrCode::MemoryOutOfBounds;
      LOG(ERROR) << ErrInfo::InfoBoundary(Offset, Length,
                                          Offset + sizeof(T) - 1);
      return Unexpect(ErrCode::MemoryOutOfBounds);
    }
    /// Check memory boundary.
    if (!checkAccessBound(Offset, Length)) {
      LOG(ERROR) << ErrCode::MemoryOutOfBounds;
      LOG(ERROR) << ErrInfo::InfoBoundary(Offset, Length, getBoundIdx());
      return Unexpect(ErrCode::MemoryOutOfBounds);
    }
    /// Load data to a value.
    if (Length > 0) {
      if (std::is_floating_point_v<T>) {
        /// Floating case. Do memory copy.
        std::memcpy(&Value, &DataPtr[Offset], sizeof(T));
      } else {
        uint64_t LoadVal = 0;
        /// Integer case. Extends to result type.
        std::memcpy(&LoadVal, &DataPtr[Offset], Length);
        if (std::is_signed_v<T> && (LoadVal >> (Length * 8 - 1))) {
          /// Signed extend.
          for (unsigned int I = Length; I < 8; I++) {
            LoadVal |= 0xFFULL << (I * 8);
          }
        }
        Value = static_cast<T>(LoadVal);
      }
    }
    return {};
  }

  /// Template of loading bytes and convert to a value.
  ///
  /// Destruct and Store the value to length of vector.
  /// Only input value of uint32, uint64, float, and double are allowed.
  ///
  /// \param Value the value want to store into data array.
  /// \param Offset the start offset in data array.
  /// \param Length the store length to data. Need to <= sizeof(T).
  ///
  /// \returns void when success, ErrCode when failed.
  template <typename T>
  typename std::enable_if_t<Support::IsWasmBuiltInV<T>, Expect<void>>
  storeValue(const T &Value, const uint32_t Offset, const uint32_t Length) {
    /// Check data boundary.
    if (Length > sizeof(T)) {
      LOG(ERROR) << ErrCode::MemoryOutOfBounds;
      LOG(ERROR) << ErrInfo::InfoBoundary(Offset, Length,
                                          Offset + sizeof(T) - 1);
      return Unexpect(ErrCode::MemoryOutOfBounds);
    }
    /// Check memory boundary.
    if (!checkAccessBound(Offset, Length)) {
      LOG(ERROR) << ErrCode::MemoryOutOfBounds;
      LOG(ERROR) << ErrInfo::InfoBoundary(Offset, Length, getBoundIdx());
      return Unexpect(ErrCode::MemoryOutOfBounds);
    }
    /// Copy store data to value.
    if (Length > 0) {
      std::memcpy(&DataPtr[Offset], &Value, Length);
    }
    return {};
  }

  /// Getter of symbol
  void *getSymbol() const { return Symbol; }
  /// Setter of symbol
  void setSymbol(void *S) {
    Symbol = reinterpret_cast<uint8_t **>(S);
    *Symbol = DataPtr;
  }

private:
  /// \name Data of memory instance.
  /// @{
  const bool HasMaxPage;
  const uint32_t MinPage;
  const uint32_t MaxPage;
  uint8_t *DataPtr = nullptr;
  uint32_t CurrPage = 0;
  uint8_t **Symbol = nullptr;
  /// @}
};

} // namespace Instance
} // namespace Runtime
} // namespace SSVM
