#include "cppdefs.h"
#if defined WEAK_CONSTRAINT   && \
   (defined POSTERIOR_ERROR_F || defined POSTERIOR_ERROR_I)
      SUBROUTINE wrt_error (ng, kout, nout)
!
!svn $Id: wrt_error.F 366 2009-07-09 04:41:00Z arango $
!================================================== Hernan G. Arango ===
!  Copyright (c) 2002-2009 The ROMS/TOMS Group                         !
!    Licensed under a MIT/X style license                              !
!    See License_ROMS.txt                                              !
!=======================================================================
!                                                                      !
!  This routine writes full posterior error covariance (diagonal) x    !
!  matrix for weak constraint 4DVar data assimilation.                 !
!                                                                      !
!=======================================================================
!
      USE mod_param
      USE mod_parallel
# ifdef ADJUST_BOUNDARY
      USE mod_boundary
# endif
      USE mod_forces
      USE mod_fourdvar
      USE mod_grid
      USE mod_iounits
      USE mod_mixing
      USE mod_ncparam
      USE mod_netcdf
      USE mod_ocean
      USE mod_scalars
      USE mod_stepping
!
      USE nf_fwrite2d_mod, ONLY : nf_fwrite2d
# ifdef ADJUST_BOUNDARY
      USE nf_fwrite2d_bry_mod, ONLY : nf_fwrite2d_bry
# endif
# ifdef SOLVE3D
      USE nf_fwrite3d_mod, ONLY : nf_fwrite3d
#  ifdef ADJUST_BOUNDARY
      USE nf_fwrite3d_bry_mod, ONLY : nf_fwrite3d_bry
#  endif
# endif
!
      implicit none
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, kout, nout
!
!  Local variable declarations.
!
      integer :: LBi, UBi, LBj, UBj
# ifdef ADJUST_BOUNDARY
      integer :: LBij, UBij
# endif
      integer :: i, j, gfactor, gtype, status
# ifdef SOLVE3D
      integer :: itrc, k
# endif
      real(r8) :: scale
!
      SourceFile='wrt_hessian.F'
!
      LBi=LBOUND(GRID(ng)%h,DIM=1)
      UBi=UBOUND(GRID(ng)%h,DIM=1)
      LBj=LBOUND(GRID(ng)%h,DIM=2)
      UBj=UBOUND(GRID(ng)%h,DIM=2)
# ifdef ADJUST_BOUNDARY
      LBij=BOUNDS(ng)%LBij
      UBij=BOUNDS(ng)%UBij
# endif
!
!-----------------------------------------------------------------------
!  Write out full posterior error covariance (diagonal) matrix.
!-----------------------------------------------------------------------
!
      IF (exit_flag.ne.NoError) RETURN
!
!  Set grid type factor to write full (gfactor=1) fields or water
!  points (gfactor=-1) fields only.
!
# if defined WRITE_WATER && defined MASKING
      gfactor=-1
# else
      gfactor=1
# endif
!
!  Set time record index.
!
      tERRindx(ng)=tERRindx(ng)+1
      NrecERR(ng)=NrecERR(ng)+1
!
!  Write out model time (s).
!
      CALL netcdf_put_fvar (ng, iTLM, ERRname(ng),                      &
     &                      TRIM(Vname(1,idtime)), time(ng:),           &
     &                      (/tERRindx(ng)/), (/1/),                    &
     &                      ncid = ncERRid(ng),                         &
     &                      varid = errVid(idtime,ng))
      IF (exit_flag.ne.NoError) RETURN
!
!  Write out inner-loop Lanczos vectors tridiagonal system.
!
      CALL netcdf_put_fvar (ng, iTLM, ERRname(ng), 'zLanczos_coef',     &
     &                      zLanczos_coef, (/1,1/), (/Ninner,Ninner/),  &
     &                      ncid = ncERRid(ng))
      IF (exit_flag.ne.NoError) RETURN

      CALL netcdf_put_fvar (ng, iTLM, ERRname(ng), 'zLanczos_inv',      &
     &                      zLanczos_inv, (/1,1/), (/Ninner,Ninner/),   &
     &                      ncid = ncERRid(ng))
      IF (exit_flag.ne.NoError) RETURN

      CALL netcdf_put_fvar (ng, iTLM, ERRname(ng), 'zLanczos_err',      &
     &                      zLanczos_err, (/1,1/), (/Ninner,Ninner/),   &
     &                      ncid = ncERRid(ng))
      IF (exit_flag.ne.NoError) RETURN
!
!  Write out free-surface error variance.
!
      scale=1.0_r8
      gtype=gfactor*r2dvar
      status=nf_fwrite2d(ng, iTLM, ncERRid(ng), errVid(idFsur,ng),      &
     &                   tERRindx(ng), gtype,                           &
     &                   LBi, UBi, LBj, UBj, scale,                     &
# ifdef MASKING
     &                   GRID(ng) % rmask,                              &
# endif
     &                   OCEAN(ng)% tl_zeta(:,:,kout))
      IF (status.ne.nf90_noerr) THEN
        IF (Master) THEN
          WRITE (stdout,10) TRIM(Vname(1,idFsur)), tERRindx(ng)
        END IF
        exit_flag=3
        ioerror=status
        RETURN
      END IF

# ifdef ADJUST_BOUNDARY
!
!  Write out free-surface open boundaries error variance.
!
      IF (ANY(Lobc(:,isFsur,ng))) THEN
        scale=1.0_r8
        status=nf_fwrite2d_bry (ng, iTLM, ERRname(ng), ncERRid(ng),     &
     &                          Vname(1,idSbry(isFsur)),                &
     &                          errVid(idSbry(isFsur),ng),              &
     &                          tERRindx(ng), r2dvar,                   &
     &                          LBij, UBij, Nbrec(ng), scale,           &
     &                          BOUNDARY(ng) % tl_zeta_obc(LBij:,:,:,   &
     &                                                     kout))
        IF (status.ne.nf90_noerr) THEN
          IF (Master) THEN
            WRITE (stdout,10) TRIM(Vname(1,idSbry(isFsur))),            &
     &                        tERRindx(ng)
          END IF
          exit_flag=3
          ioerror=status
          RETURN
        END IF
      END IF
# endif
!
!  Write out 2D U-momentum component error variance.
!
      scale=1.0_r8
      gtype=gfactor*u2dvar
      status=nf_fwrite2d(ng, iTLM, ncERRid(ng), errVid(idUbar,ng),      &
     &                   tERRindx(ng), gtype,                           &
     &                   LBi, UBi, LBj, UBj, scale,                     &
# ifdef MASKING
     &                   GRID(ng) % umask,                              &
# endif
     &                   OCEAN(ng) % tl_ubar(:,:,kout))
      IF (status.ne.nf90_noerr) THEN
        IF (Master) THEN
          WRITE (stdout,10) TRIM(Vname(1,idUbar)), tERRindx(ng)
        END IF
        exit_flag=3
        ioerror=status
        RETURN
      END IF

# ifdef ADJUST_BOUNDARY
!
!  Write out 2D U-momentum component open boundaries error variance.
!
      IF (ANY(Lobc(:,isUbar,ng))) THEN
        scale=1.0_r8
        status=nf_fwrite2d_bry (ng, iTLM, ERRname(ng), ncERRid(ng),     &
     &                          Vname(1,idSbry(isUbar)),                &
     &                          errVid(idSbry(isUbar),ng),              &
     &                          tERRindx(ng), u2dvar,                   &
     &                          LBij, UBij, Nbrec(ng), scale,           &
     &                          BOUNDARY(ng) % tl_ubar_obc(LBij:,:,:,   &
     &                                                     kout))
        IF (status.ne.nf90_noerr) THEN
          IF (Master) THEN
            WRITE (stdout,10) TRIM(Vname(1,idSbry(isUbar))),            &
     &                        tERRindx(ng)
          END IF
          exit_flag=3
          ioerror=status
          RETURN
        END IF
      END IF
# endif
!
!  Write out 2D V-momentum component error variance.
!
      scale=1.0_r8
      gtype=gfactor*v2dvar
      status=nf_fwrite2d(ng, iTLM, ncERRid(ng), errVid(idVbar,ng),      &
     &                   tERRindx(ng), gtype,                           &
     &                   LBi, UBi, LBj, UBj, scale,                     &
# ifdef MASKING
      &                  GRID(ng) % vmask,                              &
# endif
     &                   OCEAN(ng) % tl_vbar(:,:,kout))
      IF (status.ne.nf90_noerr) THEN
        IF (Master) THEN
          WRITE (stdout,10) TRIM(Vname(1,idVbar)), tERRindx(ng)
        END IF
        exit_flag=3
        ioerror=status
        RETURN
      END IF

# ifdef ADJUST_BOUNDARY
!
!  Write out 2D V-momentum component open boundaries error variance.
!
      IF (ANY(Lobc(:,isVbar,ng))) THEN
        scale=1.0_r8
        status=nf_fwrite2d_bry (ng, iTLM, ERRname(ng), ncERRid(ng),     &
     &                          Vname(1,idSbry(isVbar)),                &
     &                          errVid(idSbry(isVbar),ng),              &
     &                          tERRindx(ng), v2dvar,                   &
     &                          LBij, UBij, Nbrec(ng), scale,           &
     &                          BOUNDARY(ng) % tl_vbar_obc(LBij:,:,:,   &
     &                                                     kout))
        IF (status.ne.nf90_noerr) THEN
          IF (Master) THEN
            WRITE (stdout,10) TRIM(Vname(1,idSbry(isVbar))),            &
     &                        tERRindx(ng)
          END IF
          exit_flag=3
          ioerror=status
          RETURN
        END IF
      END IF
# endif

# ifdef SOLVE3D
!
!  Write out 3D U-momentum component error variance.
!
      scale=1.0_r8
      gtype=gfactor*u3dvar
      status=nf_fwrite3d(ng, iTLM, ncERRid(ng), errVid(idUvel,ng),      &
     &                   tERRindx(ng), gtype,                           &
     &                   LBi, UBi, LBj, UBj, 1, N(ng), scale,           &
#  ifdef MASKING
     &                   GRID(ng) % umask,                              &
#  endif
     &                   OCEAN(ng) % tl_u(:,:,:,nout))
      IF (status.ne.nf90_noerr) THEN
        IF (Master) THEN
          WRITE (stdout,10) TRIM(Vname(1,idUvel)), tERRindx(ng)
        END IF
        exit_flag=3
        ioerror=status
        RETURN
      END IF

#  ifdef ADJUST_BOUNDARY
!
!  Write out 3D U-momentum component open boundaries error variance.
!
      IF (ANY(Lobc(:,isUvel,ng))) THEN
        scale=1.0_r8
        status=nf_fwrite3d_bry (ng, iTLM, ERRname(ng), ncERRid(ng),     &
     &                          Vname(1,idSbry(isUvel)),                &
     &                          errVid(idSbry(isUvel),ng),              &
     &                          tERRindx(ng), u3dvar,                   &
     &                          LBij, UBij, 1, N(ng), Nbrec(ng), scale, &
     &                          BOUNDARY(ng) % tl_u_obc(LBij:,:,:,:,    &
     &                                                  nout))
        IF (status.ne.nf90_noerr) THEN
          IF (Master) THEN
            WRITE (stdout,10) TRIM(Vname(1,idSbry(isUvel))),            &
     &                        tERRindx(ng)
          END IF
          exit_flag=3
          ioerror=status
          RETURN
        END IF
      END IF
#  endif
!
!  Write out 3D V-momentum component error variance.
!
      scale=1.0_r8
      gtype=gfactor*v3dvar
      status=nf_fwrite3d(ng, iTLM, ncERRid(ng), errVid(idVvel,ng),      &
     &                   tERRindx(ng), gtype,                           &
     &                   LBi, UBi, LBj, UBj, 1, N(ng), scale,           &
#  ifdef MASKING
     &                   GRID(ng) % vmask,                              &
#  endif
     &                   OCEAN(ng) % tl_v(:,:,:,nout))
      IF (status.ne.nf90_noerr) THEN
        IF (Master) THEN
          WRITE (stdout,10) TRIM(Vname(1,idVvel)), tERRindx(ng)
        END IF
        exit_flag=3
        ioerror=status
        RETURN
      END IF

#  ifdef ADJUST_BOUNDARY
!
!  Write out 3D V-momentum component open boundaries error variance.
!
      IF (ANY(Lobc(:,isVvel,ng))) THEN
        scale=1.0_r8
        status=nf_fwrite3d_bry (ng, iTLM, ERRname(ng), ncERRid(ng),     &
     &                          Vname(1,idSbry(isVvel)),                &
     &                          errVid(idSbry(isVvel),ng),              &
     &                          tERRindx(ng), v3dvar,                   &
     &                          LBij, UBij, 1, N(ng), Nbrec(ng), scale, &
     &                          BOUNDARY(ng) % tl_v_obc(LBij:,:,:,:,    &
     &                                                  nout))
        IF (status.ne.nf90_noerr) THEN
          IF (Master) THEN
            WRITE (stdout,10) TRIM(Vname(1,idSbry(isVvel))),            &
     &                        tERRindx(ng)
          END IF
          exit_flag=3
          ioerror=status
          RETURN
        END IF
      END IF
#  endif
!
!  Write out tracer type variables error variance.
!
      DO itrc=1,NT(ng)
        scale=1.0_r8
        gtype=gfactor*r3dvar
        status=nf_fwrite3d(ng, iTLM, ncERRid(ng), errTid(itrc,ng),      &
     &                     tERRindx(ng), gtype,                         &
     &                     LBi, UBi, LBj, UBj, 1, N(ng), scale,         &
#  ifdef MASKING
     &                     GRID(ng) % rmask,                            &
#  endif
     &                     OCEAN(ng) % tl_t(:,:,:,nout,itrc))
        IF (status.ne.nf90_noerr) THEN
          IF (Master) THEN
            WRITE (stdout,10) TRIM(Vname(1,idTvar(itrc))),              &
     &                        tERRindx(ng)
          END IF
          exit_flag=3
          ioerror=status
          RETURN
        END IF
      END DO

#  ifdef ADJUST_BOUNDARY
!
!  Write out tracers open boundaries error variance.
!
      DO itrc=1,NT(ng)
        IF (ANY(Lobc(:,isTvar(itrc),ng))) THEN
          scale=1.0_r8
          status=nf_fwrite3d_bry (ng, iTLM, ERRname(ng), ncERRid(ng),   &
     &                            Vname(1,idSbry(isTvar(itrc))),        &
     &                            errVid(idSbry(isTvar(itrc)),ng),      &
     &                            tERRindx(ng), r3dvar,                 &
     &                            LBij, UBij, 1, N(ng), Nbrec(ng),      &
     &                            scale,                                &
     &                            BOUNDARY(ng) % tl_t_obc(LBij:,:,:,:,  &
     &                                                    nout,itrc))
          IF (status.ne.nf90_noerr) THEN
            IF (Master) THEN
              WRITE (stdout,10) TRIM(Vname(1,idSbry(isTvar(itrc)))),    &
     &                          tERRindx(ng)
            END IF
            exit_flag=3
            ioerror=status
            RETURN
          END IF
        END IF
      END DO
#  endif

#  ifdef ADJUST_STFLUX
!
!  Write out surface net tracers fluxes error variance. Notice that
!  fluxes have their own fixed time-dimension (of size Nfrec) to allow
!  4DVar adjustments at other times in addition to initialization time.
!
      DO itrc=1,NT(ng)
        IF (Lstflux(itrc,ng)) THEN
          scale=1.0_r8
          gtype=gfactor*r3dvar
          status=nf_fwrite3d(ng, iTLM, ncERRid(ng),                     &
     &                       errVid(idTsur(itrc),ng),                   &
     &                       tERRindx(ng), gtype,                       &
     &                       LBi, UBi, LBj, UBj, 1, Nfrec(ng), scale,   &
#   ifdef MASKING
     &                       GRID(ng) % rmask,                          &
#   endif
     &                       FORCES(ng) % tl_tflux(:,:,:,kout,itrc))
          IF (status.ne.nf90_noerr) THEN
            IF (Master) THEN
              WRITE (stdout,10) TRIM(Vname(1,idTsur(itrc))),            &
     &                          tERRindx(ng)
            END IF
            exit_flag=3
            ioerror=status
            RETURN
          END IF
        END IF
      END DO
#  endif
# endif
# ifdef ADJUST_WSTRESS
!
!  Write out surface U-momentum stress error variance.  Notice that the
!  stress has its own fixed time-dimension (of size Nfrec) to allow
!  4DVar adjustments at other times in addition to initialization time.
!
      scale=1.0_r8
      gtype=gfactor*u3dvar
      status=nf_fwrite3d(ng, iTLM, ncERRid(ng), errVid(idUsms,ng),      &
     &                   tERRindx(ng), gtype,                           &
     &                   LBi, UBi, LBj, UBj, 1, Nfrec(ng), scale,       &
#  ifdef MASKING
     &                   GRID(ng) % umask,                              &
#  endif
     &                   FORCES(ng) % tl_ustr(:,:,:,kout))
      IF (status.ne.nf90_noerr) THEN
        IF (Master) THEN
          WRITE (stdout,10) TRIM(Vname(1,idUsms)), tERRindx(ng)
        END IF
        exit_flag=3
        ioerror=status
        RETURN
      END IF
!
!  Write out surface V-momentum stress error variance.
!
      scale=1.0_r8
      gtype=gfactor*v3dvar
      status=nf_fwrite3d(ng, iTLM, ncERRid(ng), errVid(idVsms,ng),      &
     &                   tERRindx(ng), gtype,                           &
     &                   LBi, UBi, LBj, UBj, 1, Nfrec(ng), scale,       &
#  ifdef MASKING
     &                   GRID(ng) % vmask,                              &
#  endif
     &                   FORCES(ng) % tl_vstr(:,:,:,kout))
      IF (status.ne.nf90_noerr) THEN
        IF (Master) THEN
          WRITE (stdout,10) TRIM(Vname(1,idVsms)), tERRindx(ng)
        END IF
        exit_flag=3
        ioerror=status
        RETURN
      END IF
# endif
!
!-----------------------------------------------------------------------
!  Synchronize posterior error covariance NetCDF file to disk to allow
!  other processes to access data immediately after it is written.
!-----------------------------------------------------------------------
!
      CALL netcdf_sync (ng, iTLM, ERRname(ng), ncERRid(ng))
      IF (exit_flag.ne.NoError) RETURN

# ifdef SOLVE3D
      IF (Master) WRITE (stdout,20) kout, nout, tERRindx(ng)
# else
      IF (Master) WRITE (stdout,20) kout, tERRindx(ng)
# endif
!
  10  FORMAT (/,' WRT_ERROR - error while writing variable: ',a,/,      &
     &        15x,'into 4DVar error NetCDF file for time record: ',i4)
# ifdef SOLVE3D
  20  FORMAT (3x,'WRT_ERROR    - wrote error    fields (Index=', i1,    &
     &        ',',i1,') into time record = ',i7.7)
# else
  20  FORMAT (3x,'WRT_ERROR    - wrote error    fields (Index=', i1,    &
     &        ') into time record = ',i7.7)
# endif
      RETURN
      END SUBROUTINE wrt_error
#else
      SUBROUTINE wrt_error
      RETURN
      END SUBROUTINE wrt_error
#endif
