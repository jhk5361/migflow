#include "cppdefs.h"
      MODULE nf_fwrite2d_bry_mod
!
!svn $Id: nf_fwrite2d_bry.F 314 2009-02-20 22:06:49Z arango $
!================================================== Hernan G. Arango ===
!  Copyright (c) 2002-2009 The ROMS/TOMS Group                         !
!    Licensed under a MIT/X style license                              !
!    See License_ROMS.txt                                              !
!=======================================================================
!                                                                      !
!  This function writes out a generic floating point 2D boundary array !
!  into an output NetCDF file.                                         !
!                                                                      !
!  On Input:                                                           !
!                                                                      !
!     ng           Nested grid number (integer)                        !
!     model        Calling model identifier (integer)                  !
!     ncname       NetCDF output file name (string)                    !
!     ncid         NetCDF file ID (integer)                            !
!     ncvname      NetCDF variable name (string)                       !
!     ncvarid      NetCDF variable ID (integer)                        !
!     tindex       NetCDF time record index to write (integer)         !
!     gtype        Grid type (integer)                                 !
!     LBij         IJ-dimension Lower bound (integer)                  !
!     UBij         IJ-dimension Upper bound (integer)                  !
!     Nrec         Number of boundary records (integer)                !
!     Ascl         Factor to scale field before writing (real)         !
!     A            Boundary field to write out (real)                  !
!                                                                      !
!  On Output:                                                          !
!                                                                      !
!     status       Error flag (integer).                               !
!                                                                      !
!=======================================================================
!
      implicit none

      CONTAINS

!
!***********************************************************************
      FUNCTION nf_fwrite2d_bry (ng, model, ncname, ncid,                &
     &                          ncvname, ncvarid,                       &
     &                          tindex, gtype,                          &
     &                          LBij, UBij, Nrec,                       &
     &                          Ascl, A)  RESULT(status)
!***********************************************************************
!
      USE mod_param
      USE mod_parallel
      USE mod_ncparam
      USE mod_netcdf
      USE mod_scalars

#ifdef DISTRIBUTE
!
      USE distribute_mod, ONLY : mp_bcasti, mp_collect
#endif
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, model, ncid, ncvarid, tindex, gtype
      integer, intent(in) :: LBij, UBij, Nrec
      real(r8), intent(in) :: Ascl

      character (len=*), intent(in) :: ncname
      character (len=*), intent(in) :: ncvname

#ifdef ASSUMED_SHAPE
      real(r8), intent(in) :: A(LBij:,:,:)
#else
      real(r8), intent(in) :: A(LBij:UBij,4,Nrec)
#endif
!
!  Local variable declarations.
!
      logical, dimension(4) :: bounded

      integer :: bc, i, ib, ic, ir, j, rc, tile
      integer :: IorJ, Imin, Imax, Jmin, Jmax, Npts
      integer :: Istr, Iend, Jstr, Jend

      integer, dimension(4) :: start, total

      integer :: status

      real(r8), parameter :: Aspv = 0.0_r8

      real(r8), dimension((UBij-LBij+1)*4*Nrec) :: Aout
!
!-----------------------------------------------------------------------
!  Set starting and ending indices to process.
!-----------------------------------------------------------------------
!
#ifdef DISTRIBUTE
      tile=MyRank

      SELECT CASE (gtype)
        CASE (p2dvar, p3dvar)
          Imin=BOUNDS(ng)%Istr (tile)
          Imax=BOUNDS(ng)%Iend (tile)
          Jmin=BOUNDS(ng)%Jstr (tile)
          Jmax=BOUNDS(ng)%Jend (tile)
        CASE (r2dvar, r3dvar)
          Imin=BOUNDS(ng)%IstrR(tile)
          Imax=BOUNDS(ng)%IendR(tile)
          Jmin=BOUNDS(ng)%JstrR(tile)
          Jmax=BOUNDS(ng)%JendR(tile)
        CASE (u2dvar, u3dvar)
          Imin=BOUNDS(ng)%Istr (tile)
          Imax=BOUNDS(ng)%IendR(tile)
          Jmin=BOUNDS(ng)%JstrR(tile)
          Jmax=BOUNDS(ng)%JendR(tile)
        CASE (v2dvar, v3dvar)
          Imin=BOUNDS(ng)%IstrR(tile)
          Imax=BOUNDS(ng)%IendR(tile)
          Jmin=BOUNDS(ng)%Jstr (tile)
          Jmax=BOUNDS(ng)%JendR(tile)
        CASE DEFAULT
          Imin=BOUNDS(ng)%IstrR(tile)
          Imax=BOUNDS(ng)%IendR(tile)
          Jmin=BOUNDS(ng)%JstrR(tile)
          Jmax=BOUNDS(ng)%JendR(tile)
      END SELECT
#else
      tile=-1
      Imin=LBij
      Imax=UBij
      Jmin=LBij
      Jmax=UBij
#endif
      IorJ=IOBOUNDS(ng)%IorJ
      Npts=IorJ*4*Nrec
!
!  Get tile bounds.
!
      Istr=BOUNDS(ng)%Istr (tile)
      Iend=BOUNDS(ng)%Iend (tile)
      Jstr=BOUNDS(ng)%Jstr (tile)
      Jend=BOUNDS(ng)%Jend (tile)
!
!  Set switch to process boundary data by their associated tiles.
!
      bounded(iwest )=WESTERN_EDGE
      bounded(ieast )=EASTERN_EDGE
      bounded(isouth)=SOUTHERN_EDGE
      bounded(inorth)=NORTHERN_EDGE
!
!  Set NetCDF dimension counters for processing requested field.
!
      start(1)=1
      total(1)=IorJ
      start(2)=1
      total(2)=4
      start(3)=1
      total(3)=Nrec
      start(4)=tindex
      total(4)=1
!
!-----------------------------------------------------------------------
!  Pack and scale output data.
!-----------------------------------------------------------------------
!
      Aout=Aspv

      DO ir=1,Nrec
        rc=(ir-1)*IorJ*4
        DO ib=1,4
          IF (bounded(ib)) THEN
            bc=(ib-1)*IorJ+rc
            IF ((ib.eq.iwest).or.(ib.eq.ieast)) THEN
              DO j=Jmin,Jmax
                ic=1+(j-LBij)+bc
                Aout(ic)=A(j,ib,ir)*Ascl
              END DO
            ELSE IF ((ib.eq.isouth).or.(ib.eq.inorth)) THEN
              DO i=Imin,Imax
                ic=1+(i-LBij)+bc
                Aout(ic)=A(i,ib,ir)*Ascl
              END DO
            END IF
          END IF
        END DO
      END DO
  
#ifdef DISTRIBUTE
!
!  If distributed-memory set-up, collect data from all spawned
!  nodes.
!
      CALL mp_collect (ng, model, Npts, Aspv, Aout)
#endif
!
!-----------------------------------------------------------------------
!  Write output buffer into NetCDF file.
!-----------------------------------------------------------------------
!
      status=nf90_noerr
      IF (OutThread) THEN
        status=nf90_put_var(ncid, ncvarid, Aout, start, total)
      END IF

#ifdef DISTRIBUTE
!
!-----------------------------------------------------------------------
!  Broadcast IO error flag to all nodes.
!-----------------------------------------------------------------------
!
      CALL mp_bcasti (ng, model, status)
#endif

      RETURN
      END FUNCTION nf_fwrite2d_bry

      END MODULE nf_fwrite2d_bry_mod
