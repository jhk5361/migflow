#include "cppdefs.h"
      MODULE bc_bry3d_mod
!
!svn $Id: bc_bry3d.F 314 2009-02-20 22:06:49Z arango $
!================================================== Hernan G. Arango ===
!  Copyright (c) 2002-2009 The ROMS/TOMS Group                         !
!    Licensed under a MIT/X style license                              !
!    See License_ROMS.txt                                              !
!=======================================================================
!                                                                      !
!  This package applies gradient conditions for generic 3D boundary    !
!  fields.                                                             !
!                                                                      !
!  Routines:                                                           !
!                                                                      !
!    bc_r3d_bry_tile    Boundary conditions for field at RHO-points    !
!    bc_u3d_bry_tile    Boundary conditions for field at U-points      !
!    bc_v3d_bry_tile    Boundary conditions for field at V-points      !
!                                                                      !
!=======================================================================
!
      implicit none

      CONTAINS
! 
!***********************************************************************
      SUBROUTINE bc_r3d_bry_tile (ng, tile, boundary,                   &
     &                            LBij, UBij, LBk, UBk,                 &
     &                            A)
!***********************************************************************
!
      USE mod_param
      USE mod_scalars
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile, boundary
      integer, intent(in) :: LBij, UBij, LBk, UBk

#ifdef ASSUMED_SHAPE
      real(r8), intent(inout) :: A(LBij:,LBk:)
#else
      real(r8), intent(inout) :: A(LBij:UBij,LBk:UBk)
#endif
!
!  Local variable declarations.
!
      integer :: k

#include "set_bounds.h"
!
!-----------------------------------------------------------------------
!  Western and Eastern edges: gradient boundary conditions.
!-----------------------------------------------------------------------
!
      IF (boundary.eq.iwest) THEN
        IF ((WESTERN_EDGE).and.(SOUTHERN_EDGE)) THEN
          DO k=LBk,UBk
            A(Jstr-1,k)=A(Jstr,k)
          END DO
        END IF
        IF ((WESTERN_EDGE).and.(NORTHERN_EDGE)) THEN
          DO k=LBk,UBk
            A(Jend+1,k)=A(Jend,k)
          END DO
        END IF
      END IF

      IF (boundary.eq.ieast) THEN
        IF ((EASTERN_EDGE).and.(SOUTHERN_EDGE)) THEN
          DO k=LBk,UBk
            A(Jstr-1,k)=A(Jstr,k)
          END DO
        END IF
        IF ((EASTERN_EDGE).and.(NORTHERN_EDGE)) THEN
          DO k=LBk,UBk
            A(Jend+1,k)=A(Jend,k)
          END DO
        END IF
      END IF
!
!-----------------------------------------------------------------------
!  Southern and Northern edges: gradient boundary conditions.
!-----------------------------------------------------------------------
!
      IF (boundary.eq.isouth) THEN
        IF ((SOUTHERN_EDGE).and.(WESTERN_EDGE)) THEN
          DO k=LBk,UBk
            A(Istr-1,k)=A(Istr,k)
          END DO
        END IF
        IF ((SOUTHERN_EDGE).and.(EASTERN_EDGE)) THEN
          DO k=LBk,UBk
            A(Iend+1,k)=A(Iend,k)
          END DO
        END IF
      END IF

      IF (boundary.eq.inorth) THEN
        IF ((NORTHERN_EDGE).and.(WESTERN_EDGE)) THEN
          DO k=LBk,UBk
            A(Istr-1,k)=A(Istr,k)
          END DO
        END IF
        IF ((NORTHERN_EDGE).and.(EASTERN_EDGE)) THEN
          DO k=LBk,UBk
            A(Iend+1,k)=A(Iend,k)
          END DO
        END IF
      END IF

      RETURN
      END SUBROUTINE bc_r3d_bry_tile

! 
!***********************************************************************
      SUBROUTINE bc_u3d_bry_tile (ng, tile, boundary,                   &
     &                            LBij, UBij, LBk, UBk,                 &
     &                            A)
!***********************************************************************
!
      USE mod_param
      USE mod_scalars
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile, boundary
      integer, intent(in) :: LBij, UBij, LBk, UBk

#ifdef ASSUMED_SHAPE
      real(r8), intent(inout) :: A(LBij:,LBk:)
#else
      real(r8), intent(inout) :: A(LBij:UBij,LBk:UBk)
#endif
!
!  Local variable declarations.
!
      integer :: k

#include "set_bounds.h"
!
!-----------------------------------------------------------------------
!  Western and Eastern edges: gradient boundary conditions.
!-----------------------------------------------------------------------
!
      IF (boundary.eq.iwest) THEN
        IF ((WESTERN_EDGE).and.(SOUTHERN_EDGE)) THEN
          DO k=LBk,UBk
            A(Jstr-1,k)=A(Jstr,k)
          END DO
        END IF
        IF ((WESTERN_EDGE).and.(NORTHERN_EDGE)) THEN
          DO k=LBk,UBk
            A(Jend+1,k)=A(Jend,k)
          END DO
        END IF
      END IF

      IF (boundary.eq.ieast) THEN
        IF ((EASTERN_EDGE).and.(SOUTHERN_EDGE)) THEN
          DO k=LBk,UBk
            A(Jstr-1,k)=A(Jstr,k)
          END DO
        END IF
        IF ((EASTERN_EDGE).and.(NORTHERN_EDGE)) THEN
          DO k=LBk,UBk
            A(Jend+1,k)=A(Jend,k)
          END DO
        END IF
      END IF
!
!-----------------------------------------------------------------------
!  Southern and Northern edges: gradient boundary conditions.
!-----------------------------------------------------------------------
!
      IF (boundary.eq.isouth) THEN
        IF ((SOUTHERN_EDGE).and.(WESTERN_EDGE)) THEN
          DO k=LBk,UBk
            A(IstrU-1,k)=A(IstrU,k)
          END DO
        END IF
        IF ((SOUTHERN_EDGE).and.(EASTERN_EDGE)) THEN
          DO k=LBk,UBk
            A(Iend+1,k)=A(Iend,k)
          END DO
        END IF
      END IF

      IF (boundary.eq.inorth) THEN
        IF ((NORTHERN_EDGE).and.(WESTERN_EDGE)) THEN
          DO k=LBk,UBk
            A(IstrU-1,k)=A(IstrU,k)
          END DO
        END IF
        IF ((NORTHERN_EDGE).and.(EASTERN_EDGE)) THEN
          DO k=LBk,UBk
            A(Iend+1,k)=A(Iend,k)
          END DO
        END IF
      END IF

      RETURN
      END SUBROUTINE bc_u3d_bry_tile

! 
!***********************************************************************
      SUBROUTINE bc_v3d_bry_tile (ng, tile, boundary,                   &
     &                            LBij, UBij, LBk, UBk,                 &
     &                            A)
!***********************************************************************
!
      USE mod_param
      USE mod_scalars
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile, boundary
      integer, intent(in) :: LBij, UBij, LBk, UBk

#ifdef ASSUMED_SHAPE
      real(r8), intent(inout) :: A(LBij:,LBk:)
#else
      real(r8), intent(inout) :: A(LBij:UBij,LBk:UBk)
#endif
!
!  Local variable declarations.
!
      integer :: k

#include "set_bounds.h"
!
!-----------------------------------------------------------------------
!  Western and Eastern edges: gradient boundary conditions.
!-----------------------------------------------------------------------
!
      IF (boundary.eq.iwest) THEN
        IF ((WESTERN_EDGE).and.(SOUTHERN_EDGE)) THEN
          DO k=LBk,UBk
            A(JstrV-1,k)=A(JstrV,k)
          END DO
        END IF
        IF ((WESTERN_EDGE).and.(NORTHERN_EDGE)) THEN
          DO k=LBk,UBk
            A(Jend+1,k)=A(Jend,k)
          END DO
        END IF
      END IF

      IF (boundary.eq.ieast) THEN
        IF ((EASTERN_EDGE).and.(SOUTHERN_EDGE)) THEN
          DO k=LBk,UBk
            A(JstrV-1,k)=A(JstrV,k)
          END DO
        END IF
        IF ((EASTERN_EDGE).and.(NORTHERN_EDGE)) THEN
          DO k=LBk,UBk
            A(Jend+1,k)=A(Jend,k)
          END DO
        END IF
      END IF
!
!-----------------------------------------------------------------------
!  Southern and Northern edges: gradient boundary conditions.
!-----------------------------------------------------------------------
!
      IF (boundary.eq.isouth) THEN
        IF ((SOUTHERN_EDGE).and.(WESTERN_EDGE)) THEN
          DO k=LBk,UBk
            A(Istr-1,k)=A(Istr,k)
          END DO
        END IF
        IF ((SOUTHERN_EDGE).and.(EASTERN_EDGE)) THEN
          DO k=LBk,UBk
            A(Iend+1,k)=A(Iend,k)
          END DO
        END IF
      END IF

      IF (boundary.eq.inorth) THEN
        IF ((NORTHERN_EDGE).and.(WESTERN_EDGE)) THEN
          DO k=LBk,UBk
            A(Istr-1,k)=A(Istr,k)
          END DO
        END IF
        IF ((NORTHERN_EDGE).and.(EASTERN_EDGE)) THEN
          DO k=LBk,UBk
            A(Iend+1,k)=A(Iend,k)
          END DO
        END IF
      END IF

      RETURN
      END SUBROUTINE bc_v3d_bry_tile

      END MODULE bc_bry3d_mod
