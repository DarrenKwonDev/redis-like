# redis-like

redis-like server.

```text
❗ disclaimer
still in progress
```

## tcp stream parsing protocol

client to server stream

```text
len(4 bytes) + data, len(4 bytes) + data, len(4 bytes) + data, ...
```

문자열이 아닌 것은 모두 binary 형태로 stream에 저장됨.  
타 소켓에 전송할 때 보낼 땐 `htonl`로, 받을 땐 `ntohl`로 변환함에 유의할 것.

## don't block sacred server(game) loop

select, poll 방식의 multiplexing에서는  
i/o 작업은 non blocking, 즉, kernel space에서 작업 완료 여부와 상관없이 즉시 리턴되며 이후 user space에 작업 결과가 반환된다.  
다만 select, poll 자체는 blocking이다.

## endian check

in macos, `sysctl hw.byteorder` return 1234, which means little endian.

## simple redis cli api mimicking

-   `get k` , `set k v`, `del k` 만을 구현할 예정입니다.

## hash table implementation

self implemented hash table.  
-> [ht](https://github.com/DarrenKwonDev/ht)

## etc

-   tcp sock이 끊어지면서 EOF 송신함.
