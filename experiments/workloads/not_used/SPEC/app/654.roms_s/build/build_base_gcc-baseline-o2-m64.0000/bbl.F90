#include "cppdefs.h"
      MODULE bbl_mod
#if defined NONLINEAR && defined BBL_MODEL
!
!svn $Id: bbl.F 294 2009-01-09 21:37:26Z arango $
!================================================== Hernan G. Arango ===
!  Copyright (c) 2002-2009 The ROMS/TOMS Group                         !
!    Licensed under a MIT/X style license                              !
!    See License_ROMS.txt                                              !
!=======================================================================
!                                                                      !
!  This routine computes bottom momentun stress via a bottom boundary  !
!  layer formulation.                                                  !
!                                                                      !
!=======================================================================
!
      implicit none

      PRIVATE
      PUBLIC  :: bblm

      CONTAINS

# if defined SSW_BBL
#  include "ssw_bbl.h"
# elif defined MB_BBL
#  include "mb_bbl.h"
# elif defined SG_BBL
#  include "sg_bbl.h"
# endif

#endif

      END MODULE bbl_mod
