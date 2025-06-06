#include "cppdefs.h"
#if defined PROPAGATOR && defined CHECKPOINTING
      SUBROUTINE get_gst (ng, model)
!
!svn $Id: get_gst.F 294 2009-01-09 21:37:26Z arango $
!================================================== Hernan G. Arango ===
!  Copyright (c) 2002-2009 The ROMS/TOMS Group                         !
!    Licensed under a MIT/X style license                              !
!    See License_ROMS.txt                                              !
!=======================================================================
!                                                                      !
!  This routine reads in GST checkpointing restart NetCDF file.        !
!                                                                      !
!=======================================================================
!
      USE mod_param
      USE mod_parallel
      USE mod_iounits
      USE mod_ncparam
      USE mod_netcdf
      USE mod_scalars
      USE mod_storage

# ifdef DISTRIBUTE
!
      USE distribute_mod, ONLY : mp_bcasti
      USE distribute_mod, ONLY : mp_ncread
# endif
!
      implicit none
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, model
!
!  Local variable declarations.
!
      integer :: i, ivar, status, tile

# ifdef DISTRIBUTE
      integer :: vrecord = -1

      real(r8) :: scale =1.0_r8
# endif
      real(r8) :: rval

      character (len=1 ) :: char1, lchar(5)
      character (len=2 ) :: char2
      character (len=80) :: ncname
!
      SourceFile='get_gst.F'
!
!-----------------------------------------------------------------------
!  Read GST checkpointing restart variables.  Check for consistency.
!-----------------------------------------------------------------------
!
!  Open checkpointing NetCDF file for reading and writing.
!
      ncname=GSTname(ng)
      IF (ncGSTid(ng).eq.-1) THEN
        CALL netcdf_open (ng, model, ncname, 1, ncGSTid(ng))
        IF (exit_flag.ne.NoError) THEN
          WRITE (stdout,10) TRIM(ncname)
          RETURN
        END IF
      END IF
!
!  Read in number of eigenvalues to compute.
!
      CALL netcdf_get_ivar (ng, model, ncname, 'NEV', ivar,             &
     &                      ncid = ncGSTid(ng))
      IF (exit_flag.ne. NoError) RETURN
      IF (ivar.ne.NEV) THEN
        IF (Master) WRITE (stdout,20) ', NEV = ', ivar, NEV
        exit_flag=6
        RETURN
      END IF
!
!  Read in number of Lanczos vectors to compute.
!
      CALL netcdf_get_ivar (ng, model, ncname, 'NCV', ivar,             &
     &                      ncid = ncGSTid(ng))
      IF (exit_flag.ne. NoError) RETURN
      IF (ivar.ne.NCV) THEN
        IF (Master)  WRITE (stdout,20) ', NCV = ', ivar, NCV
        exit_flag=6
        RETURN
      END IF
!
!  Read in size of the eigenvalue problem.
!
      CALL netcdf_get_ivar (ng, model, ncname, 'Mstate', ivar,          &
     &                      ncid = ncGSTid(ng))
      IF (exit_flag.ne. NoError) RETURN
      IF (ivar.ne.Mstate(ng)) THEN
        IF (Master) WRITE (stdout,20) ', Mstate = ', ivar, Mstate(ng)
        exit_flag=6
        RETURN
      END IF

# ifdef DISTRIBUTE
!
!  Read in number of Lanczos vectors to compute.
!
      CALL netcdf_get_ivar (ng, model, ncname, 'Nnodes', ivar,          &
     &                      ncid = ncGSTid(ng))
      IF (exit_flag.ne. NoError) RETURN
      IF (ivar.ne.numnodes) THEN
        IF (Master) WRITE (stdout,20) ', Nnodes = ', ivar, numnodes
        exit_flag=6
        RETURN
      END IF
# endif
!
!  Read in iteration number.
!
      CALL netcdf_get_ivar (ng, model, ncname, 'iter', Nrun,            &
     &                      ncid = ncGSTid(ng))
      IF (exit_flag.ne. NoError) RETURN
!
!  Read in reverse communications flag.
!
      CALL netcdf_get_ivar (ng, model, ncname, 'ido', ido,              &
     &                      ncid = ncGSTid(ng))
      IF (exit_flag.ne. NoError) RETURN
!
!  Read in information and error flag.
!
      CALL netcdf_get_ivar (ng, model, ncname, 'info', ido,             &
     &                      ncid = ncGSTid(ng))
      IF (exit_flag.ne. NoError) RETURN
!
!  Read in eigenvalue problem type.
!
      CALL netcdf_get_svar (ng, model, ncname, 'bmat', char1,           &
     &                      ncid = ncGSTid(ng),                         &
     &                      start = (/1/), total = (/1/))
      IF (exit_flag.ne. NoError) RETURN
      IF (char1.ne.bmat) THEN
        IF (Master) WRITE (stdout,30) ', bmat = ', char1, bmat
        exit_flag=6
        RETURN
      END IF
!
!  Read in Ritz eigenvalues to compute.
!
      CALL netcdf_get_svar (ng, model, ncname, 'which', char2,          &
     &                      ncid = ncGSTid(ng),                         &
     &                      start = (/1/), total = (/2/))
      IF (exit_flag.ne. NoError) RETURN
      IF (char2(1:2).ne.which(1:2)) THEN
        IF (Master) WRITE (stdout,30) ', which = ', char2, which
        exit_flag=6
        RETURN
      END IF
!
!  Read in form of basis function.
!
      CALL netcdf_get_svar (ng, model, ncname, 'howmany', char1,        &
     &                      ncid = ncGSTid(ng),                         &
     &                      start = (/1/), total = (/1/))
      IF (exit_flag.ne. NoError) RETURN
      IF (char1.ne.howmany) THEN
        IF (Master) WRITE (stdout,30) ', howmany = ', char1, howmany
        exit_flag=6
        RETURN
      END IF
!
!  Read in relative accuracy of computed Ritz values.
!
      CALL netcdf_get_fvar (ng, model, ncname, 'Ritz_tol', rval,        &
     &                      ncid = ncGSTid(ng))
      IF (exit_flag.ne. NoError) RETURN
      IF (rval.ne.Ritz_tol) THEN
        IF (Master) WRITE (stdout,40) ', Ritz_tol = ', rval, Ritz_tol
      END IF
      Ritz_tol=rval
!
!  Read in eigenproblem parameters.
!
      CALL netcdf_get_ivar (ng, model, ncname, 'iparam', iparam,        &
     &                      ncid = ncGSTid(ng),                         &
     &                      start = (/1/), total = (/SIZE(iparam)/))
      IF (exit_flag.ne. NoError) RETURN
!
!  Read in pointers to mark starting location in work arrays.
!
      CALL netcdf_get_ivar (ng, model, ncname, 'ipntr', ipntr,          &
     &                      ncid = ncGSTid(ng),                         &
     &                      start = (/1/), total = (/SIZE(ipntr)/))
      IF (exit_flag.ne. NoError) RETURN
!
!  Read in ARPACK internal integer parameters to _aupd routines.
!
      CALL netcdf_get_ivar (ng, model, ncname, 'iaupd', iaupd,          &
     &                      ncid = ncGSTid(ng),                         &
     &                      start = (/1/), total = (/SIZE(iaupd)/))
      IF (exit_flag.ne. NoError) RETURN
!
!  Read in ARPACK internal integer parameters to _aitr routines.
!
      CALL netcdf_get_ivar (ng, model, ncname, 'iaitr', iaitr,          &
     &                      ncid = ncGSTid(ng),                         &
     &                      start = (/1/), total = (/SIZE(iaitr)/))
      IF (exit_flag.ne. NoError) RETURN
!
!  Read in ARPACK internal integer parameters to _aup2 routines.
!
      CALL netcdf_get_ivar (ng, model, ncname, 'iaup2', iaup2,          &
     &                      ncid = ncGSTid(ng),                         &
     &                      start = (/1/), total = (/SIZE(iaup2)/))
      IF (exit_flag.ne. NoError) RETURN
!
!  Read in ARPACK internal logical parameters to _aup2 routines.
!
      CALL netcdf_get_svar (ng, model, ncname, 'laitr', lchar,          &
     &                      ncid = ncGSTid(ng),                         &
     &                      start = (/1,1/), total = (/1,SIZE(laitr)/))
      IF (exit_flag.ne. NoError) RETURN
      DO i=1,SIZE(laitr)
        IF (lchar(i).eq.'T') THEN
          laitr(i)=.TRUE.
        ELSE
          laitr(i)=.FALSE.
        END IF
      END DO
!
!  Read in ARPACK internal logical parameters to _aup2 routines.
!
      CALL netcdf_get_svar (ng, model, ncname, 'laup2', lchar,          &
     &                      ncid = ncGSTid(ng),                         &
     &                      start = (/1,1/), total = (/1,SIZE(laup2)/))
      IF (exit_flag.ne. NoError) RETURN
      DO i=1,SIZE(laup2)
        IF (lchar(i).eq.'T') THEN
          laup2(i)=.TRUE.
        ELSE
          laup2(i)=.FALSE.
        END IF
      END DO
!
!  Read in ARPACK internal real parameters to _aup2 routines.
!
      CALL netcdf_get_fvar (ng, model, ncname, 'raitr', raitr,          &
     &                      ncid = ncGSTid(ng),                         &
     &                      start = (/1/), total = (/SIZE(raitr)/))
      IF (exit_flag.ne. NoError) RETURN
!
!  Read in ARPACK internal real parameters to _aup2 routines.
!
      CALL netcdf_get_fvar (ng, model, ncname, 'raup2', raup2,          &
     &                      ncid = ncGSTid(ng),                         &
     &                      start = (/1/), total = (/SIZE(raup2)/))
      IF (exit_flag.ne. NoError) RETURN
!
!-----------------------------------------------------------------------
!  Read in checkpointing variables associated with the state vector.
!-----------------------------------------------------------------------
!
!  Read in Lanczos/Arnoldi basis vectors.
!
# ifdef DISTRIBUTE
      status=mp_ncread(ng, model, ncGSTid(ng), 'Bvec', TRIM(ncname),    &
     &                 vrecord, Nstr(ng), Nend(ng), 1, NCV, scale,      &
     &                 Bvec(Nstr(ng):,1))
# else
      CALL netcdf_get_fvar (ng, model, ncname, 'Bvec',                  &
     &                      Bvec(Nstr(ng):,1),                          &
     &                      ncid = ncGSTid(ng),                         &
     &                      start = (/Nstr(ng),1/),                     &
     &                      total = (/Nend(ng)-Nstr(ng)+1,NCV/))
# endif
      IF (exit_flag.ne.NoError) RETURN
!
!  Read in eigenproblem residual vector.
!
# ifdef DISTRIBUTE
      status=mp_ncread(ng, model, ncGSTid(ng), 'resid', TRIM(ncname),   &
     &                 vrecord, Nstr(ng), Nend(ng), 1, 1, scale,        &
     &                 resid(Nstr(ng):))
# else
      CALL netcdf_get_fvar (ng, model, ncname, 'resid',                 &
     &                      resid(Nstr(ng):),                           &
     &                      ncid = ncGSTid(ng),                         &
     &                      start = (/Nstr(ng)/),                       &
     &                      total = (/Nend(ng)-Nstr(ng)+1/))
# endif
      IF (exit_flag.ne.NoError) RETURN
!
!  Read in state reverse communication work array.
# ifdef DISTRIBUTE
!  Notice zero arguments indicating node dimension in NetCDF file.
# endif
!
# ifdef DISTRIBUTE
      status=mp_ncread(ng, model, ncGSTid(ng), 'SworkD', TRIM(ncname),  &
     &                 vrecord, 1, 3*Nstate(ng), 0, 0, scale,           &
     &                 SworkD)
# else
      CALL netcdf_get_fvar (ng, model, ncname, 'SworkD',                &
     &                      SworkD,                                     &
     &                      ncid = ncGSTid(ng),                         &
     &                      start = (/1/),                              &
     &                      total = (/3*Nstate(ng)/))
# endif
      IF (exit_flag.ne.NoError) RETURN
!
!  Read in eigenproblem work array.
# ifdef DISTRIBUTE
!  Notice zero arguments indicating node dimension in NetCDF file.
# endif
!
# ifdef DISTRIBUTE
      status=mp_ncread(ng, model, ncGSTid(ng), 'SworkL', TRIM(ncname),  &
     &                 vrecord, 1, LworkL, 0, 0, scale,                 &
     &                 SworkL)
# else
      CALL netcdf_get_fvar (ng, model, ncname, 'SworkL',                &
     &                      SworkL,                                     &
     &                      ncid = ncGSTid(ng),                         &
     &                      start = (/1/),                              &
     &                      total = (/LworkL/))
# endif
      IF (exit_flag.ne.NoError) RETURN
!
  10  FORMAT (/,' GET_GST - unable to open checkpointing NetCDF',       &
     &          ' file:', a)
  20  FORMAT (/,' GET_GST - inconsistent input parameter', a, 2i4)
  30  FORMAT (/,' GET_GST - inconsistent input parameter', a, a, a)
  40  FORMAT (/,' GET_GST - input parameter', a, 1pe10.4,0p,            &
     &        /, 11x,'has been reset to: ', 1pe10.4)

      RETURN
      END SUBROUTINE get_gst
#else
      SUBROUTINE get_gst
      RETURN
      END SUBROUTINE get_gst
#endif

