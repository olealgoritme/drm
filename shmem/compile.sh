gcc -o drm_server drm_server.c -I/usr/include/libdrm -ldrm -lX11 -lrt
gcc -o drm_client drm_client.c -I/usr/include/libdrm -ldrm -lGL -lEGL -lX11 -lrt
