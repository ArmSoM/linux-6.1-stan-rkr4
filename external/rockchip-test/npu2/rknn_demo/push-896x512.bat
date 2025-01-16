adb push librockit.so /usr/lib/
adb push librockiva.so /usr/lib/
adb push librknnrt.so /usr/lib/
adb push libmali.so.1 /usr/lib/
adb push libgraphic_lsf.so /usr/lib/
adb push libavcodec.so.58 /usr/lib/
adb push libswresample.so.3 /usr/lib/
adb push libavutil.so.56 /usr/lib/
adb push libavformat.so.58 /usr/lib/
adb push librockchip_mpp.so.1 /usr/lib/
adb push librga.so.2 /usr/lib/

adb shell mkdir -p /userdata/rockiva_data_rk356x
adb push object_detection_v3_cls8.data /userdata/rockiva_data_rk356x/
adb push rk_mpi_sample_test /userdata/
adb push rk-car.ts /userdata/
adb push rk-0.ts /userdata/
adb push rk-1.ts /userdata/
adb push rk-2.ts /userdata/
adb push rk-3.ts /userdata/
adb push rk-4.ts /userdata/
adb push rk-5.ts /userdata/
adb push rk-6.ts /userdata/
adb push rk-7.ts /userdata/
adb push rk-8.ts /userdata/
adb push rk-9.ts /userdata/
adb push rk-10.ts /userdata/
adb push rk-11.ts /userdata/
adb push rk-12.ts /userdata/
adb push rk-13.ts /userdata/
adb push rk-14.ts /userdata/
adb push rk-15.ts /userdata/
adb push rk-16.ts /userdata/
adb push rk-17.ts /userdata/
adb push rk-17.ts /userdata/
adb push h265.raw /userdata/

adb push dumpsys /usr/bin/
adb push rk-msch-probe_V1.45_20240228 /usr/bin/rk-msch-probe
adb shell chmod 777 /usr/bin/rk-msch-probe
adb shell chmod 777 /usr/bin/dumpsys
adb shell chmod 777 /userdata/rk_mpi_sample_test

adb push config-bit-nn /userdata/

adb shell chmod 777 /userdata/config-bit-nn/*

pause
