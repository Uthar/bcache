with import <nixos> {};

stdenv.mkDerivation {
  pname="bcache";
  version="0.1.0-20210612-c2e0cffd93";
  src= fetchfossil {
    url = "https://fossil.galkowski.xyz/bcache";
    rev = "c2e0cffd934fc84006d9e84c249557d091efc8a847c39aeebdafb6abdefd2acc";
    sha256 = "19ibd7s5rkam70xjga82cfhwdh4ydfxb485y1ngfg2017wsdn7mv";
  };
  buildInputs = [ makeWrapper nixUnstable.dev boost.dev nlohmann_json zstd ];
  preBuild=''
    export MAKEFLAGS="$MAKEFLAGS -j$NIX_BUILD_CORES"
  '';
  installPhase=''
    mkdir -pv $out/bin
    cp -v bcache $out/bin
  '';
}
