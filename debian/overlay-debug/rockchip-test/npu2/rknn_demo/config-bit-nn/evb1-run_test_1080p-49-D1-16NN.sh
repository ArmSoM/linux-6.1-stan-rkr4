#!/bin/sh
ulimit -n 65536
#export rt_log_level=2

chmod 777 /userdata/rk_mpi_sample_test

/userdata/rk_mpi_sample_test -i /userdata/config-bit-nn/evb1-rk3576-sample_config-49-1080p_16-nn.json
