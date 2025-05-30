#include "cppdefs.h"
      MODULE frc_adjust_mod

#if defined ADJUST_STFLUX || defined ADJUST_WSTRESS
!
!svn $Id: frc_adjust.F 352 2009-05-29 20:57:39Z arango $
!================================================== Hernan G. Arango ===
!  Copyright (c) 2002-2009 The ROMS/TOMS Group                         !
!    Licensed under a MIT/X style license                              !
!    See License_ROMS.txt                                              !
!=======================================================================
!                                                                      !
!  This routine time-interpolates  4DVar surface forcing increments    !
!  and then adjust nonlinear model surface forcing.  The increments    !
!  can be constant (Nfrec=1) or time interpolated between snapshots    !
!  (Nfrec>1).                                                          !
!                                                                      !
!  On Input:                                                           !
!                                                                      !
!     ng        Nested grid number.                                    !
!     tile      Domain partition.                                      !
!     Linp      4DVar increment time index to process.                 !
!                                                                      !
!=======================================================================
!
      implicit none

      PRIVATE
      PUBLIC :: frc_adjust
      PUBLIC :: load_forcing

      CONTAINS
!
!***********************************************************************
      SUBROUTINE frc_adjust (ng, tile, Linp)
!***********************************************************************
!
      USE mod_param
      USE mod_forces
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile, Linp
!
!  Local variable declarations.
!
# include "tile.h"
!
# ifdef PROFILE
      CALL wclock_on (ng, iNLM, 7)
# endif
      CALL frc_adjust_tile (ng, tile,                                   &
     &                      LBi, UBi, LBj, UBj,                         &
     &                      IminS, ImaxS, JminS, JmaxS,                 &
# ifdef ADJUST_WSTRESS
     &                      FORCES(ng) % tl_ustr,                       &
     &                      FORCES(ng) % tl_vstr,                       &
     &                      FORCES(ng) % ustr,                          &
     &                      FORCES(ng) % vstr,                          &
     &                      FORCES(ng) % sustr,                         &
     &                      FORCES(ng) % svstr,                         &
# endif
# if defined ADJUST_STFLUX && defined SOLVE3D
     &                      FORCES(ng) % tl_tflux,                      &
     &                      FORCES(ng) % tflux,                         &
     &                      FORCES(ng) % stflx,                         &
# endif
     &                      Linp)
# ifdef PROFILE
      CALL wclock_off (ng, iNLM, 7)
# endif
      RETURN
      END SUBROUTINE frc_adjust
!
!***********************************************************************
      SUBROUTINE frc_adjust_tile (ng, tile,                             &
     &                            LBi, UBi, LBj, UBj,                   &
     &                            IminS, ImaxS, JminS, JmaxS,           &
# ifdef ADJUST_WSTRESS
     &                            tl_ustr,  tl_vstr,                    &
     &                            ustr, vstr,                           &
     &                            sustr, svstr,                         &
# endif
# if defined ADJUST_STFLUX && defined SOLVE3D
     &                            tl_tflux, tflux, stflx,               &
# endif
     &                            Linp)
!***********************************************************************
!
      USE mod_param
      USE mod_scalars
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile
      integer, intent(in) :: LBi, UBi, LBj, UBj
      integer, intent(in) :: IminS, ImaxS, JminS, JmaxS
      integer, intent(in) :: Linp
!
# ifdef ASSUMED_SHAPE
#  ifdef ADJUST_WSTRESS
      real(r8), intent(in) :: tl_ustr(LBi:,LBj:,:,:)
      real(r8), intent(in) :: tl_vstr(LBi:,LBj:,:,:)
#  endif
#  if defined ADJUST_STFLUX && defined SOLVE3D
      real(r8), intent(inout) :: tflux(LBi:,LBj:,:,:,:)
      real(r8), intent(inout) :: tl_tflux(LBi:,LBj:,:,:,:)
#  endif
#  ifdef ADJUST_WSTRESS
      real(r8), intent(inout) :: ustr(LBi:,LBj:,:,:)
      real(r8), intent(inout) :: vstr(LBi:,LBj:,:,:)
      real(r8), intent(inout) :: sustr(LBi:,LBj:)
      real(r8), intent(inout) :: svstr(LBi:,LBj:)
#  endif
#  if defined ADJUST_STFLUX && defined SOLVE3D
      real(r8), intent(inout) :: stflx(LBi:,LBj:,:)
#  endif
# else
#  ifdef ADJUST_WSTRESS
      real(r8), intent(in) :: tl_ustr(LBi:UBi,LBj:UBj,Nfrec(ng),2)
      real(r8), intent(in) :: tl_vstr(LBi:UBi,LBj:UBj,Nfrec(ng),2)
#  endif
#  if defined ADJUST_STFLUX && defined SOLVE3D
      real(r8), intent(in) :: tflux(LBi:UBi,LBj:UBj,                    &
     &                              Nfrec(ng),2,NT(ng))
      real(r8), intent(in) :: tl_tflux(LBi:UBi,LBj:UBj,                 &
     &                                 Nfrec(ng),2,NT(ng))
#  endif
#  ifdef ADJUST_WSTRESS
      real(r8), intent(inout) :: ustr(LBi:UBi,LBj:UBj,Nfrec(ng),2)
      real(r8), intent(inout) :: vstr(LBi:UBi,LBj:UBj,Nfrec(ng),2)
      real(r8), intent(inout) :: sustr(LBi:UBi,LBj:UBj)
      real(r8), intent(inout) :: svstr(LBi:UBi,LBj:UBj)
#  endif
#  if defined ADJUST_STFLUX && defined SOLVE3D
      real(r8), intent(inout) :: stflx(LBi:UBi,LBj:UBj,NT(ng))
#  endif
# endif
!
!  Local variable declarations.
!
      integer :: i, it1, it2, j
# ifdef SOLVE3D
      integer :: itrc
# endif
      real(r8) :: fac, fac1, fac2

# include "set_bounds.h"
!
!-----------------------------------------------------------------------
!  Adjust nonlinear surface forcing fields with 4DVar increments.
!-----------------------------------------------------------------------
!
!  Set time records and interpolation factor, if any.
!
      IF (Nfrec(ng).eq.1) THEN
        it1=1
        it2=1
        fac1=1.0_r8
        fac2=0.0_r8
      ELSE
        it1=MAX(0,(iic(ng)-1)/nSFF(ng))+1
        it2=MIN(it1+1,Nfrec(ng))
        fac1=SF_time(it2,ng)-(time(ng)+dt(ng))
        fac2=(time(ng)+dt(ng))-SF_time(it1,ng)
        fac=1.0_r8/(fac1+fac2)
        fac1=fac*fac1
        fac2=fac*fac2
      END IF

# ifdef ADJUST_WSTRESS
!
!  Adjust surface momentum stress. Interpolate between surface forcing
!  increments, if appropriate.
!
      DO j=JstrR,JendR
        DO i=Istr,IendR
          sustr(i,j)=sustr(i,j)+                                        &
     &               fac1*tl_ustr(i,j,it1,Linp)+                        &
     &               fac2*tl_ustr(i,j,it2,Linp)
        END DO
      END DO
      DO j=Jstr,JendR
        DO i=IstrR,IendR
          svstr(i,j)=svstr(i,j)+                                        &
     &               fac1*tl_vstr(i,j,it1,Linp)+                        &
     &               fac2*tl_vstr(i,j,it2,Linp)
        END DO
      END DO
# endif
# if defined ADJUST_STFLUX && defined SOLVE3D
!
!  Adjust surface tracer fluxes. Interpolate between surface forcing
!  increments, if appropriate.
!
      DO itrc=1,NT(ng)
        IF (Lstflux(itrc,ng)) THEN
          DO j=JstrR,JendR
            DO i=IstrR,IendR
              stflx(i,j,itrc)=stflx(i,j,itrc)+                          &
     &                        fac1*tl_tflux(i,j,it1,Linp,itrc)+         &
     &                        fac2*tl_tflux(i,j,it2,Linp,itrc)
            END DO
          END DO
        END IF
      END DO
# endif

      RETURN
      END SUBROUTINE frc_adjust_tile

      SUBROUTINE load_forcing (ng, tile, Lout)
!
!=======================================================================
!                                                                      !
!  This routine loads surface forcing into nonlinear storage arrays.   !
!  In  4DVAR  surface forcing adjustment,  the fluxes are stored in    !
!  arrays with extra dimensions to facilitate minimization at other    !
!  times in addition to initialization time.                           !
!                                                                      !
!  On Input:                                                           !
!                                                                      !
!     ng        Nested grid number.                                    !
!     tile      Domain partition.                                      !
!     Lout      Time index to process in storage arrays.               !
!                                                                      !
!=======================================================================
!
      USE mod_param
      USE mod_forces
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile, Lout
!
!  Local variable declarations.
!
# include "tile.h"
!
# ifdef PROFILE
      CALL wclock_on (ng, iNLM, 8)
# endif
      CALL load_forcing_tile (ng, tile,                                 &
     &                        LBi, UBi, LBj, UBj,                       &
     &                        IminS, ImaxS, JminS, JmaxS,               &
# ifdef ADJUST_WSTRESS
     &                        FORCES(ng) % sustr,                       &
     &                        FORCES(ng) % svstr,                       &
     &                        FORCES(ng) % ustr,                        &
     &                        FORCES(ng) % vstr,                        &
# endif
# if defined ADJUST_STFLUX && defined SOLVE3D
     &                        FORCES(ng) % stflx,                       &
     &                        FORCES(ng) % tflux,                       &
# endif
     &                        Lout)
# ifdef PROFILE
      CALL wclock_off (ng, iNLM, 8)
# endif

      RETURN
      END SUBROUTINE load_forcing
!
!***********************************************************************
      SUBROUTINE load_forcing_tile (ng, tile,                           &
     &                              LBi, UBi, LBj, UBj,                 &
     &                              IminS, ImaxS, JminS, JmaxS,         &
# ifdef ADJUST_WSTRESS
     &                              sustr, svstr,                       &
     &                              ustr,  vstr,                        &
# endif
# if defined ADJUST_STFLUX && defined SOLVE3D
     &                              stflx, tflux,                       &
# endif
     &                              Lout)
!***********************************************************************
!
      USE mod_param
      USE mod_scalars
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile
      integer, intent(in) :: LBi, UBi, LBj, UBj
      integer, intent(in) :: IminS, ImaxS, JminS, JmaxS
      integer, intent(in) :: Lout
!
# ifdef ASSUMED_SHAPE
#  ifdef ADJUST_WSTRESS
      real(r8), intent(in) :: sustr(LBi:,LBj:)
      real(r8), intent(in) :: svstr(LBi:,LBj:)
#  endif
#  if defined ADJUST_STFLUX && defined SOLVE3D
      real(r8), intent(in) :: stflx(LBi:,LBj:,:)
#  endif
#  ifdef ADJUST_WSTRESS
      real(r8), intent(inout) :: ustr(LBi:,LBj:,:,:)
      real(r8), intent(inout) :: vstr(LBi:,LBj:,:,:)
#  endif
#  if defined ADJUST_STFLUX && defined SOLVE3D
      real(r8), intent(inout) :: tflux(LBi:,LBj:,:,:,:)
#  endif
# else
#  ifdef ADJUST_WSTRESS
      real(r8), intent(in) :: sustr(LBi:UBi,LBj:UBj)
      real(r8), intent(in) :: svstr(LBi:UBi,LBj:UBj)
#  endif
#  if defined ADJUST_STFLUX && defined SOLVE3D
      real(r8), intent(in) :: stflx(LBi:UBi,LBj:UBj,NT(ng))
#  endif
#  ifdef ADJUST_WSTRESS
      real(r8), intent(inout) :: ustr(LBi:UBi,LBj:UBj,Nfrec(ng),2)
      real(r8), intent(inout) :: vstr(LBi:UBi,LBj:UBj,Nfrec(ng),2)
#  endif
#  if defined ADJUST_STFLUX && defined SOLVE3D
      real(r8), intent(inout) :: tflux(LBi:UBi,LBj:UBj,                 &
     &                                 Nfrec(ng),2,NT(ng))
#  endif
# endif
!
!  Local variable declarations.
!
      integer :: i, j
# ifdef SOLVE3D
      integer :: itrc
# endif

# include "set_bounds.h"
!
!-----------------------------------------------------------------------
!  Load nonlinear surface forcing fields into storage arrays.
!-----------------------------------------------------------------------
!
      IF (MOD(iic(ng)-1,nSFF(ng)).eq.0) THEN
        SFcount(ng)=SFcount(ng)+1

# ifdef ADJUST_WSTRESS
!
!  Load surface momentum stress.
!
        DO j=JstrR,JendR
          DO i=Istr,IendR
            ustr(i,j,SFcount(ng),Lout)=sustr(i,j)
          END DO
        END DO
        DO j=Jstr,JendR
          DO i=IstrR,IendR
            vstr(i,j,SFcount(ng),Lout)=svstr(i,j)
          END DO
        END DO
# endif
# if defined ADJUST_STFLUX && defined SOLVE3D
!
!  Load surface tracer fluxes.
!
        DO itrc=1,NT(ng)
          IF (Lstflux(itrc,ng)) THEN
            DO j=JstrR,JendR
              DO i=IstrR,IendR
                tflux(i,j,SFcount(ng),Lout,itrc)=stflx(i,j,itrc)
              END DO
            END DO
          END IF
        END DO
# endif
      END IF

      RETURN
      END SUBROUTINE load_forcing_tile
#endif
      END MODULE frc_adjust_mod
