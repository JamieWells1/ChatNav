# Deploy notes

## Firewall (ufw)

`ufw_rules.sh` only prints the proposed rules unless `--apply` is given. No other script calls it.

1. From a machine that can reach the droplet (not the droplet's own provider console), run
   `ssh <alias> 'bash -s -- 8080 8081 8082 8083 8084 8085 8086 8087 8088 8089 8090' < server/deploy/ufw_rules.sh`.
   Pass every port Photon/Delta needs from outside as an argument. The script lists what is
   listening first; compare the two lists.
2. Review the printed rules. Anything listening that is not allowed becomes unreachable.
3. Keep a second ssh session open, then run it again with `--apply` (as root, type `YES`).
   To undo: `ufw disable`.
4. Run `tools/security_evidence.sh --host <droplet> --cacert server.crt` from a different network
   and save the output to `assignment/evidence/m5-privacy/`.

SSH was blocked from the build environment, so none of this has been run on the droplet.
