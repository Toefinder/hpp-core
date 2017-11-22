//
// Copyright (c) 2014 CNRS
// Authors: Florent Lamiraux
//
// This file is part of hpp-core
// hpp-core is free software: you can redistribute it
// and/or modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation, either version
// 3 of the License, or (at your option) any later version.
//
// hpp-core is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Lesser Public License for more details.  You should have
// received a copy of the GNU Lesser General Public License along with
// hpp-core  If not, see
// <http://www.gnu.org/licenses/>.

#ifndef HPP_CORE_PATH_HH
# define HPP_CORE_PATH_HH

# include <boost/concept_check.hpp>
# include <hpp/util/exception.hh>
# include <hpp/core/fwd.hh>
# include <hpp/core/config.hh>
# include <hpp/core/constraint-set.hh>
# include <hpp/core/deprecated.hh>
# include <hpp/core/projection-error.hh>
# include <hpp/core/time-parameterization.hh>

namespace hpp {
  namespace core {
    /// \addtogroup path Path
    /// Path abstraction, implementation and decorators
    /// \{

    /// Abstraction of paths: mapping from time to configuration space
    ///
    class HPP_CORE_DLLAPI Path
    {
    public:
      /// \name Construction, destruction, copy
      /// \{

      /// Destructor
      virtual ~Path () throw () {}

      /// Return a shared pointer to a copy of this
      virtual PathPtr_t copy () const = 0;

      /// Return a shared pointer to a copy of this and set constraints
      ///
      /// \param constraints constraints to apply to the copy
      /// \precond *this should not have constraints.
      virtual PathPtr_t copy (const ConstraintSetPtr_t& constraints) const = 0;

      /// Static cast into a derived type
      template <class T> boost::shared_ptr<T> as (void)
      {
	assert (HPP_DYNAMIC_PTR_CAST (T, weak_.lock ()));
	return HPP_STATIC_PTR_CAST (T, weak_.lock ());
      }

      /// Static cast into a derived type
      template <class T> boost::shared_ptr<const T> as (void) const
      {
	assert (HPP_DYNAMIC_PTR_CAST (const T, weak_.lock ()));
	return HPP_STATIC_PTR_CAST (const T, weak_.lock ());
      }

      /// Extraction/Reversion of a sub-path
      /// \param subInterval interval of definition of the extract path
      /// If upper bound of subInterval is smaller than lower bound,
      /// result is reversed.
      /// \exception projection_error is thrown when an end configuration of
      ///                             the returned path could not be computed
      ///                             due to projection failure.
      virtual PathPtr_t extract (const interval_t& subInterval) const
        throw (projection_error);

      /// Reversion of a path
      /// \return a new path that is this one reversed.
      virtual PathPtr_t reverse () const;

      /// \}

      Configuration_t operator () (const value_type& time) const
        HPP_CORE_DEPRECATED
      {
	Configuration_t result (outputSize ());
	impl_compute (result, paramAtTime(time));
	if (constraints_) {
	  constraints_->apply (result);
	}
	return result;
      }

      Configuration_t operator () (const value_type& time, bool& success) const
      {
        return configAtParam (paramAtTime(time), success);
      }

      bool operator () (ConfigurationOut_t result, const value_type& time)
       const throw ()
      {
	bool success = impl_compute (result, paramAtTime(time));
	if (!success) return false;
	return (!constraints_ || constraints_->apply (result));
      }

      /// Get the configuration at a parameter without applying the constraints.
      bool at (const value_type& time, ConfigurationOut_t result) const
      {
        return impl_compute (result, paramAtTime(time));
      }

      /// Get derivative with respect to parameter at given parameter
      /// \param time value of the time in the definition interval,
      /// \param order order of the derivative
      /// \retval result derivative. Should be allocated and of correct size.
      /// \warning the method is not implemented in this class and throws if
      ///          called without being implemented in the derived class.
      /// \note unless otherwise stated, this method is not compatible with
      ///       constraints. The derivative of the non-constrained path will
      ///       be computed.
      void derivative (vectorOut_t result, const value_type& time,
          size_type order) const
      {
        if (timeParam_) {
          assert (order == 1);
          impl_derivative (result, timeParam_->value(time), order);
          result *= timeParam_->derivative(time);
        } else {
          impl_derivative (result, time, order);
        }
      }

      /// Get the maximum velocity on a sub-interval of this path
      /// \param t0 begin of the interval
      /// \param t1 end of the interval
      /// \retval result maximal derivative on the sub-interval. Should be allocated and of correct size.
      /// \warning the method is not implemented in this class and throws if
      ///          called without being implemented in the derived class.
      /// \note unless otherwise stated, this method is not compatible with
      ///       constraints. The derivative of the non-constrained path will
      ///       be computed.
      void velocityBound (vectorOut_t result, const value_type& t0, const value_type& t1) const
      {
        assert(result.size() == outputDerivativeSize());
        assert(t0 <= t1);
	impl_velocityBound (result,
            paramAtTime (std::max(t0, timeRange().first )),
            paramAtTime (std::min(t1, timeRange().second)));
        if (timeParam_)
          result *= timeParam_->derivativeBound (t0, t1);
      }

      /// \name Constraints
      /// \{

      /// Get constraints the path is subject to
      const ConstraintSetPtr_t& constraints () const
      {
	return constraints_;
      }
      /// \}

      /// Get size of configuration space
      size_type outputSize () const
      {
	return outputSize_;
      }

      /// Get size of velocity
      size_type outputDerivativeSize () const
      {
	return outputDerivativeSize_;
      }

      /// Get interval of definition
      const interval_t& timeRange () const
      {
	return timeRange_;
      }

      /// Get interval of parameters.
      /// If the instance contains a \ref timeParam_
      /// this returns timeParam_ (timeRange())
      /// otherwise, it returns \ref timeRange
      const interval_t& paramRange () const
      {
	return paramRange_;
      }

      /// Set the time parameterization function
      void timeParameterization (const TimeParameterizationPtr_t& tp,
          const interval_t& tr)
      {
        timeParam_ = tp;
        timeRange (tr);
      }

      /// Get length of definition interval
      value_type length () const
      {
	return timeRange_.second - timeRange_.first;
      }

      /// Get the initial configuration
      virtual Configuration_t initial () const = 0;

      /// Get the final configuration
      virtual Configuration_t end () const = 0;

    protected:
      /// Print interval of definition (and of parameters if relevant)
      /// in a stream
      virtual std::ostream& print (std::ostream &os) const;

      /// Constructor
      /// \param interval interval of definition of the path,
      /// \param outputSize size of the output configuration,
      /// \param outputDerivativeSize number of degrees of freedom of  the
      ///        underlying robot
      /// \param constraints constraints the set is subject to,
      ///        constraints are solved at each evaluation of the output
      ///        configuration.
      /// \note Constraints are copied.
      Path (const interval_t& interval, size_type outputSize,
	    size_type outputDerivativeSize,
	    const ConstraintSetPtr_t& constraints);

      /// Constructor
      /// \param interval interval of definition of the path,
      /// \param outputSize size of the output configuration,
      /// \param outputDerivativeSize number of degrees of freedom of  the
      ///        underlying robot
      Path (const interval_t& interval, size_type outputSize,
	    size_type outputDerivativeSize);

      /// Copy constructor
      Path (const Path& path);

      /// Copy constructor with constraints
      Path (const Path& path, const ConstraintSetPtr_t& constraints);

      /// Store weak pointer to itself
      ///
      /// should be called at construction of derived class instances
      void init (const PathWkPtr_t& self);

      /// Interval of parameters
      interval_t paramRange_;

      /// Set the constraints
      /// \warning this method is protected for child classes that need to
      ///          initialize themselves before being sure that the initial and
      ///          end configuration satisfy the constraints
      void constraints (const ConstraintSetPtr_t& constraint) {
        constraints_ = constraint;
      }

      /// Should be called by child classes after having init.
      virtual void checkPath () const;

      void timeRange (const interval_t& timeRange)
      {
        timeRange_ = timeRange;
        if (timeParam_) {
          paramRange_.first  = timeParam_->value(timeRange_.first );
          paramRange_.second = timeParam_->value(timeRange_.second);
        } else
          paramRange_ = timeRange_;
      }

      const TimeParameterizationPtr_t& timeParameterization() const
      {
        return timeParam_;
      }

      value_type paramLength() const
      {
        return paramRange_.second - paramRange_.first;
      }

      Configuration_t configAtParam (const value_type& param, bool& success) const
      {
	Configuration_t result (outputSize ());
	success = impl_compute (result, param);
	if (!success) return result;
	if (constraints_) {
	  success = constraints_->apply (result);
	}
	return result;
      }

      /// \brief Function evaluation without applying constraints
      ///
      /// \return true if everything went good.
      virtual bool impl_compute (ConfigurationOut_t configuration,
				 value_type param) const = 0;

      /// Virtual implementation of derivative
      virtual void impl_derivative (vectorOut_t, const value_type&,
				    size_type) const
      {
	HPP_THROW_EXCEPTION (hpp::Exception, "not implemented");
      }

      /// Virtual implementation of velocityBound
      virtual void impl_velocityBound (vectorOut_t, const value_type&, const value_type&) const
      {
	HPP_THROW_EXCEPTION (hpp::Exception, "not implemented");
      }

    private:
      /// Interval of definition
      interval_t timeRange_;

      value_type paramAtTime (const value_type& time) const
      {
        if (timeParam_) {
          return timeParam_->value (time);
        }
        return time;
      }

      /// Size of the configuration space
      size_type outputSize_;
      /// Number of degrees of freedom of the robot
      size_type outputDerivativeSize_;
      /// Constraints that apply to the robot
      ConstraintSetPtr_t constraints_;
      /// Time parameterization
      TimeParameterizationPtr_t timeParam_;
      /// Weak pointer to itself
      PathWkPtr_t weak_;
      friend std::ostream& operator<< (std::ostream& os, const Path& path);
    }; // class Path
    inline std::ostream& operator<< (std::ostream& os, const Path& path)
    {
      return path.print (os);
    }
    /// \}

  } //   namespace core
} // namespace hpp
#endif // HPP_CORE_PATH_HH
