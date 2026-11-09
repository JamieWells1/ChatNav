#!/usr/bin/env bash
# Deploys the ChatNav server to the droplet. Run from a network that can reach it.
#
#   server/deploy.sh <ssh-host> <public-host-or-ip> [--dry-run]
#
# Steps: rsync the repo to a staging dir, then run deploy/remote_install.sh as root over
# ssh. Safe to re-run. It never enables or edits ufw and never touches Photon/Delta.
set -euo pipefail

SSH_HOST="${1:?usage: deploy.sh <ssh-host> <public-host-or-ip> [--dry-run]}"
PUBLIC_HOST="${2:?usage: deploy.sh <ssh-host> <public-host-or-ip> [--dry-run]}"
DRY="${3:-}"

here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/.." && pwd)"
stage="/tmp/chatnav-stage"

run() {
    echo "+ $*"
    [ "$DRY" = "--dry-run" ] || "$@"
}

echo "Deploying ChatNav to $SSH_HOST (public name $PUBLIC_HOST)"
echo "Step 1: copy server code to $SSH_HOST:$stage"
run ssh "$SSH_HOST" "mkdir -p $stage"
run rsync -a --delete --exclude .venv --exclude __pycache__ --exclude '*.db' --exclude .env \
    --exclude .pytest_cache --exclude .ruff_cache --exclude '*.egg-info' \
    "$repo/server" "$SSH_HOST:$stage/"
echo "Step 2: run remote_install.sh as root (nine steps, printed as they run)"
run ssh -t "$SSH_HOST" "sudo bash $stage/server/deploy/remote_install.sh '$PUBLIC_HOST' '$stage'"
echo "Done. Enter the printed fingerprint and a device token in the device setup portal."
