======================================================
README.TXT for TFTP Client & Server
Copyright (c) 2015 Gkware e.K.
======================================================

The files in this archive are freeware.
If you paid for them, please ask for a refund.

 The server
===============
The server always listens on UDP port 69 and
provides remote access to the working directory where the 
server has been started. The server will refuse requests
for all files requests with "..", slashes and colons due
to security reasons. Without these restrictions, all
Files on the Host system would be accessible via TFTP. 

 The client
===============
The client can replace the standard windows TFTP client.
The commandline is compatible to the TFTP.EXE that
ships with most OS versions, but it supports a number of
useful extensions that are missing in the standard client.
These include the 
- TFTP Blocksize option (RFC 2348)
- TFTP Timeout Interval and Transfer Size Option (RFC 2349)

TFTPClient [-i] [-b blocksize] [-t timeout] [-p portnumber] host [GET | PUT] local_file [destination_file]

 Source code
===============
If you think that the source code of these two tools
would help you, please feel free to contact us.

  GkWare e.K.
