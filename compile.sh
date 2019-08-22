#! /bin/bash
grev=$(git log | grep commit -m 1)
rev=${grev:7}
sha=$(nix-prefetch-url --unpack https://github.com/p0l0satik/libfluid_base/archive/"$rev".tar.gz)
echo "{ stdenv, fetchFromGitHub, autoconf, automake, libtool, pkgconfig, openssl, libevent }:

stdenv.mkDerivation rec {
  name = \"libfluid_base\";

  src = fetchFromGitHub {
    owner = \"p0l0satik\";
    repo = \"libfluid_base\";
    rev = \"$rev\";
    sha256 = \"$sha\";
  };

  nativeBuildInputs = [ autoconf automake libtool pkgconfig ];
  buildInputs = [ openssl ];
  propagatedBuildInputs = [ libevent ];
  CXXFLAGS = [\"-std=c++11\"];

  preConfigure = ''
    ./autogen.sh
  '';

  enableParallelBuilding = true;
}" > /home/polosatik/R_test/r1/nixpkgs/libfluid_base.nix

echo "{ stdenv, fetchFromGitHub, autoconf, automake, libtool, pkgconfig, openssl, libevent }:

stdenv.mkDerivation rec {
  name = \"libfluid_base\";

  src = fetchFromGitHub {
    owner = \"p0l0satik\";
    repo = \"libfluid_base\";
    rev = \"$rev\";
    sha256 = \"$sha\";
  };

  nativeBuildInputs = [ autoconf automake libtool pkgconfig ];
  buildInputs = [ openssl ];
  propagatedBuildInputs = [ libevent ];
  CXXFLAGS = [\"-std=c++11\"];

  preConfigure = ''
    ./autogen.sh
  '';

  enableParallelBuilding = true;
}" > /home/polosatik/Slicer/dev/runos/nixpkgs/libfluid_base.nix


echo "{ stdenv, fetchFromGitHub, autoconf, automake, libtool, pkgconfig, openssl, libevent }:

stdenv.mkDerivation rec {
  name = \"libfluid_base\";

  src = fetchFromGitHub {
    owner = \"p0l0satik\";
    repo = \"libfluid_base\";
    rev = \"$rev\";
    sha256 = \"$sha\";
  };

  nativeBuildInputs = [ autoconf automake libtool pkgconfig ];
  buildInputs = [ openssl ];
  propagatedBuildInputs = [ libevent ];
  CXXFLAGS = [\"-std=c++11\"];

  preConfigure = ''
    ./autogen.sh
  '';

  enableParallelBuilding = true;
}" > /home/polosatik/Slicer/dev/runos_stable/runos/nixpkgs/libfluid_base.nix
