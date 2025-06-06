



























































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































!
!svn $Id: mp_routines.F 294 2009-01-09 21:37:26Z arango $
!================================================== Hernan G. Arango ===
!  Copyright (c) 2002-2009 The ROMS/TOMS Group                         !
!    Licensed under a MIT/X style license                              !
!    See License_ROMS.txt                                              !
!=======================================================================
!                                                                      !
!  This package contains multi-processing routines used during         !
!  parallel applications:                                              !
!                                                                      !
!     my_flush         Flushes the contents of a unit buffer.          !
!     my_getarg        Returns the argument from command-line.         !
!     my_getpid        Returns process ID of the calling process.      !
!     my_numthreads    Returns number of threads that would            !
!                        execute in parallel regions.                  !
!     my_threadnum     Returns which thread number is working          !
!                        in a parallel region.                         !
!     my_wtime         Returns an elapsed wall time in seconds since   !
!                        an arbitrary time in the past.                !
!                                                                      !
!=======================================================================
!
!
!-----------------------------------------------------------------------
      SUBROUTINE my_flush (unit)
!-----------------------------------------------------------------------
!
      USE mod_kinds
!
      implicit none
!
!  Imported variable declarations.
!
      integer, intent(in) :: unit
!

      RETURN
      END SUBROUTINE my_flush
!
!-----------------------------------------------------------------------
      FUNCTION my_getpid ()
!-----------------------------------------------------------------------
!
      USE mod_kinds

      implicit none

      integer :: getpid, my_getpid

      my_getpid=0

      RETURN
      END FUNCTION my_getpid
!
!-----------------------------------------------------------------------
      FUNCTION my_numthreads ()
!-----------------------------------------------------------------------
!
      USE mod_kinds

      implicit none

      integer :: my_numthreads

      integer :: omp_get_max_threads
!!    integer :: omp_get_num_threads

      my_numthreads=omp_get_max_threads()
!!    my_numthreads=omp_get_num_threads()

      RETURN
      END FUNCTION my_numthreads
!
!-----------------------------------------------------------------------
      FUNCTION my_threadnum ()
!-----------------------------------------------------------------------
!
      USE mod_kinds

      implicit none

      integer :: my_threadnum

      integer :: omp_get_thread_num

      my_threadnum=omp_get_thread_num()

      RETURN
      END FUNCTION my_threadnum
!
!-----------------------------------------------------------------------
      FUNCTION my_wtime (wtime)
!-----------------------------------------------------------------------
!
      USE mod_kinds

      implicit none

      real(r8), intent(out) :: wtime(2)
      real(r8) :: my_wtime
        wtime(1) = REAL(0,r8)
        wtime(2) = REAL(0,r8)
        my_wtime=wtime(1)
      RETURN
      END FUNCTION my_wtime
