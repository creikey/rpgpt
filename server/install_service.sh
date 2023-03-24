#!/usr/bin/env bash
systemctl stop rpgpt
cp rpgpt.service /etc/systemd/system/
echo "Installed. 'systemctl start rpgpt' to run it"
