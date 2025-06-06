#include "cppdefs.h"
      SUBROUTINE get_bounds (ng, tile, gtype, Nghost, Itile, Jtile,     &
     &                       LBi, UBi, LBj, UBj)
!
!svn $Id: get_bounds.F 303 2009-01-26 22:49:13Z arango $
!================================================== Hernan G. Arango ===
!  Copyright (c) 2002-2009 The ROMS/TOMS Group                         !
!    Licensed under a MIT/X style license                              !
!    See License_ROMS.txt                                              !
!=======================================================================
!                                                                      !
!  This routine compute grid bounds in the I- and J-directions.        !
!                                                                      !
!  On Input:                                                           !
!                                                                      !
!     ng         Nested grid number.                                   !
!     tile       Domain partition.                                     !
!     gtype      C-grid type. If zero, compute array allocation bounds.!
!                  Otherwise, compute bounds for IO processing.        !
!     Nghost     Number of ghost-points in the halo region:            !
!                  Nghost = 0,  compute non-overlapping bounds.        !
!                  Nghost > 0,  compute overlapping bounds.            !
!                                                                      !
!  On Output:                                                          !
!                                                                      !
!     Itile      Tile coordinate in the I-direction, use only in       !
!                  distributed-memory applications.                    !
!     Jtile      Tile coordinate in the J-direction, use only in       !
!                  distributed-memory applications.                    !
!     LBi        I-dimension Lower bound.                              !
!     UBi        I-dimension Upper bound.                              !
!     LBj        J-dimension Lower bound.                              !
!     UBj        J-dimension Upper bound.                              !
!                                                                      !
!=======================================================================
!
      USE mod_param
      USE mod_parallel
      USE mod_ncparam
!
      implicit none
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile, gtype, Nghost
      integer, intent(out) :: Itile, Jtile, LBi, UBi, LBj, UBj
#ifdef DISTRIBUTE
!
!  Local variable declarations.
!
      integer :: Imin, Imax, Jmin, Jmax
      integer :: Istr, IstrR, IstrT, IstrU, Iend, IendR, IendT
      integer :: Jstr, JstrR, JstrT, JstrV, Jend, JendR, JendT
      integer :: MyType
!
!-----------------------------------------------------------------------
!  Set array bounds in the I- and J-direction for distributed-memory
!  configurations.
!-----------------------------------------------------------------------
!
!  Set first and last grid-points according to staggered C-grid
!  classification.  If gtype = 0, it returns the values needed for
!  array allocation. Otherwise, it returns the values needed for IO
!  processing.
!
      MyType=ABS(gtype)
      IF (MyType.eq.0) THEN
        Imin=LOWER_BOUND_I
        Imax=UPPER_BOUND_I
        Jmin=LOWER_BOUND_J
        Jmax=UPPER_BOUND_J
      ELSE
        Imin=0
        Imax=Lm(ng)+1
        Jmin=0
        Jmax=Mm(ng)+1
        IF ((MyType.eq.p2dvar).or.(MyType.eq.u2dvar).or.                &
     &      (MyType.eq.p3dvar).or.(MyType.eq.u3dvar)) Imin=1
        IF ((MyType.eq.p2dvar).or.(MyType.eq.v2dvar).or.                &
     &      (MyType.eq.p3dvar).or.(MyType.eq.v3dvar)) Jmin=1
      END IF
!
!  Set physical, overlapping (Nghost>0) or non-overlapping (Nghost=0) 
!  grid bounds according to tile rank.
!
      CALL get_tile (ng, tile, Itile, Jtile,                            &
     &               Istr, Iend, Jstr, Jend,                            &
     &               IstrR, IstrT, IstrU, IendR, IendT,                 &
     &               JstrR, JstrT, JstrV, JendR, JendT)
!
      IF ((Itile.eq.-1).or.(Itile.eq.0)) THEN
        LBi=Imin
      ELSE
        LBi=Istr-Nghost
      END IF
      IF ((Itile.eq.-1).or.(Itile.eq.(NtileI(ng)-1))) THEN
        UBi=Imax
      ELSE
        UBi=Iend+Nghost
      END IF
      IF ((Jtile.eq.-1).or.(Jtile.eq.0)) THEN
        LBj=Jmin
      ELSE
        LBj=Jstr-Nghost
      END IF
      IF ((Jtile.eq.-1).or.(Jtile.eq.(NtileJ(ng)-1))) THEN
        UBj=Jmax
      ELSE
        UBj=Jend+Nghost
      END IF
#else
!
!-----------------------------------------------------------------------
!  Set array allocation bounds in the I- and J-direction for serial and
!  shared-memory configurations.
!-----------------------------------------------------------------------
!
      Itile=-1
      Jtile=-1
      LBi=LOWER_BOUND_I
      UBi=UPPER_BOUND_I
      LBj=LOWER_BOUND_J
      UBj=UPPER_BOUND_J
#endif
      RETURN
      END SUBROUTINE get_bounds

      SUBROUTINE get_domain (ng, tile, gtype, Nghost,                   &
     &                       epsilon, Lfullgrid,                        &
     &                       Xmin, Xmax, Ymin, Ymax)
!
!=======================================================================
!                                                                      !
!  This routine computes tile minimum and maximum fractional grid      !
!  coordinates.                                                        !
!                                                                      !
!  On Input:                                                           !
!                                                                      !
!     ng         Nested grid number.                                   !
!     tile       Domain partition.                                     !
!     Nghost     Number of ghost-points in the halo region:            !
!                  Nghost = 0,  compute non-overlapping coordinates.   !
!                  Nghost > 0,  compute overlapping bounds.            !
!     gtype      C-grid type                                           !
!     epsilon    Small value to add to Xmax and Ymax when the tile     !
!                  is lying on the eastern and northern boundaries     !
!                  of the grid. This is usefull when processing        !
!                  observations.                                       !
!     Lfullgrid  Switch to include interior and boundaries points      !
!                  (TRUE) or just interior points (FALSE).             !
!                                                                      !
!  On Output:                                                          !
!                                                                      !
!     Xmin       Minimum tile fractional X-coordinate.                 !
!     Xmax       Maximum tile fractional X-coordinate.                 !
!     Ymin       Minimum tile fractional Y-coordinate.                 !
!     Ymax       Maximum tile fractional Y-coordinate.                 !
!                                                                      !
!=======================================================================
!
      USE mod_param
      USE mod_parallel
      USE mod_ncparam
!
      implicit none
!
!  Imported variable declarations.
!
      logical, intent(in) :: Lfullgrid

      integer, intent(in) :: ng, tile, gtype, Nghost

      real(r8), intent(in) :: epsilon
      real(r8), intent(out) :: Xmin, Xmax, Ymin, Ymax
!
!  Local variable declarations.
!
      integer :: Imin, Imax, Jmin, Jmax
      integer :: Itile, Jtile
!
!-----------------------------------------------------------------------
!  Computes tile minimum and maximum fractional-grid coordinates.
!-----------------------------------------------------------------------
! 
      CALL get_bounds (ng, tile, gtype, Nghost, Itile, Jtile,           &
     &                 Imin, Imax, Jmin, Jmax)
!
!  Include interior and boundary points.
!
      IF (Lfullgrid) THEN
        IF ((Itile.eq.0).and.                                           &
     &      ((gtype.eq.r2dvar).or.(gtype.eq.r3dvar).or.                 &
     &       (gtype.eq.v2dvar).or.(gtype.eq.v3dvar))) THEN
          Xmin=REAL(Imin,r8)
        ELSE
          Xmin=REAL(Imin,r8)-0.5_r8
        END IF
        IF (Itile.eq.(NtileI(ng)-1)) THEN
          IF ((gtype.eq.u2dvar).or.(gtype.eq.u3dvar)) THEN
            Xmax=REAL(Imax,r8)-0.5_r8
          ELSE
            Xmax=REAL(Imax,r8)
          END IF
        ELSE
          Xmax=REAL(Imax,r8)+0.5_r8
        END IF
        IF ((Jtile.eq.0).and.                                           &
     &      ((gtype.eq.r2dvar).or.(gtype.eq.r3dvar).or.                 &
     &       (gtype.eq.u2dvar).or.(gtype.eq.u3dvar))) THEN
          Ymin=REAL(Jmin,r8)
        ELSE
          Ymin=REAL(Jmin,r8)-0.5_r8
        END IF
        IF (Jtile.eq.(NtileJ(ng)-1)) THEN
          IF ((gtype.eq.v2dvar).or.(gtype.eq.v3dvar)) THEN
            Ymax=REAL(Jmax,r8)-0.5_r8
          ELSE
            Ymax=REAL(Jmax,r8)
          END IF
        ELSE
          Ymax=REAL(Jmax,r8)+0.5_r8
        END IF
!
!   Include only interior points.
!
      ELSE
        IF (Itile.eq.0) THEN
          IF ((gtype.eq.u2dvar).or.(gtype.eq.u3dvar)) THEN
             Xmin=REAL(Imin,r8)
          ELSE
             Xmin=REAL(Imin,r8)+0.5_r8
          END IF
        ELSE
          Xmin=REAL(Imin,r8)-0.5_r8
        END IF
        IF (Itile.eq.(NtileI(ng)-1)) THEN
          IF ((gtype.eq.u2dvar).or.(gtype.eq.u3dvar)) THEN
            Xmax=REAL(Imax,r8)-1.0_r8
          ELSE
            Xmax=REAL(Imax,r8)-0.5_r8
          END IF
        ELSE
          Xmax=REAL(Imax,r8)+0.5_r8
        END IF
        IF (Jtile.eq.0) THEN
          IF ((gtype.eq.v2dvar).or.(gtype.eq.v3dvar)) THEN
            Ymin=REAL(Jmin,r8)
          ELSE
            Ymin=REAL(Jmin,r8)+0.5
          END IF
        ELSE
          Ymin=REAL(Jmin,r8)-0.5_r8
        END IF
        IF (Jtile.eq.(NtileJ(ng)-1)) THEN
          IF ((gtype.eq.v2dvar).or.(gtype.eq.v3dvar)) THEN
            Ymax=REAL(Jmax,r8)-1.0_r8
          ELSE
            Ymax=REAL(Jmax,r8)-0.5_r8
          END IF
        ELSE
          Ymax=REAL(Jmax,r8)+0.5_r8
        END IF
      END IF
!
!  If tile lie at the grid eastern or northen boundary, add provided
!  offset value to allow processing at those boundaries.
!
      IF (Itile.eq.(NtileI(ng)-1)) THEN
        Xmax=Xmax+epsilon
      END IF
      IF (Jtile.eq.(NtileJ(ng)-1)) THEN
        Ymax=Ymax+epsilon
      END IF

      RETURN     

      END SUBROUTINE get_domain

      SUBROUTINE get_iobounds (ng)
!
!=======================================================================
!                                                                      !
!  This routine computes the horizontal lower bound, upper bound, and  !
!  grid size for IO (NetCDF) variables.  Nested grids require special  !
!  attention due to their connetivity.                                 !
!                                                                      !
!  On Input:                                                           !
!                                                                      !
!     ng         Nested grid number.                                   !
!                                                                      !
!  On Output, the horizontal lower/upper bounds and grid size for      !
!  each variable type and nested grid number  are loaded into the      !
!  IOBOUNDS structure which is declared in module MOD_PARAM:           !
!                                                                      ! 
!   IOBOUNDS(ng) % ILB_psi     I-direction lower bound (PSI)           !
!   IOBOUNDS(ng) % IUB_psi     I-direction upper bound (PSI)           !
!   IOBOUNDS(ng) % JLB_psi     J-direction lower bound (PSI)           !
!   IOBOUNDS(ng) % JUB_psi     J-direction upper bound (PSI)           !
!                                                                      !
!   IOBOUNDS(ng) % ILB_rho     I-direction lower bound (RHO)           !
!   IOBOUNDS(ng) % IUB_rho     I-direction upper bound (RHO)           !
!   IOBOUNDS(ng) % JLB_rho     J-direction lower bound (RHO)           !
!   IOBOUNDS(ng) % JUB_rho     J-direction upper bound (RHO)           !
!                                                                      !
!   IOBOUNDS(ng) % ILB_u       I-direction lower bound (U)             !
!   IOBOUNDS(ng) % IUB_u       I-direction upper bound (U)             !
!   IOBOUNDS(ng) % JLB_u       J-direction lower bound (U)             !
!   IOBOUNDS(ng) % JUB_u       J-direction upper bound (U)             !
!                                                                      !
!   IOBOUNDS(ng) % ILB_v       I-direction lower bound (V)             !
!   IOBOUNDS(ng) % IUB_v       I-direction upper bound (V)             !
!   IOBOUNDS(ng) % JLB_v       J-direction lower bound (V)             !
!   IOBOUNDS(ng) % JUB_v       J-direction upper bound (V)             !
!                                                                      !
!   IOBOUNDS(ng) % xi_psi      Number of I-direction points (PSI)      !
!   IOBOUNDS(ng) % xi_rho      Number of I-direction points (RHO)      !
!   IOBOUNDS(ng) % xi_u        Number of I-direction points (U)        !
!   IOBOUNDS(ng) % xi_v        Number of I-direction points (V)        !
!                                                                      !
!   IOBOUNDS(ng) % eta_psi     Number of J-direction points (PSI)      !
!   IOBOUNDS(ng) % eta_rho     Number of J-direction points (RHO)      !
!   IOBOUNDS(ng) % eta_u       Number of I-direction points (U)        !
!   IOBOUNDS(ng) % eta_v       Number of I-direction points (V)        !
!                                                                      !
!=======================================================================
!
      USE mod_param
!
      implicit none
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng
!
!-----------------------------------------------------------------------
!  Set IO lower/upper bounds and grid size for each C-grid type
!  variable.
!-----------------------------------------------------------------------
!
!  Recall that in non-nested applications the horizontal range,
!  including interior and boundary points, for all variable types
!  are:
!
!    PSI-type      [xi_psi, eta_psi] = [1:Lm(ng)+1, 1:Mm(ng)+1]
!    RHO-type      [xi_rho, eta_rho] = [0:Lm(ng)+1, 0:Mm(ng)+1]
!    U-type        [xi_u,   eta_u  ] = [1:Lm(ng)+1, 0:Mm(ng)+1]
!    V-type        [xi_v,   eta_v  ] = [0:Lm(ng)+1, 1:Mm(ng)+1]
!
      IOBOUNDS(ng) % ILB_psi = 1
      IOBOUNDS(ng) % IUB_psi = Lm(ng)+1
      IOBOUNDS(ng) % JLB_psi = 1
      IOBOUNDS(ng) % JUB_psi = Mm(ng)+1
!
      IOBOUNDS(ng) % ILB_rho = 0
      IOBOUNDS(ng) % IUB_rho = Lm(ng)+1
      IOBOUNDS(ng) % JLB_rho = 0
      IOBOUNDS(ng) % JUB_rho = Mm(ng)+1
!
      IOBOUNDS(ng) % ILB_u = 1
      IOBOUNDS(ng) % IUB_u = Lm(ng)+1
      IOBOUNDS(ng) % JLB_u = 0
      IOBOUNDS(ng) % JUB_u = Mm(ng)+1
!
      IOBOUNDS(ng) % ILB_v = 0
      IOBOUNDS(ng) % IUB_v = Lm(ng)+1
      IOBOUNDS(ng) % JLB_v = 1
      IOBOUNDS(ng) % JUB_v = Mm(ng)+1
!
!  Set IO NetCDF files horizontal dimension size. Recall that NetCDF
!  does not support arrays with zero index as an array element.
!
      IOBOUNDS(ng) % IorJ    = BOUNDS(ng) % UBij -                      &
     &                         BOUNDS(ng) % LBij + 1
!
      IOBOUNDS(ng) % xi_psi  = IOBOUNDS(ng) % IUB_psi -                 &
     &                         IOBOUNDS(ng) % ILB_psi + 1
      IOBOUNDS(ng) % xi_rho  = IOBOUNDS(ng) % IUB_rho -                 &
     &                         IOBOUNDS(ng) % ILB_rho + 1
      IOBOUNDS(ng) % xi_u    = IOBOUNDS(ng) % IUB_u   -                 &
     &                         IOBOUNDS(ng) % ILB_u   + 1
      IOBOUNDS(ng) % xi_v    = IOBOUNDS(ng) % IUB_v   -                 &
     &                         IOBOUNDS(ng) % ILB_v   + 1
!
      IOBOUNDS(ng) % eta_psi = IOBOUNDS(ng) % JUB_psi -                 &
     &                         IOBOUNDS(ng) % JLB_psi + 1
      IOBOUNDS(ng) % eta_rho = IOBOUNDS(ng) % JUB_rho -                 &
     &                         IOBOUNDS(ng) % JLB_rho + 1
      IOBOUNDS(ng) % eta_u   = IOBOUNDS(ng) % JUB_u   -                 &
     &                         IOBOUNDS(ng) % JLB_u   + 1
      IOBOUNDS(ng) % eta_v   = IOBOUNDS(ng) % JUB_v   -                 &
     &                         IOBOUNDS(ng) % JLB_v   + 1
      
      RETURN
      END SUBROUTINE get_iobounds

      SUBROUTINE get_tile (ng, tile, Itile, Jtile,                      &
     &                     Istr, Iend, Jstr, Jend,                      &
     &                     IstrR, IstrT, IstrU, IendR, IendT,           &
     &                     JstrR, JstrT, JstrV, JendR, JendT)
!
!=======================================================================
!                                                                      !
!  This routine computes the starting and ending horizontal indices    !
!  for each sub-domain partition or tile.                              !
!                                                                      !
!  On Input:                                                           !
!                                                                      !
!     ng         Nested grid number (integer).                         !
!     tile       Sub-domain partition.                                 !
!                                                                      !
!  On Output:                                                          !
!                                                                      !
!     Itile      I-tile coordinate (a value from 0 to NtileI(ng)).     !
!     Jtile      J-tile coordinate (a value from 0 to NtileJ(ng)).     !
!     Istr       Starting tile index in the I-direction.               !
!     Iend       Ending   tile index in the I-direction.               !
!     Jstr       Starting tile index in the J-direction.               !
!     Jend       Ending   tile index in the J-direction.               !
!                                                                      !
!     IstrR      Starting tile index in the I-direction (RHO-points).  !
!     IstrT      Starting nest tile  in the I-direction (RHO-points).  !
!     IstrU      Starting tile index in the I-direction (U-points).    !
!     IendR      Ending   tile index in the I-direction (RHO_points).  !
!     IendT      Ending   nest tile  in the I-direction (RHO_points).  !
!                                                                      !
!     JstrR      Starting tile index in the J-direction (RHO-points).  !
!     JstrT      Starting nest tile  in the J-direction (RHO-points).  !
!     JstrV      Starting tile index in the J-direction (V-points).    !
!     JendR      Ending   tile index in the J-direction (RHO_points).  !
!     JendT      Ending   nest tile  in the J-direction (RHO-points).  !
!                                                                      !
!======================================================================!
!
      USE mod_param
!
      implicit none
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile
      integer, intent(out) :: Itile, Jtile
      integer, intent(out) :: Iend, Istr, Jend, Jstr
      integer, intent(out) :: IstrR, IstrT, IstrU, IendR, IendT
      integer, intent(out) :: JstrR, JstrT, JstrV, JendR, JendT
!
!  Local variable declarations.
!
      integer :: ChunkSizeI, ChunkSizeJ, MarginI, MarginJ
!
!-----------------------------------------------------------------------
!  Set physical non-overlapping grid bounds according to tile rank.
!-----------------------------------------------------------------------
!
!  Non-tiled grid bounds.  This is used in serial or shared-memory
!  modes to compute values in the full grid outside of parallel
!  regions.
!
      IF (tile.eq.-1) THEN
        Itile=-1
        Jtile=-1
        Istr=1
        Iend=Lm(ng)
        Jstr=1
        Jend=Mm(ng)
!
! Tiled grids bounds.
!
      ELSE
        ChunkSizeI=(Lm(ng)+NtileI(ng)-1)/NtileI(ng)
        ChunkSizeJ=(Mm(ng)+NtileJ(ng)-1)/NtileJ(ng)
        MarginI=(NtileI(ng)*ChunkSizeI-Lm(ng))/2
        MarginJ=(NtileJ(ng)*ChunkSizeJ-Mm(ng))/2
        Jtile=tile/NtileI(ng)
        Itile=tile-Jtile*NtileI(ng)
!
!  Tile bounds in the I-direction.
!
        Istr=1+Itile*ChunkSizeI-MarginI
        Iend=Istr+ChunkSizeI-1
        Istr=MAX(Istr,1)
        Iend=MIN(Iend,Lm(ng))
!
!  Tile bounds in the J-direction.
!
        Jstr=1+Jtile*ChunkSizeJ-MarginJ
        Jend=Jstr+ChunkSizeJ-1
        Jstr=MAX(Jstr,1)
        Jend=MIN(Jend,Mm(ng))
      END IF
!
!  Compute C-staggered variables bounds from tile bounds.
!
      CALL var_bounds (ng, Istr, Iend, Jstr, Jend,                      &
     &                 IstrR, IstrT, IstrU, IendR, IendT,               &
     &                 JstrR, JstrT, JstrV, JendR, JendT)

      RETURN
      END SUBROUTINE get_tile

      SUBROUTINE tile_bounds_1d (ng, tile, Imax, Istr, Iend)
!
!=======================================================================
!                                                                      !
!  This routine computes the starting and ending indices for the 1D    !
!  decomposition between all available threads or partitions.          !
!                                                                      !
!                    1 _____________________  Imax                     !
!                                                                      !
!  On Input:                                                           !
!                                                                      !
!     ng         Nested grid number                                    !
!     tile       Thread or partition                                   !
!     Imax       Global number of points                               !
!                                                                      !
!  On Output:                                                          !
!                                                                      !
!     Istr       Starting partition index                              !
!     Iend       Ending   partition index                              !
!                                                                      !
!======================================================================!
!
      USE mod_param
!
      implicit none
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile, Imax
      integer, intent(out) :: Iend, Istr
!
!  Local variable declarations.
!
      integer :: ChunkSize, Margin, Nnodes
!
!-----------------------------------------------------------------------
!  Compute 1D decomposition starting and ending indices.
!-----------------------------------------------------------------------
!
      Nnodes=NtileI(ng)*NtileJ(ng)
      ChunkSize=(Imax+Nnodes-1)/Nnodes
      Margin=(Nnodes*ChunkSize-Imax)/2

      IF (Imax.ge.Nnodes) THEN
        Istr=1+tile*ChunkSize-Margin
        Iend=Istr+ChunkSize-1
        Istr=MAX(Istr,1)
        Iend=MIN(Iend,Imax)
      ELSE
        Istr=1
        Iend=Imax
      END IF

      RETURN
      END SUBROUTINE tile_bounds_1d

      SUBROUTINE tile_bounds_2d (ng, tile, Imax, Jmax, Itile, Jtile,    &
     &                           Istr, Iend, Jstr, Jend)
!
!=======================================================================
!                                                                      !
!  This routine computes the starting and ending horizontal indices    !
!  for each sub-domain partition or tile for a grid bounded between    !
!  (1,1) and (Imax,Jmax):                                              !
!                                                                      !
!                      _________  (Imax,Jmax)                          !
!                     |         |                                      !
!                     |         |                                      !
!                     |_________|                                      !
!                (1,1)                                                 !
!                                                                      !
!  On Input:                                                           !
!                                                                      !
!     ng         Nested grid number                                    !
!     tile       Sub-domain partition                                  !
!     Imax       Global number of points in the I-direction            !
!     Jmax       Global number of points in the J-direction            !
!                                                                      !
!  On Output:                                                          !
!                                                                      !
!     Itile      I-tile coordinate (a value from 0 to NtileI(ng))      !
!     Jtile      J-tile coordinate (a value from 0 to NtileJ(ng))      !
!     Istr       Starting tile index in the I-direction                !
!     Iend       Ending   tile index in the I-direction                !
!     Jstr       Starting tile index in the J-direction                !
!     Jend       Ending   tile index in the J-direction                !
!                                                                      !
!======================================================================!
!
      USE mod_param
!
      implicit none
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile, Imax, Jmax
      integer, intent(out) :: Itile, Jtile
      integer, intent(out) :: Iend, Istr, Jend, Jstr
!
!  Local variable declarations.
!
      integer :: ChunkSizeI, ChunkSizeJ, MarginI, MarginJ
!
!-----------------------------------------------------------------------
!  Compute tile decomposition for a horizontal grid bounded between
!  (1,1) and (Imax,Jmax).
!-----------------------------------------------------------------------
!
      ChunkSizeI=(Imax+NtileI(ng)-1)/NtileI(ng)
      ChunkSizeJ=(Jmax+NtileJ(ng)-1)/NtileJ(ng)
      MarginI=(NtileI(ng)*ChunkSizeI-Imax)/2
      MarginJ=(NtileJ(ng)*ChunkSizeJ-Jmax)/2
      Jtile=tile/NtileI(ng)
      Itile=tile-Jtile*NtileI(ng)
!
!  Tile bounds in the I-direction.
!
      Istr=1+Itile*ChunkSizeI-MarginI
      Iend=Istr+ChunkSizeI-1
      Istr=MAX(Istr,1)
      Iend=MIN(Iend,Imax)
!
!  Tile bounds in the J-direction.
!
      Jstr=1+Jtile*ChunkSizeJ-MarginJ
      Jend=Jstr+ChunkSizeJ-1
      Jstr=MAX(Jstr,1)
      Jend=MIN(Jend,Jmax)

      RETURN
      END SUBROUTINE tile_bounds_2d

      SUBROUTINE var_bounds (ng, Istr, Iend, Jstr, Jend,                &
     &                       IstrR, IstrT, IstrU, IendR, IendT,         &
     &                       JstrR, JstrT, JstrV, JendR, JendT)
!
!=======================================================================
!                                                                      !
!  This routine computes the starting and ending horizontal indices    !
!  for each  C-staggered variable  from the sub-domain partition or    !
!  tile.                                                               !
!                                                                      !
!  On Input:                                                           !
!                                                                      !
!     ng         Nested grid number.                                   !
!     Istr       Starting tile index in the I-direction.               !
!     Iend       Ending   tile index in the I-direction.               !
!     Jstr       Starting tile index in the J-direction.               !
!     Jend       Ending   tile index in the J-direction.               !
!                                                                      !
!  On Output:                                                          !
!                                                                      !
!     IstrR      Starting tile index in the I-direction (RHO-points).  !
!     IstrT      Starting nest tile  in the I-direction (RHO-points).  !
!     IstrU      Starting tile index in the I-direction (U-points).    !
!     IendR      Ending   tile index in the I-direction (RHO_points).  !
!     IendT      Ending   nest tile  in the I-direction (RHO_points).  !
!                                                                      !
!     JstrR      Starting tile index in the J-direction (RHO-points).  !
!     JstrT      Starting nest tile  in the J-direction (RHO-points).  !
!     JstrV      Starting tile index in the J-direction (V-points).    !
!     JendR      Ending   tile index in the J-direction (RHO_points).  !
!     JendT      Ending   nest tile  in the J-direction (RHO-points).  !
!                                                                      !
!======================================================================!
!
      USE mod_param
!
      implicit none
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, Iend, Istr, Jend, Jstr
      integer, intent(out) :: IstrR, IstrT, IstrU, IendR, IendT
      integer, intent(out) :: JstrR, JstrT, JstrV, JendR, JendT
!
!-----------------------------------------------------------------------
!  Compute lower and upper bounds over a particular domain partition or
!  tile for RHO-, U-, and V-variables.
!-----------------------------------------------------------------------
!
!  ROMS uses at staggered stencil:
!
!        -------v(i,j+1,k)-------               ------W(i,j,k)-------
!        |                      |               |                   |
!     u(i,j,k)   r(i,j,k)   u(i+1,j,k)          |     r(i,j,k)      |
!        |                      |               |                   |
!        --------v(i,j,k)--------               -----W(i,j,k-1)------
!
!            horizontal stencil                   vertical stencil
!                 C-grid
!
!
!  M   r..u..r..u..r..u..r..u..r..u..r..u..r..u..r..u..r..u..r..u..r
!      :                                                           :
!   M  v  p++v++p++v++p++v++p++v++p++v++p++v++p++v++p++v++p++v++p  v
!      :  +     |     |     |     |     |     |     |     |     +  :
!  Mm  r  u  r  u  r  u  r  u  r  u  r  u  r  u  r  u  r  u  r  u  r
!      :  +     |     |     |     |     |     |     |     |     +  :
!   Mm v  p--v--p--v--p--v--p--v--p--v--p--v--p--v--p--v--p--v--p  v
!      :  +     |     |     |     |     |     |     |     |     +  :
!      r  u  r  u  r  u  r  u  r  u  r  u  r  u  r  u  r  u  r  u  r
!      :  +     |     |     |     |     |     |     |     |     +  :
!      v  p--v--p--v--p--v--p--v--p--v--p--v--p--v--p--v--p--v--p  v
!      :  +     |     |     |     |     |     |     |     |     +  :
!      r  u  r  u  r  u  r  u  r  u  r  u  r  u  r  u  r  u  r  u  r
!      :  +     |     |     |     |     |     |     |     |     +  :
!      v  p--v--p--v--p--v--p--v--p--v--p--v--p--v--p--v--p--v--p  v
!      :  +     |     |     |     |     |     |     |     |     +  :
!  2   r  u  r  u  r  u  r  u  r  u  r  u  r  u  r  u  r  u  r  u  r
!      :  +     |     |     |     |     |     |     |     |     +  :
!   2  v  p--v--p--v--p--v--p--v--p--v--p--v--p--v--p--v--p--v--p  v
!      :  +     |     |     |     |     |     |     |     |     +  :
!  1   r  u  r  u  r  u  r  u  r  u  r  u  r  u  r  u  r  u  r  u  r
!      :  +     |     |     |     |     |     |     |     |     +  :
!   1  v  p++v++p++v++p++v++p++v++p++v++p++v++p++v++p++v++p++v++p  v
!      :                                                           :
!  0   r..u..r..u..r..u..r..u..r..u..r..u..r..u..r..u..r..u..r..u..r
!         1     2                                        Lm     L
!      0     1     2                                         Lm    L 
!
!                           interior       Boundary Conditions
!                         computations     W     E     S     N
!
!    RH0-type variables:  [1:Lm, 1:Mm]   [0,:] [L,:] [:,0] [:,M] 
!    PSI-type variables:  [2:Lm, 2:Mm]   [1,:] [L,:] [:,1] [:,M]
!      U-type variables:  [2:Lm, 1:Mm]   [1,:] [L,:] [:,0] [:,M]
!      V-type variables:  [1:Lm, 2:Mm]   [0,:] [L,:] [:,1] [:,M]
!
!  Compute derived bounds for the loop indices over a subdomain tile.
!  The extended bounds (labelled by suffix R) are designed to cover
!  also the outer grid points (outlined above with :), if the subdomain
!  tile is adjacent to the physical boundary (outlined above with +).
!  Notice that IstrR, IendR, JstrR, JendR tile bounds computed here
!  DO NOT COVER ghost points (outlined below with *) associated with
!  periodic boundaries (if any) or the computational margins of MPI
!  subdomains.
!
!           Left/Top Tile                        Right/Top Tile
!
! JendR r..u..r..u..r..u..r..u  *  *      *  *  u..r..u..r..u..r..u..r
!       : Istr             Iend                Istr             Iend :
!       v  p++v++p++v++p++v++p  *  * Jend *  *  p++v++p++v++p++v++p  v
!       :  +     |     |     |                  |     |     |     +  :
!       r  u  r  u  r  u  r  u  *  *      *  *  u  r  u  r  u  r  u  r
!       :  +     |     |     |                  |     |     |     +  :
!       v  p--v--p--v--p--v--p  *  *      *  *  p--v--p--v--p--v--p  v
!       :  +     |     |     |                  |     |     |     +  :
!       r  u  r  u  r  u  r  u  *  *      *  *  u  r  u  r  u  r  u  r
!       :  +     |     |     |                  |     |     |     +  :
!       v  p--v--p--v--p--v--p  *  * Jstr *  *  p--v--p--v--p--v--p  v
!
!       *  *  *  *  *  *  *  *  *  *      *  *  *  *  *  *  *  *  *  *
!
!       *  *  *  *  *  *  *  *  *  *      *  *  *  *  *  *  *  *  *  *
!
!     IstrR    IstrU                                               IendR
!
!
!
!                     *  *  *  *  *  *  *  *  *  *  *
!                               Ghost Points
!                     *  *  *  *  *  *  *  *  *  *  *
!
!                     *  *  p--v--p--v--p--v--p  *  *   Jend   IstrR=Istr
!                           |     |     |     |                IstrT=Istr
!     Interior        *  *  u  r  u  r  u  r  u  *  *          IstrU=Istr
!     Tile                  |     |     |     |                IendR=Iend
!                     *  *  p--v--p--v--p--v--p  *  *          IendT=Iend
!                           |     |     |     |                JstrR=Jstr
!                     *  *  u  r  u  r  u  r  u  *  *          JstrT=Jstr
!                           |     |     |     |                JstrV=Jstr
!                     *  *  p--v--p--v--p--v--p  *  *   Jstr   JendR=Jend
!                                                              JendT=Jend
!                     *  *  *  *  *  *  *  *  *  *  *
!
!                     *  *  *  *  *  *  *  *  *  *  *
!
!                          Istr              Iend
!
!
!
!       *  *  *  *  *  *  *  *  *  *      *  *  *  *  *  *  *  *  *  *
!
!       *  *  *  *  *  *  *  *  *  *      *  *  *  *  *  *  *  *  *  *
!         Istr             Iend
!       v  p--v--p--v--p--v--p  *  * Jend *  *  p--v--p--v--p--v--p  v
!       :  +     |     |     |                  |     |     |     +  :
!       r  u  r  u  r  u  r  u  *  *      *  *  u  r  u  r  u  r  u  r
!       :  +     |     |     |                  |     |     |     +  :
! JstrV v  p--v--p--v--p--v--p  *  *      *  *  p--v--p--v--p--v--p  v
!       :  +     |     |     |                  |     |     |     +  :
!       r  u  r  u  r  u  r  u  *  *      *  *  u  r  u  r  u  r  u  r
!       :  +     |     |     |                  |     |     |     +  :
!       v  p++v++p++v++p++v++p  *  * Jstr *  *  p++v++p++v++p++v++p  v
!       :                                                            :
!       r..u..r..u..r..u..r..u  *  *      *  *  u..r..u..r..u..r..u..r
!
!     IstrR    IstrU                                               IendR
!
!           Left/Bottom Tile                    Right/Bottom Tile
!
!
!  It also computes loop-bounds for U- and V-type variables which
!  belong to the interior of the computational domain. These are
!  labelled by suffixes U,V and they step one grid point inward from
!  the side of the subdomain adjacent to the physical boundary.
!  Conversely, for an internal subdomain which does not include a
!  segments of the physical boundary, all bounds with suffixes R,U,V
!  are set to the same values of corresponding non-suffixed bounds.
! 
!  Notice that the indices IstrT, IendT, JstrT, JendT are used during
!  nesting and include the overlap contact points between refined,
!  mosaic, or composed grids.  If not nesting, they are set to the
!  same values as IstrR, IendR, JstrR, JendR, respectively.
!
      IF (WESTERN_EDGE) THEN      ! Western Boundary Tile
#if defined EW_PERIODIC
        IstrR=Istr
        IstrT=IstrR
        IstrU=Istr
#elif defined NESTING
        IstrR=Istr
        IstrT=-NghostPoints
        IstrU=Istr
#else
        IstrR=Istr-1
        IstrT=IstrR
        IstrU=Istr+1
#endif
      ELSE                        ! Interior Tile
        IstrR=Istr
        IstrT=IstrR
        IstrU=Istr
      END IF
      IF (EASTERN_EDGE) THEN      ! Eastern Boundary Tile
#if defined EW_PERIODIC
        IendR=Iend
        IendT=IendR
#elif defined NESTING
        IendR=Iend
        IendT=Iend+NghostPoints
#else
        IendR=Iend+1
        IendT=IendR
#endif
      ELSE                        ! Interior Tile
        IendR=Iend
        IendT=IendR
      END IF
      IF (SOUTHERN_EDGE) THEN     ! Southern Boundary Tile
#if defined NS_PERIODIC
        JstrR=Jstr
        JstrT=JstrR
        JstrV=Jstr
#elif defined EXTENDED_GRID
        JstrR=Jstr
        JstrT=-NghostPoints
        JstrV=Jstr
#else
        JstrR=Jstr-1
        JstrT=JstrR
        JstrV=Jstr+1
#endif
      ELSE                        ! Interior Tile
        JstrR=Jstr
        JstrT=JstrR
        JstrV=Jstr
      END IF
      IF (NORTHERN_EDGE) THEN     ! Northern Boundary Tile
#if defined NS_PERIODIC
        JendR=Jend
        JendT=JendR
#elif defined EXTENDED_GRID
        JendR=Jend
        JendT=Jend+NghostPoints
#else
        JendR=Jend+1
        JendT=JendR
#endif
      ELSE                        ! Interior Tile
        JendR=Jend
        JendT=JendR
      END IF

      RETURN
      END SUBROUTINE var_bounds
