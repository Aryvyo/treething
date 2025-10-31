{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  packages = with pkgs; [
    wayland
    wayland-protocols
    pkg-config

    gcc gdb strace
 ];

}

