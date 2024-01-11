# kvv-test

Some KVV (Karlsruher Verkehrs Verbund) "EFA"-API tests for train schedules and information.

> **Warn**
> This API doesn't seem like a permanent solution, it could change at any time completely.

## Basic API:

- Searching for a station:

```
https://www.kvv.de/tunnelEfaDirect.php?action=XSLT_STOPFINDER_REQUEST&coordOutputFormat=WGS84[dd.ddddd]&name_sf=<your station name>&outputFormat=JSON&type_sf=any]()
```

- Requesting station departures:

```
https://projekte.kvv-efa.de/sl3-alone/XSLT_DM_REQUEST?outputFormat=JSON&coordOutputFormat=WGS84[dd.ddddd]&depType=stopEvents&locationServerActive=1&mode=direct&name_dm=<your station id>&type_dm=stop&useOnlyStops=1&useRealtime=1&limit=10
```

## Rust Impl:

![](./assets/departures.png)

## ESP32 Departure Board

Using an HUB75 32x64 rbg display and an ESP32:

![](./assets/departure-board.jpg)

> Based on an ESP32 dev board and a 32x64 px. HUB75 rgb display

