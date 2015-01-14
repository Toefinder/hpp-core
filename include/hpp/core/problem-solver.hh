//
// Copyright (c) 2005, 2006, 2007, 2008, 2009, 2010, 2011 CNRS
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

#ifndef HPP_CORE_PROBLEM_SOLVER_HH
# define HPP_CORE_PROBLEM_SOLVER_HH

# include <stdexcept>

# include <hpp/model/fwd.hh>
# include <boost/function.hpp>
# include <hpp/core/deprecated.hh>
# include <hpp/core/problem.hh>
# include <hpp/core/fwd.hh>
# include <hpp/core/config.hh>
# include <hpp/core/config-projector.hh>

namespace hpp {
  namespace core {
    /// Set and solve a path planning problem
    ///
    /// This class is a container that does the interface between
    /// hpp-core library and component to be running in a middleware
    /// like CORBA or ROS.
    class HPP_CORE_DLLAPI ProblemSolver {
    public:
      typedef boost::function < PathPlannerPtr_t (const Problem&,
						  const RoadmapPtr_t&) >
	PathPlannerBuilder_t;
      typedef boost::function < PathOptimizerPtr_t (const Problem&) >
	PathOptimizerBuilder_t;
      typedef boost::function < PathValidationPtr_t (const DevicePtr_t&,
						     const value_type&) >
	PathValidationBuilder_t;
      /// Constructor
      ProblemSolver ();

      /// Destructor
      virtual ~ProblemSolver ();

      /// Set robot
      virtual void robot (const DevicePtr_t& robot);

      /// Get robot
      const DevicePtr_t& robot () const;

      /// Get pointer to problem
      ProblemPtr_t problem ()
      {
	return problem_;
      }
      /// Get shared pointer to initial configuration.
      const ConfigurationPtr_t& initConfig () const
      {
	return initConf_;
      }
      /// Set initial configuration.
      void initConfig (const ConfigurationPtr_t& config);
      /// Get number of goal configuration.
      const Configurations_t& goalConfigs () const;
      /// Add goal configuration.
      void addGoalConfig (const ConfigurationPtr_t& config);
      /// Reset the set of goal configurations
      void resetGoalConfigs ();
      /// Set path planner type
      void pathPlannerType (const std::string& type);
      /// Add a path planner type
      /// \param type name of the new path planner type
      /// \param static method that creates a path planner with a problem
      /// and a roadmap as input
      void addPathPlannerType (const std::string& type,
			       const PathPlannerBuilder_t& builder)
      {
	pathPlannerFactory_ [type] = builder;
      }
      /// Get path planner
      const PathPlannerPtr_t& pathPlanner () const
      {
	return pathPlanner_;
      }

      /// Set path optimizer type
      void pathOptimizerType (const std::string& type);
      /// Get path optimizer
      const PathOptimizerPtr_t& pathOptimizer () const
      {
	return pathOptimizer_;
      }
      /// Add a path optimizer type
      /// \param type name of the new path optimizer type
      /// \param static method that creates a path optimizer with a problem
      /// as input
      void addPathOptimizerType (const std::string& type,
				 const PathOptimizerBuilder_t& builder)
      {
	pathOptimizerFactory_ [type] = builder;
      }

      /// Set path validation method
      /// \param type name of new path validation method
      /// \param tolerance acceptable penetration for path validation
      /// Path validation methods are used to validate edges in path planning
      /// path optimization methods.
      void pathValidationType (const std::string& type,
			       const value_type& tolerance);

      /// Add a path validation type
      /// \param type name of the new path validation method,
      /// \param static method that creates a path validation with a robot
      /// and tolerance as input.
      void addPathValidationType (const std::string& type,
				 const PathValidationBuilder_t& builder)
      {
	pathValidationFactory_ [type] = builder;
      }

      const RoadmapPtr_t& roadmap () const
      {
	return roadmap_;
      }

      /// \name Constraints
      /// \{

      /// Add a constraint
      void addConstraint (const ConstraintPtr_t& constraint);

      /// Add a LockedJoint
      void addLockedJoint (const LockedJointPtr_t& lockedJoint);

      /// Get constraint set
      const ConstraintSetPtr_t& constraints () const
      {
	return constraints_;
      }

      /// Reset constraint set
      virtual void resetConstraints ();

      /// Add differential function to the config projector
      /// \param constraintName Name given to config projector if created by
      ///        this method.
      /// \param functionName name of the function as stored in internal map.
      /// Build the config projector if not yet constructed.
      virtual void addFunctionToConfigProjector
	(const std::string& constraintName, const std::string& functionName);

      /// Add differentialFunction to the config projector
      /// Build the config projector if not constructed
      /// \deprecated use addFunctionToConfigProjector instead.
      virtual void addConstraintToConfigProjector
	(const std::string& constraintName,
	 const DifferentiableFunctionPtr_t& constraint,
	 const ComparisonTypePtr_t comp = Equality::create ())
	HPP_CORE_DEPRECATED
      {
	std::string functionName;
	bool found = false;
	// Check whether constraint is in the map
	for (DifferentiableFunctionMap_t::const_iterator it =
	       NumericalConstraintMap_.begin ();
	     it != NumericalConstraintMap_.end (); ++it) {
	  if (it->second == constraint) {
	    functionName = it->first;
	    found = true;
	  }
	}
	if (!found) {
	  throw std::runtime_error ("constraint is not in the map");
	}
	comparisonTypeMap_ [functionName] = comp;
	addFunctionToConfigProjector (constraintName, functionName);
      }

      /// Add a a numerical constraint in local map.
      /// \param name name of the numerical constraint as stored in local map,
      /// \param constraint numerical constraint
      ///
      /// Numerical constraints are to be inserted in the ConfigProjector of
      /// the constraint set.
      void addNumericalConstraint (const std::string& name,
				   const DifferentiableFunctionPtr_t&
				   constraint)
      {
	NumericalConstraintMap_ [name] = constraint;
        comparisonTypeMap_ [name] = ComparisonType::createDefault();
      }

      /// Set the comparison types of a constraint.
      /// \param name name of the differentiable function.
      void comparisonType (const std::string& name,
			   const ComparisonType::VectorOfTypes types)
      {
        DifferentiableFunctionPtr_t df = NumericalConstraintMap_ [name];
        if (!df)
          throw std::logic_error (std::string ("Numerical constraint ") +
				  name + std::string (" not defined."));
        ComparisonTypesPtr_t eqtypes = ComparisonTypes::create (types);
        comparisonTypeMap_ [name] = eqtypes;
      }

      /// Set the comparison type of a constraint
      /// \param name name of the differentiable function.
      void comparisonType (const std::string& name,
			   const ComparisonTypePtr_t eq)
      {
        comparisonTypeMap_ [name] = eq;
      }

      ComparisonTypePtr_t comparisonType (const std::string& name) const
      {
        ComparisonTypeMap_t::const_iterator it = comparisonTypeMap_.find (name);
        if (it == comparisonTypeMap_.end ())
          return Equality::create ();
        return it->second;
      }

      /// Get constraint with given name
      DifferentiableFunctionPtr_t numericalConstraint (const std::string& name)
      {
	return NumericalConstraintMap_ [name];
      }

      /// Set maximal number of iterations in config projector
      void maxIterations (size_type iterations)
      {
	maxIterations_ = iterations;
	if (constraints_ && constraints_->configProjector ()) {
	  constraints_->configProjector ()->maxIterations (iterations);
	}
      }
      /// Get maximal number of iterations in config projector
      size_type maxIterations () const
      {
	return maxIterations_;
      }

      /// Set error threshold in config projector
      void errorThreshold (const value_type& threshold)
      {
	errorThreshold_ = threshold;
	if (constraints_ && constraints_->configProjector ()) {
	  constraints_->configProjector ()->errorThreshold (threshold);
	}
      }
      /// Get errorimal number of threshold in config projector
      value_type errorThreshold () const
      {
	return errorThreshold_;
      }
      /// \}

      /// Create new problem.
      virtual void resetProblem ();

      /// Reset the roadmap.
      /// \note When joints bounds are changed, the roadmap must be reset
      ///       because the kd tree must be resized.
      virtual void resetRoadmap ();

      /// \name Solve problem and get paths
      /// \{

      /// Create Path optimizer if needed
      ///
      /// If a path optimizer is already set, do nothing.
      /// Type of optimizer is determined by method selectPathOptimizer.
      void createPathOptimizer ();

      /// Prepare the solver for a step by step planning.
      /// and try to make direct connections (call PathPlanner::tryDirectPath)
      /// \return the return value of PathPlanner::pathExists
      virtual bool prepareSolveStepByStep ();

      /// Execute one step of the planner.
      /// \return the return value of PathPlanner::pathExists of the selected planner.
      /// \note This won't check if a solution has been found before doing one step.
      /// The decision to stop planning is let to the user.
      virtual bool executeOneStep ();

      /// Finish the solving procedure
      /// The path optimizer is not called
      virtual void finishSolveStepByStep ();

      /// Set and solve the problem
      virtual void solve ();

      /// Add a path
      void addPath (const PathVectorPtr_t& path)
      {
	paths_.push_back (path);
      }

      /// Return vector of paths
      const PathVectors_t& paths () const
      {
	return paths_;
      }
      /// \}

      /// \name Obstacles
      /// \{

      /// Add obstacle to the list.
      /// \param inObject a new object.
      /// \param collision whether collision checking should be performed
      ///        for this object.
      /// \param distance whether distance computation should be performed
      ///        for this object.
      void addObstacle (const CollisionObjectPtr_t& inObject, bool collision,
			bool distance);

      /// Remove collision pair between a joint and an obstacle
      /// \param jointName name of the joint,
      /// \param obstacleName name of the obstacle
      void removeObstacleFromJoint (const std::string& jointName,
				    const std::string& obstacleName);

      /// Get obstacle by name
      const CollisionObjectPtr_t& obstacle (const std::string& name);

      /// Get list of obstacle names
      ///
      /// \param collision whether to return collision obstacle names
      /// \param distance whether to return distance obstacle names
      /// \return list of obstacle names
      std::list <std::string> obstacleNames (bool collision, bool distance)
	const;

      /// Return list of pair of distance computations
      const DistanceBetweenObjectsPtr_t& distanceBetweenObjects () const
      {
	return distanceBetweenObjects_;
      }
      /// \}

      /// Local vector of objects considered for collision detection
      const ObjectVector_t& collisionObstacles () const;
      /// Local vector of objects considered for distance computation
      const ObjectVector_t& distanceObstacles () const;

    protected:
      /// Store constraints until call to solve.
      ConstraintSetPtr_t constraints_;

      /// Set pointer to problem
      void problem (ProblemPtr_t problem)
      {
        if (problem_)
          delete problem_;
	problem_ = problem;
      }

      /// Set the roadmap
      void roadmap (const RoadmapPtr_t& roadmap)
      {
	roadmap_ = roadmap;
      }

      /// Initialize the new problem
      /// \param problem is inserted in the ProblemSolver and initialized.
      /// \note The previous Problem, if any, is not deleted. The function
      ///       should be called when creating Problem object, in resetProblem()
      ///       and all reimplementation in inherited class.
      void initializeProblem (ProblemPtr_t problem);

    private:
      /// Map (string , constructor of path planner)
      typedef std::map < std::string, PathPlannerBuilder_t >
	PathPlannerFactory_t;
      /// Map (string , constructor of path optimizer)
      typedef std::map < std::string, PathOptimizerBuilder_t >
	PathOptimizerFactory_t;
      /// Map (string , constructor of path validation method)
      typedef std::map <std::string, PathValidationBuilder_t >
	PathValidationFactory_t;
      /// Robot
      DevicePtr_t robot_;
      /// Problem
      ProblemPtr_t problem_;
      /// Shared pointer to initial configuration.
      ConfigurationPtr_t initConf_;
      /// Shared pointer to goal configuration.
      Configurations_t goalConfigurations_;
      /// Path planner
      std::string pathPlannerType_;
      PathPlannerPtr_t pathPlanner_;
      /// Path optimizer
      std::string pathOptimizerType_;
      PathOptimizerPtr_t pathOptimizer_;
      /// Path validation method
      std::string pathValidationType_;
      /// Tolerance of path validation
      value_type pathValidationTolerance_;
      /// Store roadmap
      RoadmapPtr_t roadmap_;
      /// Paths
      PathVectors_t paths_;
      /// Path planner factory
      PathPlannerFactory_t pathPlannerFactory_;
      /// Path optimizer factory
      PathOptimizerFactory_t pathOptimizerFactory_;
      /// Path validation factory
      PathValidationFactory_t pathValidationFactory_;
      /// Store obstacles until call to solve.
      ObjectVector_t collisionObstacles_;
      ObjectVector_t distanceObstacles_;
      /// Map of obstacles by names
      std::map <std::string, CollisionObjectPtr_t> obstacleMap_;
      // Tolerance for numerical constraint resolution
      value_type errorThreshold_;
      // Maximal number of iterations for numerical constraint resolution
      size_type maxIterations_;
      /// Map of constraints
      DifferentiableFunctionMap_t NumericalConstraintMap_;
      /// Map of inequality
      ComparisonTypeMap_t comparisonTypeMap_;
      /// Computation of distances to obstacles
      DistanceBetweenObjectsPtr_t distanceBetweenObjects_;
    }; // class ProblemSolver
  } // namespace core
} // namespace hpp

#endif // HPP_CORE_PROBLEM_SOLVER_HH
