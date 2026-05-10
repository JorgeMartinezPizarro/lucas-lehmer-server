program prime
    implicit none
    integer, parameter :: k4 = 4, k8 = 8
    integer(k4) :: p, i, nw
    integer(k4), allocatable :: s(:)
    character(len=64) :: arg
    logical :: is_mersenne_prime

    if (command_argument_count() /= 1) then
        print *, "Uso: ./llt <exponente_p>"
        stop
    end if
    call get_command_argument(1, arg)
    read(arg, *) p

    if (p < 2) then
        is_mersenne_prime = .false.
    else if (p == 2) then
        is_mersenne_prime = .true.
    else
        nw = (p / 32) + 1
        allocate(s(nw))
        s = 0
        s(1) = 4

        do i = 1, p - 2
            call llt_step(s, p, nw)
        end do

        is_mersenne_prime = check_zero(s, p)
        deallocate(s)
    end if

    if (is_mersenne_prime) then
        print *, "true"
    else
        print *, "false"
    end if

contains

    subroutine llt_step(s, p, nw)
        integer(k4), intent(inout) :: s(:)
        integer(k4), intent(in) :: p, nw
        integer(k4) :: wide(2*nw), high(nw), low(nw)
        integer(k8) :: carry, temp, mask
        integer :: i, j, shift_w, shift_b

        mask = int(z'FFFFFFFF', k8)
        
        ! 1. S^2
        wide = 0
        do i = 1, nw
            carry = 0_k8
            do j = 1, nw
                temp = iand(int(wide(i+j-1), k8), mask) + &
                       iand(int(s(i), k8), mask) * &
                       iand(int(s(j), k8), mask) + carry
                wide(i+j-1) = int(iand(temp, mask), k4)
                carry = shiftr(temp, 32)
            end do
            wide(i+nw) = int(iand(carry, mask), k4)
        end do

        ! 2. Reducción Modular
        low = wide(1:nw)
        low(nw) = iand(low(nw), int(ishft(1_k8, mod(p, 32)) - 1, k4))

        shift_w = p / 32
        shift_b = mod(p, 32)
        do i = 1, nw
            temp = iand(int(wide(i + shift_w), k8), mask)
            if (i + shift_w + 1 <= 2*nw) then
                temp = temp + ishft(iand(int(wide(i + shift_w + 1), k8), mask), 32)
            end if
            high(i) = int(iand(ishft(temp, -shift_b), mask), k4)
        end do

        carry = 0
        do i = 1, nw
            temp = iand(int(low(i), k8), mask) + &
                   iand(int(high(i), k8), mask) + carry
            s(i) = int(iand(temp, mask), k4)
            carry = shiftr(temp, 32)
        end do
        
        if (carry > 0 .or. btest(s(nw), mod(p, 32))) then
            if (mod(p, 32) /= 0) s(nw) = ibclr(s(nw), mod(p, 32))
            temp = iand(int(s(1), k8), mask) + carry + 1
            s(1) = int(iand(temp, mask), k4)
        end if

        ! 3. Restar 2
        temp = iand(int(s(1), k8), mask) - 2
        if (temp >= 0) then
            s(1) = int(temp, k4)
        else
            s(1) = int(temp, k4)
            do i = 2, nw
                s(i) = s(i) - 1
                if (s(i) /= -1) exit
            end do
        end if
    end subroutine

    function check_zero(s, p) result(res)
        integer(k4), intent(in) :: s(:), p
        logical :: res
        integer :: i, nw
        nw = size(s)
        res = .true.
        do i = 1, nw - 1
            if (s(i) /= 0) res = .false.
        end do
        if (iand(s(nw), int(ishft(1_k8, mod(p, 32)) - 1, k4)) /= 0) res = .false.
    end function

end program llt_prime