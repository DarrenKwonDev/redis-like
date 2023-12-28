# redis-like

redis-like server.

```text
❗ disclaimer
still in progress
```

## tcp stream parsing protocol

### client -> server stream

```text
(stream_byte_size, 몇 어절?, len, cmd, len, cmd, ...)

쪼개어 생각하면 다음과 같다.
(stream_byte_size, 2, [len, cmd], [len, cmd])

payload는 stream_byte_size를 제외한 다음 부분을 지칭한다.
(2, [len, cmd], [len, cmd])

# ex
(stream_byte_size, 3, [len1, "set"], [len2, "key"], [len3, "value"])
(stream_byte_size, 2, [len1, "get"], [len2, "k"])
```

### server -> client stream

```text
(res_len, res_code, data)

# ex
(res_len, 0, "value")
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

-   `get k` , `set k v`, `del k` 만 구현되었습니다.

## hash table implementation

self implemented hash table.  
-> [ht](https://github.com/DarrenKwonDev/ht)

## known issues

-   네트워크, 호스트 바이트 오더 변환에서 현재 shotgun surgery를 하고 있음.  
    -> 추후에는 모든 네트워크, 호스트 바이트 오더 변환을 위해 리팩토링이 필요.

## etc

-   tcp sock이 끊어지면서 EOF 송신함.
