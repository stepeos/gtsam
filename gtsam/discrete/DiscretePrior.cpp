/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 *  @file DiscretePrior.cpp
 *  @date December 2021
 *  @author Frank Dellaert
 */

#include <gtsam/discrete/DiscretePrior.h>

namespace gtsam {

void DiscretePrior::print(const std::string& s,
                          const KeyFormatter& formatter) const {
  Base::print(s, formatter);
}

double DiscretePrior::operator()(size_t value) const {
  if (nrFrontals() != 1)
    throw std::invalid_argument(
        "Single value operator can only be invoked on single-variable "
        "priors");
  DiscreteValues values;
  values.emplace(keys_[0], value);
  return Base::operator()(values);
}

std::vector<double> DiscretePrior::pmf() const {
  if (nrFrontals() != 1)
    throw std::invalid_argument(
        "DiscretePrior::pmf only defined for single-variable priors");
  const size_t nrValues = cardinalities_.at(keys_[0]);
  std::vector<double> array;
  array.reserve(nrValues);
  for (size_t v = 0; v < nrValues; v++) {
    array.push_back(operator()(v));
  }
  return array;
}

}  // namespace gtsam
