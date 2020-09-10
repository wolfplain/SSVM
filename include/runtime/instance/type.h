// SPDX-License-Identifier: Apache-2.0
//===-- ssvm/runtime/instance/type.h - Function type definition -----------===//
//
// Part of the SSVM Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the function type definition used in module instance.
///
//===----------------------------------------------------------------------===//
#pragma once

#include "common/types.h"
#include "common/value.h"
#include "support/span.h"

#include <vector>

namespace SSVM {
namespace Runtime {
namespace Instance {

/// Function type definition in this module.
struct FType {
  using Wrapper = void (*)(void *Function, const ValVariant *Args,
                           ValVariant *Rets);

  FType() = default;
  FType(Span<const ValType> P, Span<const ValType> R, void *S)
      : Params(P.begin(), P.end()), Returns(R.begin(), R.end()),
        Symbol(reinterpret_cast<Wrapper>(S)) {}
  std::vector<ValType> Params;
  std::vector<ValType> Returns;

  /// Getter of symbol
  Wrapper getSymbol() const { return Symbol; }
  /// Setter of symbol
  void setSymbol(void *S) { Symbol = reinterpret_cast<Wrapper>(S); }

  Wrapper Symbol = nullptr;
};

} // namespace Instance
} // namespace Runtime
} // namespace SSVM
