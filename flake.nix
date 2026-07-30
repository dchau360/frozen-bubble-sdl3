# Based on the template from the sweepr repo: https://github.com/Grazen0/sweepr/blob/main/flake.nix
{
  description = "SDL3 C++ Port of Frozen-Bubble 2";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-26.05";  # 24.11 has no sdl3-ttf or sdl3-mixer
  };

  outputs =
    {
      self,
      nixpkgs,
    }:
    let
      forAllSystems = nixpkgs.lib.genAttrs [
        "aarch64-linux"
        "i686-linux"
        "x86_64-linux"
        "aarch64-darwin"
        "x86_64-darwin"
      ];

      pkgsFor = nixpkgs.legacyPackages;
    in
    {
      packages = forAllSystems (system: rec {
        frozen-bubble = pkgsFor.${system}.callPackage ./default.nix { };
        default = frozen-bubble;
      });

      devShells = forAllSystems (system: {
        default = pkgsFor.${system}.callPackage ./shell.nix { };
      });
    };
}
