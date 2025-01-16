mkdir -p /userdata/rockiva_data

export LD_LIBRARY_PATH=/rockchip-test/npu2/rknn_demo/

echo performance | tee $(find /sys/ -name *governor) > /dev/null 2>&1 || true

/etc/init.d/S49weston stop
sync
cp -pf object_detection_v3_cls8.data /userdata/rockiva_data
cp -pf rk_mpi_sample_test /userdata/
cp -rpf config-bit-nn /userdata/
cp -pf rk-*ts /userdata/
chmod 777 /userdata/config-bit-nn/*
sync

COMPATIBLE=$(cat /proc/device-tree/compatible)
if [[ $(expr $COMPATIBLE : ".*rk3588") -ne 0 ]]; then
        rknn_common_test /usr/share/model/RK3588/mobilenet_v1.rknn /usr/share/model/dog_224x224.jpg 10
elif [[ $(expr $COMPATIBLE : ".*rk3568") -ne 0 ]]; then
        rknn_common_test /usr/share/model/RK3566_RK3568/mobilenet_v1.rknn /usr/share/model/dog_224x224.jpg 10
elif [[ $(expr $COMPATIBLE : ".*rk3566") -ne 0 ]]; then
        rknn_common_test /usr/share/model/RK3566_RK3568/mobilenet_v1.rknn /usr/share/model/dog_224x224.jpg 10
elif [[ $(expr $COMPATIBLE : ".*rk3562") -ne 0 ]]; then
        rknn_common_test /usr/share/model/RK3562/mobilenet_v1.rknn /usr/share/model/dog_224x224.jpg 10
elif [[ $(expr $COMPATIBLE : ".*rk3576") -ne 0 ]]; then
	#/userdata/config-bit-nn/run_test_1080p-2_1080p.sh
	/userdata/config-bit-nn/evb1-run_test_1080p-49-D1-16NN.sh
else
        echo "The RKNPU2 did't support this Socs yet..."
fi
