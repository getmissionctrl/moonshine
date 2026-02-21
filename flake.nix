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
          # Use fetchgit with lfs=true to properly fetch Git LFS files
          # (model .ort files, speaker-embedding-model-data.cpp)
          src = pkgs.fetchgit {
            url = "https://github.com/getmissionctrl/moonshine";
            rev = "a735e244a2a47f9b9d7ea750acaf819bd4326754";
            hash = "sha256-5FqOJydch9e+IIA5KIvRm/RwzunwYEMhCeQ+uMf6Lro=";
            fetchLFS = true;
          };
          moonshine = pkgs.callPackage ./nix/package.nix { inherit src; };
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
        moonshine = final.callPackage ./nix/package.nix {
          src = final.fetchgit {
            url = "https://github.com/getmissionctrl/moonshine";
            rev = "a735e244a2a47f9b9d7ea750acaf819bd4326754";
            hash = "sha256-5FqOJydch9e+IIA5KIvRm/RwzunwYEMhCeQ+uMf6Lro=";
            fetchLFS = true;
          };
        };
        moonshine-cli = final.callPackage ./nix/cli.nix {
          moonshine = final.moonshine;
        };
      };
    };
}
