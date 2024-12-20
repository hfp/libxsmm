!=======================================================================!
! Copyright (c) Intel Corporation - All rights reserved.                !
! This file is part of the LIBXSMM library.                             !
!                                                                       !
! For information on the license, see the LICENSE file.                 !
! Further information: https://github.com/libxsmm/libxsmm/              !
! SPDX-License-Identifier: BSD-3-Clause                                 !
!=======================================================================!
! Hans Pabst (Intel Corp.)
!=======================================================================!

      PROGRAM transposef
        USE :: LIBXSMM, ONLY: LIBXSMM_BLASINT_KIND,                     &
     &                        duration => libxsmm_timer_duration,       &
     &                        tick => libxsmm_timer_tick,               &
     !&                        otrans => libxsmm_otrans_omp,             &
     &                        otrans => libxsmm_otrans,                 &
     &                        itrans => libxsmm_itrans,                 &
     &                        ptr => libxsmm_ptr
        IMPLICIT NONE

        INTEGER, PARAMETER :: T = KIND(0D0)
        INTEGER, PARAMETER :: S = 8

        REAL(T), ALLOCATABLE, TARGET :: a1(:), b1(:)
        !DIR$ ATTRIBUTES ALIGN:64 :: a1, b1
        INTEGER(LIBXSMM_BLASINT_KIND) :: m, n, ldi, ldo, i, j, k
        REAL(T), POINTER :: an(:,:), bn(:,:), bt(:,:)
        DOUBLE PRECISION :: dxsmm, dfort
        INTEGER(8) :: nbytes, start
        INTEGER :: nrepeat
        REAL(T) :: diff, v

        CHARACTER(32) :: argv
        CHARACTER :: trans
        INTEGER :: argc

        argc = COMMAND_ARGUMENT_COUNT()
        IF (1 <= argc) THEN
          CALL GET_COMMAND_ARGUMENT(1, trans)
        ELSE
          trans = 'o'
        END IF
        IF (2 <= argc) THEN
          CALL GET_COMMAND_ARGUMENT(2, argv)
          READ(argv, "(I32)") m
        ELSE
          m = 4096
        END IF
        IF (3 <= argc) THEN
          CALL GET_COMMAND_ARGUMENT(3, argv)
          READ(argv, "(I32)") n
        ELSE
          n = m
        END IF
        IF (4 <= argc) THEN
          CALL GET_COMMAND_ARGUMENT(4, argv)
          READ(argv, "(I32)") ldi
        ELSE
          ldi = m
        END IF
        IF (5 <= argc) THEN
          CALL GET_COMMAND_ARGUMENT(5, argv)
          READ(argv, "(I32)") ldo
        ELSE
          ldo = ldi
        END IF
        IF (6 <= argc) THEN
          CALL GET_COMMAND_ARGUMENT(6, argv)
          READ(argv, "(I32)") nrepeat
        ELSE
          CALL GET_ENVIRONMENT_VARIABLE("NREPEAT", argv, nrepeat)
          IF (0.LT.nrepeat) THEN
            READ(argv, "(I32)") nrepeat
          ELSE
            nrepeat = 3
          END IF
        END IF

        nbytes = INT(m * n, 8) * T ! size in Byte
        WRITE(*, "(2(A,I0),2(A,I0),A,I0,A,I0)")                         &
     &    "m=", m, " n=", n, " ldi=", ldi, " ldo=", ldo,                &
     &    " size=", (nbytes / ISHFT(1, 20)), "MB nrepeat=", nrepeat

        ALLOCATE(b1(ldo*MAX(m,n)))
        bn(1:ldo,1:n) => b1
        bt(1:ldo,1:m) => b1

        dfort = 0D0
        IF (('o'.EQ.trans).OR.('O'.EQ.trans)) THEN
          ALLOCATE(a1(ldi*n))
          an(1:ldi,1:n) => a1
          !$OMP PARALLEL DO PRIVATE(i, j) DEFAULT(NONE) SHARED(m, n, an)
          DO j = 1, n
            DO i = 1, m
              an(i,j) = initial_value(i - 1, j - 1, m)
            END DO
          END DO
          !$OMP END PARALLEL DO
          start = tick()
          DO k = 1, nrepeat
            !CALL otrans(ptr(b1), ptr(a1), S, m, n, ldi, ldo)
            !CALL otrans(bn, an, m, n, ldi, ldo)
            CALL otrans(b1, a1, m, n, ldi, ldo)
          END DO
          dxsmm = duration(start, tick())
          IF ((ldi.EQ.ldo).AND.(ldi.EQ.m)) THEN
            start = tick()
            DO k = 1, nrepeat
              bn = TRANSPOSE(an)
            END DO
            dfort = duration(start, tick())
          END IF
          DEALLOCATE(a1)
        ELSE ! in-place
          !$OMP PARALLEL DO PRIVATE(i, j) DEFAULT(NONE) SHARED(m, n, bn)
          DO j = 1, n
            DO i = 1, m
              bn(i,j) = initial_value(i - 1, j - 1, m)
            END DO
          END DO
          !$OMP END PARALLEL DO
          start = tick()
          DO k = 1, nrepeat
            !CALL itrans(ptr(b1), S, m, n, ldi, ldo)
            !CALL itrans(bn, m, n, ldi)
            CALL itrans(b1, m, n, ldi)
          END DO
          dxsmm = duration(start, tick())
        END IF

        diff = REAL(0, T)
        DO j = 1, n
          DO i = 1, m
            v = initial_value(i - 1, j - 1, m)
            diff = MAX(diff, ABS(bt(j,i) - v))
          END DO
        END DO
        DEALLOCATE(b1)

        IF (0.EQ.diff) THEN
          IF ((0.LT.dxsmm).AND.(0.LT.nrepeat)) THEN
            ! out-of-place transpose bandwidth assumes RFO
            WRITE(*, "(1A,A,F10.1,A)") CHAR(9), "bandwidth:  ",         &
     &        REAL(nbytes, T)                                           &
     &        * MERGE(3D0, 2D0, ('o'.EQ.trans).OR.('O'.EQ.trans))       &
     &        * REAL(nrepeat, T) / (dxsmm * REAL(ISHFT(1_8, 30), T)),   &
     &        " GB/s"
            WRITE(*, "(1A,A,F10.1,A)") CHAR(9), "duration:   ",         &
     &        1D3 * dxsmm / REAL(nrepeat, T), " ms"
            IF (0.LT.dfort) THEN
              WRITE(*, "(1A,A,F10.1,A)") CHAR(9), "baseline:   ",       &
     &          1D3 * dfort / REAL(nrepeat, T), " ms"
            END IF
          END IF
        ELSE
          WRITE(*,*) "Validation failed!"
          STOP 1
        END IF

      CONTAINS
        PURE REAL(T) FUNCTION initial_value(i, j, m)
          INTEGER(LIBXSMM_BLASINT_KIND), INTENT(IN) :: i, j, m
          initial_value = REAL(j * m + i, T)
        END FUNCTION
      END PROGRAM

