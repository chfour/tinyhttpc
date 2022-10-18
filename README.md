# tinyhttpc

this is a tiny (and dumb, in a way) HTTP client written in pure C.

compile with gcc like `gcc -Wall httpc.c -o httpc` or run directly with `tcc`: `tcc -Wall -run httpc.c`

then pass the URL (without the protocol - `http://`)!

```
$ tcc -Wall -run httpc.c nohttps-files.eeep.ee/test.txt
parsed host='nohttps-files.eeep.ee' port='80' path='/test.txt'
trying '173.212.254.236' port 80...
connected!
> GET /test.txt HTTP/1.1
> Host: nohttps-files.eeep.ee
> Accept: */*
> Connection: close
>
> [end of request headers]
< HTTP/1.1 200 OK
< Accept-Ranges: bytes
< Content-Length: 13
* got Content-Length=13
< Etag: "rfs94qd"
< Last-Modified: Fri, 29 Jul 2022 12:53:14 GMT
< Server: Caddy
< Date: Tue, 18 Oct 2022 12:32:15 GMT
< Connection: close
<
< [end of response headers]
hello world!
* closing because of Content-Length
```
