# redis-like

redis-like server.

## application level tcp stream parsing protocol

```text
len(4 bytes) + data, len(4 bytes) + data, len(4 bytes) + data, ...
```

## don't block sacred server(game) loop
