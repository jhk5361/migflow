



























































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































      MODULE mod_grid
!
!svn $Id: mod_grid.F 352 2009-05-29 20:57:39Z arango $
!================================================== Hernan G. Arango ===
!  Copyright (c) 2002-2009 The ROMS/TOMS Group                         !
!    Licensed under a MIT/X style license                              !
!    See License_ROMS.txt                                              !
!=======================================================================
!                                                                      !
!  IJwaterR   Water points IJ couter for RHO-points masked variables.  !
!  IJwaterU   Water points IJ couter for   U-points masked variables.  !
!  IJwaterV   Water points IJ couter for   V-points masked variables.  !
!  Hz         Thicknesses (m) of vertical RHO-points.                  !
!  Huon       Total U-momentum flux term, Hz*u/pn.                     !
!  Hvom       Total V-momentum flux term, Hz*v/pm.                     !
!  IcePress   Pressure under the ice shelf at RHO-points.              !
!  Rscope     Adjoint sensitivity spatial scope mask at RHO-points.    !
!  Tcline     Width (m) of surface or bottom boundary layer where      !
!               higher vertical resolution is required during          !
!               stretching.                                            !
!  Uscope     Adjoint sensitivity spatial scope mask at U-points.      !
!  Vscope     Adjoint sensitivity spatial scope mask at V-points.      !
!  angler     Angle (radians) between XI-axis and true EAST at         !
!               RHO-points.                                            !
!  CosAngler  Cosine of curvilinear angle, angler.                     !
!  SinAngler  Sine of curvilinear angle, angler.                       !
!  bed_thick0 Sum all initial bed layer thicknesses (m)                !
!  bed_thick  Instantaneous total bed thickness, 2 time levels (m)     !
!  dmde       ETA-derivative of inverse metric factor pm,              !
!               d(1/pm)/d(ETA).                                        !
!  dndx       XI-derivative  of inverse metric factor pn,              !
!               d(1/pn)/d(XI).                                         !
!  f          Coriolis parameter (1/s).                                !
!  fomn       Compound term, f/(pm*pn) at RHO points.                  !
!  grdscl     Grid scale used to adjust horizontal mixing according    !
!               to grid area.                                          !
!  h          Bottom depth (m) at RHO-points.                          !
!  hinv       Inverse of Bottom depth (1/m) at RHO-points.             !
!  latp       Latitude (degrees_north) at PSI-points.                  !
!  latr       Latitude (degrees_north) at RHO-points.                  !
!  latu       Latitude (degrees_north) at U-points.                    !
!  latv       Latitude (degrees_north) at V-points.                    !
!  lonp       Longitude (degrees_east) at PSI-points.                  !
!  lonr       Longitude (degrees_east) at RHO-points.                  !
!  lonu       Longitude (degrees_east) at U-points.                    !
!  lonv       Longitude (degrees_east) at V-points.                    !
!  omm        RHO-grid area (meters2).                                 !
!  om_p       PSI-grid spacing (meters) in the XI-direction.           !
!  om_r       RHO-grid spacing (meters) in the XI-direction.           !
!  om_u       U-grid spacing (meters) in the XI-direction.             !
!  om_v       V-grid spacing (meters) in the XI-direction.             !
!  on_p       PSI-grid spacing (meters) in the ETA-direction.          !
!  on_r       RHO-grid spacing (meters) in the ETA-direction.          !
!  on_u       U-grid spacing (meters) in the ETA-direction.            !
!  on_v       V-grid spacing (meters) in the ETA-direction.            !
!  pm         Coordinate transformation metric "m" (1/meters)          !
!               associated with the differential distances in XI.      !
!  pmask      Slipperiness mask at PSI-points:                         !
!               (0=Land, 1=Sea, 1-gamma2=boundary).                    !
!  pmon_p     Compound term, pm/pn at PSI-points.                      !
!  pmon_r     Compound term, pm/pn at RHO-points.                      !
!  pmon_u     Compound term, pm/pn at U-points.                        !
!  pmon_v     Compound term, pm/pn at V-points.                        !
!  pn         Coordinate transformation metric "n" (1/meters)          !
!               associated with the differential distances in ETA.     !
!  pnom_p     Compound term, pn/pm at PSI-points.                      !
!  pnom_r     Compound term, pn/pm at RHO-points.                      !
!  pnom_u     Compound term, pn/pm at U-points.                        !
!  pnom_v     Compound term, pn/pm at V-points.                        !
!  rmask      Mask at RHO-points (0=Land, 1=Sea).                      !
!  rmask_wet  Mask at RHO-points for wetting and drying (0=dry, 1=wet) !
!  umask      Mask at U-points (0=Land, 1=Sea).                        !
!  umask_wet  Mask at U-points for wetting and drying (0=dry, 1,2=wet) !
!  vmask      Mask at V-points (0=Land, 1=Sea).                        !
!  vmask_wet  Mask at V-points for wetting and drying (0=dry, 1,2=wet) !
!  xp         XI-coordinates (m) at PSI-points.                        !
!  xr         XI-coordinates (m) at RHO-points.                        !
!  xu         XI-coordinates (m) at U-points.                          !
!  xv         XI-coordinates (m) at V-points.                          !
!  yp         ETA-coordinates (m) at PSI-points.                       !
!  yr         ETA-coordinates (m) at RHO-points.                       !
!  yu         ETA-coordinates (m) at U-points.                         !
!  yv         ETA-coordinates (m) at V-points.                         !
!  zice       Depth of ice shelf cavity (m, negative) at               !
!               RHO-points.                                            !
!  z_r        Actual depths (m) at horizontal RHO-points and           !
!               vertical RHO-points.                                   !
!  z_w        Actual depths (m) at horizontal RHO-points and           !
!               vertical W-points.                                     !
!                                                                      !
!=======================================================================
!
        USE mod_kinds

        implicit none

        TYPE T_GRID
!
!  Nonlinear model state.
!

          real(r8), pointer :: angler(:,:)
          real(r8), pointer :: CosAngler(:,:)
          real(r8), pointer :: SinAngler(:,:)
          real(r8), pointer :: dmde(:,:)
          real(r8), pointer :: dndx(:,:)
          real(r8), pointer :: f(:,:)
          real(r8), pointer :: fomn(:,:)
          real(r8), pointer :: grdscl(:,:)
          real(r8), pointer :: h(:,:)
          real(r8), pointer :: hinv(:,:)
          real(r8), pointer :: latp(:,:)
          real(r8), pointer :: latr(:,:)
          real(r8), pointer :: latu(:,:)
          real(r8), pointer :: latv(:,:)
          real(r8), pointer :: lonp(:,:)
          real(r8), pointer :: lonr(:,:)
          real(r8), pointer :: lonu(:,:)
          real(r8), pointer :: lonv(:,:)
          real(r8), pointer :: omn(:,:)
          real(r8), pointer :: om_p(:,:)
          real(r8), pointer :: om_r(:,:)
          real(r8), pointer :: om_u(:,:)
          real(r8), pointer :: om_v(:,:)
          real(r8), pointer :: on_p(:,:)
          real(r8), pointer :: on_r(:,:)
          real(r8), pointer :: on_u(:,:)
          real(r8), pointer :: on_v(:,:)
          real(r8), pointer :: pm(:,:)
          real(r8), pointer :: pn(:,:)
          real(r8), pointer :: pmon_p(:,:)
          real(r8), pointer :: pmon_r(:,:)
          real(r8), pointer :: pmon_u(:,:)
          real(r8), pointer :: pmon_v(:,:)
          real(r8), pointer :: pnom_p(:,:)
          real(r8), pointer :: pnom_r(:,:)
          real(r8), pointer :: pnom_u(:,:)
          real(r8), pointer :: pnom_v(:,:)
          real(r8), pointer :: xp(:,:)
          real(r8), pointer :: xr(:,:)
          real(r8), pointer :: xu(:,:)
          real(r8), pointer :: xv(:,:)
          real(r8), pointer :: yp(:,:)
          real(r8), pointer :: yr(:,:)
          real(r8), pointer :: yu(:,:)
          real(r8), pointer :: yv(:,:)
          real(r8), pointer :: Hz(:,:,:)
          real(r8), pointer :: Huon(:,:,:)
          real(r8), pointer :: Hvom(:,:,:)
          real(r8), pointer :: z_r(:,:,:)
          real(r8), pointer :: z_w(:,:,:)

        END TYPE T_GRID

        TYPE (T_GRID), allocatable :: GRID(:)

      CONTAINS

      SUBROUTINE allocate_grid (ng, LBi, UBi, LBj, UBj, LBij, UBij)
!
!=======================================================================
!                                                                      !
!  This routine allocates all variables in the module for all nested   !
!  grids.                                                              !
!                                                                      !
!=======================================================================
!
      USE mod_param
!
!  Local variable declarations.
!
      integer, intent(in) :: ng, LBi, UBi, LBj, UBj, LBij, UBij
!
!-----------------------------------------------------------------------
!  Allocate and initialize module variables.
!-----------------------------------------------------------------------
!
      IF (ng.eq.1) allocate ( GRID(Ngrids) )
!
!  Nonlinear model state.
!

      allocate ( GRID(ng) % angler(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % CosAngler(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % SinAngler(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % dmde(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % dndx(LBi:UBi,LBj:UBj) )

      allocate ( GRID(ng) % f(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % fomn(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % grdscl(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % h(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % hinv(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % latp(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % latr(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % latu(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % latv(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % lonp(LBi:UBi,LBj:UBj))
      allocate ( GRID(ng) % lonr(LBi:UBi,LBj:UBj))
      allocate ( GRID(ng) % lonu(LBi:UBi,LBj:UBj))
      allocate ( GRID(ng) % lonv(LBi:UBi,LBj:UBj))
      allocate ( GRID(ng) % omn(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % om_p(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % om_r(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % om_u(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % om_v(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % on_p(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % on_r(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % on_u(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % on_v(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % pm(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % pn(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % pmon_p(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % pmon_r(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % pmon_u(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % pmon_v(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % pnom_p(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % pnom_r(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % pnom_u(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % pnom_v(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % xp(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % xr(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % xu(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % xv(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % yp(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % yr(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % yu(LBi:UBi,LBj:UBj) )
      allocate ( GRID(ng) % yv(LBi:UBi,LBj:UBj) )

      allocate ( GRID(ng) % Hz(LBi:UBi,LBj:UBj,N(ng)) )
      allocate ( GRID(ng) % Huon(LBi:UBi,LBj:UBj,N(ng)) )
      allocate ( GRID(ng) % Hvom(LBi:UBi,LBj:UBj,N(ng)) )
      allocate ( GRID(ng) % z_r(LBi:UBi,LBj:UBj,N(ng)) )
      allocate ( GRID(ng) % z_w(LBi:UBi,LBj:UBj,0:N(ng)) )








      RETURN
      END SUBROUTINE allocate_grid

      SUBROUTINE initialize_grid (ng, tile, model)
!
!=======================================================================
!                                                                      !
!  This routine initialize all variables in the module using first     !
!  touch distribution policy. In shared-memory configuration, this     !
!  operation actually performs propagation of the  "shared arrays"     !
!  across the cluster, unless another policy is specified to           !
!  override the default.                                               !
!                                                                      !
!=======================================================================
!
      USE mod_param
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile, model
!
!  Local variable declarations.
!
      integer :: Imin, Imax, Jmin, Jmax
      integer :: i, j
      integer :: k

      real(r8), parameter :: IniVal = 0.0_r8












 

!
!-----------------------------------------------------------------------
!  Set lower and upper tile bounds and staggered variables bounds for
!  this horizontal domain partition.  Notice that if tile=-1, it will
!  set the values for the global grid.
!-----------------------------------------------------------------------
!
      integer :: Istr, IstrR, IstrT, IstrU, Iend, IendR, IendT
      integer :: Jstr, JstrR, JstrT, JstrV, Jend, JendR, JendT
!
      Istr =BOUNDS(ng)%Istr (tile)
      IstrR=BOUNDS(ng)%IstrR(tile)
      IstrT=BOUNDS(ng)%IstrT(tile)
      IstrU=BOUNDS(ng)%IstrU(tile)
      Iend =BOUNDS(ng)%Iend (tile)
      IendR=BOUNDS(ng)%IendR(tile)
      IendT=BOUNDS(ng)%IendT(tile)

      Jstr =BOUNDS(ng)%Jstr (tile)
      JstrR=BOUNDS(ng)%JstrR(tile)
      JstrT=BOUNDS(ng)%JstrT(tile)
      JstrV=BOUNDS(ng)%JstrV(tile)
      Jend =BOUNDS(ng)%Jend (tile)
      JendR=BOUNDS(ng)%JendR(tile)
      JendT=BOUNDS(ng)%JendT(tile)
!
!  Set array initialization range.
!
      IF (Istr.eq.1) THEN
        Imin=BOUNDS(ng)%LBi(tile)
      ELSE
        Imin=Istr
      END IF
      IF (Iend.eq.Lm(ng)) THEN
        Imax=BOUNDS(ng)%UBi(tile)
      ELSE
        Imax=Iend
      END IF
      IF (Jstr.eq.1) THEN
        Jmin=BOUNDS(ng)%LBj(tile)
      ELSE
        Jmin=Jstr
      END IF
      IF (Jend.eq.Mm(ng)) THEN
        Jmax=BOUNDS(ng)%UBj(tile)
      ELSE
        Jmax=Jend
      END IF
!
!-----------------------------------------------------------------------
!  Initialize module variables.
!-----------------------------------------------------------------------
!
!  Nonlinear model state.
!
      IF ((model.eq.0).or.(model.eq.iNLM)) THEN
        DO j=Jmin,Jmax
          DO i=Imin,Imax
            GRID(ng) % angler(i,j) = IniVal
            GRID(ng) % CosAngler(i,j) = IniVal
            GRID(ng) % SinAngler(i,j) = IniVal
            GRID(ng) % dmde(i,j) = IniVal
            GRID(ng) % dndx(i,j) = IniVal
            GRID(ng) % f(i,j) = IniVal
            GRID(ng) % fomn(i,j) = IniVal
            GRID(ng) % grdscl(i,j) = IniVal

            GRID(ng) % h(i,j) = IniVal
            GRID(ng) % hinv(i,j) = IniVal

            GRID(ng) % latp(i,j) = IniVal
            GRID(ng) % latr(i,j) = IniVal
            GRID(ng) % latu(i,j) = IniVal
            GRID(ng) % latv(i,j) = IniVal
            GRID(ng) % lonp(i,j) = IniVal
            GRID(ng) % lonr(i,j) = IniVal
            GRID(ng) % lonu(i,j) = IniVal
            GRID(ng) % lonv(i,j) = IniVal

            GRID(ng) % omn(i,j) = IniVal
            GRID(ng) % om_p(i,j) = IniVal
            GRID(ng) % om_r(i,j) = IniVal
            GRID(ng) % om_u(i,j) = IniVal
            GRID(ng) % om_v(i,j) = IniVal
            GRID(ng) % on_p(i,j) = IniVal
            GRID(ng) % on_r(i,j) = IniVal
            GRID(ng) % on_u(i,j) = IniVal
            GRID(ng) % on_v(i,j) = IniVal

            GRID(ng) % pm(i,j) = IniVal
            GRID(ng) % pn(i,j) = IniVal

            GRID(ng) % pmon_p(i,j) = IniVal
            GRID(ng) % pmon_r(i,j) = IniVal
            GRID(ng) % pmon_u(i,j) = IniVal
            GRID(ng) % pmon_v(i,j) = IniVal
            GRID(ng) % pnom_p(i,j) = IniVal
            GRID(ng) % pnom_r(i,j) = IniVal
            GRID(ng) % pnom_u(i,j) = IniVal
            GRID(ng) % pnom_v(i,j) = IniVal

            GRID(ng) % xp(i,j) = IniVal
            GRID(ng) % xr(i,j) = IniVal
            GRID(ng) % xu(i,j) = IniVal
            GRID(ng) % xv(i,j) = IniVal
            GRID(ng) % yp(i,j) = IniVal
            GRID(ng) % yu(i,j) = IniVal
            GRID(ng) % yv(i,j) = IniVal




          END DO

          DO k=1,N(ng)
            DO i=Imin,Imax
              GRID(ng) % Hz(i,j,k) = IniVal
              GRID(ng) % Huon(i,j,k) = IniVal
              GRID(ng) % Hvom(i,j,k) = IniVal
              GRID(ng) % z_r(i,j,k) = IniVal
            END DO
          END DO
          DO k=0,N(ng)
            DO i=Imin,Imax
              GRID(ng) % z_w(i,j,k) = IniVal
            END DO
          END DO
        END DO
      END IF



      RETURN
      END SUBROUTINE initialize_grid

      END MODULE mod_grid
