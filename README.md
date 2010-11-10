fidcap
====

* **F**ake V**id**eo **Cap**ture *  or * **F**SR V**id**eo **Cap**ture *

`fidcap` is designed to work with the `FSR172X` `ISP`/`CCDC` raw data acquisition module for `OMAP3530`.

The `FSR`, or **F**idcap **S**tream **R**ecorder connects to a raw data acquisition unit which streams data
uses all 12-bits available on the CCDC interface. It can be thought of as a RAW12 movie camera.

Status
====

The `fsr172x` kernel module is not complete, hence `fidcap` `mmap`s some registers to set them directly.

The RAW12 data is *mostly* captured (about 99.4% of it) and written out to `fsr.raw12`.

The best way to see the problem is to redirect the logging to memory:

    ln -s /dev/shm/fsr.raw12 ./
    ./fidcap
    analyze /dev/shm/fsr.raw12
    rm /dev/shm/fsr.raw12 # because fidcap appends forever

BUGs
---

During data acquisition the rest of the system is entirely unusable.

Notice that a simple loop in another process which should take 100ms takes 1 second each time capture is happening.

    make
    ./loop-100ms

`fsr_buffer_get` 
calls `fsr_dqbuf` 
which calls `ioctl(fsr_fd, VIDIOC_DQBUF, &v4l2_buf)` 
which does magic in the driver
which causes the system to freeze for the duration of the capture
