gcc -o drm_server2 drm_server2.c -I/usr/include/libdrm -ldrm -lX11 -lrt
gcc -o drm_client2 drm_client2.c -I/usr/include/libdrm -ldrm -lGL -lEGL -lX11 -lrt
