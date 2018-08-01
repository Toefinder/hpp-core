// Copyright (c) 2018 LAAS-CNRS
// Authors: Joseph Mirabel
//
// This file is part of hpp-core.
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
// hpp-core. If not, see <http://www.gnu.org/licenses/>.

#ifndef HPP_CORE_SRC_TIMES_FRAME_FUNCTION_HH
# define HPP_CORE_SRC_TIMES_FRAME_FUNCTION_HH

#include <Eigen/Geometry>
#include <pinocchio/spatial/explog.hpp>
#include <hpp/pinocchio/liegroup-element.hh>
#include <hpp/constraints/differentiable-function.hh>

namespace hpp {
  namespace core {
    typedef constraints::vector6_t vector6_t;
    typedef pinocchio::LiegroupConstElementRef LiegroupConstElementRef;
    /// Right multiplication be a constant in SE(3)
    ///
    /// Mapping from SE(3) to SE(3) that maps
    ///
    ///    x -> x + log (M)
    /// where M is a constant element of SE (3)
    struct TimesFrameFunction : public DifferentiableFunction {

      typedef Eigen::Quaternion<value_type>  Quaternion_t;
      typedef Eigen::Map<const Quaternion_t> QuaternionMap_t;

      TimesFrameFunction (const Transform3f& M, std::string name)
        : DifferentiableFunction (7, 6, LiegroupSpace::SE3 (), name),
          oMi_ (M), logM_ (se3::log6 (M).toVector ()),
        oQi_ (M.rotation())
      {}

      /// \f$ SE3(y) \gets SE3(x) \times {}^0M_i \f$
      inline void impl_compute (LiegroupElement& y, vectorIn_t x) const
      {
        // Returns oMi * SE3(x)
        // QuaternionMap_t iQ (x.tail<4>().data());
        // y.vector().head<3>() = oMi_.actOnEigenObject(x.head<3>());
        // y.vector().tail<4>() = (oQi_ * iQ).coeffs();

        // Returns SE3(x) * oMi
        QuaternionMap_t iQ (x.tail<4>().data());
        y.vector().head<3>() = iQ._transformVector(oMi_.translation()) + x.head<3>();
        y.vector().tail<4>() = (iQ * oQi_).coeffs();
        LiegroupConstElementRef x1 (x, LiegroupSpace::SE3 ());
        LiegroupElement y1 (x1 + logM_);
        assert ((y - y1).squaredNorm () < 1e-12);
      }

      /** Returns a constant Jacobian.
       *  \f$ J \gets \left( \begin{array}{cc} 
       *   {}^0R^T_i & - {}^0R^T_i \left[ {}^0t_i\right]_X \\
       *     0_3     & {}^iR^T_0
       *  \end{array} \right) \f$
      **/
      inline void impl_jacobian (matrixOut_t J, vectorIn_t) const
      {
        // Returns oMi * SE3(x)
        // J. topLeftCorner<3,3>().noalias() = oMi_.rotation();
        // J.topRightCorner<3,3>().setZero();
        // J. bottomLeftCorner<3,3>().setZero();
        // J.bottomRightCorner<3,3>().noalias() = oMi_.rotation();

        // Returns SE3(x) * oMi
        // Local frame
        J. topLeftCorner<3,3>().noalias() = oMi_.rotation().transpose();
        J.topRightCorner<3,3>().noalias() = - oMi_.rotation().transpose() * se3::skew(oMi_.translation());
        J. bottomLeftCorner<3,3>().setZero();
        J.bottomRightCorner<3,3>().noalias() = oMi_.rotation().transpose();
      }

      Transform3f oMi_;
      vector6_t logM_;
      Quaternion_t oQi_;
    };
  } // namespace core
} // namespace hpp
#endif // HPP_CORE_SRC_TIMES_FRAME_FUNCTION_HH
