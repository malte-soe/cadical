{ lib
, cmake
, llvmPackages_11
}:

llvmPackages_11.stdenv.mkDerivation rec {
  pname = "cadical";
  version = "0.1.0";

  src = ./.;

  nativeBuildInputs = [ cmake ];
  shellHook = ''
    export MACOSX_DEPLOYMENT_TARGET="10.15"
    mkdir -p build && cd build && cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=YES && cd ..
  '';
}
