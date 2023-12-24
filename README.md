# redis-like

redis-like server.

## application level tcp stream parsing protocol

```text
len(4 bytes) + data, len(4 bytes) + data, len(4 bytes) + data, ...
```

주의할 점이, 여기서는 len을 binary 형태로 char buffer에 저장함.  
따라서 보낼 땐 `htonl`로, 받을 땐 `ntohl`로 변환해야 함.

## don't block sacred server(game) loop

## endian check

in macos, `sysctl hw.byteorder` return 1234, which means little endian.

## etc

-   tcp sock이 끊어지면서 EOF 송신함.
