{ config, lib, pkgs,  ... }:

with lib;

let
  cfg = config.services.bcache;
  bcache = import ../. {};
in
{

  options = {
    services.bcache = {
      enable = mkEnableOption "simple Nix binary cache server";

      socketPath = mkOption {
        type = types.string;
        default = "/run/bcache.sock";
        description = ''
          Path to unix socket to listen on.
        '';
      };

      secretKeyFile = mkOption {
        type = types.nullOr types.str;
        default = null;
        description = ''
          The path to the file used for signing derivation data.
          Generate with:

          ```
          nix-store --generate-binary-cache-key key-name secret-key-file public-key-file
          ```

          Make sure user `nix-serve` has read access to the private key file.

          For more details see <citerefentry><refentrytitle>nix-store</refentrytitle><manvolnum>1</manvolnum></citerefentry>.
        '';
      };
    };

    config = {
      systemd.services.bcache = {
        description = "simple binary cache server";
        after = [ "network.target" ];
        wantedBy = [ "multi-user.target" ];

        path = [ config.nix.package.out pkgs.bzip2.bin ];
        environment.NIX_REMOTE = "daemon";
        environment.NIX_SECRET_KEY_FILE = cfg.secretKeyFile;

        serviceConfig = {
          Restart = "always";
          RestartSec = "5s";
          ExecStart = "${bcache}/bin/bcache";
          User = "bcache";
          Group = "nogroup";
        };
      };

      users.users.bcache = {
        description = "Nix binary cache user";
        uid = config.ids.uids.bcache;
      };
    };

  };
}
