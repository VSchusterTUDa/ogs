/**
 * \copyright
 * Copyright (c) 2012-2016, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/project/license
 *
 */

#ifndef MATHLIB_ODE_ODESOLVERFACTORY_H
#define MATHLIB_ODE_ODESOLVERFACTORY_H

#include <memory>

#include "BaseLib/ConfigTree.h"

#include "OdeSolver.h"

#include "CVodeSolver.h"

namespace MathLib
{
namespace detail
{
template <unsigned N, typename... FunctionArguments>
struct Handles;

template <unsigned N, typename FunctionArgument>
struct Handles<N, FunctionArgument> : public MathLib::FunctionHandles
{
	using Function = MathLib::Function<N, FunctionArgument>;
	using JacobianFunction = MathLib::JacobianFunction<N, FunctionArgument>;

	Handles(Function& f, JacobianFunction& df, FunctionArgument& arg)
	    : f(f), df(df), _data(arg)
	{
	}

	bool call(const double t, const double* const y,
	          double* const ydot) override
	{
		// looks like f and df could be any callable object with suitable
		// signature
		// consider omission of data pointer and switch to std::function or
		// alike
		if (f)
			return f(t,
			         Eigen::Map<const Eigen::Matrix<double, N, 1>>{y},
			         Eigen::Map<Eigen::Matrix<double, N, 1>>{ydot},
			         _data);
		return true;
	}

	bool callJacobian(const double t, const double* const y, double* const ydot,
	                  double* const jac) override
	{
		if (df)
			return df(t,
			          Eigen::Map<const Eigen::Matrix<double, N, 1>>{y},
			          Eigen::Map<Eigen::Matrix<double, N, 1>>{ydot},
			          Eigen::Map<Eigen::Matrix<double, N, N>>{jac /*, order*/},
			          _data);
		return true;
	}

	bool hasJacobian() const override { return df != nullptr; }
	unsigned getNumEquations() const override { return N; }
private:
	Function f = nullptr;
	JacobianFunction df = nullptr;
	FunctionArgument& _data;
};

template <unsigned N>
struct Handles<N> : public MathLib::FunctionHandles
{
	using Function = MathLib::Function<N>;
	using JacobianFunction = MathLib::JacobianFunction<N>;

	Handles(Function& f, JacobianFunction& df) : f(f), df(df) {}
	bool call(const double t, const double* const y,
	          double* const ydot) override
	{
		if (f)
		{
			// auto ydot_ = Eigen::Map<Eigen::Matrix<double, N, 1>>{y};
			// auto ydot_ = Eigen::Map<Eigen::Matrix<double, N, 1>>{ydot};
			return f(t,
			         Eigen::Map<const Eigen::Matrix<double, N, 1>>{y},
			         Eigen::Map<Eigen::Matrix<double, N, 1>>{ydot});
		}
		return true;
	}

	bool callJacobian(const double t, const double* const y, double* const ydot,
	                  double* const jac) override
	{
		if (df)
			return df(t,
			          Eigen::Map<const Eigen::Matrix<double, N, 1>>{y},
			          Eigen::Map<Eigen::Matrix<double, N, 1>>{ydot},
			          Eigen::Map<Eigen::Matrix<double, N, N>>{jac /*, order*/});
		return true;
	}

	bool hasJacobian() const override { return df != nullptr; }
	unsigned getNumEquations() const override { return N; }
	Function f = nullptr;
	JacobianFunction df = nullptr;
};

}  // namespace detail

template <unsigned NumEquations, typename... FunctionArguments>
std::unique_ptr<OdeSolver<NumEquations, FunctionArguments...>> createOdeSolver(
    BaseLib::ConfigTree const& config);

/**
 * ODE solver with a bounds-safe interface.
 *
 * This class makes contact between the abstract \c OdeSolver interface and a
 * certain solver \c Implementation.
 *
 * The interface of this class inherits the array bounds checking from \c
 * OdeSolver.
 * Its methods forward calls to the \c Implementation erasing array bounds info
 * by
 * passing \c std::array as raw pointer.
 *
 * This way the \c Implementation does not need to be templated.
 */
template <unsigned NumEquations, typename Implementation,
          typename... FunctionArguments>
class ConcreteOdeSolver final
    : public OdeSolver<NumEquations, FunctionArguments...>,
      private Implementation
{
public:
	using Interface = OdeSolver<NumEquations, FunctionArguments...>;
	using Arr = typename Interface::Arr;
	using ConstArrRef = typename Interface::ConstArrRef;
	using Function = typename Interface::Function;
	using JacobianFunction = typename Interface::JacobianFunction;

	void setFunction(Function f, JacobianFunction df,
	                 FunctionArguments&... args) override
	{
		Implementation::init(NumEquations);
		_handles.reset(new detail::Handles<NumEquations, FunctionArguments...>{
		    f, df, args...});
		Implementation::setFunction(_handles.get());
	}

	void setTolerance(const Arr& abstol, const double reltol) override
	{
		Implementation::setTolerance(abstol.data(), reltol);
	}

	void setTolerance(const double abstol, const double reltol) override
	{
		Implementation::setTolerance(abstol, reltol);
	}

	void setIC(const double t0, const Arr& y0) override
	{
		Implementation::setIC(t0, y0.data());
	}

	void preSolve() override { Implementation::preSolve(); }
	void solve(const double t) override { Implementation::solve(t); }
	ConstArrRef getSolution() const override
	{
		return ConstArrRef(Implementation::getSolution());
	}

	double getTime() const override { return Implementation::getTime(); }
	Arr getYDot(const double t, const Arr& y) const override
	{
		Arr ydot;
		Implementation::getYDot(t, y.data(), ydot.data());
		return ydot;
	}

private:
	/// instances of this class shall only be constructed by
	/// the friend function listed below
	ConcreteOdeSolver(BaseLib::ConfigTree const& config)
	    : Implementation{config}
	{
	}

	std::unique_ptr<detail::Handles<NumEquations, FunctionArguments...>>
	    _handles;

	friend std::unique_ptr<OdeSolver<NumEquations, FunctionArguments...>>
	createOdeSolver<NumEquations, FunctionArguments...>(
	    BaseLib::ConfigTree const& config);
};

template <unsigned NumEquations, typename... FunctionArguments>
std::unique_ptr<OdeSolver<NumEquations, FunctionArguments...>> createOdeSolver(
    BaseLib::ConfigTree const& config)
{
	return std::unique_ptr<OdeSolver<NumEquations, FunctionArguments...>>(
	    new ConcreteOdeSolver<NumEquations, CVodeSolverInternal,
	                          FunctionArguments...>(config));
}

}  // namespace MathLib

#endif  // MATHLIB_ODE_ODESOLVERFACTORY_H
