# HTTP Proxy
Multi-threaded (with threadpool) HTTP Proxy server. Made in C.

## Usage
Compile:
```sh
gcc proxyServer.c threadpool.c -lpthread -o proxyServer
```

Run:
```sh
./proxyServer <port> <pool-size> <max-number-of-requests> <filter-path>
```

port: The port that the proxy will bind to

pool-size: Size of the threadpool (max number of concurrent threads)

max-number-of-requests: Number of requests before shutting the proxy down and cleaning up

filter-path: Path to a filter file that contains hostnames/ip & subnet addresses to blacklist