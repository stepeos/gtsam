/**
 * @file  gtsam.cpp
 * @brief   The auto-generated wrapper C++ source code.
 * @author  Duy-Nguyen Ta, Fan Jiang, Matthew Sklar
 * @date  Aug. 18, 2020
 *
 * ** THIS FILE IS AUTO-GENERATED, DO NOT MODIFY! **
 */

// Include relevant boost libraries required by GTSAM
{include_boost}

#include <pybind11/eigen.h>
#include <pybind11/stl_bind.h>
#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/iostream.h>
#include "gtsam/config.h"
#include "gtsam/base/serialization.h"
#include "gtsam/nonlinear/utilities.h"  // for RedirectCout.

// These are the included headers listed in `gtsam.i`
{includes}
#include <boost/serialization/export.hpp>

// Export classes for serialization
{boost_class_export}

// Holder type for pybind11
{holder_type}

// Preamble for STL classes
// TODO(fan): make this automatic
#include "python/gtsam/preamble.h"

using namespace std;

namespace py = pybind11;

PYBIND11_MODULE({module_name}, m_) {{
    m_.doc() = "pybind11 wrapper of {module_name}";

{wrapped_namespace}

// Specializations for STL classes
// TODO(fan): make this automatic
#include "python/gtsam/specializations.h"

}}

