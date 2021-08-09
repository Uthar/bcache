{ config, lib, pkgs,  ... }:
with lib;
let
  cfg = config.services.bcache;
in
{

  options.services.bcache = {
    enable = mkOption {
      type = types.bool;
      default = false;
      description = ''
        simple Nix binary cache service
      '';
    };

    socketPath = mkOption {
      type = types.str;
      default = "/run/bcache.sock";
      description = ''
          Path to unix socket to listen on.
        '';
    };

    compressionType = mkOption {
      type = types.enum ["xz" "bzip2" "none" "lz"];
      default = "none";
      description = ''
          Type of compression to use
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

    user = mkOption {
      type = types.str;
      default = "nobody";
      description = ''
        User to run 'bcache' as
      '';
    };

    group = mkOption {
      type = types.str;
      default = "nobody";
      description = ''
        Group to run 'bcache' as
      '';
    };
  };

  config = mkIf cfg.enable {
    systemd.services.bcache = {
      after = [ "network.target" ];
      wantedBy = [ "multi-user.target" ];

      environment.NIX_REMOTE = "daemon";
      environment.NIX_SECRET_KEY_FILE = cfg.secretKeyFile;

      serviceConfig = {
        Restart = "always";
        RestartSec = "5s";
        ExecStart = "${pkgs.bcache}/bin/bcache --socket ${cfg.socketPath} --compression ${cfg.compressionType}";
        User = cfg.user;
        Group = cfg.group;
      };
    };

  };
}
