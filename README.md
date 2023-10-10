## Flashing tool 

Modprobe the MTD flash only while flashing, otherwise the kernel module should not be loaded, as it can create conflicts with the other SPI things on the same bus.

To do that host controller should be defined as kernel module and the other device who uses the same MTD flash should be in reset mode.

### Usage
```
make
sudo ./fcp [OPTIONS]
```
