# TFTP
Free TFTP client and server for Windows, with [RFC2348](https://tools.ietf.org/html/rfc2348) (Blocksize option) and [RFC2349](https://tools.ietf.org/html/rfc2349) (Timeout interval, Transfer size option).

The binaries and code found in this repository work, but this project is old and not actively maintained (feature-complete, nothing else to do except cosmetics).

## Why
TFTP is simple. It is still widely used in embedded software R&D environments to bootstrap or update devices because of this simplicity. TFTP is also one of the transfer options supported by u-boot.
Unfortunately the basic TFTP protocol [RFC1350](https://tools.ietf.org/html/rfc1350) is not very good at transfer speeds, even on 100% error-free links where none of the UDP packets are lost.
Especially the RFC2348 Blocksize option can massively improve throughput.

* Several Windows variants either ship with no TFTP client/server at all or they do not support the RFC2348/RFC2349 options
* other 3rd-party TFTP solutions
.* cost money
.* also lack support for these options
.* have horrible UIs or dependencies

So we wrote what we needed ourselves.

## Prebuilt binaries
The Binaries directory contains pre-built binaries of the tftp client and server executables.
__**These binaries should be code-signed with an Authenticode certificate. At least verify the "Digital Signature" if you just download the binaries instead of building the code yourself.**__
If the signatures are missing or broken, someone managed to break GitHub or take over this repository.
These are 32bit binaries. If you think that you need 64bit binaries, TFTP is quite definitely not what you need.

## How to use the server
The server always listens on UDP port 69 and provides remote access to the working directory where the server has been started. 
The server will refuse requests for all files requests with "..", slashes and colons due to security reasons. 
Without these restrictions, all files on the host system would be accessible via TFTP. 

If you just double-click the executable, the server will serve everything in and below the path where the executable is located.
When you create a shortcut to the executable instead, Windows allows you to explicitly the working directory. On english versions, this is the "Start in" field.

The server allows up- and downloads and displays progress information on the console.

## Security / Code quality
This is an engineering tool that is ~10 years old. It was never meant to be more than an engineering tool. Do not run the TFTP server on a machine that is reachable over the internet unless 
you are absolutely sure that you know what you are doing.

## How to use the client
The client can replace the standard windows TFTP client. The commandline is compatible to the TFTP.EXE that ships with some of the OS versions with just a few extensions for the extra-supported extensions.

Usage: TFTPClient [-i] [-b blocksize] [-t timeout] [-p portnumber] host [GET | PUT] local_file [destination_file]

## License
The source coce package is hereby released as open source under the [MIT license](LICENSE).
The pre-built binaries are Freeware. If you charge money for them, you will be struck by lightning.

## Contributing
The copyrights for this package are held by [GkWare e.K.](https://www.gkware.com)
Contributors agree to license their contributions(s) under the same (MIT) license.
Additional authors will be listed in an AUTHORS file if/once applicable.

## Build requirements
This code has been tested to compile using Visual C++ 6.0 (TFTPServer.dsw) and Visual Studio 2017 (TFTPServer.sln).
This project is fully self-contained and does neither use our cross-platform HAL nor any other 3rd-party code.
Just clone the repository, double-click the workspace/solution file and click build.

