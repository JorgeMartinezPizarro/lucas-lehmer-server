program prime
    implicit none

    integer, parameter :: k4 = selected_int_kind(9)
    integer, parameter :: k8 = selected_int_kind(18)

    integer(k4) :: p
    character(len=64) :: arg
    logical :: ok

    if (command_argument_count() /= 1) then
        print *, "Uso: ./llt <p>"
        stop
    end if

    call get_command_argument(1, arg)
    read(arg, *) p

    ok = lucas_lehmer(p)

    if (ok) then
        print *, "true"
    else
        print *, "false"
    end if

contains

    function lucas_lehmer(p) result(isprime)
        integer(k4), intent(in) :: p
        logical :: isprime

        integer(k4), allocatable :: s(:)
        integer :: nw, i

        if (p == 2) then
            isprime = .true.
            return
        end if

        if (p < 2) then
            isprime = .false.
            return
        end if

        nw = (p + 31) / 32

        allocate(s(nw))
        s = 0
        s(1) = 4

        do i = 1, p - 2
            call llt_step(s, p)
        end do

        isprime = is_zero(s, p)

        deallocate(s)

    end function


    subroutine llt_step(s, p)
        integer(k4), intent(inout) :: s(:)
        integer(k4), intent(in) :: p

        integer(k4), allocatable :: tmp(:)
        integer :: nw

        nw = size(s)

        allocate(tmp(2*nw))
        tmp = 0

        call square_bigint(s, tmp)
        call mersenne_reduce(tmp, s, p)
        call sub_small(s, 2_k4)

        deallocate(tmp)

    end subroutine


    subroutine square_bigint(a, out)
        integer(k4), intent(in) :: a(:)
        integer(k4), intent(out) :: out(:)

        integer(k8) :: mask
        integer(k8) :: av, bv
        integer(k8) :: temp
        integer(k8) :: carry

        integer :: i, j

        mask = int(z'FFFFFFFF', k8)

        out = 0

        do i = 1, size(a)

            carry = 0_k8
            av = iand(int(a(i), k8), mask)

            do j = 1, size(a)

                bv = iand(int(a(j), k8), mask)

                temp = iand(int(out(i+j-1), k8), mask) + av*bv + carry

                out(i+j-1) = int(iand(temp, mask), k4)
                carry = shiftr(temp, 32)

            end do

            j = i + size(a)

            do while (carry > 0)

                temp = iand(int(out(j), k8), mask) + carry

                out(j) = int(iand(temp, mask), k4)
                carry = shiftr(temp, 32)

                j = j + 1

            end do

        end do

    end subroutine


    subroutine mersenne_reduce(x, out, p)
        integer(k4), intent(in) :: x(:)
        integer(k4), intent(out) :: out(:)
        integer(k4), intent(in) :: p

        integer(k4), allocatable :: work(:)

        integer(k8) :: mask
        integer(k8) :: low
        integer(k8) :: high
        integer(k8) :: temp
        integer(k8) :: carry

        integer :: nw
        integer :: i
        integer :: bit_shift
        integer :: word_shift

        mask = int(z'FFFFFFFF', k8)

        nw = (p + 31) / 32

        allocate(work(2*nw + 2))
        work = 0

        work(1:size(x)) = x

        do

            word_shift = p / 32
            bit_shift = mod(p, 32)

            carry = 0

            do i = 1, nw

                low = iand(int(work(i), k8), mask)

                high = 0

                if (i + word_shift <= size(work)) then
                    high = iand(int(work(i + word_shift), k8), mask)
                end if

                if (bit_shift /= 0) then

                    high = shiftr(high, bit_shift)

                    if (i + word_shift + 1 <= size(work)) then
                        high = high + ishft( &
                            iand(int(work(i + word_shift + 1), k8), mask), &
                            32 - bit_shift)
                    end if

                end if

                temp = low + high + carry

                work(i) = int(iand(temp, mask), k4)
                carry = shiftr(temp, 32)

            end do

            work(nw+1) = int(carry, k4)

            do i = nw+2, size(work)
                work(i) = 0
            end do

            if (.not. has_high_bits(work, p)) exit

        end do

        out = work(1:nw)

        call normalize_mersenne(out, p)

        deallocate(work)

    end subroutine


    subroutine normalize_mersenne(a, p)
        integer(k4), intent(inout) :: a(:)
        integer(k4), intent(in) :: p

        integer(k8) :: mask
        integer(k8) :: temp
        integer(k8) :: carry

        integer :: nw
        integer :: bits
        integer :: i

        mask = int(z'FFFFFFFF', k8)

        nw = size(a)
        bits = mod(p, 32)

        if (bits /= 0) then

            if (btest(a(nw), bits)) then

                a(nw) = ibclr(a(nw), bits)

                carry = 1

                do i = 1, nw

                    temp = iand(int(a(i), k8), mask) + carry

                    a(i) = int(iand(temp, mask), k4)
                    carry = shiftr(temp, 32)

                    if (carry == 0) exit

                end do

            end if

            a(nw) = iand(a(nw), int(ishft(1_k8, bits) - 1, k4))

        end if

    end subroutine


    subroutine sub_small(a, v)
        integer(k4), intent(inout) :: a(:)
        integer(k4), intent(in) :: v

        integer(k8) :: mask
        integer(k8) :: temp

        integer :: i

        mask = int(z'FFFFFFFF', k8)

        temp = iand(int(a(1), k8), mask) - v

        a(1) = int(iand(temp, mask), k4)

        if (temp < 0) then

            do i = 2, size(a)

                temp = iand(int(a(i), k8), mask) - 1

                a(i) = int(iand(temp, mask), k4)

                if (temp >= 0) exit

            end do

        end if

    end subroutine


    logical function has_high_bits(a, p)
        integer(k4), intent(in) :: a(:)
        integer(k4), intent(in) :: p

        integer :: nw
        integer :: bits
        integer :: i

        nw = (p + 31) / 32
        bits = mod(p, 32)

        has_high_bits = .false.

        do i = nw+1, size(a)
            if (a(i) /= 0) then
                has_high_bits = .true.
                return
            end if
        end do

        if (bits /= 0) then
            if (shiftr(a(nw), bits) /= 0) then
                has_high_bits = .true.
            end if
        end if

    end function


    logical function is_zero(a, p)
        integer(k4), intent(in) :: a(:)
        integer(k4), intent(in) :: p

        integer :: i
        integer :: nw
        integer :: bits

        nw = size(a)
        bits = mod(p, 32)

        is_zero = .true.

        do i = 1, nw-1
            if (a(i) /= 0) then
                is_zero = .false.
                return
            end if
        end do

        if (bits == 0) then
            if (a(nw) /= 0) is_zero = .false.
        else
            if (iand(a(nw), int(ishft(1_k8, bits)-1, k4)) /= 0) then
                is_zero = .false.
            end if
        end if

    end function

end program