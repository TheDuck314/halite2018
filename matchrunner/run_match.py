#!/usr/bin/python
import subprocess
import random
import datetime
import os
import json
import glob
import argparse
from multiprocessing import Process, Queue
import socket
import sys
import time

LOCAL_HALITE_BINARY = "/home/greg/coding/halite/2018/starter_kit_cpp/halite"
EC2_HALITE_BINARY = "/home/ec2-user/halite"

EC2_RUNMATCH = os.path.join("/home/ec2-user", os.path.basename(__file__))

def on_ec2():
    return socket.gethostname() != "home"

def get_halite_binary():
    if on_ec2():
        return EC2_HALITE_BINARY
    else:
        return LOCAL_HALITE_BINARY
    
def get_model_fn(bot_binary):
    bot_dir = os.path.dirname(bot_binary)
    assert os.path.basename(bot_dir) == "build", bot_dir
    bot_dir = os.path.dirname(bot_dir)
    model_fn = os.path.join(bot_dir, "safety_model.pt")
    if os.path.isfile(model_fn):
        return model_fn
    else:
        return None

def with_predictor_path(binary):
    bot_dir = os.path.dirname(binary)
    assert os.path.basename(bot_dir) == "build", bot_dir
    bot_dir = os.path.dirname(bot_dir)
    model_fn = os.path.join(bot_dir, "safety_model.pt")
    predictor_dir_param = "PREDICTOR_MODEL={}".format(model_fn)
    return " ".join([binary, predictor_dir_param])

LOCAL_BOT_BINARIES = {
    "Zeta":    "/home/greg/coding/halite/2018/repo/bots/5_zeta/build/MyBot",
    "Eta":     "/home/greg/coding/halite/2018/repo/bots/6_eta/build/MyBot",
    "Theta":   "/home/greg/coding/halite/2018/repo/bots/7_theta/build/MyBot",
    "Iota":    "/home/greg/coding/halite/2018/repo/bots/8_iota/build/MyBot",
    "Kappa":   "/home/greg/coding/halite/2018/repo/bots/9_kappa/build/MyBot",
    "Lambda":  "/home/greg/coding/halite/2018/repo/bots/10_lambda/build/MyBot",
    "Mu":      "/home/greg/coding/halite/2018/repo/bots/11_mu/build/MyBot",
    "Nu":      "/home/greg/coding/halite/2018/repo/bots/12_nu/build/MyBot",
    "Xi":      "/home/greg/coding/halite/2018/repo/bots/13_xi/build/MyBot",
    "Omicron": "/home/greg/coding/halite/2018/repo/bots/14_omicron/build/MyBot",
    "Pi":      "/home/greg/coding/halite/2018/repo/bots/15_pi/build/MyBot",
    "Rho":     "/home/greg/coding/halite/2018/repo/bots/16_rho/build/MyBot",
    "Sigma":   "/home/greg/coding/halite/2018/repo/bots/17_sigma/build/MyBot",
    "Tau":     "/home/greg/coding/halite/2018/repo/bots/18_tau/build/MyBot",
    "Upsilon": "/home/greg/coding/halite/2018/repo/bots/19_upsilon/build/MyBot",
    "Phi":     "/home/greg/coding/halite/2018/repo/bots/20_phi/build/MyBot",
    "Chi":     "/home/greg/coding/halite/2018/repo/bots/21_chi/build/MyBot",
    "Psi":     "/home/greg/coding/halite/2018/repo/bots/22_psi/build/MyBot",
    "Omega":   "/home/greg/coding/halite/2018/repo/bots/23_omega/build/MyBot",
    "OmegaPlusOne":  "/home/greg/coding/halite/2018/repo/bots/24_omegaplusone/build/MyBot",
    "OmegaTimesTwo": "/home/greg/coding/halite/2018/repo/bots/28_omegatimestwo/build/MyBot",
    "OmegaSquared":  "/home/greg/coding/halite/2018/repo/bots/29_omegasquared/build/MyBot",
    "Omega^Omega":   "/home/greg/coding/halite/2018/repo/bots/30_powomegaomega/build/MyBot",
    "Epsilon_0":     "/home/greg/coding/halite/2018/repo/bots/31_epsilon0/build/MyBot",
    "Gamma_0":       "/home/greg/coding/halite/2018/repo/bots/32_gamma0/build/MyBot",
    "SmallVeblen":   "/home/greg/coding/halite/2018/repo/bots/33_smallveblen/build/MyBot",
    "LargeVeblen":   "/home/greg/coding/halite/2018/repo/bots/34_largeveblen/build/MyBot",
    "ChurchKleene":  "/home/greg/coding/halite/2018/repo/bots/35_churchkleene/build/MyBot",
    "Predictor":     with_predictor_path("/home/greg/coding/halite/2018/repo/bots/36_predictor/build/MyBot"),
    "Omega_1":       with_predictor_path("/home/greg/coding/halite/2018/repo/bots/37_omega1/build/MyBot"),
    "Aleph_1":       with_predictor_path("/home/greg/coding/halite/2018/repo/bots/38_aleph1/build/MyBot"),
}

def get_ec2_dir(local_fn):
    local_dir = os.path.dirname(local_fn)
    if os.path.basename(local_dir) == "build":
        local_dir = os.path.dirname(local_dir)
    remote_dir = os.path.join("/home/ec2-user", os.path.basename(local_dir))
    return remote_dir

def local_filename_to_ec2(local_fn):
    return os.path.join(get_ec2_dir(local_fn), os.path.basename(local_fn))

def get_bot_command(local_bot_command):
    if on_ec2():
        parts = local_bot_command.split()
        parts[0] = local_filename_to_ec2(parts[0])
        for i in range(1, len(parts)):
            if parts[i].startswith("PREDICTOR_MODEL="):
                [key, val] = parts[i].split("=")
                val = local_filename_to_ec2(val)
                parts[i] = "=".join([key, val])
        return " ".join(parts)
    else:
        return local_bot_command

def run_game(game_dir, seed, mapsize, bots, bot_to_command):
    command = [
        get_halite_binary(),
        "--results-as-json",
        "-s", str(seed),
        "--replay-directory", game_dir,
    ]
    if mapsize:
        command.extend([
            "--width", str(mapsize),
            "--height", str(mapsize),
        ])

    for bot in bots:
        command.append(get_bot_command(bot_to_command[bot]))
    #print "GAME COMMAND:    {}".format(command)

    if on_ec2():
        # make sure bots can find torch libraries
        os.environ["LD_LIBRARY_PATH"] = os.environ.get("LD_LIBRARY_PATH", "") + ";/home/ec2-user/libtorch/lib"
        # make torch only use one thread otherwise things get super slow!
        os.environ["OMP_NUM_THREADS"] = "1"
        os.environ["MKL_NUM_THREADS"] = "1"

    with open(os.path.join(game_dir, "command.txt"), "w") as f:
        f.write("\n".join(command) + "\n")
    results_string = subprocess.check_output(command).decode()
    with open(os.path.join(game_dir, "results.json"), "w") as f:
        f.write(results_string)

    replay_glob = os.path.join(game_dir, "replay-*.hlt")
    replays = glob.glob(replay_glob)
    assert len(replays) == 1, replay_glob
    #print(replays[0])

    # todo: on ec2, auto-rm log and hlt files

    results = json.loads(results_string)

    # first check if anybody errored out. if so start screaming
    #print results["terminated"]
    for _, terminated in results["terminated"].iteritems():
        if terminated:
            while True:
                formatted_command_line = ""
                for part in command:
                    if " " in part:
                        formatted_command_line += ' "' + part + '"'
                    else:
                        formatted_command_line += " " + part
                print "GAME HAD A TERMINATION: {}".format(formatted_command_line)
                time.sleep(1)

    rank_to_bot = {}
    for player_id, stats in results["stats"].items():
        rank_to_bot[int(stats["rank"])] = bots[int(player_id)]
    #print("  ".join("{}: {}".format(rank, bot) for rank, bot in rank_to_bot.items()))
    return rank_to_bot


def worker(input_q, output_q, match_dir, bot_to_command):
    """ runs games indefinitely and puts the results in a Queue """
    print("worker start match_dir={}".format(match_dir))
    while True:
        (game_num, seed, mapsize, bots) = input_q.get()        
        game_dir = os.path.join(match_dir, str(game_num))
        os.mkdir(game_dir)
        os.chdir(game_dir)  # so bot logs go in the game dir
        #print("running game {} in {}".format(game_num, game_dir))
        result = run_game(game_dir, seed, mapsize, bots, bot_to_command)
        output_q.put(result)



def print_bot_to_stats(bots, bot_to_stats):
    print("--------------------")
    for bot in bots:
        line = "{:>55}".format(bot)
        for num_players in [2, 4]:
            rank_to_counts = bot_to_stats[bot][num_players]
            count_str = "-".join(map(str, ["{:>2}".format(rank_to_counts[rank]) for rank in range(1, num_players+1)]))
            line += "    {}p:  {}".format(num_players, count_str)
        print(line)
#            for rank in range(1, num_players+1):
#                print("    Rank {}:  {} times".format(rank, [rank]))
    print("--------------------")

def get_seed():
    return random.randint(1, 999999)

def get_num_players(force_player_count):
    if force_player_count is not None:
        return force_player_count
    else:
        # The matchmaking server chooses 2 player and 4 player games with
        # equal probability. So conditional on us being in the game, there's
        # a 2/3 chance it's a four-player game.
        return 2 if random.random() < 1.0/3.0 else 4


EC2_KEYPAIR_FN = "/home/greg/coding/halite/2018/ec2/awskeypair1.pem"
EC2_SPOT_REQUEST_TOKEN = "SpotRequestClientToken34"  # submitting another request won't do anything unless you increment this token
EC2_INSTANCE_DNS_NAME = None

def get_spot_requests():
    command = [
        "/home/greg/.local/bin/aws",
        "ec2",
        "describe-spot-instance-requests"
    ]
    output = subprocess.check_output(command).decode()
    return json.loads(output)


def check_for_a_fulfilled_spot_request():
    requests = get_spot_requests()
    fulfilled_requests = []
    print("status of spot requests:")
    for req in requests["SpotInstanceRequests"]:
        print("{} {} {}".format(req["SpotInstanceRequestId"], req["Status"]["UpdateTime"], req["Status"]["Code"]))
        if req["Status"]["Code"] == "fulfilled":
            fulfilled_requests.append(req)
    if len(fulfilled_requests) > 2:
        raise Exception("found multiple fulfilled spot instance requests: {}".format(fulfilled_requests))
    elif len(fulfilled_requests) == 1:
        return fulfilled_requests[0]
    else:
        print("No fulfilled spot requests!")
        return None

def get_instance_info(instance_id):
    command = [
        "/home/greg/.local/bin/aws",
        "ec2",
        "describe-instances",
        "--instance-ids", instance_id
    ]
    output = subprocess.check_output(command).decode()
    return json.loads(output)

def get_instance_dns_name(instance_id):
    return get_instance_info(instance_id)["Reservations"][0]["Instances"][0]["PublicDnsName"]

def check_for_spot_instance():
    request = check_for_a_fulfilled_spot_request()
    if request is None:
        return None
    instance_id = request["InstanceId"]
    return get_instance_dns_name(instance_id)

def request_spot_instance():
    command = [
        "/home/greg/.local/bin/aws",
        "ec2",
        "request-spot-instances",
        "--client-token", EC2_SPOT_REQUEST_TOKEN,
        "--launch-specification", "file:///home/greg/coding/halite/2018/ec2/spot_launch_specification.json"
    ]
    output = subprocess.check_output(command).decode()
    print("Requested a spot instance:")
    print(json.loads(output))
    return output

def request_spot_instance_and_wait_until_fulfilled():
    global EC2_INSTANCE_DNS_NAME
    print "Finding or requesting a spot instance..."
    instance_dns_name = check_for_spot_instance()    
    if instance_dns_name is None:
        print("Didn't find any existing instance. Will request one.")
        request_spot_instance()
        while True:
            instance_dns_name = check_for_spot_instance()
            if instance_dns_name:
                break
            print("no instance yet, sleeping...")
            time.sleep(3)
        print("OK we got a new instance. Try this command to ssh into it:")
        print
        print("ssh -i {} ec2-user@{}".format(EC2_KEYPAIR_FN, instance_dns_name))
        print
        print("Once you can successfully ssh into it, press enter to continue.")
        raw_input()
    print("This instance is ready to use:  {}".format(instance_dns_name))
    EC2_INSTANCE_DNS_NAME = "ec2-user@" + instance_dns_name

def do_scp(src, dest):
    cmd = [
        "rsync",
        "-e", "ssh -i {}".format(EC2_KEYPAIR_FN),
        src,
        "{}:{}".format(EC2_INSTANCE_DNS_NAME, dest)
    ]
    print "DOING RSYNC:    {}".format(" ".join(cmd))
    subprocess.check_output(cmd)

def create_ec2_dir(ec2_dir):
    cmd = [
        "ssh",
        "-i", EC2_KEYPAIR_FN,
        EC2_INSTANCE_DNS_NAME,
        "mkdir -p {}".format(ec2_dir)
    ]
    print "CREATING EC2 DIR WITH THIS COMMAND:     {}".format(" ".join(cmd))
    subprocess.check_output(cmd)

def run_script_on_ec2(local_script_fn):
    ec2_script_fn = os.path.join("/home/ec2-user", os.path.basename(local_script_fn))
    do_scp(local_script_fn, ec2_script_fn)
    cmd = [
        "ssh",
        "-i", EC2_KEYPAIR_FN,
        EC2_INSTANCE_DNS_NAME,
        ec2_script_fn
    ]
    print "RUNNING SCRIPT ON EC2:  {}".format(" ".join(cmd))
    subprocess.check_output(cmd)

def install_torch_on_ec2():
    print "Installing torch on ec2."
    run_script_on_ec2("/home/greg/coding/halite/2018/repo/matchrunner/install_torch_on_ec2.sh")
    print "Dpne installing torch on ec2."

def kill_ec2_bot_processes():
    # super hacky
    # if we previously were running a match which we ctrl-c'd then there will be orphaned
    # MyBot processes still around. This will cause scp to fail when it tries to overwrite
    # their binaries. So first, we need to kill the MyBot processes
    print "Killing remote MyBot processes."
    run_script_on_ec2("/home/greg/coding/halite/2018/repo/kill_bots.sh")
    print "Done killing remote MyBot processes."

def relaunch_self_on_ec2():
    remote_runmatch_cmd = list(sys.argv)
    remote_runmatch_cmd[0] = EC2_RUNMATCH
    remote_runmatch_cmd = " ".join(remote_runmatch_cmd)
    cmd = [
        "ssh",
        "-tt",  # makes output line buffered?
        "-i", EC2_KEYPAIR_FN,
        EC2_INSTANCE_DNS_NAME,
        remote_runmatch_cmd
    ]
    subprocess.Popen(cmd).wait()
    sys.exit(0)

def copy_bot_files_to_ec2(bot_to_command, bots):
    local_binaries_used = sorted(set(bot_to_command[bot].split()[0] for bot in bots))
    for local_bin in local_binaries_used:
        create_ec2_dir(get_ec2_dir(local_bin))
        do_scp(local_bin, local_filename_to_ec2(local_bin))
        local_model_fn = get_model_fn(local_bin)
        if local_model_fn:
            do_scp(local_model_fn, local_filename_to_ec2(local_model_fn))


def launch_on_ec2(bot_to_command, bots):
    print "PREPARING EC2 RUN."
    request_spot_instance_and_wait_until_fulfilled()

    kill_ec2_bot_processes()

    print "COPYING BINARIES:"
    # copy the halite game binary
    do_scp(LOCAL_HALITE_BINARY, EC2_HALITE_BINARY)
    # copy bot binaries and model files
    copy_bot_files_to_ec2(bot_to_command, bots)
    # install torch
    install_torch_on_ec2()
    # copy this run_match.py file
    do_scp(__file__, EC2_RUNMATCH)

    print "ok, now going to relaunch myself on ec2"
    relaunch_self_on_ec2()

def main():
    if on_ec2():
        print "RUN_MATCH STARTING UP ON EC2"

    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--players", type=int, help="force a given number of players")
    parser.add_argument("--mapsize", type=int, help="force a given map size")
    parser.add_argument("--ec2", action="store_true", help="run in ec2")
    args = parser.parse_args()

    bot_to_command = LOCAL_BOT_BINARIES.copy()

    challenger_bots = ["Aleph_1"]
    ref_bot = "Omega_1"
    """
    challenger_base = "Aleph_1"
    param = "MINING_DIST_FROM_ME_MULT"
    values = [1.0, 1.25, 1.5, 1.75, 2.0]
#    param = "SHIPS_PER_LATER_DROPOFF"
#    values = [16, 18, 20, 22, 24]
    challenger_bots = []
    for val in values:
        override = param + "=" + str(val)
        challenger_bot = challenger_base + "." + override       
        challenger_bots.append(challenger_bot)
        bot_to_command[challenger_bot] = bot_to_command[challenger_base] + " " + override

    ref_bot = challenger_bots.pop()
    #ref_bot = "SmallVeblen"
    """

    bots = challenger_bots + [ref_bot]
    assert challenger_bots


    if args.ec2 and not on_ec2():
        launch_on_ec2(bot_to_command, bots)
        

    bot_to_stats = {}
    for bot in bots:
        bot_to_stats[bot] = {}
        for num_players in [2, 4]:
            bot_to_stats[bot][num_players] = {}
            for rank in range(1, num_players+1):
                bot_to_stats[bot][num_players][rank] = 0

#    match_dir = os.path.join(os.getcwd(), "matches", "{}_{}".format(datetime.datetime.now().strftime("%Y%m%d_%H.%M.%S"), "_".join(bots)))
    match_superdir = os.path.join(os.getcwd(), "matches")
    if not os.path.isdir(match_superdir):
        os.mkdir(match_superdir)
    match_dir = os.path.join(match_superdir, datetime.datetime.now().strftime("%Y%m%d_%H.%M.%S"))
    assert not os.path.isdir(match_dir)
    os.mkdir(match_dir)

    game_num = 0

    input_q = Queue()
    output_q = Queue()
    if on_ec2():
        num_workers = 40
    else:
        num_workers = 3
    for _ in range(num_workers):
        t = Process(target=worker,
                    args=(input_q, output_q, match_dir, bot_to_command))
        t.daemon = True
        t.start()

    while True:
        # if necessary, queue up more games for the workers to play
        if input_q.qsize() < 3 * num_workers:
            print("queue is small, adding jobs")
            for _ in range(3 * num_workers):
                # Queue up one game for each challenger bot. All the games will use the same seed
                # and the challenger bot will have the same player id in each game, to reduce
                # variance.
                seed = get_seed()  # each challenger plays a game on the same seed, to reduce variance
                num_players = get_num_players(args.players)
                game_bots_template = [ref_bot] * num_players
                challenger_pid = random.randint(0, num_players - 1)
                for challenger_bot in challenger_bots:
                    game_bots = list(game_bots_template)
                    game_bots[challenger_pid] = challenger_bot
                    game_spec = (game_num, seed, args.mapsize, game_bots)
                    input_q.put(game_spec)
                    game_num += 1

        # get the results of a game and update our stats
        rank_to_bot = output_q.get()
        num_players = len(rank_to_bot)
        for rank, bot in rank_to_bot.items():
            bot_to_stats[bot][num_players][rank] += 1

        print_bot_to_stats(bots, bot_to_stats)


if __name__ == "__main__":
    main()
