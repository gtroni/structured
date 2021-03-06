/*
 *  OGF/Graphite: Geometry and Graphics Programming Library + Utilities
 *  Copyright (C) 2000 Bruno Levy
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  If you modify this software, you should include a notice giving the
 *  name of the person performing the modification, the date of modification,
 *  and the reason for such modification.
 *
 *  Contact: Bruno Levy
 *
 *     levy@loria.fr
 *
 *     ISA Project
 *     LORIA, INRIA Lorraine, 
 *     Campus Scientifique, BP 239
 *     54506 VANDOEUVRE LES NANCY CEDEX 
 *     FRANCE
 *
 *  Note that the GNU General Public License does not permit incorporating
 *  the Software into proprietary programs. 
 */
 
 

#include <OGF/math/geometry/trgl_grad.h>
#include <OGF/basic/debug/logger.h>

namespace OGF {
    
//_________________________________________________________
    
    TrglGradient::TrglGradient(
        const Point3d& p0, const Point3d& p1, const Point3d& p2
    ) {
        initialize(p0, p1, p2) ;
    }
    
    TrglGradient::TrglGradient() {
    }
    
    void TrglGradient::initialize(
        const Point3d& p0, const Point3d& p1, const Point3d& p2
    ) {
        
        // Computing TX[] and TY[],
        // i.e. the coefficients such that:
        //    | df/dX = TX[0].f(v0) + TX[1].f(v1) + TX[2].f(v2)
        //    | df/dY = TY[0].f(v0) + TY[1].f(v1) + TY[2].f(v2)
        //	
        // (in other words, these coefficient give the gradient of a property
        // interpolated in the triangle T = (a0, a1, a2).
        
        // The equations of the coefficients can be simplified, 
        // the general equations are given as code in comments
        // marked by the tag [simplified]
        
        vertex_[0] = p0 ;
        vertex_[1] = p1 ;
        vertex_[2] = p2 ;    
        
        // Step1: find an orthonormal basis (X, Y) for the triangle.
        //----------------------------------------------------------
        
        Point3d origin ;
        Vector3d X ;
        Vector3d Y ;
        Vector3d Z ;
        
        basis(origin, X, Y, Z) ; //  Rem: origin = v0. 
        
        // Step2: compute the coordinates of the three vertices of the
        // triangle in this basis.
        //------------------------------------------------------------
        
        // [simplified] double x0 = 0.0 ;
        // [simplified] double y0 = 0.0 ; 
        Vector3d V1 = p1 - p0 ;
        double x1 = sqrt(V1 * V1) ;
        
        // [simplified] double y1 = 0.0 ;
        Vector3d V2 = p2 - p0 ;
        double x2 = V2 * X ;
        double y2 = V2 * Y ;
        
        
        // Step3: compute the six coefficients TXi and TYi allowing to
        // compute the two components of the gradient of a function in
        // the basis (I,X,Y).
        //------------------------------------------------------------    
        
        // [simplified] 
        // double d = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0) ;
        
        double d = x1 * y2 ;

        if(fabs(d) < 1e-10)  {
            Logger::warn("TrglGrad")
                << "Attempted gradient computation on a flat triangle"
                << std::endl ;
            d = 1.0 ;
            is_flat_ = true ;
        } else {
            is_flat_ = false ;
        }
        
        // [simplified] TX_[0] = (y1 - y2) / d ;
        // [simplified] TX_[1] = (y2 - y0) / d ;
        // [simplified] TX_[2] = (y0 - y1) / d ;
        
        double xx1 = (x1 == 0.0) ? 1.0 : x1 ; 
        
        TX_[0] = -1.0 / xx1 ;
        TX_[1] =  1.0 / xx1 ;
        TX_[2] =  0.0 ;    
        
        // [simplified] TY_[0] = (x2 - x1) / d ;
        // [simplified] TY_[1] = (x0 - x2) / d ;
        // [simplified] TY_[2] = (x1 - x0) / d ;
        
        double yy2 = (y2 == 0.0) ? 1.0 : y2 ;     
        
        TY_[0] =  x2 / d - 1.0 / yy2 ;
        TY_[1] =  - x2 / d ;
        TY_[2] =  1.0 / yy2 ;
    }
    
    void TrglGradient::basis(
        Point3d& origin, Vector3d& X, Vector3d& Y, Vector3d& Z
    ) const {
        const Point3d& p0 = vertex_[0] ;
        const Point3d& p1 = vertex_[1] ;
        const Point3d& p2 = vertex_[2] ;
        
        X = p1 - p0 ;
        X.normalize() ;
        Z = X ^ (p2 - p0) ;
        Z.normalize() ;
        Y = Z ^ X ;
        origin = p0 ;
    }

    Vector3d TrglGradient::gradient_3d(double value0, double value1, double value2) const {
        double x = TX_[0] * value0 + TX_[1] * value1 ; // Note: TX_[2] = 0
        double y = TY_[0] * value0 + TY_[1] * value1 + TY_[2] * value2 ; 
        Point3d O ;
        Vector3d X, Y, Z ;
        basis(O, X, Y, Z) ;
        return x*X + y*Y ;
    }

    
//_________________________________________________________


    ParamTrglGradient::ParamTrglGradient() {
    }

    ParamTrglGradient::ParamTrglGradient(
        const Point2d& p1, const Point2d& p2, const Point2d& p3
    ) {
        vertex_[0] = p1 ;
        vertex_[1] = p2 ;
        vertex_[2] = p3 ;

        double x1 = p1.x() ;
        double y1 = p1.y() ;
        double x2 = p2.x() ;
        double y2 = p2.y() ;
        double x3 = p3.x() ;
        double y3 = p3.y() ;

        double d = x2*y3 - y2*x3 + x3*y1 - y3*x1 + x1*y2 - y1*x2 ;

        if(fabs(d) < 1e-10) {
            Logger::warn("TrglGrad")
                << "Attempted gradient computation on a flat triangle"
                << std::endl ;
            d = 1 ;
            is_flat_ = true ;
        } else {
            is_flat_ = false ;
        }

        TX_[0] = (y2 - y3)/d ;
        TX_[1] = (y3 - y1)/d ;
        TX_[2] = (y1 - y2)/d ;

        TY_[0] = -(x2 - x3)/d ;
        TY_[1] = -(x3 - x1)/d ;
        TY_[2] = -(x1 - x2)/d ;
    }
    
}

