//===- Utils.cpp - General utilities for Presburger library ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Utility functions required by the Presburger Library.
//
//===----------------------------------------------------------------------===//

#include "mlir/Analysis/Presburger/Utils.h"
#include "mlir/Analysis/Presburger/IntegerRelation.h"
#include "mlir/Analysis/Presburger/PresburgerSpace.h"
#include "llvm/ADT/STLFunctionalExtras.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <cstdint>
#include <numeric>
#include <optional>

using namespace mlir;
using namespace presburger;
using llvm::dynamicAPIntFromInt64;

/// Normalize a division's `dividend` and the `divisor` by their GCD. For
/// example: if the dividend and divisor are [2,0,4] and 4 respectively,
/// they get normalized to [1,0,2] and 2. The divisor must be non-negative;
/// it is allowed for the divisor to be zero, but nothing is done in this case.
static void normalizeDivisionByGCD(MutableArrayRef<DynamicAPInt> dividend,
                                   DynamicAPInt &divisor) {
  assert(divisor > 0 && "divisor must be non-negative!");
  if (dividend.empty())
    return;
  // We take the absolute value of dividend's coefficients to make sure that
  // `gcd` is positive.
  DynamicAPInt gcd = llvm::gcd(abs(dividend.front()), divisor);

  // The reason for ignoring the constant term is as follows.
  // For a division:
  //      floor((a + m.f(x))/(m.d))
  // It can be replaced by:
  //      floor((floor(a/m) + f(x))/d)
  // Since `{a/m}/d` in the dividend satisfies 0 <= {a/m}/d < 1/d, it will not
  // influence the result of the floor division and thus, can be ignored.
  for (size_t i = 1, m = dividend.size() - 1; i < m; i++) {
    gcd = llvm::gcd(abs(dividend[i]), gcd);
    if (gcd == 1)
      return;
  }

  // Normalize the dividend and the denominator.
  std::transform(dividend.begin(), dividend.end(), dividend.begin(),
                 [gcd](DynamicAPInt &n) { return floorDiv(n, gcd); });
  divisor /= gcd;
}

/// Check if the pos^th variable can be represented as a division using upper
/// bound inequality at position `ubIneq` and lower bound inequality at position
/// `lbIneq`.
///
/// Let `var` be the pos^th variable, then `var` is equivalent to
/// `expr floordiv divisor` if there are constraints of the form:
///      0 <= expr - divisor * var <= divisor - 1
/// Rearranging, we have:
///       divisor * var - expr + (divisor - 1) >= 0  <-- Lower bound for 'var'
///      -divisor * var + expr                 >= 0  <-- Upper bound for 'var'
///
/// For example:
///     32*k >= 16*i + j - 31                 <-- Lower bound for 'k'
///     32*k  <= 16*i + j                     <-- Upper bound for 'k'
///     expr = 16*i + j, divisor = 32
///     k = ( 16*i + j ) floordiv 32
///
///     4q >= i + j - 2                       <-- Lower bound for 'q'
///     4q <= i + j + 1                       <-- Upper bound for 'q'
///     expr = i + j + 1, divisor = 4
///     q = (i + j + 1) floordiv 4
//
/// This function also supports detecting divisions from bounds that are
/// strictly tighter than the division bounds described above, since tighter
/// bounds imply the division bounds. For example:
///     4q - i - j + 2 >= 0                       <-- Lower bound for 'q'
///    -4q + i + j     >= 0                       <-- Tight upper bound for 'q'
///
/// To extract floor divisions with tighter bounds, we assume that the
/// constraints are of the form:
///     c <= expr - divisior * var <= divisor - 1, where 0 <= c <= divisor - 1
/// Rearranging, we have:
///     divisor * var - expr + (divisor - 1) >= 0  <-- Lower bound for 'var'
///    -divisor * var + expr - c             >= 0  <-- Upper bound for 'var'
///
/// If successful, `expr` is set to dividend of the division and `divisor` is
/// set to the denominator of the division, which will be positive.
/// The final division expression is normalized by GCD.
static LogicalResult getDivRepr(const IntegerRelation &cst, unsigned pos,
                                unsigned ubIneq, unsigned lbIneq,
                                MutableArrayRef<DynamicAPInt> expr,
                                DynamicAPInt &divisor) {

  assert(pos <= cst.getNumVars() && "Invalid variable position");
  assert(ubIneq <= cst.getNumInequalities() &&
         "Invalid upper bound inequality position");
  assert(lbIneq <= cst.getNumInequalities() &&
         "Invalid upper bound inequality position");
  assert(expr.size() == cst.getNumCols() && "Invalid expression size");
  assert(cst.atIneq(lbIneq, pos) > 0 && "lbIneq is not a lower bound!");
  assert(cst.atIneq(ubIneq, pos) < 0 && "ubIneq is not an upper bound!");

  // Extract divisor from the lower bound.
  divisor = cst.atIneq(lbIneq, pos);

  // First, check if the constraints are opposite of each other except the
  // constant term.
  unsigned i = 0, e = 0;
  for (i = 0, e = cst.getNumVars(); i < e; ++i)
    if (cst.atIneq(ubIneq, i) != -cst.atIneq(lbIneq, i))
      break;

  if (i < e)
    return failure();

  // Then, check if the constant term is of the proper form.
  // Due to the form of the upper/lower bound inequalities, the sum of their
  // constants is `divisor - 1 - c`. From this, we can extract c:
  DynamicAPInt constantSum = cst.atIneq(lbIneq, cst.getNumCols() - 1) +
                             cst.atIneq(ubIneq, cst.getNumCols() - 1);
  DynamicAPInt c = divisor - 1 - constantSum;

  // Check if `c` satisfies the condition `0 <= c <= divisor - 1`.
  // This also implictly checks that `divisor` is positive.
  if (!(0 <= c && c <= divisor - 1)) // NOLINT
    return failure();

  // The inequality pair can be used to extract the division.
  // Set `expr` to the dividend of the division except the constant term, which
  // is set below.
  for (i = 0, e = cst.getNumVars(); i < e; ++i)
    if (i != pos)
      expr[i] = cst.atIneq(ubIneq, i);

  // From the upper bound inequality's form, its constant term is equal to the
  // constant term of `expr`, minus `c`. From this,
  // constant term of `expr` = constant term of upper bound + `c`.
  expr.back() = cst.atIneq(ubIneq, cst.getNumCols() - 1) + c;
  normalizeDivisionByGCD(expr, divisor);

  return success();
}

/// Check if the pos^th variable can be represented as a division using
/// equality at position `eqInd`.
///
/// For example:
///     32*k == 16*i + j - 31                 <-- `eqInd` for 'k'
///     expr = 16*i + j - 31, divisor = 32
///     k = (16*i + j - 31) floordiv 32
///
/// If successful, `expr` is set to dividend of the division and `divisor` is
/// set to the denominator of the division. The final division expression is
/// normalized by GCD.
static LogicalResult getDivRepr(const IntegerRelation &cst, unsigned pos,
                                unsigned eqInd,
                                MutableArrayRef<DynamicAPInt> expr,
                                DynamicAPInt &divisor) {

  assert(pos <= cst.getNumVars() && "Invalid variable position");
  assert(eqInd <= cst.getNumEqualities() && "Invalid equality position");
  assert(expr.size() == cst.getNumCols() && "Invalid expression size");

  // Extract divisor, the divisor can be negative and hence its sign information
  // is stored in `signDiv` to reverse the sign of dividend's coefficients.
  // Equality must involve the pos-th variable and hence `tempDiv` != 0.
  DynamicAPInt tempDiv = cst.atEq(eqInd, pos);
  if (tempDiv == 0)
    return failure();
  int signDiv = tempDiv < 0 ? -1 : 1;

  // The divisor is always a positive integer.
  divisor = tempDiv * signDiv;

  for (unsigned i = 0, e = cst.getNumVars(); i < e; ++i)
    if (i != pos)
      expr[i] = -signDiv * cst.atEq(eqInd, i);

  expr.back() = -signDiv * cst.atEq(eqInd, cst.getNumCols() - 1);
  normalizeDivisionByGCD(expr, divisor);

  return success();
}

// Returns `false` if the constraints depends on a variable for which an
// explicit representation has not been found yet, otherwise returns `true`.
static bool checkExplicitRepresentation(const IntegerRelation &cst,
                                        ArrayRef<bool> foundRepr,
                                        ArrayRef<DynamicAPInt> dividend,
                                        unsigned pos) {
  // Exit to avoid circular dependencies between divisions.
  for (unsigned c = 0, e = cst.getNumVars(); c < e; ++c) {
    if (c == pos)
      continue;

    if (!foundRepr[c] && dividend[c] != 0) {
      // Expression can't be constructed as it depends on a yet unknown
      // variable.
      //
      // TODO: Visit/compute the variables in an order so that this doesn't
      // happen. More complex but much more efficient.
      return false;
    }
  }

  return true;
}

/// Check if the pos^th variable can be expressed as a floordiv of an affine
/// function of other variables (where the divisor is a positive constant).
/// `foundRepr` contains a boolean for each variable indicating if the
/// explicit representation for that variable has already been computed.
/// Returns the `MaybeLocalRepr` struct which contains the indices of the
/// constraints that can be expressed as a floordiv of an affine function. If
/// the representation could be computed, `dividend` and `denominator` are set.
/// If the representation could not be computed, the kind attribute in
/// `MaybeLocalRepr` is set to None.
MaybeLocalRepr presburger::computeSingleVarRepr(
    const IntegerRelation &cst, ArrayRef<bool> foundRepr, unsigned pos,
    MutableArrayRef<DynamicAPInt> dividend, DynamicAPInt &divisor) {
  assert(pos < cst.getNumVars() && "invalid position");
  assert(foundRepr.size() == cst.getNumVars() &&
         "Size of foundRepr does not match total number of variables");
  assert(dividend.size() == cst.getNumCols() && "Invalid dividend size");

  SmallVector<unsigned, 4> lbIndices, ubIndices, eqIndices;
  cst.getLowerAndUpperBoundIndices(pos, &lbIndices, &ubIndices, &eqIndices);
  MaybeLocalRepr repr{};

  for (unsigned ubPos : ubIndices) {
    for (unsigned lbPos : lbIndices) {
      // Attempt to get divison representation from ubPos, lbPos.
      if (getDivRepr(cst, pos, ubPos, lbPos, dividend, divisor).failed())
        continue;

      if (!checkExplicitRepresentation(cst, foundRepr, dividend, pos))
        continue;

      repr.kind = ReprKind::Inequality;
      repr.repr.inequalityPair = {ubPos, lbPos};
      return repr;
    }
  }
  for (unsigned eqPos : eqIndices) {
    // Attempt to get divison representation from eqPos.
    if (getDivRepr(cst, pos, eqPos, dividend, divisor).failed())
      continue;

    if (!checkExplicitRepresentation(cst, foundRepr, dividend, pos))
      continue;

    repr.kind = ReprKind::Equality;
    repr.repr.equalityIdx = eqPos;
    return repr;
  }
  return repr;
}

MaybeLocalRepr presburger::computeSingleVarRepr(
    const IntegerRelation &cst, ArrayRef<bool> foundRepr, unsigned pos,
    SmallVector<int64_t, 8> &dividend, unsigned &divisor) {
  SmallVector<DynamicAPInt, 8> dividendDynamicAPInt(cst.getNumCols());
  DynamicAPInt divisorDynamicAPInt;
  MaybeLocalRepr result = computeSingleVarRepr(
      cst, foundRepr, pos, dividendDynamicAPInt, divisorDynamicAPInt);
  dividend = getInt64Vec(dividendDynamicAPInt);
  divisor = unsigned(int64_t(divisorDynamicAPInt));
  return result;
}

llvm::SmallBitVector presburger::getSubrangeBitVector(unsigned len,
                                                      unsigned setOffset,
                                                      unsigned numSet) {
  llvm::SmallBitVector vec(len, false);
  vec.set(setOffset, setOffset + numSet);
  return vec;
}

void presburger::mergeLocalVars(
    IntegerRelation &relA, IntegerRelation &relB,
    llvm::function_ref<bool(unsigned i, unsigned j)> merge) {
  assert(relA.getSpace().isCompatible(relB.getSpace()) &&
         "Spaces should be compatible.");

  // Merge local vars of relA and relB without using division information,
  // i.e. append local vars of `relB` to `relA` and insert local vars of `relA`
  // to `relB` at start of its local vars.
  unsigned initLocals = relA.getNumLocalVars();
  relA.insertVar(VarKind::Local, relA.getNumLocalVars(),
                 relB.getNumLocalVars());
  relB.insertVar(VarKind::Local, 0, initLocals);

  // Get division representations from each rel.
  DivisionRepr divsA = relA.getLocalReprs();
  DivisionRepr divsB = relB.getLocalReprs();

  for (unsigned i = initLocals, e = divsB.getNumDivs(); i < e; ++i)
    divsA.setDiv(i, divsB.getDividend(i), divsB.getDenom(i));

  // Remove duplicate divisions from divsA. The removing duplicate divisions
  // call, calls `merge` to effectively merge divisions in relA and relB.
  divsA.removeDuplicateDivs(merge);
}

SmallVector<DynamicAPInt, 8>
presburger::getDivUpperBound(ArrayRef<DynamicAPInt> dividend,
                             const DynamicAPInt &divisor,
                             unsigned localVarIdx) {
  assert(divisor > 0 && "divisor must be positive!");
  assert(dividend[localVarIdx] == 0 &&
         "Local to be set to division must have zero coeff!");
  SmallVector<DynamicAPInt, 8> ineq(dividend);
  ineq[localVarIdx] = -divisor;
  return ineq;
}

SmallVector<DynamicAPInt, 8>
presburger::getDivLowerBound(ArrayRef<DynamicAPInt> dividend,
                             const DynamicAPInt &divisor,
                             unsigned localVarIdx) {
  assert(divisor > 0 && "divisor must be positive!");
  assert(dividend[localVarIdx] == 0 &&
         "Local to be set to division must have zero coeff!");
  SmallVector<DynamicAPInt, 8> ineq(dividend.size());
  std::transform(dividend.begin(), dividend.end(), ineq.begin(),
                 std::negate<DynamicAPInt>());
  ineq[localVarIdx] = divisor;
  ineq.back() += divisor - 1;
  return ineq;
}

DynamicAPInt presburger::gcdRange(ArrayRef<DynamicAPInt> range) {
  DynamicAPInt gcd(0);
  for (const DynamicAPInt &elem : range) {
    gcd = llvm::gcd(gcd, abs(elem));
    if (gcd == 1)
      return gcd;
  }
  return gcd;
}

DynamicAPInt presburger::normalizeRange(MutableArrayRef<DynamicAPInt> range) {
  DynamicAPInt gcd = gcdRange(range);
  if ((gcd == 0) || (gcd == 1))
    return gcd;
  for (DynamicAPInt &elem : range)
    elem /= gcd;
  return gcd;
}

void presburger::normalizeDiv(MutableArrayRef<DynamicAPInt> num,
                              DynamicAPInt &denom) {
  assert(denom > 0 && "denom must be positive!");
  DynamicAPInt gcd = llvm::gcd(gcdRange(num), denom);
  if (gcd == 1)
    return;
  for (DynamicAPInt &coeff : num)
    coeff /= gcd;
  denom /= gcd;
}

SmallVector<DynamicAPInt, 8>
presburger::getNegatedCoeffs(ArrayRef<DynamicAPInt> coeffs) {
  SmallVector<DynamicAPInt, 8> negatedCoeffs;
  negatedCoeffs.reserve(coeffs.size());
  for (const DynamicAPInt &coeff : coeffs)
    negatedCoeffs.emplace_back(-coeff);
  return negatedCoeffs;
}

SmallVector<DynamicAPInt, 8>
presburger::getComplementIneq(ArrayRef<DynamicAPInt> ineq) {
  SmallVector<DynamicAPInt, 8> coeffs;
  coeffs.reserve(ineq.size());
  for (const DynamicAPInt &coeff : ineq)
    coeffs.emplace_back(-coeff);
  --coeffs.back();
  return coeffs;
}

SmallVector<std::optional<DynamicAPInt>, 4>
DivisionRepr::divValuesAt(ArrayRef<DynamicAPInt> point) const {
  assert(point.size() == getNumNonDivs() && "Incorrect point size");

  SmallVector<std::optional<DynamicAPInt>, 4> divValues(getNumDivs(),
                                                        std::nullopt);
  bool changed = true;
  while (changed) {
    changed = false;
    for (unsigned i = 0, e = getNumDivs(); i < e; ++i) {
      // If division value is found, continue;
      if (divValues[i])
        continue;

      ArrayRef<DynamicAPInt> dividend = getDividend(i);
      DynamicAPInt divVal(0);

      // Check if we have all the division values required for this division.
      unsigned j, f;
      for (j = 0, f = getNumDivs(); j < f; ++j) {
        if (dividend[getDivOffset() + j] == 0)
          continue;
        // Division value required, but not found yet.
        if (!divValues[j])
          break;
        divVal += dividend[getDivOffset() + j] * *divValues[j];
      }

      // We have some division values that are still not found, but are required
      // to find the value of this division.
      if (j < f)
        continue;

      // Fill remaining values.
      divVal = std::inner_product(point.begin(), point.end(), dividend.begin(),
                                  divVal);
      // Add constant.
      divVal += dividend.back();
      // Take floor division with denominator.
      divVal = floorDiv(divVal, denoms[i]);

      // Set div value and continue.
      divValues[i] = divVal;
      changed = true;
    }
  }

  return divValues;
}

void DivisionRepr::removeDuplicateDivs(
    llvm::function_ref<bool(unsigned i, unsigned j)> merge) {

  // Find and merge duplicate divisions.
  // TODO: Add division normalization to support divisions that differ by
  // a constant.
  // TODO: Add division ordering such that a division representation for local
  // variable at position `i` only depends on local variables at position <
  // `i`. This would make sure that all divisions depending on other local
  // variables that can be merged, are merged.
  normalizeDivs();
  for (unsigned i = 0; i < getNumDivs(); ++i) {
    // Check if a division representation exists for the `i^th` local var.
    if (denoms[i] == 0)
      continue;
    // Check if a division exists which is a duplicate of the division at `i`.
    for (unsigned j = i + 1; j < getNumDivs(); ++j) {
      // Check if a division representation exists for the `j^th` local var.
      if (denoms[j] == 0)
        continue;
      // Check if the denominators match.
      if (denoms[i] != denoms[j])
        continue;
      // Check if the representations are equal.
      if (dividends.getRow(i) != dividends.getRow(j))
        continue;

      // Merge divisions at position `j` into division at position `i`. If
      // merge fails, do not merge these divs.
      bool mergeResult = merge(i, j);
      if (!mergeResult)
        continue;

      // Update division information to reflect merging.
      unsigned divOffset = getDivOffset();
      dividends.addToColumn(divOffset + j, divOffset + i, /*scale=*/1);
      dividends.removeColumn(divOffset + j);
      dividends.removeRow(j);
      denoms.erase(denoms.begin() + j);

      // Since `j` can never be zero, we do not need to worry about overflows.
      --j;
    }
  }
}

void DivisionRepr::normalizeDivs() {
  for (unsigned i = 0, e = getNumDivs(); i < e; ++i) {
    if (getDenom(i) == 0 || getDividend(i).empty())
      continue;
    normalizeDiv(getDividend(i), getDenom(i));
  }
}

void DivisionRepr::insertDiv(unsigned pos, ArrayRef<DynamicAPInt> dividend,
                             const DynamicAPInt &divisor) {
  assert(pos <= getNumDivs() && "Invalid insertion position");
  assert(dividend.size() == getNumVars() + 1 && "Incorrect dividend size");

  dividends.appendExtraRow(dividend);
  denoms.insert(denoms.begin() + pos, divisor);
  dividends.insertColumn(getDivOffset() + pos);
}

void DivisionRepr::insertDiv(unsigned pos, unsigned num) {
  assert(pos <= getNumDivs() && "Invalid insertion position");
  dividends.insertColumns(getDivOffset() + pos, num);
  dividends.insertRows(pos, num);
  denoms.insert(denoms.begin() + pos, num, DynamicAPInt(0));
}

void DivisionRepr::print(raw_ostream &os) const {
  os << "Dividends:\n";
  dividends.print(os);
  os << "Denominators\n";
  for (const DynamicAPInt &denom : denoms)
    os << denom << " ";
  os << "\n";
}

void DivisionRepr::dump() const { print(llvm::errs()); }

SmallVector<DynamicAPInt, 8>
presburger::getDynamicAPIntVec(ArrayRef<int64_t> range) {
  SmallVector<DynamicAPInt, 8> result(range.size());
  std::transform(range.begin(), range.end(), result.begin(),
                 dynamicAPIntFromInt64);
  return result;
}

SmallVector<int64_t, 8> presburger::getInt64Vec(ArrayRef<DynamicAPInt> range) {
  SmallVector<int64_t, 8> result(range.size());
  std::transform(range.begin(), range.end(), result.begin(),
                 int64fromDynamicAPInt);
  return result;
}

Fraction presburger::dotProduct(ArrayRef<Fraction> a, ArrayRef<Fraction> b) {
  assert(a.size() == b.size() &&
         "dot product is only valid for vectors of equal sizes!");
  Fraction sum = 0;
  for (unsigned i = 0, e = a.size(); i < e; i++)
    sum += a[i] * b[i];
  return sum;
}

/// Find the product of two polynomials, each given by an array of
/// coefficients, by taking the convolution.
std::vector<Fraction> presburger::multiplyPolynomials(ArrayRef<Fraction> a,
                                                      ArrayRef<Fraction> b) {
  // The length of the convolution is the sum of the lengths
  // of the two sequences. We pad the shorter one with zeroes.
  unsigned len = a.size() + b.size() - 1;

  // We define accessors to avoid out-of-bounds errors.
  auto getCoeff = [](ArrayRef<Fraction> arr, unsigned i) -> Fraction {
    if (i < arr.size())
      return arr[i];
    return 0;
  };

  std::vector<Fraction> convolution;
  convolution.reserve(len);
  for (unsigned k = 0; k < len; ++k) {
    Fraction sum(0, 1);
    for (unsigned l = 0; l <= k; ++l)
      sum += getCoeff(a, l) * getCoeff(b, k - l);
    convolution.emplace_back(sum);
  }
  return convolution;
}

bool presburger::isRangeZero(ArrayRef<Fraction> arr) {
  return llvm::all_of(arr, [](const Fraction &f) { return f == 0; });
}
