program sieve
    implicit none
    integer, allocatable :: primes(:)
    integer :: n, i, j
    character(len=10) :: arg  ! Variable para leer el argumento como cadena

    ! Leer el argumento desde la línea de comandos
    if (command_argument_count() /= 1) then
        print *, "Usage: ./sieve <n>"
        stop
    end if

    call get_command_argument(1, arg)  ! Leer el argumento como cadena
    read(arg, *) n                     ! Convertir la cadena a un entero

    ! Validar el valor de n
    if (n <= 0) then
        print *, "Error: n must be a positive integer."
        stop
    end if

    allocate(primes(n))
    primes = 1

    primes(1) = 0 ! 1 no es primo

    do i = 2, int(sqrt(real(n)))
        if (primes(i) == 1) then
            do j = i * i, n, i
                primes(j) = 0
            end do
        end if
    end do

    ! Imprimir los números primos
    do i = 1, n
        if (primes(i) == 1) then
            write(*,*) i
        end if
    end do

    deallocate(primes)
end program sieve
