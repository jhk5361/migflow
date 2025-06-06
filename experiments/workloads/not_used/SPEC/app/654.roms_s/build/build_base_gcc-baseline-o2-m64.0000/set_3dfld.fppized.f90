



























































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































      MODULE set_3dfld_mod
!
!svn $Id: set_3dfld.F 294 2009-01-09 21:37:26Z arango $
!================================================== Hernan G. Arango ===
!  Copyright (c) 2002-2009 The ROMS/TOMS Group                         !
!    Licensed under a MIT/X style license                              !
!    See License_ROMS.txt                                              !
!=======================================================================
!                                                                      !
!  This routine time-interpolates requested 3D field from snapshots    !
!  of input data.                                                      !
!                                                                      !
!=======================================================================
!
      implicit none

      CONTAINS
!
!***********************************************************************
      SUBROUTINE set_3dfld_tile (ng, tile, model, ifield,               &
     &                           LBi, UBi, LBj, UBj, LBk, UBk,          &
     &                           Finp, Fout, update)
!***********************************************************************
!
      USE mod_param
      USE mod_parallel
      USE mod_iounits
      USE mod_ncparam
      USE mod_scalars

!
      USE exchange_3d_mod
!
!  Imported variable declarations.
!
      logical, intent(out) :: update

      integer, intent(in) :: ng, tile, model, ifield
      integer, intent(in) :: LBi, UBi, LBj, UBj, LBk, UBk
!
      real(r8), intent(in) :: Finp(LBi:,LBj:,LBk:,:)
      real(r8), intent(out) :: Fout(LBi:,LBj:,LBk:)
!
!  Local variable declarations.
!
      logical :: Lgrided, Lonerec
      integer :: Tindex, gtype, i, it1, it2, j, k

      real(r8) :: Fval, fac, fac1, fac2












 

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
!--------------------------------------------------------------------
!  Set-up requested field for current tile.
!--------------------------------------------------------------------
!
!  Get requested field information from global storage.
!
      Lgrided=Linfo(1,ifield,ng)
      Lonerec=Linfo(3,ifield,ng)
      gtype  =Iinfo(1,ifield,ng)
      Tindex =Iinfo(8,ifield,ng)
      update=.TRUE.
!
!  Set linear-interpolation factors.
!
      it1=3-Tindex
      it2=Tindex
      fac1=ANINT(Tintrp(it2,ifield,ng)-time(ng),r8)
      fac2=ANINT(time(ng)-Tintrp(it1,ifield,ng),r8)
!
!  Load time-invariant data. Time interpolation is not necessary.
!
      IF (Lonerec) THEN
        IF (Lgrided) THEN
          DO k=LBk,UBk
            DO j=JstrR,JendR
              DO i=IstrR,IendR
                Fout(i,j,k)=Finp(i,j,k,Tindex)
              END DO
            END DO
          END DO
        ELSE
          Fval=Fpoint(Tindex,ifield,ng)
          DO k=LBk,UBk
            DO j=JstrR,JendR
              DO i=IstrR,IendR
                Fout(i,j,k)=Fval
              END DO
            END DO
          END DO
        END IF
!
!  Time-interpolate from grided or point data.
!
      ELSE IF (((fac1*fac2).ge.0.0_r8).and.                             &
     &        ((fac1+fac2).gt.0.0_r8)) THEN
        fac=1.0_r8/(fac1+fac2)
        fac1=fac*fac1
        fac2=fac*fac2
        IF (Lgrided) THEN
          DO k=LBk,UBk
            DO j=JstrR,JendR
              DO i=IstrR,IendR
                Fout(i,j,k)=fac1*Finp(i,j,k,it1)+fac2*Finp(i,j,k,it2)
              END DO
            END DO
          END DO
        ELSE
          Fval=fac1*Fpoint(it1,ifield,ng)+fac2*Fpoint(it2,ifield,ng)
          DO k=LBk,UBk
            DO j=JstrR,JendR
              DO i=IstrR,IendR
                Fout(i,j,k)=Fval
              END DO
            END DO
          END DO
        END IF
!
!  Activate synchronization flag if a new time record needs to be
!  read in at the next time step.
!
        IF ((time(ng)+dt(ng)).gt.Tintrp(it2,ifield,ng)) THEN
          IF ((Istr.eq.1).and.(Jstr.eq.1)) synchro_flag(ng)=.TRUE.
        END IF
!
!  Unable to set-up requested field.  Activate error flag to quit.
!
      ELSE
        IF ((Istr.eq.1).and.(Jstr.eq.1)) THEN
          IF (Master) THEN
            WRITE (stdout,10) TRIM(Vname(1,ifield)), tdays(ng),         &
     &                        Finfo(1,ifield,ng), Finfo(2,ifield,ng),   &
     &                        Finfo(3,ifield,ng), Finfo(4,ifield,ng),   &
     &                        Tintrp(it1,ifield,ng)*sec2day,            &
     &                        Tintrp(it2,ifield,ng)*sec2day,            &
     &                        fac1*sec2day, fac2*sec2day
          END IF
  10      FORMAT (/,' SET_3DFLD  - current model time',                 &
     &            ' exceeds ending value for variable: ',a,             &
     &            /,14x,'TDAYS     = ',f15.4,                           &
     &            /,14x,'Data Tmin = ',f15.4,2x,'Data Tmax = ',f15.4,   &
     &            /,14x,'Data Tstr = ',f15.4,2x,'Data Tend = ',f15.4,   &
     &            /,14x,'TINTRP1   = ',f15.4,2x,'TINTRP2   = ',f15.4,   &
     &            /,14x,'FAC1      = ',f15.4,2x,'FAC2      = ',f15.4)
          exit_flag=2
          update=.FALSE.
        END IF
      END IF
!
!  Exchange boundary data.
!
      IF (update) THEN
        IF (gtype.eq.r3dvar) THEN
          CALL exchange_r3d_tile (ng, tile,                             &
     &                            LBi, UBi, LBj, UBj, LBk, UBk,         &
     &                            Fout)
        ELSE IF (gtype.eq.u3dvar) THEN
          CALL exchange_u3d_tile (ng, tile,                             &
     &                            LBi, UBi, LBj, UBj, LBk, UBk,         &
     &                            Fout)
        ELSE IF (gtype.eq.v3dvar) THEN
          CALL exchange_v3d_tile (ng, tile,                             &
     &                            LBi, UBi, LBj, UBj, LBk, UBk,         &
     &                            Fout)
        ELSE IF (gtype.eq.w3dvar) THEN
          CALL exchange_w3d_tile (ng, tile,                             &
     &                            LBi, UBi, LBj, UBj, LBk, UBk,         &
     &                            Fout)
        END IF
      END IF
      RETURN
      END SUBROUTINE set_3dfld_tile
      END MODULE set_3dfld_mod
