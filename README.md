# linux-ping
self made linux's ping command implemented by C.
can't customize ICMP packages.
send 5 packages each time.

# Usage:
```
> gcc -o my_ping ping.c
> my_ping 127.0.0.1
ping 127.0.0.1 (127.0.0.1) : 24 bytes of data.
24 bytes from 0.0.0.0 : icmp_seq = 1  ttl = 64  rtt = 0.000000ms 
24 bytes from 0.0.0.0 : icmp_seq = 2  ttl = 64  rtt = 0.000000ms 
24 bytes from 0.0.0.0 : icmp_seq = 3  ttl = 64  rtt = 0.000000ms 
24 bytes from 0.0.0.0 : icmp_seq = 4  ttl = 64  rtt = 0.000000ms 
24 bytes from 0.0.0.0 : icmp_seq = 5  ttl = 64  rtt = 0.000000ms 
--- 127.0.0.1 ping statistics ---
5 packets transmitted, 5 received, 0% packets lost
```