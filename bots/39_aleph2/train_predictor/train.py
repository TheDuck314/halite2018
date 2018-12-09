#!/usr/bin/python3
import struct
import numpy as np
import gzip
import os
import time

import torch
import torch.utils.data as data
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim

class MyDataSet(data.Dataset):
    base_data_dir = "/home/greg/coding/halite/2018/training_data/preprocessed_games"

    def __init__(self, num_players):
        self.data_dir = os.path.join(self.base_data_dir, "{}p".format(num_players))
        self.load_all_games()

    def load_all_games(self):
        all_inputs = []
        all_outputs = []
        num_files = 0
        for bn in sorted(os.listdir(self.data_dir))[-300:]:
            fn = os.path.join(self.data_dir, bn)
            inputs, outputs = self.load_one_game(fn)
            all_inputs.append(inputs)
            all_outputs.append(outputs)
            num_files += 1
        print("loaded {} files".format(num_files))
        self.inputs = np.concatenate(all_inputs, axis=0)
        #self.inputs[:, 75:] = 0
        self.outputs = np.concatenate(all_outputs, axis=0)
        print("loaded {} instances".format(len(self)))
        print("self.inputs.shape = {}".format(self.inputs.shape))
        print("self.outputs.shape = {}".format(self.outputs.shape))
        print("self.inputs[0] = {}".format(self.inputs[0]))

    def __len__(self):
        return self.inputs.shape[0]
#        return 512

    def __getitem__(self, index):
        return self.inputs[index], self.outputs[index]

    def load_one_game(self, fn):
        #print("loading {}".format(fn))
        assert fn.endswith(".bin.gz"), fn
        with gzip.open(fn, "rb") as f:
            def read_int():
                return struct.unpack('i', f.read(4))[0]
            
            def read_int_tuple(n):
                ret = []
                for _ in range(n):
                    ret.append(read_int())
                return tuple(ret)

            def read_tensor_shape():
                rank = read_int()
                shape = read_int_tuple(rank)
                return shape

            def read_float32_tensors(n, shape):
                num_floats = n
                for s in shape:
                    num_floats *= s
                buffer = f.read(num_floats * 4)
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
            input_shape = read_tensor_shape()
            output_shape = read_tensor_shape()
            #print("input_shape = {}".format(input_shape))
            #print("output_shape = {}".format(output_shape))
            inputs  = read_float32_tensors(num_instances, input_shape)
            outputs = read_float32_tensors(num_instances, output_shape)
        assert inputs.shape[0] == outputs.shape[0], (inputs.shape, outputs.shape)
        return inputs, outputs


class Net(nn.Module):
    def __init__(self):
        super(Net, self).__init__()
        Nhidden_1 = 512
        Nhidden_2 = 16
        self.fc1 = nn.Linear(7 * (5 * 5) + 1, Nhidden_1)
        self.fc2 = nn.Linear(Nhidden_1, Nhidden_2)
        self.fc_last = nn.Linear(Nhidden_2, 1)

    def forward(self, x):
        # immedately flatten
        x = F.relu(self.fc1(x))
        x = F.relu(self.fc2(x))
        x = self.fc_last(x)
        # add a logistic??
        return x


num_players = 4
dataset = MyDataSet(num_players)

validation_frac = 0.1
validation_shuffle_seed = 1234

dataset_indices = list(range(len(dataset)))
np.random.seed(validation_shuffle_seed)
np.random.shuffle(dataset_indices)
validation_split = int(validation_frac * len(dataset))
validation_indices = dataset_indices[:validation_split]
train_indices = dataset_indices[validation_split:]
validation_set = data.Subset(dataset, validation_indices)
train_set = data.Subset(dataset, train_indices)
print("split: train set = {} instances; val set = {} instances".format(len(train_set), len(validation_set)))

minibatch_size = 512
learning_rate = 0.01
momentum =  0.9

train_loader = torch.utils.data.DataLoader(train_set, batch_size=minibatch_size, drop_last=False, shuffle=True)
validation_loader = torch.utils.data.DataLoader(validation_set, batch_size=minibatch_size, drop_last=False, shuffle=True)

use_gpu = False

net = Net()
if use_gpu:
    device = torch.device("cuda:0")
    print(device)
    net.to(device)

loss_fn = nn.MSELoss()  # TODO MSELoss is probably bad
optimizer = optim.SGD(net.parameters(), lr=learning_rate, momentum=momentum)  # TODO experiment with optimizer

model_fn = "/home/greg/coding/halite/2018/repo/bots/36_predictor/safety_model.pt"
print("After each epoch will save to:  {}".format(model_fn))

start_time = time.time()
total_minibatches = 0
for epoch in range(1, 1000+1):
    running_loss = 0.0
    interval = 0
    for mb, (inputs, true_outputs) in enumerate(train_loader, 1):
        if use_gpu:
            # send tensors to GPU
            inputs, true_outputs = inputs.to(device), true_outputs.to(device)

        optimizer.zero_grad()

        outputs = net(inputs)
        loss = loss_fn(outputs, true_outputs)
        loss.backward()
        optimizer.step()

        total_minibatches += 1
        interval += 1
        running_loss += loss.item()

    now = time.time()
    mb_per_sec = total_minibatches / (now - start_time)
    mean_training_loss = running_loss / interval
    running_loss = 0.0
    train_mbs = mb

    # run on validation set
    with torch.no_grad():
        val_loss_sum = 0.0
        val_loss_denom = 0
        val_num_pos_examples = 0
        val_num_neg_examples = 0
        val_pos_example_output_sum = 0.0
        val_neg_example_output_sum = 0.0
        for mb, (inputs, true_outputs) in enumerate(validation_loader, 1):
            outputs = net(inputs)
            loss = loss_fn(outputs, true_outputs)

            val_loss_sum += loss.item()
            val_loss_denom += 1

            val_num_pos_examples += int(true_outputs.sum().item())
            val_num_neg_examples += int((1 - true_outputs).sum().item())
            val_pos_example_output_sum += (true_outputs * outputs).sum().item()
            val_neg_example_output_sum += ((1 - true_outputs) * outputs).sum().item()

    print("epoch {} train: mbs {}  mean loss {:.6f} mb/sec={:.1f}   val: mbs {} mean loss {:.6f}  #+ex={} +exmeanout={:.6f} #-ex={} -exmeanout={:.6f}"
          .format(epoch, train_mbs, mean_training_loss, mb_per_sec,
                  val_loss_denom, val_loss_sum / val_loss_denom,
                  val_num_pos_examples, val_pos_example_output_sum / val_num_pos_examples,
                  val_num_neg_examples, val_neg_example_output_sum / val_num_neg_examples))


    # export
    example_batch_input, _ = train_set[0]
    example_batch_input = torch.tensor(example_batch_input)
    traced_script_module = torch.jit.trace(net, example_batch_input)
    traced_script_module.save(model_fn)

"""
# testing
dataiter = iter(loader)
while True:
    inputs, true_outputs = dataiter.next()
    
    print("inputs:")
    print(inputs)
    true_outputs = true_outputs.numpy()

    outputs = net(inputs)
    outputs = outputs.detach()
    outputs = outputs.cpu()
    outputs = outputs.numpy()
    print("true outputs and net outputs:")
    print(np.hstack([true_outputs, outputs]))

    input()
"""
