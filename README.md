## Requirements
 * g++
 * cmake
 * Google protocol buffers (protoc)

To install all dependencies on Ubuntu variants, run:
`sudo apt-get install g++ cmake protobuf-compiler libprotobuf-dev`

## Compilation
Run `make` in the project directory.

## Usage
There are three main executables, the logger, playback, and the evaluator.

### Logger
The logger listens to configured UDP multicast/unicast streams, and logs all
received packets. The logger by default will log SSL-Vision and refbox without
any arguments:
```
 ./bin/logger
```

To log additional streams, for example, Autoref1 publishing referee messages to
224.5.23.1:10030, and Autoref1 publishing remote control to 224.5.23.3:10030,
specify them to the logger as:
```
 ./bin/logger 224.5.23.1:10030 224.5.23.3:10030
```

To run the logger in verbose mode to announce whenever it receives a packet on
any of its configured streams, use the "-v" flag:
```
 ./bin/logger -v 224.5.23.1:10030
```
