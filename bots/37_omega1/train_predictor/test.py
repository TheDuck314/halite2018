#!/usr/bin/python3
import struct
import numpy as np
import gzip
import sys
import io

fn = "/home/greg/coding/halite/2018/training_data/preprocessed_games/2752177.pid3.bin.gz"

assert fn.endswith(".bin.gz")
with gzip.open(fn, "rb") as f:
    def read_int():
        return struct.unpack('i', f.read(4))[0]
    
    def read_int_tuple(n):
        ret = []
        for _ in range(n):
            ret.append(read_int())
        return tuple(ret)

    def read_float32_tensors(n, shape):
        num_floats = n
        for s in shape:
            num_floats *= s
        #flat_data = np.fromfile(f, dtype=np.float32, count=num_floats)
        buffer = f.read(num_floats * 4)
        print(type(buffer))
        flat_data = np.frombuffer(buffer, dtype=np.float32, count=num_floats)
        return flat_data.reshape((n,) + shape)

    # format is described in PredictorDataCollector::save_training_data():
    #    // HEADER:
    #    // - int32: number of instances
    #    // - three int32's: shape of input tensor
    #    // - one int32: shape of output tensor
    #    // INPUTS: one rank-3 tensor of float32's for each instance
    #    // OUTPUTS: one size (1,) tensor for each instance
    num_instances = read_int()
    input_shape = read_int_tuple(3)
    output_shape = read_int_tuple(1)



    print(num_instances)
    print(input_shape)
    print(output_shape)

    inputs  = read_float32_tensors(num_instances, input_shape)
    outputs = read_float32_tensors(num_instances, output_shape)
    print(inputs)
    print()
    print(outputs)
    print(outputs.mean())