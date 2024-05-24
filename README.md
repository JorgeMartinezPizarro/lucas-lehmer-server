# lucas-lehmer-server

## Abstract

This project is a compilation of web servers with a single endpoint lltp (lucas-lehmer test parallelized).

Used primary to benchmark the different languages, c++ go scala and rust, each one with a unbounded integer type.

## Usage

TODO: add each endpoint spec (PORT, parameters)

You can use make build - start to add your own changes. 

make push runs exclusively for the hub docker repository owner. 

Feel free to modify the tag value of the docker images to create your own fork - hub repository.

```
make build start
```

will start the 4 servers.
