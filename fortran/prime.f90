program prime
    implicit none

    integer(8) :: n, i
    logical :: is_prime

    character(len=64) :: arg

    call get_command_argument(1, arg)
    read(arg, *) n

    if (n <= 1) then
        is_prime = .false.
    else if (n == 2) then
        is_prime = .true.
    else if (mod(n, 2_8) == 0) then
        is_prime = .false.
    else
        is_prime = .true.
        i = 3

        do while (i * i <= n)
            if (mod(n, i) == 0) then
                is_prime = .false.
                exit
            end if
            i = i + 2
        end do
    end if

    if (is_prime) then
        print *, "true"
    else
        print *, "false"
    end if

end program prime