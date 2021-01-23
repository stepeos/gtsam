/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file    testGncOptimizer.cpp
 * @brief   Unit tests for GncOptimizer class
 * @author  Jingnan Shi
 * @author  Luca Carlone
 * @author  Frank Dellaert
 *
 * Implementation of the paper: Yang, Antonante, Tzoumas, Carlone, "Graduated
 * Non-Convexity for Robust Spatial Perception: From Non-Minimal Solvers to
 * Global Outlier Rejection", ICRA/RAL, 2020. (arxiv version:
 * https://arxiv.org/pdf/1909.08605.pdf)
 *
 * See also:
 * Antonante, Tzoumas, Yang, Carlone, "Outlier-Robust Estimation: Hardness,
 * Minimally-Tuned Algorithms, and Applications", arxiv:
 * https://arxiv.org/pdf/2007.15109.pdf, 2020.
 */

#include <CppUnitLite/TestHarness.h>
#include <gtsam/nonlinear/GncOptimizer.h>
#include <gtsam/nonlinear/LinearContainerFactor.h>
#include <gtsam/slam/dataset.h>
#include <tests/smallExample.h>

using namespace std;
using namespace gtsam;

using symbol_shorthand::L;
using symbol_shorthand::X;
static double tol = 1e-7;

/* ************************************************************************* */
TEST(GncOptimizer, gncParamsConstructor) {
  // check params are correctly parsed
  LevenbergMarquardtParams lmParams;
  GncParams<LevenbergMarquardtParams> gncParams1(lmParams);
  CHECK(lmParams.equals(gncParams1.baseOptimizerParams));

  // check also default constructor
  GncParams<LevenbergMarquardtParams> gncParams1b;
  CHECK(lmParams.equals(gncParams1b.baseOptimizerParams));

  // and check params become different if we change lmParams
  lmParams.setVerbosity("DELTA");
  CHECK(!lmParams.equals(gncParams1.baseOptimizerParams));

  // and same for GN
  GaussNewtonParams gnParams;
  GncParams<GaussNewtonParams> gncParams2(gnParams);
  CHECK(gnParams.equals(gncParams2.baseOptimizerParams));

  // check default constructor
  GncParams<GaussNewtonParams> gncParams2b;
  CHECK(gnParams.equals(gncParams2b.baseOptimizerParams));

  // change something at the gncParams level
  GncParams<GaussNewtonParams> gncParams2c(gncParams2b);
  gncParams2c.setLossType(GncLossType::GM);
  CHECK(!gncParams2c.equals(gncParams2b.baseOptimizerParams));
}

/* ************************************************************************* */
TEST(GncOptimizer, gncConstructor) {
  // has to have Gaussian noise models !
  auto fg = example::createReallyNonlinearFactorGraph();  // just a unary factor
                                                          // on a 2D point

  Point2 p0(3, 3);
  Values initial;
  initial.insert(X(1), p0);

  GncParams<LevenbergMarquardtParams> gncParams;
  auto gnc = GncOptimizer<GncParams<LevenbergMarquardtParams>>(fg, initial,
                                                               gncParams);

  CHECK(gnc.getFactors().equals(fg));
  CHECK(gnc.getState().equals(initial));
  CHECK(gnc.getParams().equals(gncParams));

  auto gnc2 = GncOptimizer<GncParams<LevenbergMarquardtParams>>(fg, initial,
                                                                 gncParams);

  // check the equal works
  CHECK(gnc.equals(gnc2));
}

/* ************************************************************************* */
TEST(GncOptimizer, gncConstructorWithRobustGraphAsInput) {
  auto fg = example::sharedNonRobustFactorGraphWithOutliers();
  // same graph with robust noise model
  auto fg_robust = example::sharedRobustFactorGraphWithOutliers();

  Point2 p0(3, 3);
  Values initial;
  initial.insert(X(1), p0);

  GncParams<LevenbergMarquardtParams> gncParams;
  auto gnc = GncOptimizer<GncParams<LevenbergMarquardtParams>>(fg_robust,
                                                               initial,
                                                               gncParams);

  // make sure that when parsing the graph is transformed into one without
  // robust loss
  CHECK(fg.equals(gnc.getFactors()));
}

/* ************************************************************************* */
TEST(GncOptimizer, initializeMu) {
  auto fg = example::createReallyNonlinearFactorGraph();

  Point2 p0(3, 3);
  Values initial;
  initial.insert(X(1), p0);

  // testing GM mu initialization
  GncParams<LevenbergMarquardtParams> gncParams;
  gncParams.setLossType(GncLossType::GM);
  auto gnc_gm = GncOptimizer<GncParams<LevenbergMarquardtParams>>(fg, initial,
                                                                  gncParams);
  // according to rmk 5 in the gnc paper: m0 = 2 rmax^2 / barcSq
  // (barcSq=1 in this example)
  EXPECT_DOUBLES_EQUAL(gnc_gm.initializeMu(), 2 * 198.999, 1e-3);

  // testing TLS mu initialization
  gncParams.setLossType(GncLossType::TLS);
  auto gnc_tls = GncOptimizer<GncParams<LevenbergMarquardtParams>>(fg, initial,
                                                                   gncParams);
  // according to rmk 5 in the gnc paper: m0 =  barcSq / (2 * rmax^2 - barcSq)
  // (barcSq=1 in this example)
  EXPECT_DOUBLES_EQUAL(gnc_tls.initializeMu(), 1 / (2 * 198.999 - 1), 1e-3);
}

/* ************************************************************************* */
TEST(GncOptimizer, updateMuGM) {
  // has to have Gaussian noise models !
  auto fg = example::createReallyNonlinearFactorGraph();

  Point2 p0(3, 3);
  Values initial;
  initial.insert(X(1), p0);

  GncParams<LevenbergMarquardtParams> gncParams;
  gncParams.setLossType(GncLossType::GM);
  gncParams.setMuStep(1.4);
  auto gnc = GncOptimizer<GncParams<LevenbergMarquardtParams>>(fg, initial,
                                                               gncParams);

  double mu = 5.0;
  EXPECT_DOUBLES_EQUAL(gnc.updateMu(mu), mu / 1.4, tol);

  // check it correctly saturates to 1 for GM
  mu = 1.2;
  EXPECT_DOUBLES_EQUAL(gnc.updateMu(mu), 1.0, tol);
}

/* ************************************************************************* */
TEST(GncOptimizer, updateMuTLS) {
  // has to have Gaussian noise models !
  auto fg = example::createReallyNonlinearFactorGraph();

  Point2 p0(3, 3);
  Values initial;
  initial.insert(X(1), p0);

  GncParams<LevenbergMarquardtParams> gncParams;
  gncParams.setMuStep(1.4);
  gncParams.setLossType(GncLossType::TLS);
  auto gnc = GncOptimizer<GncParams<LevenbergMarquardtParams>>(fg, initial,
                                                               gncParams);

  double mu = 5.0;
  EXPECT_DOUBLES_EQUAL(gnc.updateMu(mu), mu * 1.4, tol);
}

/* ************************************************************************* */
TEST(GncOptimizer, checkMuConvergence) {
  // has to have Gaussian noise models !
  auto fg = example::createReallyNonlinearFactorGraph();

  Point2 p0(3, 3);
  Values initial;
  initial.insert(X(1), p0);

  {
    GncParams<LevenbergMarquardtParams> gncParams;
    gncParams.setLossType(GncLossType::GM);
    auto gnc = GncOptimizer<GncParams<LevenbergMarquardtParams>>(fg, initial,
                                                                 gncParams);

    double mu = 1.0;
    CHECK(gnc.checkMuConvergence(mu));
  }
  {
    GncParams<LevenbergMarquardtParams> gncParams;
    gncParams.setLossType(
        GncLossType::TLS);
    auto gnc = GncOptimizer<GncParams<LevenbergMarquardtParams>>(fg, initial,
                                                                 gncParams);

    double mu = 1.0;
    CHECK(!gnc.checkMuConvergence(mu));  //always false for TLS
  }
}

/* ************************************************************************* */
TEST(GncOptimizer, checkCostConvergence) {
  // has to have Gaussian noise models !
  auto fg = example::createReallyNonlinearFactorGraph();

  Point2 p0(3, 3);
  Values initial;
  initial.insert(X(1), p0);

  {
    GncParams<LevenbergMarquardtParams> gncParams;
    gncParams.setRelativeCostTol(0.49);
    auto gnc = GncOptimizer<GncParams<LevenbergMarquardtParams>>(fg, initial,
                                                                 gncParams);

    double prev_cost = 1.0;
    double cost = 0.5;
    // relative cost reduction = 0.5 > 0.49, hence checkCostConvergence = false
    CHECK(!gnc.checkCostConvergence(cost, prev_cost));
  }
  {
    GncParams<LevenbergMarquardtParams> gncParams;
    gncParams.setRelativeCostTol(0.51);
    auto gnc = GncOptimizer<GncParams<LevenbergMarquardtParams>>(fg, initial,
                                                                 gncParams);

    double prev_cost = 1.0;
    double cost = 0.5;
    // relative cost reduction = 0.5 < 0.51, hence checkCostConvergence = true
    CHECK(gnc.checkCostConvergence(cost, prev_cost));
  }
}

/* ************************************************************************* */
TEST(GncOptimizer, checkWeightsConvergence) {
  // has to have Gaussian noise models !
  auto fg = example::createReallyNonlinearFactorGraph();

  Point2 p0(3, 3);
  Values initial;
  initial.insert(X(1), p0);

  {
    GncParams<LevenbergMarquardtParams> gncParams;
    gncParams.setLossType(GncLossType::GM);
    auto gnc = GncOptimizer<GncParams<LevenbergMarquardtParams>>(fg, initial,
                                                                 gncParams);

    Vector weights = Vector::Ones(fg.size());
    CHECK(!gnc.checkWeightsConvergence(weights));  //always false for GM
  }
  {
    GncParams<LevenbergMarquardtParams> gncParams;
    gncParams.setLossType(
        GncLossType::TLS);
    auto gnc = GncOptimizer<GncParams<LevenbergMarquardtParams>>(fg, initial,
                                                                 gncParams);

    Vector weights = Vector::Ones(fg.size());
    // weights are binary, so checkWeightsConvergence = true
    CHECK(gnc.checkWeightsConvergence(weights));
  }
  {
    GncParams<LevenbergMarquardtParams> gncParams;
    gncParams.setLossType(
        GncLossType::TLS);
    auto gnc = GncOptimizer<GncParams<LevenbergMarquardtParams>>(fg, initial,
                                                                 gncParams);

    Vector weights = Vector::Ones(fg.size());
    weights[0] = 0.9;  // more than weightsTol = 1e-4 from 1, hence checkWeightsConvergence = false
    CHECK(!gnc.checkWeightsConvergence(weights));
  }
  {
    GncParams<LevenbergMarquardtParams> gncParams;
    gncParams.setLossType(
        GncLossType::TLS);
    gncParams.setWeightsTol(0.1);
    auto gnc = GncOptimizer<GncParams<LevenbergMarquardtParams>>(fg, initial,
                                                                 gncParams);

    Vector weights = Vector::Ones(fg.size());
    weights[0] = 0.9;  // exactly weightsTol = 0.1 from 1, hence checkWeightsConvergence = true
    CHECK(gnc.checkWeightsConvergence(weights));
  }
}

/* ************************************************************************* */
TEST(GncOptimizer, checkConvergenceTLS) {
  // has to have Gaussian noise models !
  auto fg = example::createReallyNonlinearFactorGraph();

  Point2 p0(3, 3);
  Values initial;
  initial.insert(X(1), p0);

  GncParams<LevenbergMarquardtParams> gncParams;
  gncParams.setRelativeCostTol(1e-5);
  gncParams.setLossType(GncLossType::TLS);
  auto gnc = GncOptimizer<GncParams<LevenbergMarquardtParams>>(fg, initial,
                                                               gncParams);

  CHECK(gnc.checkCostConvergence(1.0, 1.0));
  CHECK(!gnc.checkCostConvergence(1.0, 2.0));
}

/* ************************************************************************* */
TEST(GncOptimizer, calculateWeightsGM) {
  auto fg = example::sharedNonRobustFactorGraphWithOutliers();

  Point2 p0(0, 0);
  Values initial;
  initial.insert(X(1), p0);

  // we have 4 factors, 3 with zero errors (inliers), 1 with error 50 = 0.5 *
  // 1/sigma^2 || [1;0] - [0;0] ||^2 (outlier)
  Vector weights_expected = Vector::Zero(4);
  weights_expected[0] = 1.0;                             // zero error
  weights_expected[1] = 1.0;                             // zero error
  weights_expected[2] = 1.0;                             // zero error
  weights_expected[3] = std::pow(1.0 / (50.0 + 1.0), 2);  // outlier, error = 50

  GaussNewtonParams gnParams;
  GncParams<GaussNewtonParams> gncParams(gnParams);
  gncParams.setLossType(GncLossType::GM);
  auto gnc = GncOptimizer<GncParams<GaussNewtonParams>>(fg, initial, gncParams);
  double mu = 1.0;
  Vector weights_actual = gnc.calculateWeights(initial, mu);
  CHECK(assert_equal(weights_expected, weights_actual, tol));

  mu = 2.0;
  double barcSq = 5.0;
  weights_expected[3] = std::pow(mu * barcSq / (50.0 + mu * barcSq), 2);  // outlier, error = 50

  auto gnc2 = GncOptimizer<GncParams<GaussNewtonParams>>(fg, initial,
                                                         gncParams);
  gnc2.setInlierCostThresholds(barcSq);
  weights_actual = gnc2.calculateWeights(initial, mu);
  CHECK(assert_equal(weights_expected, weights_actual, tol));
}

/* ************************************************************************* */
TEST(GncOptimizer, calculateWeightsTLS) {
  auto fg = example::sharedNonRobustFactorGraphWithOutliers();

  Point2 p0(0, 0);
  Values initial;
  initial.insert(X(1), p0);

  // we have 4 factors, 3 with zero errors (inliers), 1 with error
  Vector weights_expected = Vector::Zero(4);
  weights_expected[0] = 1.0;                             // zero error
  weights_expected[1] = 1.0;                             // zero error
  weights_expected[2] = 1.0;                             // zero error
  weights_expected[3] = 0;                               // outliers

  GaussNewtonParams gnParams;
  GncParams<GaussNewtonParams> gncParams(gnParams);
  gncParams.setLossType(GncLossType::TLS);
  auto gnc = GncOptimizer<GncParams<GaussNewtonParams>>(fg, initial, gncParams);
  double mu = 1.0;
  Vector weights_actual = gnc.calculateWeights(initial, mu);
  CHECK(assert_equal(weights_expected, weights_actual, tol));
}

/* ************************************************************************* */
TEST(GncOptimizer, calculateWeightsTLS2) {

  // create values
  Point2 x_val(0.0, 0.0);
  Point2 x_prior(1.0, 0.0);
  Values initial;
  initial.insert(X(1), x_val);

  // create very simple factor graph with a single factor 0.5 * 1/sigma^2 * || x - [1;0] ||^2
  double sigma = 1;
  SharedDiagonal noise = noiseModel::Diagonal::Sigmas(Vector2(sigma, sigma));
  NonlinearFactorGraph nfg;
  nfg.add(PriorFactor<Point2>(X(1), x_prior, noise));

  // cost of the factor:
  DOUBLES_EQUAL(0.5 * 1 / (sigma * sigma), nfg.error(initial), tol);

  // check the TLS weights are correct: CASE 1: residual below barcsq
  {
    // expected:
    Vector weights_expected = Vector::Zero(1);
    weights_expected[0] = 1.0;  // inlier
    // actual:
    GaussNewtonParams gnParams;
    GncParams<GaussNewtonParams> gncParams(gnParams);
    gncParams.setLossType(GncLossType::TLS);
    auto gnc = GncOptimizer<GncParams<GaussNewtonParams>>(nfg, initial,
                                                          gncParams);
    gnc.setInlierCostThresholds(0.51);  // if inlier threshold is slightly larger than 0.5, then measurement is inlier

    double mu = 1e6;
    Vector weights_actual = gnc.calculateWeights(initial, mu);
    CHECK(assert_equal(weights_expected, weights_actual, tol));
  }
  // check the TLS weights are correct: CASE 2: residual above barcsq
  {
    // expected:
    Vector weights_expected = Vector::Zero(1);
    weights_expected[0] = 0.0;  // outlier
    // actual:
    GaussNewtonParams gnParams;
    GncParams<GaussNewtonParams> gncParams(gnParams);
    gncParams.setLossType(GncLossType::TLS);
    auto gnc = GncOptimizer<GncParams<GaussNewtonParams>>(nfg, initial,
                                                          gncParams);
    gnc.setInlierCostThresholds(0.49);  // if inlier threshold is slightly below 0.5, then measurement is outlier
    double mu = 1e6;  // very large mu recovers original TLS cost
    Vector weights_actual = gnc.calculateWeights(initial, mu);
    CHECK(assert_equal(weights_expected, weights_actual, tol));
  }
  // check the TLS weights are correct: CASE 2: residual at barcsq
  {
    // expected:
    Vector weights_expected = Vector::Zero(1);
    weights_expected[0] = 0.5;  // undecided
    // actual:
    GaussNewtonParams gnParams;
    GncParams<GaussNewtonParams> gncParams(gnParams);
    gncParams.setLossType(GncLossType::TLS);
    auto gnc = GncOptimizer<GncParams<GaussNewtonParams>>(nfg, initial,
                                                          gncParams);
    gnc.setInlierCostThresholds(0.5);  // if inlier threshold is slightly below 0.5, then measurement is outlier
    double mu = 1e6;  // very large mu recovers original TLS cost
    Vector weights_actual = gnc.calculateWeights(initial, mu);
    CHECK(assert_equal(weights_expected, weights_actual, 1e-5));
  }
}

/* ************************************************************************* */
TEST(GncOptimizer, makeWeightedGraph) {
  // create original factor
  double sigma1 = 0.1;
  NonlinearFactorGraph nfg = example::nonlinearFactorGraphWithGivenSigma(
      sigma1);

  // create expected
  double sigma2 = 10;
  NonlinearFactorGraph expected = example::nonlinearFactorGraphWithGivenSigma(
      sigma2);

  // create weights
  Vector weights = Vector::Ones(1);  // original info:1/0.1^2 = 100. New info: 1/10^2 = 0.01. Ratio is 10-4
  weights[0] = 1e-4;

  // create actual
  Point2 p0(3, 3);
  Values initial;
  initial.insert(X(1), p0);

  LevenbergMarquardtParams lmParams;
  GncParams<LevenbergMarquardtParams> gncParams(lmParams);
  auto gnc = GncOptimizer<GncParams<LevenbergMarquardtParams>>(nfg, initial,
                                                               gncParams);
  NonlinearFactorGraph actual = gnc.makeWeightedGraph(weights);

  // check it's all good
  CHECK(assert_equal(expected, actual));
}

/* ************************************************************************* */
TEST(GncOptimizer, optimizeSimple) {
  auto fg = example::createReallyNonlinearFactorGraph();

  Point2 p0(3, 3);
  Values initial;
  initial.insert(X(1), p0);

  LevenbergMarquardtParams lmParams;
  GncParams<LevenbergMarquardtParams> gncParams(lmParams);
  auto gnc = GncOptimizer<GncParams<LevenbergMarquardtParams>>(fg, initial,
                                                               gncParams);

  Values actual = gnc.optimize();
  DOUBLES_EQUAL(0, fg.error(actual), tol);
}

/* ************************************************************************* */
TEST(GncOptimizer, optimize) {
  auto fg = example::sharedNonRobustFactorGraphWithOutliers();

  Point2 p0(1, 0);
  Values initial;
  initial.insert(X(1), p0);

  // try with nonrobust cost function and standard GN
  GaussNewtonParams gnParams;
  GaussNewtonOptimizer gn(fg, initial, gnParams);
  Values gn_results = gn.optimize();
  // converges to incorrect point due to lack of robustness to an outlier, ideal
  // solution is Point2(0,0)
  CHECK(assert_equal(Point2(0.25, 0.0), gn_results.at<Point2>(X(1)), 1e-3));

  // try with robust loss function and standard GN
  auto fg_robust = example::sharedRobustFactorGraphWithOutliers();  // same as fg, but with
                                                                    // factors wrapped in
                                                                    // Geman McClure losses
  GaussNewtonOptimizer gn2(fg_robust, initial, gnParams);
  Values gn2_results = gn2.optimize();
  // converges to incorrect point, this time due to the nonconvexity of the loss
  CHECK(assert_equal(Point2(0.999706, 0.0), gn2_results.at<Point2>(X(1)), 1e-3));

  // .. but graduated nonconvexity ensures both robustness and convergence in
  // the face of nonconvexity
  GncParams<GaussNewtonParams> gncParams(gnParams);
  // gncParams.setVerbosityGNC(GncParams<GaussNewtonParams>::Verbosity::SUMMARY);
  auto gnc = GncOptimizer<GncParams<GaussNewtonParams>>(fg, initial, gncParams);
  Values gnc_result = gnc.optimize();
  CHECK(assert_equal(Point2(0.0, 0.0), gnc_result.at<Point2>(X(1)), 1e-3));
}

/* ************************************************************************* */
TEST(GncOptimizer, optimizeWithKnownInliers) {
  auto fg = example::sharedNonRobustFactorGraphWithOutliers();

  Point2 p0(1, 0);
  Values initial;
  initial.insert(X(1), p0);

  std::vector<size_t> knownInliers;
  knownInliers.push_back(0);
  knownInliers.push_back(1);
  knownInliers.push_back(2);

  // nonconvexity with known inliers
  {
    GncParams<GaussNewtonParams> gncParams;
    gncParams.setKnownInliers(knownInliers);
    gncParams.setLossType(GncLossType::GM);
    //gncParams.setVerbosityGNC(GncParams<GaussNewtonParams>::Verbosity::SUMMARY);
    auto gnc = GncOptimizer<GncParams<GaussNewtonParams>>(fg, initial,
                                                          gncParams);

    Values gnc_result = gnc.optimize();
    CHECK(assert_equal(Point2(0.0, 0.0), gnc_result.at<Point2>(X(1)), 1e-3));

    // check weights were actually fixed:
    Vector finalWeights = gnc.getWeights();
    DOUBLES_EQUAL(1.0, finalWeights[0], tol);
    DOUBLES_EQUAL(1.0, finalWeights[1], tol);
    DOUBLES_EQUAL(1.0, finalWeights[2], tol);
  }
  {
    GncParams<GaussNewtonParams> gncParams;
    gncParams.setKnownInliers(knownInliers);
    gncParams.setLossType(GncLossType::TLS);
    // gncParams.setVerbosityGNC(GncParams<GaussNewtonParams>::Verbosity::SUMMARY);
    auto gnc = GncOptimizer<GncParams<GaussNewtonParams>>(fg, initial,
                                                          gncParams);

    Values gnc_result = gnc.optimize();
    CHECK(assert_equal(Point2(0.0, 0.0), gnc_result.at<Point2>(X(1)), 1e-3));

    // check weights were actually fixed:
    Vector finalWeights = gnc.getWeights();
    DOUBLES_EQUAL(1.0, finalWeights[0], tol);
    DOUBLES_EQUAL(1.0, finalWeights[1], tol);
    DOUBLES_EQUAL(1.0, finalWeights[2], tol);
    DOUBLES_EQUAL(0.0, finalWeights[3], tol);
  }
  {
    // if we set the threshold large, they are all inliers
    GncParams<GaussNewtonParams> gncParams;
    gncParams.setKnownInliers(knownInliers);
    gncParams.setLossType(GncLossType::TLS);
    //gncParams.setVerbosityGNC(GncParams<GaussNewtonParams>::Verbosity::VALUES);
    auto gnc = GncOptimizer<GncParams<GaussNewtonParams>>(fg, initial,
                                                          gncParams);
    gnc.setInlierCostThresholds(100.0);

    Values gnc_result = gnc.optimize();
    CHECK(assert_equal(Point2(0.25, 0.0), gnc_result.at<Point2>(X(1)), 1e-3));

    // check weights were actually fixed:
    Vector finalWeights = gnc.getWeights();
    DOUBLES_EQUAL(1.0, finalWeights[0], tol);
    DOUBLES_EQUAL(1.0, finalWeights[1], tol);
    DOUBLES_EQUAL(1.0, finalWeights[2], tol);
    DOUBLES_EQUAL(1.0, finalWeights[3], tol);
  }
}

/* ************************************************************************* */
TEST(GncOptimizer, setWeights) {
  auto fg = example::sharedNonRobustFactorGraphWithOutliers();

  Point2 p0(1, 0);
  Values initial;
  initial.insert(X(1), p0);

  std::vector<size_t> knownInliers;
  knownInliers.push_back(0);
  knownInliers.push_back(1);
  knownInliers.push_back(2);
  {
    GncParams<GaussNewtonParams> gncParams;
    gncParams.setKnownInliers(knownInliers);
    gncParams.setLossType(GncLossType::GM);
    //gncParams.setVerbosityGNC(GncParams<GaussNewtonParams>::Verbosity::SUMMARY);
    auto gnc = GncOptimizer<GncParams<GaussNewtonParams>>(fg, initial,
        gncParams);
    gnc.setInlierCostThresholds(2.0);
    Values gnc_result = gnc.optimize();
    CHECK(assert_equal(Point2(0.0, 0.0), gnc_result.at<Point2>(X(1)), 1e-3));

    // check weights were actually fixed:
    Vector finalWeights = gnc.getWeights();
    DOUBLES_EQUAL(1.0, finalWeights[0], tol);
    DOUBLES_EQUAL(1.0, finalWeights[1], tol);
    DOUBLES_EQUAL(1.0, finalWeights[2], tol);
    CHECK(assert_equal(2.0 * Vector::Ones(fg.size()), gnc.getInlierCostThresholds(), 1e-3));
  }
  {
    GncParams<GaussNewtonParams> gncParams;
    gncParams.setKnownInliers(knownInliers);
    gncParams.setLossType(GncLossType::GM);
    //gncParams.setVerbosityGNC(GncParams<GaussNewtonParams>::Verbosity::SUMMARY);
    auto gnc = GncOptimizer<GncParams<GaussNewtonParams>>(fg, initial,
        gncParams);
    gnc.setInlierCostThresholds(2.0 * Vector::Ones(fg.size()));
    Values gnc_result = gnc.optimize();
    CHECK(assert_equal(Point2(0.0, 0.0), gnc_result.at<Point2>(X(1)), 1e-3));

    // check weights were actually fixed:
    Vector finalWeights = gnc.getWeights();
    DOUBLES_EQUAL(1.0, finalWeights[0], tol);
    DOUBLES_EQUAL(1.0, finalWeights[1], tol);
    DOUBLES_EQUAL(1.0, finalWeights[2], tol);
    CHECK(assert_equal(2.0 * Vector::Ones(fg.size()), gnc.getInlierCostThresholds(), 1e-3));
  }
}

/* ************************************************************************* */
TEST(GncOptimizer, optimizeSmallPoseGraph) {
  /// load small pose graph
  const string filename = findExampleDataFile("w100.graph");
  NonlinearFactorGraph::shared_ptr graph;
  Values::shared_ptr initial;
  boost::tie(graph, initial) = load2D(filename);
  // Add a Gaussian prior on first poses
  Pose2 priorMean(0.0, 0.0, 0.0);  // prior at origin
  SharedDiagonal priorNoise = noiseModel::Diagonal::Sigmas(
      Vector3(0.01, 0.01, 0.01));
  graph->addPrior(0, priorMean, priorNoise);

  /// get expected values by optimizing outlier-free graph
  Values expected = LevenbergMarquardtOptimizer(*graph, *initial).optimize();

  // add a few outliers
  SharedDiagonal betweenNoise = noiseModel::Diagonal::Sigmas(
      Vector3(0.1, 0.1, 0.01));
  graph->push_back(BetweenFactor<Pose2>(90, 50, Pose2(), betweenNoise));  // some arbitrary and incorrect between factor

  /// get expected values by optimizing outlier-free graph
  Values expectedWithOutliers = LevenbergMarquardtOptimizer(*graph, *initial)
      .optimize();
  // as expected, the following test fails due to the presence of an outlier!
  // CHECK(assert_equal(expected, expectedWithOutliers, 1e-3));

  // GNC
  // Note: in difficult instances, we set the odometry measurements to be
  // inliers, but this problem is simple enought to succeed even without that
  // assumption std::vector<size_t> knownInliers;
  GncParams<GaussNewtonParams> gncParams;
  auto gnc = GncOptimizer<GncParams<GaussNewtonParams>>(*graph, *initial,
                                                        gncParams);
  Values actual = gnc.optimize();

  // compare
  CHECK(assert_equal(expected, actual, 1e-3));  // yay! we are robust to outliers!
}

/* ************************************************************************* */
int main() {
  TestResult tr;
  return TestRegistry::runAllTests(tr);
}
/* ************************************************************************* */
