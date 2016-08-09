/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file    NonlinearInequalityFactorGraph.h
 * @author  Duy-Nguyen Ta
 * @author  Krunal Chande
 * @author  Luca Carlone
 * @date    Dec 15, 2014
 */

#pragma once
#include <gtsam/linear/VectorValues.h>
#include <gtsam_unstable/linear/InequalityFactorGraph.h>
#include <gtsam_unstable/nonlinear/NonlinearEqualityConstraint.h>

namespace gtsam {
class NonlinearInequalityFactorGraph: public FactorGraph<NonlinearFactor> {

public:
  /// Default constructor
  NonlinearInequalityFactorGraph() {
  }

  /// Linearize to a InequalityFactorGraph
  InequalityFactorGraph::shared_ptr linearize(
      const Values& linearizationPoint) const {
    InequalityFactorGraph::shared_ptr linearGraph(new InequalityFactorGraph());
    for (const NonlinearFactor::shared_ptr& factor : *this) {
      JacobianFactor::shared_ptr jacobian = boost::dynamic_pointer_cast
          < JacobianFactor > (factor->linearize(linearizationPoint));
      NonlinearEqualityConstraint::shared_ptr constraint =
          boost::dynamic_pointer_cast < NonlinearEqualityConstraint > (factor);
      linearGraph->add(LinearInequality(*jacobian, constraint->dualKey()));
    }
    return linearGraph;
  }

  /**
   * Return true if the all errors are <= 0.0
   */
  bool checkFeasibilityAndComplimentary(const Values& values,
      const VectorValues& duals, double tol) const {
    for (const NonlinearFactor::shared_ptr& factor : *this) {
      NoiseModelFactor::shared_ptr noiseModelFactor =
          boost::dynamic_pointer_cast < NoiseModelFactor > (factor);
      Vector error = noiseModelFactor->unwhitenedError(values);

      // Primal feasibility condition: all constraints need to be <= 0.0
      if (error[0] > tol) {
        return false;
      }

      // Complimentary condition: errors of active constraints need to be 0.0
      NonlinearEqualityConstraint::shared_ptr constraint =
          boost::dynamic_pointer_cast < NonlinearEqualityConstraint > (factor);
      Key dualKey = constraint->dualKey();
      if (!duals.exists(dualKey))
        continue; // if dualKey doesn't exist, it is an inactive constraint!
      if (fabs(error[0]) > tol) // for active constraint, the error should be 0.0
        return false;

    }
    return true;
  }
};
}