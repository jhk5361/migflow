#include "cppdefs.h"
      MODULE set_depth_mod
#ifdef SOLVE3D
!
!svn $Id: set_depth.F 357 2009-06-26 15:57:27Z arango $
!=======================================================================
!  Copyright (c) 2002-2009 The ROMS/TOMS Group                         !
!    Licensed under a MIT/X style license                              !
!    See License_ROMS.txt                           Hernan G. Arango   !
!========================================== Alexander F. Shchepetkin ===
!                                                                      !
!  This routine computes the time evolving depths of the model grid    !
!  and its associated vertical transformation metric (thickness).      !
!                                                                      !
!  Currently, two vertical coordinate transformations are available    !
!  with various possible vertical stretching, C(s), functions, (see    !
!  routine "set_scoord.F" for details).                                !
!                                                                      !
!=======================================================================
!
      implicit none

      PRIVATE
      PUBLIC  :: set_depth, set_depth_tile
# ifdef ADJUST_BOUNDARY
      PUBLIC  :: set_depth_bry
# endif

      CONTAINS
!
!***********************************************************************
      SUBROUTINE set_depth (ng, tile)
!***********************************************************************
!
      USE mod_param
      USE mod_coupling
      USE mod_grid
      USE mod_ocean
      USE mod_stepping
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile
!
!  Local variable declarations.
!
# include "tile.h"
!
      CALL set_depth_tile (ng, tile,                                    &
     &                     LBi, UBi, LBj, UBj,                          &
     &                     IminS, ImaxS, JminS, JmaxS,                  &
     &                     nstp(ng), nnew(ng),                          &
     &                     GRID(ng) % h,                                &
# ifdef ICESHELF
     &                     GRID(ng) % zice,                             &
# endif
# if defined SEDIMENT && defined SED_MORPH
     &                     GRID(ng) % bed_thick,                        &
# endif
     &                     COUPLING(ng) % Zt_avg1,                      &
     &                     GRID(ng) % Hz,                               &
     &                     GRID(ng) % z_r,                              &
     &                     GRID(ng) % z_w)
      RETURN
      END SUBROUTINE set_depth
!
!***********************************************************************
      SUBROUTINE set_depth_tile (ng, tile,                              &
     &                           LBi, UBi, LBj, UBj,                    &
     &                           IminS, ImaxS, JminS, JmaxS,            &
     &                           nstp, nnew,                            &
     &                           h,                                     &
# ifdef ICESHELF
     &                           zice,                                  &
# endif
# if defined SEDIMENT && defined SED_MORPH
     &                           bed_thick,                             &
# endif
     &                           Zt_avg1,                               &
     &                           Hz, z_r, z_w)
!***********************************************************************
!
      USE mod_param
      USE mod_scalars
!
# if defined EW_PERIODIC || defined NS_PERIODIC
      USE exchange_2d_mod
      USE exchange_3d_mod
# endif
# ifdef DISTRIBUTE
      USE mp_exchange_mod, ONLY : mp_exchange2d, mp_exchange3d
# endif
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile
      integer, intent(in) :: LBi, UBi, LBj, UBj
      integer, intent(in) :: IminS, ImaxS, JminS, JmaxS
      integer, intent(in) :: nstp, nnew
!
# ifdef ASSUMED_SHAPE
#  ifdef ICESHELF
      real(r8), intent(in) :: zice(LBi:,LBj:)
#  endif
#  if defined SEDIMENT && defined SED_MORPH
      real(r8), intent(in):: bed_thick(LBi:,LBj:,:)
#  endif
      real(r8), intent(in) :: Zt_avg1(LBi:,LBj:)
      real(r8), intent(inout) :: h(LBi:,LBj:)
      real(r8), intent(out) :: Hz(LBi:,LBj:,:)
      real(r8), intent(out) :: z_r(LBi:,LBj:,:)
      real(r8), intent(out) :: z_w(LBi:,LBj:,0:)
# else
#  ifdef ICESHELF
      real(r8), intent(in) :: zice(LBi:UBi,LBj:UBj)
#  endif
#  if defined SEDIMENT && defined SED_MORPH
      real(r8), intent(in):: bed_thick(LBi:UBi,LBj:UBj,2)
#  endif
      real(r8), intent(in) :: Zt_avg1(LBi:UBi,LBj:UBj)
      real(r8), intent(inout) :: h(LBi:UBi,LBj:UBj)
      real(r8), intent(out) :: Hz(LBi:UBi,LBj:UBj,N(ng))
      real(r8), intent(out) :: z_r(LBi:UBi,LBj:UBj,N(ng))
      real(r8), intent(out) :: z_w(LBi:UBi,LBj:UBj,0:N(ng))
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
      integer :: i, j, k

      real(r8) :: cff_r, cff1_r, cff2_r, cff_w, cff1_w, cff2_w
      real(r8) :: hinv, hwater, z_r0, z_w0
# ifdef WET_DRY
      real(r8), parameter :: eps = 1.0E-14_r8
# endif

# include "set_bounds.h"
!
!-----------------------------------------------------------------------
!  Original formulation: Compute vertical depths (meters, negative) at
!                        RHO- and W-points, and vertical grid
!  thicknesses. Various stretching functions are possible.
!
!         z_w(x,y,s,t) = Zo_w + zeta(x,y,t) * [1.0 + Zo_w / h(x,y)]
!
!                 Zo_w = hc * [s(k) - C(k)] + C(k) * h(x,y)
!
!-----------------------------------------------------------------------
!
      IF (Vtransform(ng).eq.1) THEN
        DO j=JstrR,JendR
          DO i=IstrR,IendR
# if defined SEDIMENT && defined SED_MORPH
            h(i,j)=h(i,j)-bed_thick(i,j,nstp)+bed_thick(i,j,nnew)
# endif
# if defined WET_DRY
            IF (h(i,j).eq.0.0_r8) THEN
              h(i,j)=eps
            END IF
# endif
            z_w(i,j,0)=-h(i,j)
          END DO
          DO k=1,N(ng)
            cff_r=hc(ng)*(SCALARS(ng)%sc_r(k)-SCALARS(ng)%Cs_r(k))
            cff_w=hc(ng)*(SCALARS(ng)%sc_w(k)-SCALARS(ng)%Cs_w(k))
            cff1_r=SCALARS(ng)%Cs_r(k)
            cff1_w=SCALARS(ng)%Cs_w(k)
            DO i=IstrR,IendR
              hwater=h(i,j)
# ifdef ICESHELF
              hwater=hwater-ABS(zice(i,j))
# endif
              hinv=1.0_r8/hwater
              z_w0=cff_w+cff1_w*hwater
              z_w(i,j,k)=z_w0+Zt_avg1(i,j)*(1.0_r8+z_w0*hinv)
              z_r0=cff_r+cff1_r*hwater
              z_r(i,j,k)=z_r0+Zt_avg1(i,j)*(1.0_r8+z_r0*hinv)
# ifdef ICESHELF
              z_w(i,j,k)=z_w(i,j,k)-ABS(zice(i,j))
              z_r(i,j,k)=z_r(i,j,k)-ABS(zice(i,j))
# endif
              Hz(i,j,k)=z_w(i,j,k)-z_w(i,j,k-1)
            END DO
          END DO
        END DO
!
!-----------------------------------------------------------------------
!  New formulation: Compute vertical depths (meters, negative) at
!                   RHO- and W-points, and vertical grid thicknesses.
!  Various stretching functions are possible.
!
!         z_w(x,y,s,t) = zeta(x,y,t) + [zeta(x,y,t)+ h(x,y)] * Zo_w
!
!                 Zo_w = [hc * s(k) + C(k) * h(x,y)] / [hc + h(x,y)]
!
!-----------------------------------------------------------------------
!
      ELSE IF (Vtransform(ng).eq.2) THEN
        DO j=JstrR,JendR
          DO i=IstrR,IendR
# if defined SEDIMENT && defined SED_MORPH
            h(i,j)=h(i,j)-bed_thick(i,j,nstp)+bed_thick(i,j,nnew)
# endif
# if defined WET_DRY
            IF (h(i,j).eq.0.0_r8) THEN
              h(i,j)=eps
            END IF
# endif
            z_w(i,j,0)=-h(i,j)
          END DO
          DO k=1,N(ng)
            cff_r=hc(ng)*SCALARS(ng)%sc_r(k)
            cff_w=hc(ng)*SCALARS(ng)%sc_w(k)
            cff1_r=SCALARS(ng)%Cs_r(k)
            cff1_w=SCALARS(ng)%Cs_w(k)
            DO i=IstrR,IendR
              hwater=h(i,j)
# ifdef ICESHELF
              hwater=hwater-ABS(zice(i,j))
# endif
              hinv=1.0_r8/(hc(ng)+hwater)
              cff2_r=(cff_r+cff1_r*hwater)*hinv
              cff2_w=(cff_w+cff1_w*hwater)*hinv

              z_w(i,j,k)=Zt_avg1(i,j)+(Zt_avg1(i,j)+hwater)*cff2_w
              z_r(i,j,k)=Zt_avg1(i,j)+(Zt_avg1(i,j)+hwater)*cff2_r
# ifdef ICESHELF
              z_w(i,j,k)=z_w(i,j,k)-ABS(zice(i,j))
              z_r(i,j,k)=z_r(i,j,k)-ABS(zice(i,j))
# endif
              Hz(i,j,k)=z_w(i,j,k)-z_w(i,j,k-1)
            END DO
          END DO
        END DO
      END IF

# if defined EW_PERIODIC || defined NS_PERIODIC || defined DISTRIBUTE
!
!-----------------------------------------------------------------------
!  Exchange boundary information.
!-----------------------------------------------------------------------
!
#  if defined EW_PERIODIC || defined NS_PERIODIC
      CALL exchange_r2d_tile (ng, tile,                                 &
     &                        LBi, UBi, LBj, UBj,                       &
     &                        h)
      CALL exchange_w3d_tile (ng, tile,                                 &
     &                        LBi, UBi, LBj, UBj, 0, N(ng),             &
     &                        z_w)
      CALL exchange_r3d_tile (ng, tile,                                 &
     &                        LBi, UBi, LBj, UBj, 1, N(ng),             &
     &                        z_r)
      CALL exchange_r3d_tile (ng, tile,                                 &
     &                        LBi, UBi, LBj, UBj, 1, N(ng),             &
     &                        Hz)
#  endif
#  ifdef DISTRIBUTE
      CALL mp_exchange2d (ng, tile, iNLM, 1,                            &
     &                    LBi, UBi, LBj, UBj,                           &
     &                    NghostPoints, EWperiodic, NSperiodic,         &
     &                    h)
      CALL mp_exchange3d (ng, tile, iNLM, 1,                            &
     &                    LBi, UBi, LBj, UBj, 0, N(ng),                 &
     &                    NghostPoints, EWperiodic, NSperiodic,         &
     &                    z_w)
      CALL mp_exchange3d (ng, tile, iNLM, 2,                            &
     &                    LBi, UBi, LBj, UBj, 1, N(ng),                 &
     &                    NghostPoints, EWperiodic, NSperiodic,         &
     &                    z_r, Hz)
#  endif
# endif

# ifdef ADJUST_BOUNDARY
!
!-----------------------------------------------------------------------
!  Compute level ticknesses at the open boundaries using the provided
!  free-surface values (zeta_west, zeta_east, zeta_south, zeta_north).
!-----------------------------------------------------------------------
!
      CALL set_depth_bry (ng, tile)
# endif

      RETURN
      END SUBROUTINE set_depth_tile

# ifdef ADJUST_BOUNDARY
!
!***********************************************************************
      SUBROUTINE set_depth_bry (ng, tile)
!***********************************************************************
!
      USE mod_param
      USE mod_boundary
      USE mod_grid
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile
!
!  Local variable declarations.
!
#  include "tile.h"
!
      CALL set_depth_bry_tile (ng, tile,                                &
     &                         LBi, UBi, LBj, UBj, LBij, UBij,          &
     &                         IminS, ImaxS, JminS, JmaxS,              &
     &                         GRID(ng) % h,                            &
#  ifdef ICESHELF
     &                         GRID(ng) % zice,                         &
#  endif
#  ifdef WEST_FSOBC
     &                         BOUNDARY(ng) % zeta_west,                &
#  endif
#  ifdef EAST_FSOBC
     &                         BOUNDARY(ng) % zeta_east,                &
#  endif
#  ifdef SOUTH_FSOBC
     &                         BOUNDARY(ng) % zeta_south,               &
#  endif
#  ifdef NORTH_FSOBC
     &                         BOUNDARY(ng) % zeta_north,               &
#  endif
     &                         GRID(ng) % Hz_bry)
      RETURN
      END SUBROUTINE set_depth_bry
!
!***********************************************************************
      SUBROUTINE set_depth_bry_tile (ng, tile,                          &
     &                               LBi, UBi, LBj, UBj, LBij, UBij,    &
     &                               IminS, ImaxS, JminS, JmaxS,        &
     &                               h,                                 &
#  ifdef ICESHELF
     &                               zice,                              &
#  endif
#  ifdef WEST_FSOBC
     &                               zeta_west,                         &
#  endif
#  ifdef EAST_FSOBC
     &                               zeta_east,                         &
#  endif
#  ifdef SOUTH_FSOBC
     &                               zeta_south,                        &
#  endif
#  ifdef NORTH_FSOBC
     &                               zeta_north,                        &
#  endif
     &                               Hz_bry)
!***********************************************************************
!
      USE mod_param
      USE mod_ncparam
      USE mod_scalars
!
#  ifdef DISTRIBUTE
      USE mp_exchange_mod, ONLY : mp_exchange3d_bry
#  endif
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile
      integer, intent(in) :: LBi, UBi, LBj, UBj, LBij, UBij
      integer, intent(in) :: IminS, ImaxS, JminS, JmaxS
!
#  ifdef ASSUMED_SHAPE
      real(r8), intent(in) :: h(LBi:,LBj:)
#   ifdef ICESHELF
      real(r8), intent(in) :: zice(LBi:,LBj:)
#   endif
#   ifdef WEST_FSOBC
      real(r8), intent(in) :: zeta_west(0:)
#   endif
#   ifdef EAST_FSOBC
      real(r8), intent(in) :: zeta_east(0:)
#   endif
#   ifdef SOUTH_FSOBC
      real(r8), intent(in) :: zeta_south(0:)
#   endif
#   ifdef NORTH_FSOBC
      real(r8), intent(in) :: zeta_north(0:)
#   endif
      real(r8), intent(out) :: Hz_bry(LBij:,:,:)
#  else
      real(r8), intent(in) :: h(LBi:UBi,LBj:UBj)
#   ifdef ICESHELF
      real(r8), intent(in) :: zice(LBi:UBi,LBj:UBj)
#   endif
#   ifdef WEST_FSOBC
      real(r8), intent(inout) :: zeta_west(0:Jm(ng)+1)
#   endif
#   ifdef EAST_FSOBC
      real(r8), intent(inout) :: zeta_east(0:Jm(ng)+1)
#   endif
#   ifdef SOUTH_FSOBC
      real(r8), intent(inout) :: zeta_south(0:Im(ng)+1)
#   endif
#   ifdef NORTH_FSOBC
      real(r8), intent(inout) :: zeta_north(0:Im(ng)+1)
#   endif
      real(r8), intent(out) :: Hz_bry(LBij:UBij,N(ng),4)
#  endif
!
!  Local variable declarations.
!
#  ifdef DISTRIBUTE
#   ifdef EW_PERIODIC
      logical :: EWperiodic=.TRUE.
#   else
      logical :: EWperiodic=.FALSE.
#   endif
#   ifdef NS_PERIODIC
      logical :: NSperiodic=.TRUE.
#   else
      logical :: NSperiodic=.FALSE.
#   endif
#  endif
      integer :: i, ibry, j, k

      real(r8) :: cff_w, cff1_w, cff2_w
      real(r8) :: hinv, hwater, z_w0

      real(r8), dimension(0:N(ng)) :: Zw

#  include "set_bounds.h"
!
!-----------------------------------------------------------------------
!  Original formulation: Compute vertical depths (meters, negative) at
!                        RHO- and W-points, and vertical grid
!  thicknesses. Various stretching functions are possible.
!
!         z_w(x,y,s,t) = Zo_w + zeta(x,y,t) * [1.0 + Zo_w / h(x,y)]
!
!                 Zo_w = hc * [s(k) - C(k)] + C(k) * h(x,y)
!
!-----------------------------------------------------------------------
!
      IF (Vtransform(ng).eq.1) THEN

#  ifdef WEST_FSOBC
        IF (WESTERN_EDGE) THEN
          i=BOUNDS(ng)%edge(iwest,r2dvar)
          DO j=JstrR,JendR
            hwater=h(i,j)
#   ifdef ICESHELF
            hwater=hwater-ABS(zice(i,j))
#   endif
            hinv=1.0_r8/hwater
            Zw(0)=-h(i,j)
            DO k=1,N(ng)
              cff_w=hc(ng)*(SCALARS(ng)%sc_w(k)-SCALARS(ng)%Cs_w(k))
              cff1_w=SCALARS(ng)%Cs_w(k)
              z_w0=cff_w+cff1_w*hwater
              Zw(k)=z_w0+zeta_west(j)*(1.0_r8+z_w0*hinv)
#   ifdef ICESHELF
              Zw(k)=Zw(k)-ABS(zice(i,j))
#   endif
              Hz_bry(j,k,iwest)=Zw(k)-Zw(k-1)
            END DO
          END DO
        END IF
#  endif

#  ifdef EAST_FSOBC
        IF (EASTERN_EDGE) THEN
          i=BOUNDS(ng)%edge(ieast,r2dvar)
          DO j=JstrR,JendR
            hwater=h(i,j)
#   ifdef ICESHELF
            hwater=hwater-ABS(zice(i,j))
#   endif
            hinv=1.0_r8/hwater
            Zw(0)=-h(i,j)
            DO k=1,N(ng)
              cff_w=hc(ng)*(SCALARS(ng)%sc_w(k)-SCALARS(ng)%Cs_w(k))
              cff1_w=SCALARS(ng)%Cs_w(k)
              z_w0=cff_w+cff1_w*hwater
              Zw(k)=z_w0+zeta_east(j)*(1.0_r8+z_w0*hinv)
#   ifdef ICESHELF
              Zw(k)=Zw(k)-ABS(zice(i,j))
#   endif
              Hz_bry(j,k,ieast)=Zw(k)-Zw(k-1)
            END DO
          END DO
        END IF
#  endif

#  ifdef SOUTH_FSOBC
        IF (SOUTHERN_EDGE) THEN
          j=BOUNDS(ng)%edge(isouth,r2dvar)
          DO i=IstrR,IendR
            hwater=h(i,j)
#   ifdef ICESHELF
            hwater=hwater-ABS(zice(i,j))
#   endif
            hinv=1.0_r8/hwater
            Zw(0)=-h(i,j)
            DO k=1,N(ng)
              cff_w=hc(ng)*(SCALARS(ng)%sc_w(k)-SCALARS(ng)%Cs_w(k))
              cff1_w=SCALARS(ng)%Cs_w(k)
              z_w0=cff_w+cff1_w*hwater
              Zw(k)=z_w0+zeta_south(i)*(1.0_r8+z_w0*hinv)
#   ifdef ICESHELF
              Zw(k)=Zw(k)-ABS(zice(i,j))
#   endif
              Hz_bry(i,k,isouth)=Zw(k)-Zw(k-1)
            END DO
          END DO
        END IF
#  endif

#  ifdef NORTH_FSOBC
        IF (NORTHERN_EDGE) THEN
          j=BOUNDS(ng)%edge(inorth,r2dvar)
          DO i=IstrR,IendR
            hwater=h(i,j)
#   ifdef ICESHELF
            hwater=hwater-ABS(zice(i,j))
#   endif
            hinv=1.0_r8/hwater
            Zw(0)=-h(i,j)
            DO k=1,N(ng)
              cff_w=hc(ng)*(SCALARS(ng)%sc_w(k)-SCALARS(ng)%Cs_w(k))
              cff1_w=SCALARS(ng)%Cs_w(k)
              z_w0=cff_w+cff1_w*hwater
              Zw(k)=z_w0+zeta_north(i)*(1.0_r8+z_w0*hinv)
#   ifdef ICESHELF
              Zw(k)=Zw(k)-ABS(zice(i,j))
#   endif
              Hz_bry(i,k,inorth)=Zw(k)-Zw(k-1)
            END DO
          END DO
        END IF
#  endif
!
!-----------------------------------------------------------------------
!  New formulation: Compute vertical depths (meters, negative) at
!                   RHO- and W-points, and vertical grid thicknesses.
!  Various stretching functions are possible.
!
!         z_w(x,y,s,t) = zeta(x,y,t) + [zeta(x,y,t)+ h(x,y)] * Zo_w
!
!                 Zo_w = [hc * s(k) + C(k) * h(x,y)] / [hc + h(x,y)]
!
!-----------------------------------------------------------------------
!
      ELSE IF (Vtransform(ng).eq.2) THEN

#  ifdef WEST_FSOBC
        IF (WESTERN_EDGE) THEN
          i=BOUNDS(ng)%edge(iwest,r2dvar)
          DO j=JstrR,JendR
            hwater=h(i,j)
#   ifdef ICESHELF
            hwater=hwater-ABS(zice(i,j))
#   endif
            hinv=1.0_r8/(hc(ng)+hwater)
            Zw(0)=-h(i,j)
            DO k=1,N(ng)
              cff_w=hc(ng)*SCALARS(ng)%sc_w(k)
              cff1_w=SCALARS(ng)%Cs_w(k)
              cff2_w=(cff_w+cff1_w*hwater)*hinv
              Zw(k)=zeta_west(j)+(zeta_west(j)+hwater)*cff2_w
#   ifdef ICESHELF
              Zw(k)=Zw(k)-ABS(zice(i,j))
#   endif
              Hz_bry(j,k,iwest)=Zw(k)-Zw(k-1)
            END DO
          END DO
        END IF
#  endif

#  ifdef EAST_FSOBC
        IF (EASTERN_EDGE) THEN
          i=BOUNDS(ng)%edge(ieast,r2dvar)
          DO j=JstrR,JendR
            hwater=h(i,j)
#   ifdef ICESHELF
            hwater=hwater-ABS(zice(i,j))
#   endif
            hinv=1.0_r8/(hc(ng)+hwater)
            Zw(0)=-h(i,j)
            DO k=1,N(ng)
              cff_w=hc(ng)*SCALARS(ng)%sc_w(k)
              cff1_w=SCALARS(ng)%Cs_w(k)
              cff2_w=(cff_w+cff1_w*hwater)*hinv
              Zw(k)=zeta_east(j)+(zeta_east(j)+hwater)*cff2_w
#   ifdef ICESHELF
              Zw(k)=Zw(k)-ABS(zice(i,j))
#   endif
              Hz_bry(j,k,ieast)=Zw(k)-Zw(k-1)
            END DO
          END DO
        END IF
#  endif

#  ifdef SOUTH_FSOBC
        IF (SOUTHERN_EDGE) THEN
          j=BOUNDS(ng)%edge(isouth,r2dvar)
          DO i=IstrR,IendR
            hwater=h(i,j)
#   ifdef ICESHELF
            hwater=hwater-ABS(zice(i,j))
#   endif
            hinv=1.0_r8/(hc(ng)+hwater)
            Zw(0)=-h(i,j)
            DO k=1,N(ng)
              cff_w=hc(ng)*SCALARS(ng)%sc_w(k)
              cff1_w=SCALARS(ng)%Cs_w(k)
              cff2_w=(cff_w+cff1_w*hwater)*hinv
              Zw(k)=zeta_south(i)+(zeta_south(i)+hwater)*cff2_w
#   ifdef ICESHELF
              Zw(k)=Zw(k)-ABS(zice(i,j))
#   endif
              Hz_bry(i,k,isouth)=Zw(k)-Zw(k-1)
            END DO
          END DO
        END IF
#  endif

#  ifdef NORTH_FSOBC
        IF (NORTHERN_EDGE) THEN
          j=BOUNDS(ng)%edge(inorth,r2dvar)
          DO i=IstrR,IendR
            hwater=h(i,j)
#   ifdef ICESHELF
            hwater=hwater-ABS(zice(i,j))
#   endif
            hinv=1.0_r8/(hc(ng)+hwater)
            Zw(0)=-h(i,j)
            DO k=1,N(ng)
              cff_w=hc(ng)*SCALARS(ng)%sc_w(k)
              cff1_w=SCALARS(ng)%Cs_w(k)
              cff2_w=(cff_w+cff1_w*hwater)*hinv
              Zw(k)=zeta_north(i)+(zeta_north(i)+hwater)*cff2_w
#   ifdef ICESHELF
              Zw(k)=Zw(k)-ABS(zice(i,j))
#   endif
              Hz_bry(i,k,inorth)=Zw(k)-Zw(k-1)
            END DO
          END DO
        END IF
#  endif
      END IF

#  ifdef DISTRIBUTE
!
!-----------------------------------------------------------------------
!  Exchange boundary information.
!-----------------------------------------------------------------------
!
      DO ibry=1,4
        CALL mp_exchange3d_bry (ng, tile, iNLM, 1, ibry,                &
     &                          LBij, UBij, 1, N(ng),                   &
     &                          NghostPoints, EWperiodic, NSperiodic,   &
     &                          Hz_bry(:,:,ibry))
      END DO
#  endif

      RETURN
      END SUBROUTINE set_depth_bry_tile
# endif
#endif
      END MODULE set_depth_mod
