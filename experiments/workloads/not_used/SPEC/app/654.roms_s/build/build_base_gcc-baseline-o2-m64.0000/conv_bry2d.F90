#include "cppdefs.h"
      MODULE conv_bry2d_mod

#if defined NONLINEAR && defined FOUR_DVAR && defined ADJUST_BOUNDARY
!
!svn $Id: conv_bry2d.F 314 2009-02-20 22:06:49Z arango $
!================================================== Hernan G. Arango ===
!  Copyright (c) 2002-2009 The ROMS/TOMS Group       Andrew M. Moore   !
!    Licensed under a MIT/X style license                              !
!    See License_ROMS.txt                                              !
!=======================================================================
!                                                                      !
!  These routines applies the background error covariance to data      !
!  assimilation fields via the space convolution of the diffusion      !
!  equation (filter) for 3D state variables. The diffusion filter      !
!  is solved using an explicit (inefficient) algorithm.                !
!                                                                      !
!  For Gaussian (bell-shaped) correlations, the space convolution      !
!  of the diffusion operator is an efficient way  to estimate the      !
!  finite domain error covariances.                                    !
!                                                                      !
!  On Input:                                                           !
!                                                                      !
!     ng         Nested grid number                                    !
!     tile       Tile partition                                        !
!     model      Calling model identifier                              !
!     boundary   Boundary edge to convolve                             !
!     edge       Boundary edges index                                  !
!     LBij       Lower bound MIN(I,J)-dimension                        !
!     LBij       Lower bound MAX(I,J)-dimension                        !
!     LBi        I-dimension Lower bound                               !
!     UBi        I-dimension Upper bound                               !
!     LBj        J-dimension Lower bound                               !
!     UBj        J-dimension Upper bound                               !
!     Nghost     Number of ghost points                                !
!     NHsteps    Number of horizontal diffusion integration steps      !
!     DTsizeH    Horizontal diffusion pseudo time-step size            !
!     Kh         Horizontal diffusion coefficients                     !
!     A          2D boundary state variable to diffuse                 !
!                                                                      !
!  On Output:                                                          !
!                                                                      !
!     A          Convolved 2D boundary state variable                  !
!                                                                      !
!  Routines:                                                           !
!                                                                      !
!    conv_r2d_bry_tile  Nonlinear 2D boundary convolution at RHO-points!
!    conv_u2d_bry_tile  Nonlinear 2D boundary convolution at U-points  !
!    conv_v2d_bry_tile  Nonlinear 2D boundary convolution at V-points  !
!                                                                      !
!=======================================================================
!
      implicit none

      PUBLIC

      CONTAINS
!
!***********************************************************************
      SUBROUTINE conv_r2d_bry_tile (ng, tile, model, boundary,          &
     &                              edge, LBij, UBij,                   &
     &                              LBi, UBi, LBj, UBj,                 &
     &                              IminS, ImaxS, JminS, JmaxS,         &
     &                              Nghost, NHsteps, DTsizeH,           &
     &                              Kh,                                 &
     &                              pm, pn, pmon_u, pnom_v,             &
# ifdef MASKING
     &                              rmask, umask, vmask,                &
# endif
     &                              A)
!***********************************************************************
!
      USE mod_param
      USE mod_scalars
!
      USE bc_bry2d_mod, ONLY: bc_r2d_bry_tile
# ifdef DISTRIBUTE
      USE mp_exchange_mod, ONLY : mp_exchange2d_bry
# endif
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile, model, boundary
      integer, intent(in) :: edge(4)
      integer, intent(in) :: LBij, UBij
      integer, intent(in) :: LBi, UBi, LBj, UBj
      integer, intent(in) :: IminS, ImaxS, JminS, JmaxS
      integer, intent(in) :: Nghost
      integer, intent(in) :: NHsteps

      real(r8), intent(in) :: DTsizeH
!
# ifdef ASSUMED_SHAPE
      real(r8), intent(in) :: pm(LBi:,LBj:)
      real(r8), intent(in) :: pn(LBi:,LBj:)
      real(r8), intent(in) :: pmon_u(LBi:,LBj:)
      real(r8), intent(in) :: pnom_v(LBi:,LBj:)
#  ifdef MASKING
      real(r8), intent(in) :: rmask(LBi:,LBj:)
      real(r8), intent(in) :: umask(LBi:,LBj:)
      real(r8), intent(in) :: vmask(LBi:,LBj:)
#  endif
      real(r8), intent(in) :: Kh(LBi:,LBj:)
      real(r8), intent(inout) :: A(LBij:)
# else
      real(r8), intent(in) :: pm(LBi:UBi,LBj:UBj)
      real(r8), intent(in) :: pn(LBi:UBi,LBj:UBj)
      real(r8), intent(in) :: pmon_u(LBi:UBi,LBj:UBj)
      real(r8), intent(in) :: pnom_v(LBi:UBi,LBj:UBj)
#  ifdef MASKING
      real(r8), intent(in) :: rmask(LBi:UBi,LBj:UBj)
      real(r8), intent(in) :: umask(LBi:UBi,LBj:UBj)
      real(r8), intent(in) :: vmask(LBi:UBi,LBj:UBj)
#  endif
      real(r8), intent(in) :: Kh(LBi:UBi,LBj:UBj)
      real(r8), intent(inout) :: A(LBij:UBij)
# endif
!
!  Local variable declarations.
!
# ifdef DISTRIBUTE
#  ifdef EW_PERIODIC
      logical :: EWperiodic=.TRUE.
#  else
      logical :: EWperiodic=.FALSE.
#  endif
#  ifdef NS_PERIODIC
      logical :: NSperiodic=.TRUE.
#  else
      logical :: NSperiodic=.FALSE.
#  endif
# endif
      logical, dimension(4) :: Lconvolve

      integer :: Nnew, Nold, Nsav, i, j, step

      real(r8), dimension(LBij:UBij,2) :: Awrk

      real(r8), dimension(JminS:JmaxS) :: FE
      real(r8), dimension(IminS:ImaxS) :: FX
      real(r8), dimension(LBij:UBij) :: Hfac

# include "set_bounds.h"
!
!-----------------------------------------------------------------------
!  Space convolution of the diffusion equation for a 1D state variable
!  at RHO-points.
!-----------------------------------------------------------------------
!
      Lconvolve(iwest )=WESTERN_EDGE
      Lconvolve(ieast )=EASTERN_EDGE
      Lconvolve(isouth)=SOUTHERN_EDGE
      Lconvolve(inorth)=NORTHERN_EDGE
!
!  Compute metrics factor.
!
      IF (Lconvolve(boundary)) THEN
        IF ((boundary.eq.iwest).or.(boundary.eq.ieast)) THEN
          i=edge(boundary)
          DO j=Jstr,Jend
            Hfac(j)=DTsizeH*pm(i,j)*pn(i,j)
          END DO
        ELSE IF ((boundary.eq.isouth).or.(boundary.eq.inorth)) THEN
          j=edge(boundary)
          DO i=Istr,Iend
            Hfac(i)=DTsizeH*pm(i,j)*pn(i,j)
          END DO
        END IF
      END IF
!
!  Set integration indices and initial conditions.
!
      Nold=1
      Nnew=2

      CALL bc_r2d_bry_tile (ng, tile, boundary,                         &
     &                      LBij, UBij,                                 &
     &                      A)
# ifdef DISTRIBUTE
      CALL mp_exchange2d_bry (ng, tile, model, 1, boundary,             &
     &                        LBij, UBij,                               &
     &                        Nghost, EWperiodic, NSperiodic,           &
     &                        A)
# endif
      IF (Lconvolve(boundary)) THEN
        IF ((boundary.eq.iwest).or.(boundary.eq.ieast)) THEN
          DO j=Jstr-1,Jend+1
            Awrk(j,Nold)=A(j)
          END DO
        ELSE IF ((boundary.eq.isouth).or.(boundary.eq.inorth)) THEN
          DO i=Istr-1,Iend+1
            Awrk(i,Nold)=A(i)
          END DO
        END IF
      END IF
!
!-----------------------------------------------------------------------
!  Integrate horizontal diffusion terms.
!-----------------------------------------------------------------------
!
      DO step=1,NHsteps
!
!  Compute XI- and ETA-components of diffusive flux.
!
        IF (Lconvolve(boundary)) THEN
          IF ((boundary.eq.iwest).or.(boundary.eq.ieast)) THEN
            i=edge(boundary)
            DO j=Jstr,Jend+1
              FE(j)=pnom_v(i,j)*0.5_r8*(Kh(i,j-1)+Kh(i,j))*             &
     &              (Awrk(j  ,Nold)-                                    &
     &               Awrk(j-1,Nold))
# ifdef MASKING
              FE(j)=FE(j)*vmask(i,j)
# endif
            END DO
          ELSE IF ((boundary.eq.isouth).or.(boundary.eq.inorth)) THEN
            j=edge(boundary)
            DO i=Istr,Iend+1
              FX(i)=pmon_u(i,j)*0.5_r8*(Kh(i-1,j)+Kh(i,j))*             &
     &              (Awrk(i  ,Nold)-                                    &
     &               Awrk(i-1,Nold))
# ifdef MASKING
              FX(i)=FX(i)*umask(i,j)
# endif
            END DO
          END IF
        END IF
!
!  Time-step horizontal diffusion terms.
!
        IF (Lconvolve(boundary)) THEN
          IF ((boundary.eq.iwest).or.(boundary.eq.ieast)) THEN
            DO j=Jstr,Jend
              Awrk(j,Nnew)=Awrk(j,Nold)+                                &
     &                     Hfac(j)*                                     &
     &                     (FE(j+1)-FE(j))
            END DO
          ELSE IF ((boundary.eq.isouth).or.(boundary.eq.inorth)) THEN
            DO i=Istr,Iend
              Awrk(i,Nnew)=Awrk(i,Nold)+                                &
     &                     Hfac(i)*                                     &
     &                     (FX(i+1)-FX(i))
            END DO
          END IF
        END IF
!
!  Apply boundary conditions. If applicable, exchange boundary data.
!
        CALL bc_r2d_bry_tile (ng, tile, boundary,                       &
     &                        LBij, UBij,                               &
     &                        Awrk(:,Nnew))
# ifdef DISTRIBUTE
        CALL mp_exchange2d_bry (ng, tile, model, 1, boundary,           &
     &                          LBij, UBij,                             &
     &                          Nghost, EWperiodic, NSperiodic,         &
     &                          Awrk(:,Nnew))
# endif
!
!  Update integration indices.
!
        Nsav=Nold
        Nold=Nnew
        Nnew=Nsav
      END DO
!
!-----------------------------------------------------------------------
!  Load convolved solution.
!-----------------------------------------------------------------------
!
      IF (Lconvolve(boundary)) THEN
        IF ((boundary.eq.iwest).or.(boundary.eq.ieast)) THEN
          DO j=Jstr,Jend
            A(j)=Awrk(j,Nold)
          END DO
        ELSE IF ((boundary.eq.isouth).or.(boundary.eq.inorth)) THEN
          DO i=Istr,Iend
            A(i)=Awrk(i,Nold)
          END DO
        END IF
      END IF
      CALL bc_r2d_bry_tile (ng, tile, boundary,                         &
     &                      LBij, UBij,                                 &
     &                      A)
# ifdef DISTRIBUTE
      CALL mp_exchange2d_bry (ng, tile, model, 1, boundary,             &
     &                        LBij, UBij,                               &
     &                        Nghost, EWperiodic, NSperiodic,           &
     &                        A)
# endif

      RETURN
      END SUBROUTINE conv_r2d_bry_tile

!
!***********************************************************************
      SUBROUTINE conv_u2d_bry_tile (ng, tile, model, boundary,          &
     &                              edge, LBij, UBij,                   &
     &                              LBi, UBi, LBj, UBj,                 &
     &                              IminS, ImaxS, JminS, JmaxS,         &
     &                              Nghost, NHsteps, DTsizeH,           &
     &                              Kh,                                 &
     &                              pm, pn, pmon_r, pnom_p,             &
# ifdef MASKING
     &                              umask, pmask,                       &
# endif
     &                              A)
!***********************************************************************
!
      USE mod_param
      USE mod_scalars
!
      USE bc_bry2d_mod, ONLY: bc_u2d_bry_tile
# ifdef DISTRIBUTE
      USE mp_exchange_mod, ONLY : mp_exchange2d_bry
# endif
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile, model, boundary
      integer, intent(in) :: edge(4)
      integer, intent(in) :: LBij, UBij
      integer, intent(in) :: LBi, UBi, LBj, UBj
      integer, intent(in) :: IminS, ImaxS, JminS, JmaxS
      integer, intent(in) :: Nghost
      integer, intent(in) :: NHsteps

      real(r8), intent(in) :: DTsizeH
!
# ifdef ASSUMED_SHAPE
      real(r8), intent(in) :: pm(LBi:,LBj:)
      real(r8), intent(in) :: pn(LBi:,LBj:)
      real(r8), intent(in) :: pmon_r(LBi:,LBj:)
      real(r8), intent(in) :: pnom_p(LBi:,LBj:)
#  ifdef MASKING
      real(r8), intent(in) :: umask(LBi:,LBj:)
      real(r8), intent(in) :: pmask(LBi:,LBj:)
#  endif
      real(r8), intent(in) :: Kh(LBi:,LBj:)
      real(r8), intent(inout) :: A(LBij:)
# else
      real(r8), intent(in) :: pm(LBi:UBi,LBj:UBj)
      real(r8), intent(in) :: pn(LBi:UBi,LBj:UBj)
      real(r8), intent(in) :: pmon_r(LBi:UBi,LBj:UBj)
      real(r8), intent(in) :: pnom_p(LBi:UBi,LBj:UBj)
#  ifdef MASKING
      real(r8), intent(in) :: umask(LBi:UBi,LBj:UBj)
      real(r8), intent(in) :: pmask(LBi:UBi,LBj:UBj)
#  endif
      real(r8), intent(in) :: Kh(LBi:UBi,LBj:UBj)
      real(r8), intent(inout) :: A(LBij:UBij)
# endif
!
!  Local variable declarations.
!
# ifdef DISTRIBUTE
#  ifdef EW_PERIODIC
      logical :: EWperiodic=.TRUE.
#  else
      logical :: EWperiodic=.FALSE.
#  endif
#  ifdef NS_PERIODIC
      logical :: NSperiodic=.TRUE.
#  else
      logical :: NSperiodic=.FALSE.
#  endif
# endif
      logical, dimension(4) :: Lconvolve

      integer :: Nnew, Nold, Nsav, i, j, step

      real(r8) :: cff

      real(r8), dimension(LBij:UBij,2) :: Awrk

      real(r8), dimension(JminS:JmaxS) :: FE
      real(r8), dimension(IminS:ImaxS) :: FX
      real(r8), dimension(LBij:UBij) :: Hfac

# include "set_bounds.h"
!
!-----------------------------------------------------------------------
!  Space convolution of the diffusion equation for a 1D state variable
!  at U-points.
!-----------------------------------------------------------------------
!
      Lconvolve(iwest )=WESTERN_EDGE
      Lconvolve(ieast )=EASTERN_EDGE
      Lconvolve(isouth)=SOUTHERN_EDGE
      Lconvolve(inorth)=NORTHERN_EDGE
!
!  Compute metrics factor.
!
      cff=DTsizeH*0.25_r8
      IF (Lconvolve(boundary)) THEN
        IF ((boundary.eq.iwest).or.(boundary.eq.ieast)) THEN
          i=edge(boundary)
          DO j=Jstr,Jend
            Hfac(j)=cff*(pm(i-1,j)+pm(i,j))*(pn(i-1,j)+pn(i,j))
          END DO
        ELSE IF ((boundary.eq.isouth).or.(boundary.eq.inorth)) THEN
          j=edge(boundary)
          DO i=IstrU,Iend
            Hfac(i)=cff*(pm(i-1,j)+pm(i,j))*(pn(i-1,j)+pn(i,j))
          END DO
        END IF
      END IF
!
!  Set integration indices and initial conditions.
!
      Nold=1
      Nnew=2

      CALL bc_u2d_bry_tile (ng, tile, boundary,                         &
     &                      LBij, UBij,                                 &
     &                      A)
# ifdef DISTRIBUTE
      CALL mp_exchange2d_bry (ng, tile, model, 1, boundary,             &
     &                        LBij, UBij,                               &
     &                        Nghost, EWperiodic, NSperiodic,           &
     &                        A)
# endif
      IF (Lconvolve(boundary)) THEN
        IF ((boundary.eq.iwest).or.(boundary.eq.ieast)) THEN
          DO j=Jstr-1,Jend+1
            Awrk(j,Nold)=A(j)
          END DO
        ELSE IF ((boundary.eq.isouth).or.(boundary.eq.inorth)) THEN
          DO i=IstrU-1,Iend+1
            Awrk(i,Nold)=A(i)
          END DO
        END IF
      END IF
!
!-----------------------------------------------------------------------
!  Integrate horizontal diffusion terms.
!-----------------------------------------------------------------------
!
      DO step=1,NHsteps
!
!  Compute XI- and ETA-components of diffusive flux.
!
        IF (Lconvolve(boundary)) THEN
          IF ((boundary.eq.iwest).or.(boundary.eq.ieast)) THEN
            i=edge(boundary)
            DO j=Jstr,Jend+1
              FE(j)=pnom_p(i,j)*0.25_r8*(Kh(i-1,j  )+Kh(i,j  )+         &
     &                                   Kh(i-1,j-1)+Kh(i,j-1))*        &
     &              (Awrk(j  ,Nold)-                                    &
     &               Awrk(j-1,Nold))
# ifdef MASKING
              FE(j)=FE(j)*pmask(i,j)
# endif
            END DO
          ELSE IF ((boundary.eq.isouth).or.(boundary.eq.inorth)) THEN
            j=edge(boundary)
            DO i=IstrU-1,Iend
              FX(i)=pmon_r(i,j)*Kh(i,j)*                                &
     &              (Awrk(i+1,Nold)-                                    &
     &               Awrk(i  ,Nold))
            END DO
          END IF
        END IF
!
!  Time-step horizontal diffusion terms.
!
        IF (Lconvolve(boundary)) THEN
          IF ((boundary.eq.iwest).or.(boundary.eq.ieast)) THEN
            DO j=Jstr,Jend
              Awrk(j,Nnew)=Awrk(j,Nold)+                                &
     &                     Hfac(j)*                                     &
     &                     (FE(j+1)-FE(j))
            END DO
          ELSE IF ((boundary.eq.isouth).or.(boundary.eq.inorth)) THEN
            DO i=IstrU,Iend
              Awrk(i,Nnew)=Awrk(i,Nold)+                                &
     &                     Hfac(i)*                                     &
     &                     (FX(i)-FX(i-1))
            END DO
          END IF
        END IF
!
!  Apply boundary conditions. If applicable, exchange boundary data.
!
        CALL bc_u2d_bry_tile (ng, tile, boundary,                       &
     &                        LBij, UBij,                               &
     &                        Awrk(:,Nnew))
# ifdef DISTRIBUTE
        CALL mp_exchange2d_bry (ng, tile, model, 1, boundary,           &
     &                          LBij, UBij,                             &
     &                          Nghost, EWperiodic, NSperiodic,         &
     &                          Awrk(:,Nnew))
# endif
!
!  Update integration indices.
!
        Nsav=Nold
        Nold=Nnew
        Nnew=Nsav
      END DO
!
!-----------------------------------------------------------------------
!  Load convolved solution.
!-----------------------------------------------------------------------
!
      IF (Lconvolve(boundary)) THEN
        IF ((boundary.eq.iwest).or.(boundary.eq.ieast)) THEN
          DO j=Jstr,Jend
            A(j)=Awrk(j,Nold)
          END DO
        ELSE IF ((boundary.eq.isouth).or.(boundary.eq.inorth)) THEN
          DO i=IstrU,Iend
            A(i)=Awrk(i,Nold)
          END DO
        END IF
      END IF
      CALL bc_u2d_bry_tile (ng, tile, boundary,                         &
     &                      LBij, UBij,                                 &
     &                      A)
# ifdef DISTRIBUTE
      CALL mp_exchange2d_bry (ng, tile, model, 1, boundary,             &
     &                        LBij, UBij,                               &
     &                        Nghost, EWperiodic, NSperiodic,           &
     &                        A)
# endif

      RETURN
      END SUBROUTINE conv_u2d_bry_tile

!
!***********************************************************************
      SUBROUTINE conv_v2d_bry_tile (ng, tile, model, boundary,          &
     &                              edge, LBij, UBij,                   &
     &                              LBi, UBi, LBj, UBj,                 &
     &                              IminS, ImaxS, JminS, JmaxS,         &
     &                              Nghost, NHsteps, DTsizeH,           &
     &                              Kh,                                 &
     &                              pm, pn, pmon_p, pnom_r,             &
# ifdef MASKING
     &                              vmask, pmask,                       &
# endif
     &                              A)
!***********************************************************************
!
      USE mod_param
      USE mod_scalars
!
      USE bc_bry2d_mod, ONLY: bc_v2d_bry_tile
# ifdef DISTRIBUTE
      USE mp_exchange_mod, ONLY : mp_exchange2d_bry
# endif
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile, model, boundary
      integer, intent(in) :: edge(4)
      integer, intent(in) :: LBij, UBij
      integer, intent(in) :: LBi, UBi, LBj, UBj
      integer, intent(in) :: IminS, ImaxS, JminS, JmaxS
      integer, intent(in) :: Nghost, NHsteps

      real(r8), intent(in) :: DTsizeH
!
# ifdef ASSUMED_SHAPE
      real(r8), intent(in) :: pm(LBi:,LBj:)
      real(r8), intent(in) :: pn(LBi:,LBj:)
      real(r8), intent(in) :: pmon_p(LBi:,LBj:)
      real(r8), intent(in) :: pnom_r(LBi:,LBj:)
#  ifdef MASKING
      real(r8), intent(in) :: vmask(LBi:,LBj:)
      real(r8), intent(in) :: pmask(LBi:,LBj:)
#  endif
      real(r8), intent(in) :: Kh(LBi:,LBj:)
      real(r8), intent(inout) :: A(LBij:)
# else
      real(r8), intent(in) :: pm(LBi:UBi,LBj:UBj)
      real(r8), intent(in) :: pn(LBi:UBi,LBj:UBj)
      real(r8), intent(in) :: pmon_p(LBi:UBi,LBj:UBj)
      real(r8), intent(in) :: pnom_r(LBi:UBi,LBj:UBj)
#  ifdef MASKING
      real(r8), intent(in)  :: vmask(LBi:UBi,LBj:UBj)
      real(r8), intent(in)  :: pmask(LBi:UBi,LBj:UBj)
#  endif
      real(r8), intent(in) :: Kh(LBi:UBi,LBj:UBj)
      real(r8), intent(inout) :: A(LBij:UBij)
# endif
!
!  Local variable declarations.
!
# ifdef DISTRIBUTE
#  ifdef EW_PERIODIC
      logical :: EWperiodic=.TRUE.
#  else
      logical :: EWperiodic=.FALSE.
#  endif
#  ifdef NS_PERIODIC
      logical :: NSperiodic=.TRUE.
#  else
      logical :: NSperiodic=.FALSE.
#  endif
# endif
      logical, dimension(4) :: Lconvolve

      integer :: Nnew, Nold, Nsav, i, j, step

      real(r8) :: cff

      real(r8), dimension(LBij:UBij,2) :: Awrk

      real(r8), dimension(JminS:JmaxS) :: FE
      real(r8), dimension(IminS:ImaxS) :: FX
      real(r8), dimension(LBij:UBij) :: Hfac

# include "set_bounds.h"
!
!-----------------------------------------------------------------------
!  Space convolution of the diffusion equation for a 2D state variable
!  at RHO-points.
!-----------------------------------------------------------------------
!
      Lconvolve(iwest )=WESTERN_EDGE
      Lconvolve(ieast )=EASTERN_EDGE
      Lconvolve(isouth)=SOUTHERN_EDGE
      Lconvolve(inorth)=NORTHERN_EDGE
!
!  Compute metrics factor.
!
      cff=DTsizeH*0.25_r8
      IF (Lconvolve(boundary)) THEN
        IF ((boundary.eq.iwest).or.(boundary.eq.ieast)) THEN
          i=edge(boundary)
          DO j=JstrV,Jend
            Hfac(j)=cff*(pm(i,j-1)+pm(i,j))*(pn(i,j-1)+pn(i,j))
          END DO
        ELSE IF ((boundary.eq.isouth).or.(boundary.eq.inorth)) THEN
          j=edge(boundary)
          DO i=Istr,Iend
            Hfac(i)=cff*(pm(i,j-1)+pm(i,j))*(pn(i,j-1)+pn(i,j))
          END DO
        END IF
      END IF
!
!  Set integration indices and initial conditions.
!
      Nold=1
      Nnew=2

      CALL bc_v2d_bry_tile (ng, tile, boundary,                         &
     &                      LBij, UBij,                                 &
     &                      A)
# ifdef DISTRIBUTE
      CALL mp_exchange2d_bry (ng, tile, model, 1, boundary,             &
     &                        LBij, UBij,                               &
     &                        Nghost, EWperiodic, NSperiodic,           &
     &                        A)
# endif
      IF (Lconvolve(boundary)) THEN
        IF ((boundary.eq.iwest).or.(boundary.eq.ieast)) THEN
          DO j=JstrV-1,Jend+1
            Awrk(j,Nold)=A(j)
          END DO
        ELSE IF ((boundary.eq.isouth).or.(boundary.eq.inorth)) THEN
          DO i=Istr-1,Iend+1
            Awrk(i,Nold)=A(i)
          END DO
        END IF
      END IF
!
!-----------------------------------------------------------------------
!  Integrate horizontal diffusion terms.
!-----------------------------------------------------------------------
!
      DO step=1,NHsteps
!
!  Compute XI- and ETA-components of diffusive flux.
!
        IF (Lconvolve(boundary)) THEN
          IF ((boundary.eq.iwest).or.(boundary.eq.ieast)) THEN
            i=edge(boundary)
            DO j=JstrV-1,Jend
              FE(j)=pnom_r(i,j)*Kh(i,j)*                                &
     &              (Awrk(j+1,Nold)-                                    &
     &               Awrk(j  ,Nold))
            END DO
          ELSE IF ((boundary.eq.isouth).or.(boundary.eq.inorth)) THEN
            j=edge(boundary)
            DO i=Istr,Iend+1
              FX(i)=pmon_p(i,j)*0.25_r8*(Kh(i-1,j  )+Kh(i,j  )+         &
     &                                   Kh(i-1,j-1)+Kh(i,j-1))*        &
     &              (Awrk(i  ,Nold)-                                    &
     &               Awrk(i-1,Nold))
# ifdef MASKING
              FX(i)=FX(i)*pmask(i,j)
# endif
            END DO
          END IF
        END IF
!
!  Time-step horizontal diffusion terms.
!
        IF (Lconvolve(boundary)) THEN
          IF ((boundary.eq.iwest).or.(boundary.eq.ieast)) THEN
            DO j=JstrV,Jend
              Awrk(j,Nnew)=Awrk(j,Nold)+                                &
     &                     Hfac(j)*                                     &
     &                     (FE(j)-FE(j-1))
            END DO
          ELSE IF ((boundary.eq.isouth).or.(boundary.eq.inorth)) THEN
            DO i=Istr,Iend
              Awrk(i,Nnew)=Awrk(i,Nold)+                                &
     &                     Hfac(i)*                                     &
     &                     (FX(i+1)-FX(i))
            END DO
          END IF
        END IF
!
!  Apply boundary conditions. If applicable, exchange boundary data.
!
        CALL bc_v2d_bry_tile (ng, tile, boundary,                       &
     &                        LBij, UBij,                               &
     &                        Awrk(:,Nnew))
# ifdef DISTRIBUTE
        CALL mp_exchange2d_bry (ng, tile, model, 1, boundary,           &
     &                          LBij, UBij,                             &
     &                          Nghost, EWperiodic, NSperiodic,         &
     &                          Awrk(:,Nnew))
# endif
!
!  Update integration indices.
!
        Nsav=Nold
        Nold=Nnew
        Nnew=Nsav
      END DO
!
!-----------------------------------------------------------------------
!  Load convolved solution.
!-----------------------------------------------------------------------
!
      IF (Lconvolve(boundary)) THEN
        IF ((boundary.eq.iwest).or.(boundary.eq.ieast)) THEN
          DO j=JstrV,Jend
            A(j)=Awrk(j,Nold)
          END DO
        ELSE IF ((boundary.eq.isouth).or.(boundary.eq.inorth)) THEN
          DO i=Istr,Iend
            A(i)=Awrk(i,Nold)
          END DO
        END IF
      END IF
      CALL bc_v2d_bry_tile (ng, tile, boundary,                         &
     &                      LBij, UBij,                                 &
     &                      A)
# ifdef DISTRIBUTE
      CALL mp_exchange2d_bry (ng, tile, model, 1,  boundary,            &
     &                        LBij, UBij,                               &
     &                        Nghost, EWperiodic, NSperiodic,           &
     &                        A)
# endif

      RETURN
      END SUBROUTINE conv_v2d_bry_tile
#endif
      END MODULE conv_bry2d_mod
