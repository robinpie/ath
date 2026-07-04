# rawpacket -- crafting and sending raw TCP/UDP with !~ATH

Demonstrates !~ATH's **portal** entity (a raw or datagram socket) together with
!^CAKE header recipes and the `RECKON` internet-checksum built-in.

## Files

- `net.^CAKE`: `DENSE IMPERIAL` (packed, network-byte-order) recipes for the
  IPv4, UDP, and TCP headers, plus the 12-byte transport `Pseudo` header used
  only for checksum computation.
- `udpsend.~ATH`: unprivileged: sends a UDP datagram over loopback with a
  `"datagram"` portal and pulls it back with `APPEARIFY`. No root needed.
- `rawudp.~ATH`: privileged: hand-crafts a full IPv4+UDP packet (both checksums
  via `RECKON`) and casts it through a `"raw"` portal. Needs root/CAP_NET_RAW.

## The portal API

| construct                              | meaning                                             |
|----------------------------------------|-----------------------------------------------------|
| `import portal P("datagram"[, port]);` | a UDP socket; optional `port` binds it for receive  |
| `import portal P("raw");`              | a `SOCK_RAW`+`IP_HDRINCL` socket (you supply the IP header) |
| `SENDIFICATE(P, buf, host, port)`      | `sendto` the buffer's bytes; returns bytes sent     |
| `APPEARIFY(P, buf)`                    | `recv` into the buffer; returns bytes read (0 if none waiting) |
| `RECKON(buf[, off, len])`              | RFC 1071 internet checksum over the buffer (range)  |
| `P.DIE()` / `~ATH(P)`                  | close the socket / wait for the portal to die       |

`SENDIFICATE`/`APPEARIFY` return `0` on a would-block (the socket is
non-blocking); a hard socket error or a dead portal raises a catchable error.
Portals are POSIX-only -- on Windows and WASM `import portal` raises a catchable
runtime error.

## Running

```bash
cd ath/transpiler-to-c

# unprivileged datagram demo
./athtoc-bin < ../apps/rawpacket/udpsend.~ATH > /tmp/udpsend.c
gcc -std=c89 /tmp/udpsend.c $(ls runtime/*.c | grep -v test_runtime.c) \
    -Iruntime -ldl -lffi -o /tmp/udpsend
/tmp/udpsend

# raw crafting demo (run from the app dir so ./net.^CAKE resolves)
./athtoc-bin < ../apps/rawpacket/rawudp.~ATH > /tmp/rawudp.c
gcc -std=c89 /tmp/rawudp.c $(ls runtime/*.c | grep -v test_runtime.c) \
    -Iruntime -ldl -lffi -o /tmp/rawudp
( cd ../apps/rawpacket && sudo /tmp/rawudp )   # needs root
# watch it arrive:  sudo tcpdump -i lo -X udp port 9999
```

## Checksum notes

- `RECKON` returns the folded one's-complement value to store in a checksum
  field. It is protocol-agnostic.
- The IPv4 header checksum covers only the 20 header bytes, with the checksum
  field zeroed first: `RECKON(ip, 0, 20)`.
- The UDP/TCP checksum covers the `Pseudo` header + the transport header
  (checksum field zeroed) + the payload, staged contiguously in one buffer.
- A computed UDP checksum of `0x0000` must be transmitted as `0xFFFF`
  (`0` means "no checksum"). This rule is UDP-only -- never apply it to the IPv4
  header checksum, where `0` is legal.
