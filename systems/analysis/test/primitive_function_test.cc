/* clang-format off to disable clang-format-includes */
#include "drake/systems/analysis/primitive_function.h"
/* clang-format on */

#include <gtest/gtest.h>

#include "drake/common/eigen_types.h"
#include "drake/systems/analysis/integrator_base.h"
#include "drake/systems/analysis/runge_kutta2_integrator.h"

namespace drake {
namespace systems {
namespace {

// Checks primitive function usage with multiple integrators.
GTEST_TEST(PrimitiveFunctionTest, UsingMultipleIntegrators) {
  // Accuracy upper bound, as not all the integrators used below support
  // error control.
  const double kAccuracy = 1e-2;

  // The lower integration bound v, for function definition.
  const double kDefaultLowerIntegrationBound = 0.0;
  // The default parameters 𝐤₀, for function definition.
  const VectorX<double> kDefaultParameters =
      VectorX<double>::Constant(2, 1.0);
  // All specified values by default, for function definition.
  const PrimitiveFunction<double>::SpecifiedValues kDefaultValues(
      kDefaultLowerIntegrationBound, kDefaultParameters);

  // Defines a primitive function for f(x; 𝐤) = k₁ * x + k₂.
  PrimitiveFunction<double> primitive_f(
      [](const double& x, const VectorX<double>& k) -> double {
        return k[0] * x + k[1];
      }, kDefaultValues);

  // Testing against closed form solution of above's integral, which
  // can be written as F(u; 𝐤) = k₁/2 * u^2 + k₂ * u for the specified
  // integration lower bound.
  const double u1 = kDefaultLowerIntegrationBound + 10.0;
  const VectorX<double>& k1 = kDefaultParameters;
  EXPECT_NEAR(primitive_f.Evaluate(u1),
              k1[0]/2 * std::pow(u1, 2.0) + k1[1] * u1,
              kAccuracy);

  // Replaces default integrator.
  const double kMaximumStep = 0.1;
  const IntegratorBase<double>* default_integrator =
      primitive_f.get_integrator();
  IntegratorBase<double>* configured_integrator =
      primitive_f.reset_integrator<RungeKutta2Integrator<double>>(kMaximumStep);
  EXPECT_NE(configured_integrator, default_integrator);
  EXPECT_EQ(configured_integrator, primitive_f.get_integrator());

  // Specifies a different parameter vector, but leaves the default
  // integration lower bound.
  const VectorX<double> k2 = VectorX<double>::Constant(2, 5.0);
  const double u2 = kDefaultLowerIntegrationBound + 15.0;
  PrimitiveFunction<double>::SpecifiedValues values;
  values.k = k2;
  // Testing against closed form solution of above's integral, which
  // can be written as F(u; 𝐤) = k₁/2 * u^2 + k₂ * u for the specified
  // integration lower bound.
  EXPECT_NEAR(
      primitive_f.Evaluate(u2, values),
      k2[0]/2 * std::pow(u2, 2.0) + k2[1] * u2,
      kAccuracy);
}

// Validates preconditions when constructing any given primitive function.
GTEST_TEST(PrimitiveFunctionTest, ConstructorPreconditionValidation) {
  EXPECT_THROW({
      const PrimitiveFunction<double>::SpecifiedValues no_values;
      // Defines a primitive function for f(x; 𝐤) = x * k₁ + k₂.
      const PrimitiveFunction<double> func(
          [](const double& x, const VectorX<double>& k) -> double {
            return x * k[0] + k[1];
          }, no_values);
    }, std::logic_error);
}

// Validates preconditions when evaluating any given primitive function.
GTEST_TEST(PrimitiveFunctionTest, EvaluatePreconditionValidation) {
  // The lower integration bound v, for function definition.
  const double kDefaultLowerIntegrationBound = 0.0;
  // The default parameters 𝐤₀, for function definition.
  const VectorX<double> kDefaultParameters =
      VectorX<double>::Constant(2, 1.0);
  // All specified values by default, for function definition.
  const PrimitiveFunction<double>::SpecifiedValues kDefaultValues(
      kDefaultLowerIntegrationBound, kDefaultParameters);

  // Defines a primitive function for f(x; 𝐤) = k₁ * x + k₂.
  const PrimitiveFunction<double> primitive_f(
      [](const double& x, const VectorX<double>& k) -> double {
        return k[0] * x + k[1];
      }, kDefaultValues);

  // Instantiates an invalid integration upper bound for testing, i.e.
  // a value that's less than the default integration lower bound.
  const double kInvalidUpperIntegrationBound =
      kDefaultLowerIntegrationBound - 10.0;
  // Instantiates a valid integration upper bound for testing, i.e.
  // a value that's greater than or equal the default integration lower bound.
  const double kValidUpperIntegrationBound =
      kDefaultLowerIntegrationBound + 10.0;
  // Instantiates an invalid parameter vector for testing, i.e. a
  // parameter vector of a dimension other than the expected one.
  const VectorX<double> kInvalidParameters = VectorX<double>::Zero(3);
  // Instantiates a valid parameter vector for testing, i.e. a
  // parameter vector of the expected dimension.
  const VectorX<double> kValidParameters = VectorX<double>::Constant(2, 5.0);

  EXPECT_THROW(primitive_f.Evaluate(kInvalidUpperIntegrationBound),
               std::logic_error);

  {
    PrimitiveFunction<double>::SpecifiedValues values;
    values.k = kInvalidParameters;
    EXPECT_THROW(primitive_f.Evaluate(kValidUpperIntegrationBound, values),
                 std::logic_error);
  }

  {
    PrimitiveFunction<double>::SpecifiedValues values;
    values.k = kValidParameters;
    EXPECT_THROW(primitive_f.Evaluate(kInvalidUpperIntegrationBound, values),
                 std::logic_error);
  }
}


class PrimitiveFunctionAccuracyTest
    : public ::testing::TestWithParam<double> {
 protected:
  void SetUp() {
    integration_accuracy_ = GetParam();
  }

  // Expected accuracy for numerical integral
  // evaluation in the relative tolerance sense.
  double integration_accuracy_{0.};
};

// Accuracy test for the numerical integration of ∫₀ᵘ xⁿ dx,
// parameterized in its order n.
TEST_P(PrimitiveFunctionAccuracyTest, NthPowerMonomialTestCase) {
  // The order n of the monomial.
  const VectorX<double> kDefaultParameters =
      VectorX<double>::Constant(1, 0.0);
  // All specified values by default, for function definition.
  const PrimitiveFunction<double>::SpecifiedValues
      kDefaultValues({}, kDefaultParameters);

  PrimitiveFunction<double> primitive_function(
      [](const double& x, const VectorX<double>& k) -> double {
        const double n = k[0];
        return std::pow(x, n);
      }, kDefaultValues);

  IntegratorBase<double>* inner_integrator =
      primitive_function.get_mutable_integrator();
  inner_integrator->set_target_accuracy(integration_accuracy_);

  const int kLowestOrder = 0;
  const int kHighestOrder = 5;

  const double kArgIntervalLBound = 0.0;
  const double kArgIntervalUBound = 10.0;
  const double kArgStep = 0.1;

  for (int n = kLowestOrder; n <= kHighestOrder; ++n) {
    PrimitiveFunction<double>::SpecifiedValues values;
    values.k = VectorX<double>::Constant(1, static_cast<double>(n)).eval();
    for (double u = kArgIntervalLBound; u <= kArgIntervalUBound;
         u += kArgStep) {
      // Tests are performed against the closed form solution of
      // the integral, which is (n + 1)⁻¹ uⁿ⁺¹.
      const double exact_solution = std::pow(u, n + 1.) / (n + 1.);
      EXPECT_NEAR(primitive_function.Evaluate(u, values),
                  exact_solution, integration_accuracy_)
          << "Failure integrating ∫₀ᵘ xⁿ dx for u = "
          << u << " and n = " << n << " to an accuracy of "
          << integration_accuracy_;
    }
  }
}

// Accuracy test for the numerical integration of ∫₀ᵘ tanh(a⋅x) dx,
// parameterized in its factor a.
TEST_P(PrimitiveFunctionAccuracyTest, HyperbolicTangentTestCase) {
  // The factor a in the tangent.
  const VectorX<double> kDefaultParameters =
      VectorX<double>::Constant(1, 0.0);
  // All specified values by default, for function definition.
  const PrimitiveFunction<double>::SpecifiedValues
      kDefaultValues({}, kDefaultParameters);

  PrimitiveFunction<double> primitive_function(
      [](const double& x, const VectorX<double>& k) -> double {
        const double a = k[0];
        return std::tanh(a * x);
      }, kDefaultValues);

  IntegratorBase<double>* inner_integrator =
      primitive_function.get_mutable_integrator();
  inner_integrator->set_target_accuracy(integration_accuracy_);

  const double kParamIntervalLBound = -5.25;
  const double kParamIntervalUBound = 5.25;
  const double kParamStep = 0.5;

  const double kArgIntervalLBound = 0.0;
  const double kArgIntervalUBound = 10.0;
  const double kArgStep = 0.1;

  for (double a = kParamIntervalLBound; a <= kParamIntervalUBound;
       a += kParamStep) {
    PrimitiveFunction<double>::SpecifiedValues values;
    values.k = VectorX<double>::Constant(1, a).eval();
    for (double u = kArgIntervalLBound; u <= kArgIntervalUBound;
         u += kArgStep) {
      // Tests are performed against the closed form solution of
      // the integral, which is a⁻¹ ln(cosh(a⋅u)).
      const double exact_solution = std::log(std::cosh(a * u)) / a;
      EXPECT_NEAR(primitive_function.Evaluate(u, values),
                  exact_solution, integration_accuracy_)
          << "Failure integrating ∫₀ᵘ tanh(a⋅x) dx for"
          << " u = " << u << " and a = " << a << " to an accuracy of "
          << integration_accuracy_;
    }
  }
}

// Accuracy test for the numerical integration of ∫₀ᵘ [(x + a)⋅(x + b)]⁻¹ dx,
// parameterized in its denominator roots (or function poles) a and b.
TEST_P(PrimitiveFunctionAccuracyTest, SecondOrderRationalFunctionTestCase) {
  // The denominator roots a and b.
  const VectorX<double> kDefaultParameters = VectorX<double>::Zero(2);
    // All specified values by default, for function definition.
  const PrimitiveFunction<double>::SpecifiedValues
      kDefaultValues({}, kDefaultParameters);

  PrimitiveFunction<double> primitive_function(
      [](const double& x, const VectorX<double>& k) -> double {
        const double a = k[0];
        const double b = k[1];
        return 1. / ((x + a) * (x + b));
      }, kDefaultValues);

  IntegratorBase<double>* inner_integrator =
      primitive_function.get_mutable_integrator();
  inner_integrator->set_target_accuracy(GetParam());

  const double k1stPoleIntervalLBound = 20.0;
  const double k1stPoleIntervalUBound = 25.0;
  const double k1stPoleStep = 0.5;

  const double k2ndPoleIntervalLBound = 30.0;
  const double k2ndPoleIntervalUBound = 35.0;
  const double k2ndPoleStep = 0.5;

  const double kArgIntervalLBound = 0.0;
  const double kArgIntervalUBound = 10.0;
  const double kArgStep = 0.1;

  for (double a = k1stPoleIntervalLBound; a <= k1stPoleIntervalUBound;
       a += k1stPoleStep) {
    for (double b = k2ndPoleIntervalLBound; b <= k2ndPoleIntervalUBound;
         b += k2ndPoleStep) {
      PrimitiveFunction<double>::SpecifiedValues values;
      values.k = (VectorX<double>(2) << a, b).finished();
      for (double u = kArgIntervalLBound; u <= kArgIntervalUBound;
           u += kArgStep) {
        // Tests are performed against the closed form solution of
        // the integral, which is (b - a)⁻¹ ln [(u + a) / (u + b)].
        const double exact_solution =
            std::log((b / a) * ((u + a) / (u + b))) / (b - a);
        EXPECT_NEAR(primitive_function.Evaluate(u, values),
                    exact_solution, integration_accuracy_)
            << "Failure integrating ∫₀ᵘ [(x + a)⋅(x + b)]⁻¹ dx for"
            << " u = " << u << ", a = " << a << "and b = " << b
            << " to an accuracy of " << integration_accuracy_;
      }
    }
  }
}

// Accuracy test for the numerical integration of ∫₀ᵘ x eⁿˣ dx,
// parameterized in its exponent factor n.
TEST_P(PrimitiveFunctionAccuracyTest, ExponentialFunctionTestCase) {
  // The exponent factor n.
  const VectorX<double> kDefaultParameters = VectorX<double>::Zero(1);
  // All specified values by default, for function definition.
  const PrimitiveFunction<double>::SpecifiedValues
      kDefaultValues({}, kDefaultParameters);

  PrimitiveFunction<double> primitive_function(
      [](const double& x, const VectorX<double>& k) -> double {
        const double n = k[0];
        return x * std::exp(n * x);
      }, kDefaultValues);

  IntegratorBase<double>* inner_integrator =
      primitive_function.get_mutable_integrator();
  inner_integrator->set_target_accuracy(integration_accuracy_);

  const double kParamIntervalLBound = -5.25;
  const double kParamIntervalUBound = 5.25;
  const double kParamStep = 0.5;

  const double kArgIntervalLBound = 0.0;
  const double kArgIntervalUBound = 1.0;
  const double kArgStep = 0.01;

  for (double n = kParamIntervalLBound; n <= kParamIntervalUBound;
       n += kParamStep) {
    PrimitiveFunction<double>::SpecifiedValues values;
    values.k = VectorX<double>::Constant(1, n).eval();
    for (double u = kArgIntervalLBound; u <= kArgIntervalUBound;
         u += kArgStep) {
      // Tests are performed against the closed form solution of
      // the integral, which is (u/n - 1/n^2) * e^(n*u).
      const double exact_solution =
          (u / n - 1. / (n * n)) * std::exp(n * u) + 1. / (n * n);
      EXPECT_NEAR(primitive_function.Evaluate(u, values),
                  exact_solution, integration_accuracy_)
          << "Failure integrating ∫₀ᵘ x eⁿˣ dx for"
          << " u = " << u << " and n = " << n
          << " to an accuracy of " << integration_accuracy_;
    }
  }
}

// Accuracy test for the numerical integration of ∫₀ᵘ x⋅sin(a⋅x) dx,
// parameterized in its factor a.
TEST_P(PrimitiveFunctionAccuracyTest, TrigonometricFunctionTestCase) {
  // The factor a in the sine.
  const VectorX<double> kDefaultParameters = VectorX<double>::Zero(1);
  // All specified values by default, for function definition.
  const PrimitiveFunction<double>::SpecifiedValues
      kDefaultValues({}, kDefaultParameters);

  PrimitiveFunction<double> primitive_function(
      [](const double& x, const VectorX<double>& k) -> double {
        const double a = k[0];
        return x * std::sin(a * x);
      }, kDefaultValues);

  IntegratorBase<double>* inner_integrator =
      primitive_function.get_mutable_integrator();
  inner_integrator->set_target_accuracy(integration_accuracy_);

  const double kParamIntervalLBound = -5.25;
  const double kParamIntervalUBound = 5.25;
  const double kParamStep = 0.5;

  const double kArgIntervalLBound = 0.0;
  const double kArgIntervalUBound = 10.0;
  const double kArgStep = 0.1;

  for (double a = kParamIntervalLBound; a <= kParamIntervalUBound;
       a += kParamStep) {
    PrimitiveFunction<double>::SpecifiedValues values;
    values.k = VectorX<double>::Constant(1, a).eval();
    for (double u = kArgIntervalLBound; u <= kArgIntervalUBound;
         u += kArgStep) {
      // Tests are performed against the closed form solution of
      // the integral, which is -u⋅cos(a⋅u)/a + sin(a⋅u) / a².
      const double exact_solution =
          -u * std::cos(a * u) / a + std::sin(a * u) / (a * a);
      EXPECT_NEAR(primitive_function.Evaluate(u, values),
                  exact_solution, integration_accuracy_)
          << "Failure integrating ∫₀ᵘ x⋅sin(a⋅x) dx for"
          << " u = " << u << " and a = " << a << " to an accuracy of "
          << integration_accuracy_;
    }
  }
}

INSTANTIATE_TEST_CASE_P(IncreasingAccuracyPrimitiveFunctionTests,
                        PrimitiveFunctionAccuracyTest,
                        ::testing::Values(1e-1, 1e-2, 1e-3, 1e-4, 1e-5));

}  // namespace
}  // namespace systems
}  // namespace drake
