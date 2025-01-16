#!/bin/sh
ulimit -n 65536
#export rt_log_level=2
echo performance > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor 
echo performance > /sys/devices/system/cpu/cpufreq/policy4/scaling_governor 

echo performance > /sys/class/devfreq/27700000.npu/governor
echo performance > /sys/devices/platform/27800000.gpu/devfreq/27800000.gpu/governor

chmod 777 /userdata/rk_mpi_sample_test

/userdata/rk_mpi_sample_test -i /userdata/config-bit-nn/rk3576-sample_config-64-1080P60_2_1080P60-16-nn.json
