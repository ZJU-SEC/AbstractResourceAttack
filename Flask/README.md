# Flask
<u>F</u>lexible <u>a</u>bstract re<u>s</u>ource confinement framewor<u>k</u>.

## Environments
```
sudo apt-get install build-essential
```
<!-- **Note:** We neeed support of BCC v0.29.0, latest BCC may only be got by building from source, see [instructions](https://github.com/iovisor/bcc/blob/master/INSTALL.md#ubuntu---binary). -->

## Benchmarks
You have to unzip lmbench and run `make results` in the directory. And we use DEVELOPMENT test subset here:
```bash
SUBSET (ALL|HARWARE|OS|DEVELOPMENT) [default all]: DEVELOPMENT
=====================================================================
SYSCALL [default yes]:   
SELECT [default yes]: 
SIGNAL [default yes]: 
PROCESS CREATION [default yes]: 
PAGEFAULT [default yes]: 
FILE [default yes]: 
MMAP [default yes]: 
CONTEXT SWITCH [default yes]: no
PIPE [default yes]: 
UNIX socket [default yes]: no
UDP [default yes]: 
TCP [default yes]: 
TCP CONNECT [default yes]: no
RPC [default yes]: no
HTTP [default yes]: no
BCOPY [default yes]: 
MEMORY HIERARCHY [default yes]: no
CPU OPERATIONS [default yes]: no
=====================================================================
```

Then you can run `auto_bench` in `scripts` dir with `sudo` permissions. The results will be stored in `benchmarks` dir.
Note: you should start a docker first (no matter what it runs), and change the docker id in `auto_bench` script.
Note: we kill flask after each benchmarks, but we cannot guarentee flask is killed, you shall make sure the processes are terminated, or this could impact the benchmark results.

## Works
