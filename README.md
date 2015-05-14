# Concurrent C Programming
This is a server and client implementation for [Concurrent C Programming / Seminar ZHAW / FS 2015](https://github.com/telmich/zhaw_concurrent_c_programming_fs_2015) written in C99 using GNU extensions.

## Compilation
Running `make` will produce a `server` and `client` executable in the `bin` folder.

## Start the server
    ./bin/server -n <size> [-d]

The server will listen on port 1666.

`-d` enables debug mode and adds verbose logging and additonal console (stderr) output. It will log everything to your syslog.

## Start the client
    ./bin/client -n <name> -h <host> -p <port>

Currently no additional parameters supported. There is only one basic algorithem implemented: It will go through the whole field and try to take one by one. If it fails it will retry once.

Defaults:

*   -h localhost
*   -p 1666

## Author
Benjamin Buetikofer - <me@bueti-online.ch>
