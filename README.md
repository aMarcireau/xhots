# xhots
The ultimate toilet manager

To request an OTA flash to xhots, run:
```sh
scp /path/to/the/compiled/firmware.bin idv@134.157.180.96:websites/xhots/firmware.bin
curl --data "path=http://134.157.180.96:80/xhots/firmware.bin" http://134.157.180.174:3000
```

To request an OTA flash to the quantum switch, run:
```sh
scp /path/to/the/compiled/firmware.bin macmini@134.157.180.96:websites/xhots/quantum-firmware.bin
curl --data "path=http://134.157.180.96:80/xhots/quantum-firmware.bin" http://134.157.180.175:3001
```
