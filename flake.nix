{
  description = "my project description";

  inputs.nixpkgs.url = "nixpkgs/nixpkgs-unstable";
  inputs.flake-utils.url = "github:numtide/flake-utils";
  inputs.flake-utils.inputs.nixpkgs.follows = "nixpkgs";

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem
      (
        system:
          let
            pkgs = nixpkgs.legacyPackages.${system};
          in
            {
              name = "cadical";
              defaultPackage = pkgs.callPackage ./default.nix {};
              devShell = pkgs.callPackage ./default.nix {};
            }
      );
}
