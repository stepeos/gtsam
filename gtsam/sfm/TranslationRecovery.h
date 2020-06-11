/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010-2020, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file TranslationRecovery.h
 * @author Frank Dellaert
 * @date March 2020
 * @brief test recovering translations when rotations are given.
 */

#include <gtsam/geometry/Unit3.h>
#include <gtsam/nonlinear/LevenbergMarquardtOptimizer.h>
#include <gtsam/nonlinear/Values.h>

#include <map>
#include <utility>

namespace gtsam {

// Set up an optimization problem for the unknown translations Ti in the world
// coordinate frame, given the known camera attitudes wRi with respect to the
// world frame, and a set of (noisy) translation directions of type Unit3,
// w_aZb. The measurement equation is
//    w_aZb = Unit3(Tb - Ta)   (1)
// i.e., w_aZb is the translation direction from frame A to B, in world
// coordinates. Although Unit3 instances live on a manifold, following
// Wilson14eccv_1DSfM.pdf error we compute the *chordal distance* in the
// ambient world coordinate frame.
//
// It is clear that we cannot recover the scale, nor the absolute position,
// so the gauge freedom in this case is 3 + 1 = 4. We fix these by taking fixing
// the translations Ta and Tb associated with the first measurement w_aZb,
// clamping them to their initial values as given to this method. If no initial
// values are given, we use the origin for Tb and set Tb to make (1) come
// through, i.e.,
//    Tb = s * wRa * Point3(w_aZb)     (2)
// where s is an arbitrary scale that can be supplied, default 1.0. Hence, two
// versions are supplied below corresponding to whether we have initial values
// or not.
class TranslationRecovery {
 public:
  using KeyPair = std::pair<Key, Key>;
  using TranslationEdges = std::map<KeyPair, Unit3>;

 private:
  TranslationEdges relativeTranslations_;
  LevenbergMarquardtParams params_;

 public:
  /**
   * @brief Construct a new Translation Recovery object
   *
   * @param relativeTranslations the relative translations, in world coordinate
   * frames, indexed in a map by a pair of Pose keys.
   * @param lmParams optional LevenbergMarquardtParams object. Can be used to
   * change the optimization parameters if necessary.
   */
  TranslationRecovery(const TranslationEdges &relativeTranslations,
                      const LevenbergMarquardtParams &lmParams = LevenbergMarquardtParams())
      : relativeTranslations_(relativeTranslations), params_(lmParams) {
    params_.setVerbosityLM("Summary");
  }

  /**
   * @brief Build the factor graph to do the optimization.
   *
   * @return NonlinearFactorGraph
   */
  NonlinearFactorGraph buildGraph() const;

  /**
   * @brief Add priors on ednpoints of first measurement edge.
   *
   * @param scale scale for first relative translation which fixes gauge.
   * @param graph factor graph to which prior is added.
   */
  void addPrior(const double scale, NonlinearFactorGraph::shared_ptr graph) const;

  /**
   * @brief Create random initial translations.
   *
   * @return Values
   */
  Values initalizeRandomly() const;

  /**
   * @brief Build and optimize factor graph.
   *
   * @param scale scale for first relative translation which fixes gauge.
   * @return Values
   */
  Values run(const double scale = 1.0) const;

  /**
   * @brief Simulate translation direction measurements
   *
   * @param poses SE(3) ground truth poses stored as Values
   * @param edges pairs (a,b) for which a measurement w_aZb will be generated.
   */
  static TranslationEdges SimulateMeasurements(
      const Values &poses, const std::vector<KeyPair> &edges);
};
}  // namespace gtsam
