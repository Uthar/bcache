with import <nixos> {};

stdenv.mkDerivation {
  pname="bcache";
  version="0.1.0-f8518f700b";
  src= fetchfossil {
    url = "https://fossil.galkowski.xyz/bcache";
    rev = "f8518f700b683a5ec5beb870386816d2b1fd3a036a6aad23afb3c50edccb6108";
    sha256 = "0z9mq60xzw4l08g1bzniqlba2rqaq2f20wfjfd1a2irvkdp2f8w1";
  };
  buildInputs = [ makeWrapper nixUnstable.dev boost.dev nlohmann_json ];
  preBuild=''
    export MAKEFLAGS="$MAKEFLAGS -j$NIX_BUILD_CORES"
  '';
  installPhase=''
    mkdir -pv $out/bin
    cp -v bcache $out/bin
  '';
}
