# redis-like

redis-like server.

```text
❗ disclaimer
still in progress
```

## application level tcp stream parsing protocol

```text
len(4 bytes) + data, len(4 bytes) + data, len(4 bytes) + data, ...
```

주의할 점이, 여기서는 len을 binary 형태로 char buffer에 저장함.  
따라서 보낼 땐 `htonl`로, 받을 땐 `ntohl`로 변환해야 함.

## don't block sacred server(game) loop

select, poll 방식의 multiplexing에서는  
i/o 작업은 non blocking, 즉, kernel space에서 작업 완료 여부와 상관없이 즉시 리턴되며 이후 user space에 작업 결과가 반환된다.  
다만 select, poll 자체는 blocking이다.

## endian check

in macos, `sysctl hw.byteorder` return 1234, which means little endian.

## simple redis cli api mimicking

-

## etc

-   tcp sock이 끊어지면서 EOF 송신함.
