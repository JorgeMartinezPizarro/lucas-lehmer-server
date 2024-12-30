# lucas-lehmer-server

## Abstract

This project is a compilation of web servers with a single endpoint lltp (lucas-lehmer test parallelized).

Used primary to benchmark the different languages, c++ go scala and rust, each one with a unbounded integer type.

## Usage

You can use make build - start to add your own changes. 

make push runs exclusively for the hub docker repository owner. 

Feel free to modify the tag value of the docker images to create your own fork - hub repository.

```
make start
```

will start the 4 servers,

```
make build push
```

will build the current code (go server) and push the generated container to docker hub.

## Note

Currently I am working on a python server to connect to a fortran function.

Only go is getting further, the other languages still available in hub docker, scala, rust and c++:

https://hub.docker.com/repository/docker/jorgemartinezpizarro/lucas-lehmer-server/tags


## TODO

Add each endpoint spec (PORT, parameters)

Use Python flask to manage other services, in special Fortran and Go.