# CHIP-8 Emulator

Your typical CHIP-8 copy. This one uses SDL2 as a rendering API.

## Dependencies

* `make`
* `SDL2`

## Usage

```shell
$ cd path/to/chip8
$ make
$ cd bin
$ ./chip8 [../path/to/ROM]
```
In order to install do
```shell
$ cd path/to/chip8
$ make
$ sudo make install
$ make clean # optional
```
The binary will be installed at `/usr/local/bin/`

## To Do

* Fix flickering
