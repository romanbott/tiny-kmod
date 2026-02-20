# tiny-kmod
Tiny linux kernel module for learning and testing.

To compile the module, its necesary to install the linux kernel headers matching the current distribution.

In fedora do:

```
sudo dnf update
sudo dnf install kernel-devel kernel-headers make gcc elfutils-libelf-devel
```

Then build with `make` and load the module with `sudo insmod ouroboros.ko` and unload with `sudo rmmod ouroboros`.

To append to the circular buffer:
```
echo "Hello, World!" > /proc/ouroboros
```

To read from the buffer:
```
cat /proc/ouroboros
```
