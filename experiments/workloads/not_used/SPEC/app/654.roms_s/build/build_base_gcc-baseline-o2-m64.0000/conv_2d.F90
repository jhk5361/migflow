#include "cppdefs.h"

      MODULE conv_2d_mod

#if defined NONLINEAR && defined FOUR_DVAR
!
!svn $Id: conv_2d.F 294 2009-01-09 21:37:26Z arango $
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
!     ng         Nested grid number.                                   !
!     model      Calling model identifier.                             !
!     Istr       Starting tile index in the I-direction.               !
!     Iend       Ending   tile index in the I-direction.               !
!     Jstr       Starting tile index in the J-direction.               !
!     Jend       Ending   tile index in the J-direction.               !
!     LBi        I-dimension Lower bound.                              !
!     UBi        I-dimension Upper bound.                              !
!     LBj        J-dimension Lower bound.                              !
!     UBj        J-dimension Upper bound.                              !
!     Nghost     Number of ghost points.                               !
!     NHsteps    Number of horizontal diffusion integration steps.     !
!     DTsizeH    Horizontal diffusion pseudo time-step size.           !
!     Kh         Horizontal diffusion coefficients.                    !
!     A          2D state variable to diffuse.                         !
!                                                                      !
!  On Output:                                                          !
!                                                                      !
!     A          Diffused 2D state variable.                           !
!                                                                      !
!  Routines:                                                           !
!                                                                      !
!    conv_r2d_tile     Nonlinear 2D convolution at RHO-points          !
!    conv_u2d_tile     Nonlinear 2D convolution at U-points            !
!    conv_v2d_tile     Nonlinear 2D convolution at V-points            !
!                                                                      !
!=======================================================================
!
      implicit none

      PUBLIC

      CONTAINS
!
!***********************************************************************
      SUBROUTINE conv_r2d_tile (ng, tile, model,                        &
     &                          LBi, UBi, LBj, UBj,                     &
     &                          IminS, ImaxS, JminS, JmaxS,             &
     &                          Nghost, NHsteps, DTsizeH,               &
     &                          Kh,                                     &
     &                          pm, pn, pmon_u, pnom_v,                 &
# ifdef MASKING
     &                          rmask, umask, vmask,                    &
# endif
     &                          A)
!***********************************************************************
!
      USE mod_param
!
      USE bc_2d_mod, ONLY: bc_r2d_tile
# ifdef DISTRIBUTE
      USE mp_exchange_mod, ONLY : mp_exchange2d
# endif
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile, model
      integer, intent(in) :: LBi, UBi, LBj, UBj
      integer, intent(in) :: IminS, ImaxS, JminS, JmaxS
      integer, intent(in) :: Nghost, NHsteps

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
      real(r8), intent(inout) :: A(LBi:,LBj:)
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
      real(r8), intent(inout) :: A(LBi:UBi,LBj:UBj)
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
      integer :: Nnew, Nold, Nsav, i, j, step

      real(r8), dimension(LBi:UBi,LBj:UBj,2) :: Awrk

      real(r8), dimension(IminS:ImaxS,JminS:JmaxS) :: FE
      real(r8), dimension(IminS:ImaxS,JminS:JmaxS) :: FX
      real(r8), dimension(IminS:ImaxS,JminS:JmaxS) :: Hfac
# include "set_bounds.h"
!
!-----------------------------------------------------------------------
!  Space convolution of the diffusion equation for a 2D state variable
!  at RHO-points.
!-----------------------------------------------------------------------
!
!  Compute metrics factor.
!
      DO j=Jstr,Jend
        DO i=Istr,Iend
          Hfac(i,j)=DTsizeH*pm(i,j)*pn(i,j)
        END DO
      END DO
!
!  Set integration indices and initial conditions.
!
      Nold=1
      Nnew=2
# ifdef DISTRIBUTE
      CALL mp_exchange2d (ng, tile, model, 1,                           &
     &                    LBi, UBi, LBj, UBj,                           &
     &                    Nghost, EWperiodic, NSperiodic,               &
     &                    A)
# endif
      DO j=Jstr-1,Jend+1
        DO i=Istr-1,Iend+1
          Awrk(i,j,Nold)=A(i,j)
        END DO
      END DO
!
!-----------------------------------------------------------------------
!  Integrate horizontal diffusion terms.
!-----------------------------------------------------------------------
!
      DO step=1,NHsteps
!
!  Compute XI- and ETA-components of diffusive flux.
!
        DO j=Jstr,Jend
          DO i=Istr,Iend+1
            FX(i,j)=pmon_u(i,j)*0.5_r8*(Kh(i-1,j)+Kh(i,j))*             &
     &              (Awrk(i,j,Nold)-Awrk(i-1,j,Nold))
# ifdef MASKING
            FX(i,j)=FX(i,j)*umask(i,j)
# endif
          END DO
        END DO
        DO j=Jstr,Jend+1
          DO i=Istr,Iend
            FE(i,j)=pnom_v(i,j)*0.5_r8*(Kh(i,j-1)+Kh(i,j))*             &
     &              (Awrk(i,j,Nold)-Awrk(i,j-1,Nold))
# ifdef MASKING
            FE(i,j)=FE(i,j)*vmask(i,j)
# endif
          END DO
        END DO
!
!  Time-step horizontal diffusion terms.
!
        DO j=Jstr,Jend
          DO i=Istr,Iend
            Awrk(i,j,Nnew)=Awrk(i,j,Nold)+                              &
     &                     Hfac(i,j)*                                   &
     &                     (FX(i+1,j)-FX(i,j)+                          &
     &                      FE(i,j+1)-FE(i,j))
          END DO
        END DO
!
!  Apply boundary conditions. If applicable, exchange boundary data.
!
        CALL bc_r2d_tile (ng, tile,                                     &
     &                    LBi, UBi, LBj, UBj,                           &
     &                    Awrk(:,:,Nnew))
# ifdef DISTRIBUTE
        CALL mp_exchange2d (ng, tile, model, 1,                         &
     &                      LBi, UBi, LBj, UBj,                         &
     &                      Nghost, EWperiodic, NSperiodic,             &
     &                      Awrk(:,:,Nnew))
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
      DO j=Jstr,Jend
        DO i=Istr,Iend
          A(i,j)=Awrk(i,j,Nold)
        END DO
      END DO
      CALL bc_r2d_tile (ng, tile,                                       &
     &                  LBi, UBi, LBj, UBj,                             &
     &                  A)
# ifdef DISTRIBUTE
      CALL mp_exchange2d (ng, tile, model, 1,                           &
     &                    LBi, UBi, LBj, UBj,                           &
     &                    Nghost, EWperiodic, NSperiodic,               &
     &                    A)
# endif

      RETURN
      END SUBROUTINE conv_r2d_tile
!
!***********************************************************************
      SUBROUTINE conv_u2d_tile (ng, tile, model,                        &
     &                          LBi, UBi, LBj, UBj,                     &
     &                          IminS, ImaxS, JminS, JmaxS,             &
     &                          Nghost, NHsteps, DTsizeH,               &
     &                          Kh,                                     &
     &                          pm, pn, pmon_r, pnom_p,                 &
# ifdef MASKING
     &                          umask, pmask,                           &
# endif
     &                          A)
!***********************************************************************
!
      USE mod_param
!
      USE bc_2d_mod, ONLY: bc_u2d_tile
# ifdef DISTRIBUTE
      USE mp_exchange_mod, ONLY : mp_exchange2d
# endif
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile, model
      integer, intent(in) :: LBi, UBi, LBj, UBj
      integer, intent(in) :: IminS, ImaxS, JminS, JmaxS
      integer, intent(in) :: Nghost, NHsteps

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
      real(r8), intent(inout) :: A(LBi:,LBj:)
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
      real(r8), intent(inout) :: A(LBi:UBi,LBj:UBj)
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
      integer :: Nnew, Nold, Nsav, i, j, step

      real(r8) :: cff

      real(r8), dimension(LBi:UBi,LBj:UBj,2) :: Awrk

      real(r8), dimension(IminS:ImaxS,JminS:JmaxS) :: FE
      real(r8), dimension(IminS:ImaxS,JminS:JmaxS) :: FX
      real(r8), dimension(IminS:ImaxS,JminS:JmaxS) :: Hfac

# include "set_bounds.h"
!
!-----------------------------------------------------------------------
!  Space convolution of the diffusion equation for a 2D state variable
!  at U-points.
!-----------------------------------------------------------------------
!
!  Compute metrics factor.
!
      cff=DTsizeH*0.25_r8
      DO j=Jstr,Jend
        DO i=IstrU,Iend
          Hfac(i,j)=cff*(pm(i-1,j)+pm(i,j))*(pn(i-1,j)+pn(i,j))
        END DO
      END DO
!
!  Set integration indices and initial conditions.
!
      Nold=1
      Nnew=2
# ifdef DISTRIBUTE
      CALL mp_exchange2d (ng, tile, model, 1,                           &
     &                    LBi, UBi, LBj, UBj,                           &
     &                    Nghost, EWperiodic, NSperiodic,               &
     &                    A)
# endif
      DO j=Jstr-1,Jend+1
        DO i=IstrU-1,Iend+1
          Awrk(i,j,Nold)=A(i,j)
        END DO
      END DO
!
!-----------------------------------------------------------------------
!  Integrate horizontal diffusion terms.
!-----------------------------------------------------------------------
!
      DO step=1,NHsteps
!
!  Compute XI- and ETA-components of diffusive flux.
!
        DO j=Jstr,Jend
          DO i=IstrU-1,Iend
            FX(i,j)=pmon_r(i,j)*Kh(i,j)*                                &
     &              (Awrk(i+1,j,Nold)-Awrk(i,j,Nold))
          END DO
        END DO
        DO j=Jstr,Jend+1
          DO i=IstrU,Iend
            FE(i,j)=pnom_p(i,j)*0.25_r8*(Kh(i-1,j  )+Kh(i,j  )+         &
     &                                   Kh(i-1,j-1)+Kh(i,j-1))*        &
     &              (Awrk(i,j,Nold)-Awrk(i,j-1,Nold))
# ifdef MASKING
            FE(i,j)=FE(i,j)*pmask(i,j)
# endif
          END DO
        END DO
!
!  Time-step horizontal diffusion terms.
!
        DO j=Jstr,Jend
          DO i=IstrU,Iend
            Awrk(i,j,Nnew)=Awrk(i,j,Nold)+                              &
     &                     Hfac(i,j)*                                   &
     &                     (FX(i,j)-FX(i-1,j)+                          &
     &                      FE(i,j+1)-FE(i,j))
          END DO
        END DO
!
!  Apply boundary conditions. If applicable, exchange boundary data.
!
        CALL bc_u2d_tile (ng, tile,                                     &
     &                    LBi, UBi, LBj, UBj,                           &
     &                    Awrk(:,:,Nnew))
# ifdef DISTRIBUTE
        CALL mp_exchange2d (ng, tile, model, 1,                         &
     &                      LBi, UBi, LBj, UBj,                         &
     &                      Nghost, EWperiodic, NSperiodic,             &
     &                      Awrk(:,:,Nnew))
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
      DO j=Jstr,Jend
        DO i=IstrU,Iend
          A(i,j)=Awrk(i,j,Nold)
        END DO
      END DO
      CALL bc_u2d_tile (ng, tile,                                       &
     &                  LBi, UBi, LBj, UBj,                             &
     &                  A)
# ifdef DISTRIBUTE
      CALL mp_exchange2d (ng, tile, model, 1,                           &
     &                    LBi, UBi, LBj, UBj,                           &
     &                    Nghost, EWperiodic, NSperiodic,               &
     &                    A)
# endif

      RETURN
      END SUBROUTINE conv_u2d_tile
!
!***********************************************************************
      SUBROUTINE conv_v2d_tile (ng, tile, model,                        &
     &                          LBi, UBi, LBj, UBj,                     &
     &                          IminS, ImaxS, JminS, JmaxS,             &
     &                          Nghost, NHsteps, DTsizeH,               &
     &                          Kh,                                     &
     &                          pm, pn, pmon_p, pnom_r,                 &
# ifdef MASKING
     &                          vmask, pmask,                           &
# endif
     &                          A)
!***********************************************************************
!
      USE mod_param
!
      USE bc_2d_mod, ONLY: bc_v2d_tile
# ifdef DISTRIBUTE
      USE mp_exchange_mod, ONLY : mp_exchange2d
# endif
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile, model
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
      real(r8), intent(inout) :: A(LBi:,LBj:)
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
      real(r8), intent(inout) :: A(LBi:UBi,LBj:UBj)
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
      integer :: Nnew, Nold, Nsav, i, j, step

      real(r8) :: cff

      real(r8), dimension(LBi:UBi,LBj:UBj,2) :: Awrk

      real(r8), dimension(IminS:ImaxS,JminS:JmaxS) :: FE
      real(r8), dimension(IminS:ImaxS,JminS:JmaxS) :: FX
      real(r8), dimension(IminS:ImaxS,JminS:JmaxS) :: Hfac

# include "set_bounds.h"
!
!-----------------------------------------------------------------------
!  Space convolution of the diffusion equation for a 2D state variable
!  at V-points.
!-----------------------------------------------------------------------
!
!  Compute metrics factor.
!
      cff=DTsizeH*0.25_r8
      DO j=JstrV,Jend
        DO i=Istr,Iend
          Hfac(i,j)=cff*(pm(i,j-1)+pm(i,j))*(pn(i,j-1)+pn(i,j))
        END DO
      END DO
!
!  Set integration indices and initial conditions.
!
      Nold=1
      Nnew=2
# ifdef DISTRIBUTE
      CALL mp_exchange2d (ng, tile, model, 1,                           &
     &                    LBi, UBi, LBj, UBj,                           &
     &                    Nghost, EWperiodic, NSperiodic,               &
     &                    A)
# endif
      DO j=JstrV-1,Jend+1
        DO i=Istr-1,Iend+1
          Awrk(i,j,Nold)=A(i,j)
        END DO
      END DO
!
!-----------------------------------------------------------------------
!  Integrate horizontal diffusion terms.
!-----------------------------------------------------------------------
!
      DO step=1,NHsteps
!
!  Compute XI- and ETA-components of diffusive flux.
!
        DO j=JstrV,Jend
          DO i=Istr,Iend+1
            FX(i,j)=pmon_p(i,j)*0.25_r8*(Kh(i-1,j  )+Kh(i,j  )+         &
     &                                   Kh(i-1,j-1)+Kh(i,j-1))*        &
     &              (Awrk(i,j,Nold)-Awrk(i-1,j,Nold))
# ifdef MASKING
            FX(i,j)=FX(i,j)*pmask(i,j)
# endif
          END DO
        END DO
        DO j=JstrV-1,Jend
          DO i=Istr,Iend
            FE(i,j)=pnom_r(i,j)*Kh(i,j)*                                &
     &              (Awrk(i,j+1,Nold)-Awrk(i,j,Nold))
          END DO
        END DO
!
!  Time-step horizontal diffusion terms.
!
        DO j=JstrV,Jend
          DO i=Istr,Iend
            Awrk(i,j,Nnew)=Awrk(i,j,Nold)+                              &
     &                     Hfac(i,j)*                                   &
     &                     (FX(i+1,j)-FX(i,j)+                          &
     &                      FE(i,j)-FE(i,j-1))
          END DO
        END DO
!
!  Apply boundary conditions. If applicable, exchange boundary data.
!
        CALL bc_v2d_tile (ng, tile,                                     &
     &                    LBi, UBi, LBj, UBj,                           &
     &                    Awrk(:,:,Nnew))
# ifdef DISTRIBUTE
        CALL mp_exchange2d (ng, tile, model, 1,                         &
     &                      LBi, UBi, LBj, UBj,                         &
     &                      Nghost, EWperiodic, NSperiodic,             &
     &                      Awrk(:,:,Nnew))
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
      DO j=JstrV,Jend
        DO i=Istr,Iend
          A(i,j)=Awrk(i,j,Nold)
        END DO
      END DO
      CALL bc_v2d_tile (ng, tile,                                       &
     &                  LBi, UBi, LBj, UBj,                             &
     &                  A)
# ifdef DISTRIBUTE
      CALL mp_exchange2d (ng, tile, model, 1,                           &
     &                    LBi, UBi, LBj, UBj,                           &
     &                    Nghost, EWperiodic, NSperiodic,               &
     &                    A)
# endif

      RETURN
      END SUBROUTINE conv_v2d_tile
#endif
      END MODULE conv_2d_mod
