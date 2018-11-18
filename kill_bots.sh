#!/bin/bash

ps aux | grep MyBot | grep -v grep
kill $(ps aux | grep MyBot | grep -v grep | awk '{print $2}') || true
