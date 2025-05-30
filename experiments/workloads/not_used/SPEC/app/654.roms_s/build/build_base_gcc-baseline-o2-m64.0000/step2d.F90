#include "cppdefs.h"
      MODULE step2d_mod
#ifdef NONLINEAR
!
!svn $Id: step2d.F 294 2009-01-09 21:37:26Z arango $
!=======================================================================
!  Copyright (c) 2002-2009 The ROMS/TOMS Group                         !
!    Licensed under a MIT/X style license           Hernan G. Arango   !
!    See License_ROMS.txt                   Alexander F. Shchepetkin   !
!==================================================== John C. Warner ===
!                                                                      !
!  This subroutine performs a fast (predictor or corrector) time-step  !
!  for the free-surface  and 2D momentum nonlinear equations.
# ifdef SOLVE3D
!  It also calculates the time filtering variables over all fast-time  !
!  steps  to damp high frequency signals in 3D applications.           !
# endif
!                                                                      !
!=======================================================================
!
      implicit none
!
      PRIVATE
      PUBLIC  :: step2d

      CONTAINS

# include "step2d_LF_AM3.h"

#endif
      END MODULE step2d_mod
