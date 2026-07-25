{
  pkgs ? import <nixpkgs> { },
}:
pkgs.mkShell {
  packages = with pkgs; [
    cmake
    gcc
    clang-tools
    cmake-language-server
  ];
}
