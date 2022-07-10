/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/*
 *  @file testHybridGaussianFactorGraph.cpp
 *  @date Mar 11, 2022
 *  @author Fan Jiang
 */

#include <CppUnitLite/Test.h>
#include <CppUnitLite/TestHarness.h>
#include <gtsam/discrete/DecisionTreeFactor.h>
#include <gtsam/discrete/DiscreteKey.h>
#include <gtsam/discrete/DiscreteValues.h>
#include <gtsam/hybrid/GaussianMixture.h>
#include <gtsam/hybrid/GaussianMixtureFactor.h>
#include <gtsam/hybrid/HybridBayesNet.h>
#include <gtsam/hybrid/HybridBayesTree.h>
#include <gtsam/hybrid/HybridConditional.h>
#include <gtsam/hybrid/HybridDiscreteFactor.h>
#include <gtsam/hybrid/HybridFactor.h>
#include <gtsam/hybrid/HybridGaussianFactor.h>
#include <gtsam/hybrid/HybridGaussianFactorGraph.h>
#include <gtsam/hybrid/HybridGaussianISAM.h>
#include <gtsam/inference/BayesNet.h>
#include <gtsam/inference/DotWriter.h>
#include <gtsam/inference/Key.h>
#include <gtsam/inference/Ordering.h>
#include <gtsam/inference/Symbol.h>
#include <gtsam/linear/JacobianFactor.h>

#include <algorithm>
#include <boost/assign/std/map.hpp>
#include <cstddef>
#include <functional>
#include <iostream>
#include <iterator>
#include <vector>

#include "Switching.h"

using namespace boost::assign;

using namespace std;
using namespace gtsam;

using gtsam::symbol_shorthand::C;
using gtsam::symbol_shorthand::D;
using gtsam::symbol_shorthand::X;
using gtsam::symbol_shorthand::Y;

/* ************************************************************************* */
TEST(HybridGaussianFactorGraph, Creation) {
  HybridConditional conditional;

  HybridGaussianFactorGraph hfg;

  hfg.add(HybridGaussianFactor(JacobianFactor(X(0), I_3x3, Z_3x1)));

  // Define a gaussian mixture conditional P(x0|x1, c0) and add it to the factor
  // graph
  GaussianMixture gm({X(0)}, {X(1)}, DiscreteKeys(DiscreteKey{C(0), 2}),
                     GaussianMixture::Conditionals(
                         C(0),
                         boost::make_shared<GaussianConditional>(
                             X(0), Z_3x1, I_3x3, X(1), I_3x3),
                         boost::make_shared<GaussianConditional>(
                             X(0), Vector3::Ones(), I_3x3, X(1), I_3x3)));
  hfg.add(gm);

  EXPECT_LONGS_EQUAL(2, hfg.size());
}

/* ************************************************************************* */
TEST(HybridGaussianFactorGraph, EliminateSequential) {
  // Test elimination of a single variable.
  HybridGaussianFactorGraph hfg;

  hfg.add(HybridGaussianFactor(JacobianFactor(0, I_3x3, Z_3x1)));

  auto result = hfg.eliminatePartialSequential(KeyVector{0});

  EXPECT_LONGS_EQUAL(result.first->size(), 1);
}

/* ************************************************************************* */
TEST(HybridGaussianFactorGraph, EliminateMultifrontal) {
  // Test multifrontal elimination
  HybridGaussianFactorGraph hfg;

  DiscreteKey c(C(1), 2);

  // Add priors on x0 and c1
  hfg.add(JacobianFactor(X(0), I_3x3, Z_3x1));
  hfg.add(HybridDiscreteFactor(DecisionTreeFactor(c, {2, 8})));

  Ordering ordering;
  ordering.push_back(X(0));
  auto result = hfg.eliminatePartialMultifrontal(ordering);

  EXPECT_LONGS_EQUAL(result.first->size(), 1);
  EXPECT_LONGS_EQUAL(result.second->size(), 1);
}

/* ************************************************************************* */
TEST(HybridGaussianFactorGraph, eliminateFullSequentialEqualChance) {
  HybridGaussianFactorGraph hfg;

  DiscreteKey c1(C(1), 2);

  // Add prior on x0
  hfg.add(JacobianFactor(X(0), I_3x3, Z_3x1));
  // Add factor between x0 and x1
  hfg.add(JacobianFactor(X(0), I_3x3, X(1), -I_3x3, Z_3x1));

  // Add a gaussian mixture factor ϕ(x1, c1)
  DecisionTree<Key, GaussianFactor::shared_ptr> dt(
      C(1), boost::make_shared<JacobianFactor>(X(1), I_3x3, Z_3x1),
      boost::make_shared<JacobianFactor>(X(1), I_3x3, Vector3::Ones()));

  hfg.add(GaussianMixtureFactor({X(1)}, {c1}, dt));

  auto result =
      hfg.eliminateSequential(Ordering::ColamdConstrainedLast(hfg, {C(1)}));

  auto dc = result->at(2)->asDiscreteConditional();
  DiscreteValues mode;
  mode[C(1)] = 0;
  EXPECT_DOUBLES_EQUAL(0.6225, (*dc)(mode), 1e-3);
}

/* ************************************************************************* */
TEST(HybridGaussianFactorGraph, eliminateFullSequentialSimple) {
  HybridGaussianFactorGraph hfg;

  DiscreteKey c1(C(1), 2);

  // Add prior on x0
  hfg.add(JacobianFactor(X(0), I_3x3, Z_3x1));
  // Add factor between x0 and x1
  hfg.add(JacobianFactor(X(0), I_3x3, X(1), -I_3x3, Z_3x1));

  DecisionTree<Key, GaussianFactor::shared_ptr> dt(
      C(1), boost::make_shared<JacobianFactor>(X(1), I_3x3, Z_3x1),
      boost::make_shared<JacobianFactor>(X(1), I_3x3, Vector3::Ones()));
  hfg.add(GaussianMixtureFactor({X(1)}, {c1}, dt));

  // Discrete probability table for c1
  hfg.add(HybridDiscreteFactor(DecisionTreeFactor(c1, {2, 8})));
  // Joint discrete probability table for c1, c2
  hfg.add(HybridDiscreteFactor(
      DecisionTreeFactor({{C(1), 2}, {C(2), 2}}, "1 2 3 4")));

  auto result = hfg.eliminateSequential(
      Ordering::ColamdConstrainedLast(hfg, {C(1), C(2)}));

  EXPECT_LONGS_EQUAL(4, result->size());
}

/* ************************************************************************* */
TEST(HybridGaussianFactorGraph, eliminateFullMultifrontalSimple) {
  HybridGaussianFactorGraph hfg;

  DiscreteKey c1(C(1), 2);

  hfg.add(JacobianFactor(X(0), I_3x3, Z_3x1));
  hfg.add(JacobianFactor(X(0), I_3x3, X(1), -I_3x3, Z_3x1));

  hfg.add(GaussianMixtureFactor::FromFactors(
      {X(1)}, {{C(1), 2}},
      {boost::make_shared<JacobianFactor>(X(1), I_3x3, Z_3x1),
       boost::make_shared<JacobianFactor>(X(1), I_3x3, Vector3::Ones())}));

  hfg.add(DecisionTreeFactor(c1, {2, 8}));
  hfg.add(DecisionTreeFactor({{C(1), 2}, {C(2), 2}}, "1 2 3 4"));

  auto result = hfg.eliminateMultifrontal(
      Ordering::ColamdConstrainedLast(hfg, {C(1), C(2)}));

  // GTSAM_PRINT(*result);
  // GTSAM_PRINT(*result->marginalFactor(C(2)));
}

/* ************************************************************************* */
TEST(HybridGaussianFactorGraph, eliminateFullMultifrontalCLG) {
  HybridGaussianFactorGraph hfg;

  DiscreteKey c(C(1), 2);

  // Prior on x0
  hfg.add(JacobianFactor(X(0), I_3x3, Z_3x1));
  // Factor between x0-x1
  hfg.add(JacobianFactor(X(0), I_3x3, X(1), -I_3x3, Z_3x1));

  // Decision tree with different modes on x1
  DecisionTree<Key, GaussianFactor::shared_ptr> dt(
      C(1), boost::make_shared<JacobianFactor>(X(1), I_3x3, Z_3x1),
      boost::make_shared<JacobianFactor>(X(1), I_3x3, Vector3::Ones()));

  // Hybrid factor P(x1|c1)
  hfg.add(GaussianMixtureFactor({X(1)}, {c}, dt));
  // Prior factor on c1
  hfg.add(HybridDiscreteFactor(DecisionTreeFactor(c, {2, 8})));

  // Get a constrained ordering keeping c1 last
  auto ordering_full = Ordering::ColamdConstrainedLast(hfg, {C(1)});

  // Returns a Hybrid Bayes Tree with distribution P(x0|x1)P(x1|c1)P(c1)
  HybridBayesTree::shared_ptr hbt = hfg.eliminateMultifrontal(ordering_full);

  EXPECT_LONGS_EQUAL(3, hbt->size());
}

/* ************************************************************************* */
/*
 * This test is about how to assemble the Bayes Tree roots after we do partial
 * elimination
 */
TEST(HybridGaussianFactorGraph, eliminateFullMultifrontalTwoClique) {
  HybridGaussianFactorGraph hfg;

  hfg.add(JacobianFactor(X(0), I_3x3, X(1), -I_3x3, Z_3x1));
  hfg.add(JacobianFactor(X(1), I_3x3, X(2), -I_3x3, Z_3x1));

  {
    hfg.add(GaussianMixtureFactor::FromFactors(
        {X(0)}, {{C(0), 2}},
        {boost::make_shared<JacobianFactor>(X(0), I_3x3, Z_3x1),
         boost::make_shared<JacobianFactor>(X(0), I_3x3, Vector3::Ones())}));

    DecisionTree<Key, GaussianFactor::shared_ptr> dt1(
        C(1), boost::make_shared<JacobianFactor>(X(2), I_3x3, Z_3x1),
        boost::make_shared<JacobianFactor>(X(2), I_3x3, Vector3::Ones()));

    hfg.add(GaussianMixtureFactor({X(2)}, {{C(1), 2}}, dt1));
  }

  hfg.add(HybridDiscreteFactor(
      DecisionTreeFactor({{C(1), 2}, {C(2), 2}}, "1 2 3 4")));

  hfg.add(JacobianFactor(X(3), I_3x3, X(4), -I_3x3, Z_3x1));
  hfg.add(JacobianFactor(X(4), I_3x3, X(5), -I_3x3, Z_3x1));

  {
    DecisionTree<Key, GaussianFactor::shared_ptr> dt(
        C(3), boost::make_shared<JacobianFactor>(X(3), I_3x3, Z_3x1),
        boost::make_shared<JacobianFactor>(X(3), I_3x3, Vector3::Ones()));

    hfg.add(GaussianMixtureFactor({X(3)}, {{C(3), 2}}, dt));

    DecisionTree<Key, GaussianFactor::shared_ptr> dt1(
        C(2), boost::make_shared<JacobianFactor>(X(5), I_3x3, Z_3x1),
        boost::make_shared<JacobianFactor>(X(5), I_3x3, Vector3::Ones()));

    hfg.add(GaussianMixtureFactor({X(5)}, {{C(2), 2}}, dt1));
  }

  auto ordering_full =
      Ordering::ColamdConstrainedLast(hfg, {C(0), C(1), C(2), C(3)});

  HybridBayesTree::shared_ptr hbt;
  HybridGaussianFactorGraph::shared_ptr remaining;
  std::tie(hbt, remaining) = hfg.eliminatePartialMultifrontal(ordering_full);

  // GTSAM_PRINT(*hbt);
  // GTSAM_PRINT(*remaining);

  // hbt->marginalFactor(X(1))->print("HBT: ");
  /*
  (Fan) Explanation: the Junction tree will need to reeliminate to get to the
  marginal on X(1), which is not possible because it involves eliminating
  discrete before continuous. The solution to this, however, is in Murphy02.
  TLDR is that this is 1. expensive and 2. inexact. nevertheless it is doable.
  And I believe that we should do this.
  */
}

void dotPrint(const HybridGaussianFactorGraph::shared_ptr &hfg,
              const HybridBayesTree::shared_ptr &hbt,
              const Ordering &ordering) {
  DotWriter dw;
  dw.positionHints['c'] = 2;
  dw.positionHints['x'] = 1;
  std::cout << hfg->dot(DefaultKeyFormatter, dw);
  std::cout << "\n";
  hbt->dot(std::cout);

  std::cout << "\n";
  std::cout << hfg->eliminateSequential(ordering)->dot(DefaultKeyFormatter, dw);
}

/* ************************************************************************* */
// TODO(fan): make a graph like Varun's paper one
TEST(HybridGaussianFactorGraph, Switching) {
  auto N = 12;
  auto hfg = makeSwitchingChain(N);

  // X(5) will be the center, X(1-4), X(6-9)
  // X(3), X(7)
  // X(2), X(8)
  // X(1), X(4), X(6), X(9)
  // C(5) will be the center, C(1-4), C(6-8)
  // C(3), C(7)
  // C(1), C(4), C(2), C(6), C(8)
  // auto ordering_full =
  //     Ordering(KeyVector{X(1), X(4), X(2), X(6), X(9), X(8), X(3), X(7),
  //     X(5),
  //                        C(1), C(4), C(2), C(6), C(8), C(3), C(7), C(5)});
  KeyVector ordering;

  {
    std::vector<int> naturalX(N);
    std::iota(naturalX.begin(), naturalX.end(), 1);
    std::vector<Key> ordX;
    std::transform(naturalX.begin(), naturalX.end(), std::back_inserter(ordX),
                   [](int x) { return X(x); });

    KeyVector ndX;
    std::vector<int> lvls;
    std::tie(ndX, lvls) = makeBinaryOrdering(ordX);
    std::copy(ndX.begin(), ndX.end(), std::back_inserter(ordering));
    for (auto &l : lvls) {
      l = -l;
    }
  }
  {
    std::vector<int> naturalC(N - 1);
    std::iota(naturalC.begin(), naturalC.end(), 1);
    std::vector<Key> ordC;
    std::transform(naturalC.begin(), naturalC.end(), std::back_inserter(ordC),
                   [](int x) { return C(x); });
    KeyVector ndC;
    std::vector<int> lvls;

    // std::copy(ordC.begin(), ordC.end(), std::back_inserter(ordering));
    std::tie(ndC, lvls) = makeBinaryOrdering(ordC);
    std::copy(ndC.begin(), ndC.end(), std::back_inserter(ordering));
  }
  auto ordering_full = Ordering(ordering);

  // GTSAM_PRINT(*hfg);
  // GTSAM_PRINT(ordering_full);

  HybridBayesTree::shared_ptr hbt;
  HybridGaussianFactorGraph::shared_ptr remaining;
  std::tie(hbt, remaining) = hfg->eliminatePartialMultifrontal(ordering_full);

  // GTSAM_PRINT(*hbt);
  // GTSAM_PRINT(*remaining);
  // hbt->marginalFactor(C(11))->print("HBT: ");
}

/* ************************************************************************* */
// TODO(fan): make a graph like Varun's paper one
TEST(HybridGaussianFactorGraph, SwitchingISAM) {
  auto N = 11;
  auto hfg = makeSwitchingChain(N);

  // X(5) will be the center, X(1-4), X(6-9)
  // X(3), X(7)
  // X(2), X(8)
  // X(1), X(4), X(6), X(9)
  // C(5) will be the center, C(1-4), C(6-8)
  // C(3), C(7)
  // C(1), C(4), C(2), C(6), C(8)
  // auto ordering_full =
  //     Ordering(KeyVector{X(1), X(4), X(2), X(6), X(9), X(8), X(3), X(7),
  //     X(5),
  //                        C(1), C(4), C(2), C(6), C(8), C(3), C(7), C(5)});
  KeyVector ordering;

  {
    std::vector<int> naturalX(N);
    std::iota(naturalX.begin(), naturalX.end(), 1);
    std::vector<Key> ordX;
    std::transform(naturalX.begin(), naturalX.end(), std::back_inserter(ordX),
                   [](int x) { return X(x); });

    KeyVector ndX;
    std::vector<int> lvls;
    std::tie(ndX, lvls) = makeBinaryOrdering(ordX);
    std::copy(ndX.begin(), ndX.end(), std::back_inserter(ordering));
    for (auto &l : lvls) {
      l = -l;
    }
  }
  {
    std::vector<int> naturalC(N - 1);
    std::iota(naturalC.begin(), naturalC.end(), 1);
    std::vector<Key> ordC;
    std::transform(naturalC.begin(), naturalC.end(), std::back_inserter(ordC),
                   [](int x) { return C(x); });
    KeyVector ndC;
    std::vector<int> lvls;

    // std::copy(ordC.begin(), ordC.end(), std::back_inserter(ordering));
    std::tie(ndC, lvls) = makeBinaryOrdering(ordC);
    std::copy(ndC.begin(), ndC.end(), std::back_inserter(ordering));
  }
  auto ordering_full = Ordering(ordering);

  // GTSAM_PRINT(*hfg);
  // GTSAM_PRINT(ordering_full);

  HybridBayesTree::shared_ptr hbt;
  HybridGaussianFactorGraph::shared_ptr remaining;
  std::tie(hbt, remaining) = hfg->eliminatePartialMultifrontal(ordering_full);

  auto new_fg = makeSwitchingChain(12);
  auto isam = HybridGaussianISAM(*hbt);

  {
    HybridGaussianFactorGraph factorGraph;
    factorGraph.push_back(new_fg->at(new_fg->size() - 2));
    factorGraph.push_back(new_fg->at(new_fg->size() - 1));
    isam.update(factorGraph);
    // std::cout << isam.dot();
    // isam.marginalFactor(C(11))->print();
  }
}

/* ************************************************************************* */
// TODO(Varun) Actually test something!
TEST_DISABLED(HybridGaussianFactorGraph, SwitchingTwoVar) {
  const int N = 7;
  auto hfg = makeSwitchingChain(N, X);
  hfg->push_back(*makeSwitchingChain(N, Y, D));

  for (int t = 1; t <= N; t++) {
    hfg->add(JacobianFactor(X(t), I_3x3, Y(t), -I_3x3, Vector3(1.0, 0.0, 0.0)));
  }

  KeyVector ordering;

  KeyVector naturalX(N);
  std::iota(naturalX.begin(), naturalX.end(), 1);
  KeyVector ordX;
  for (size_t i = 1; i <= N; i++) {
    ordX.emplace_back(X(i));
    ordX.emplace_back(Y(i));
  }

  for (size_t i = 1; i <= N - 1; i++) {
    ordX.emplace_back(C(i));
  }
  for (size_t i = 1; i <= N - 1; i++) {
    ordX.emplace_back(D(i));
  }

  {
    DotWriter dw;
    dw.positionHints['x'] = 1;
    dw.positionHints['c'] = 0;
    dw.positionHints['d'] = 3;
    dw.positionHints['y'] = 2;
    std::cout << hfg->dot(DefaultKeyFormatter, dw);
    std::cout << "\n";
  }

  {
    DotWriter dw;
    dw.positionHints['y'] = 9;
    // dw.positionHints['c'] = 0;
    // dw.positionHints['d'] = 3;
    dw.positionHints['x'] = 1;
    std::cout << "\n";
    // std::cout << hfg->eliminateSequential(Ordering(ordX))
    //                  ->dot(DefaultKeyFormatter, dw);
    hfg->eliminateMultifrontal(Ordering(ordX))->dot(std::cout);
  }

  Ordering ordering_partial;
  for (size_t i = 1; i <= N; i++) {
    ordering_partial.emplace_back(X(i));
    ordering_partial.emplace_back(Y(i));
  }
  {
    HybridBayesNet::shared_ptr hbn;
    HybridGaussianFactorGraph::shared_ptr remaining;
    std::tie(hbn, remaining) =
        hfg->eliminatePartialSequential(ordering_partial);

    // remaining->print();
    {
      DotWriter dw;
      dw.positionHints['x'] = 1;
      dw.positionHints['c'] = 0;
      dw.positionHints['d'] = 3;
      dw.positionHints['y'] = 2;
      std::cout << remaining->dot(DefaultKeyFormatter, dw);
      std::cout << "\n";
    }
  }
}

/* ************************************************************************* */
int main() {
  TestResult tr;
  return TestRegistry::runAllTests(tr);
}
/* ************************************************************************* */
