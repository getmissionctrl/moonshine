{
  description = "Moonshine speech-to-text library and CLI";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in {
      packages = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          moonshine = pkgs.callPackage ./nix/package.nix { };
          moonshine-cli = pkgs.callPackage ./nix/cli.nix { inherit moonshine; };
        in {
          default = moonshine-cli;
          inherit moonshine moonshine-cli;
        }
      );

      devShells = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in {
          default = import ./nix/devshell.nix { inherit pkgs; };
        }
      );

      overlays.default = final: prev: {
        moonshine = final.callPackage ./nix/package.nix { };
        moonshine-cli = final.callPackage ./nix/cli.nix {
          moonshine = final.moonshine;
        };
      };
    };
}
