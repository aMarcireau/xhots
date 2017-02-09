# xhots
The ultimate toilet manager

To request an OTA flash to xhots, run:
```sh
scp /path/to/the/compiled/firmware.bin macmini@134.157.180.144:Websites/xhots/firmware.bin
curl --data "path=http://134.157.180.144:3003/firmware.bin" http://134.157.180.105:3000
```

To request an OTA flash to the quantum switch, run:
```sh
scp /path/to/the/compiled/firmware.bin macmini@134.157.180.144:Websites/xhots/quantum-firmware.bin
curl --data "path=http://134.157.180.144:3003/quantum-firmware.bin" http://134.157.180.105:3001
```
