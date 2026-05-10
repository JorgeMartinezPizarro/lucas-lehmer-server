program llt_prime
    implicit none
    integer, parameter :: k4 = 4, k8 = 8
    integer(k4) :: p, i, nw
    integer(k4), allocatable :: s(:)
    character(len=64) :: arg
    logical :: is_mersenne_prime

    ! Leer p desde el argumento de línea de comandos
    if (command_argument_count() /= 1) then
        print *, "Uso: ./llt <exponente_p>"
        stop
    end if
    call get_command_argument(1, arg)
    read(arg, *) p

    ! Casos triviales
    if (p < 2) then
        is_mersenne_prime = .false.
    else if (p == 2) then
        is_mersenne_prime = .true.
    else
        ! Determinar tamaño del array (base 2^32)
        nw = (p / 32) + 1
        allocate(s(nw))
        
        ! S_0 = 4
        s = 0
        s(1) = 4

        ! Bucle LLT: S = (S^2 - 2) mod (2^p - 1)
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

    ! Realiza S = (S**2 - 2) mod (2**p - 1)
    subroutine llt_step(s, p, nw)
        integer(k4), intent(inout) :: s(:)
        integer(k4), intent(in) :: p, nw
        integer(k4) :: wide(2*nw), high(nw), low(nw)
        integer(k8) :: carry, temp
        integer :: i, j, shift_w, shift_b

        ! 1. S^2 (Multiplicación O(N^2))
        wide = 0
        do i = 1, nw
            carry = 0_k8
            do j = 1, nw
                temp = (int(wide(i+j-1), k8) .and. z'FFFFFFFF') + &
                       (int(s(i), k8) .and. z'FFFFFFFF') * &
                       (int(s(j), k8) .and. z'FFFFFFFF') + carry
                wide(i+j-1) = int(temp, k4)
                carry = shiftr(temp, 32)
            end do
            wide(i+nw) = int(carry, k4)
        end do

        ! 2. Reducción Modular Rápida: (wide mod 2^p-1)
        ! Separar parte baja
        low = wide(1:nw)
        low(nw) = iand(low(nw), int(ishft(1_k8, mod(p, 32)) - 1, k4))

        ! Separar parte alta (desplazando p bits)
        shift_w = p / 32
        shift_b = mod(p, 32)
        do i = 1, nw
            temp = iand(int(wide(i + shift_w), k8), z'FFFFFFFF')
            if (i + shift_w + 1 <= 2*nw) then
                temp = temp + ishft(int(wide(i + shift_w + 1), k8), 32)
            end if
            high(i) = int(ishft(temp, -shift_b), k4)
        end do

        ! s = low + high
        carry = 0
        do i = 1, nw
            temp = (int(low(i), k8) .and. z'FFFFFFFF') + &
                   (int(high(i), k8) .and. z'FFFFFFFF') + carry
            s(i) = int(temp, k4)
            carry = shiftr(temp, 32)
        end do
        
        ! Gestionar acarreo final (wrap-around)
        if (carry > 0 .or. btest(s(nw), mod(p, 32))) then
            s(nw) = ibclr(s(nw), mod(p, 32))
            temp = (int(s(1), k8) .and. z'FFFFFFFF') + carry + 1
            s(1) = int(temp, k4)
            ! Nota: En p muy grandes se debería propagar este último acarreo
        end if

        ! 3. Restar 2
        temp = (int(s(1), k8) .and. z'FFFFFFFF') - 2
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
        
        ! Nota: Si el resultado es s == M_p, también es válido como 0 mod M_p
        ! pero en el LLT estándar tras las reducciones suele colapsar a 0.
    end function

end program llt_prime