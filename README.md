# Linux-BP
This is a debugger tool source code repository that includes hardware breakpoints and software breakpoints, but the software breakpoint tool has not been fully developed.

# HW-BP Usage

```
hw_bp -p <pid> -t <type> <addr>
hw_bp -p <pid> <addr>
```

For example:
```bash
pearl:/ # /data/local/tmp/hw_bp -p 29719 -t rw 0x62a8f81000
  X0 : 0x00000062a8f81000  X1 : 0x0000000000000000  X2 : 0x0000000000000000  X3 : 0x0000000000000000
  X4 : 0x0000000000000010  X5 : 0xb400007c330b4048  X6 : 0x000000000000000a  X7 : 0x3030303138663861
  X8 : 0x980241eb89d3fe3e  X9 : 0x980241eb89d3fe3e  X10: 0x0000000000007417  X11: 0x0000000000007417
  X12: 0xffffff80ffffffc8  X13: 0x0000000000000004  X14: 0x0000000000000000  X15: 0x0000000000000101
  X16: 0x0000007e03183ef0  X17: 0x0000007e03116ee0  X18: 0x0000007e0ac90000  X19: 0x00000062a8f70828
  X20: 0x0000000000000001  X21: 0x0000000000000001  X22: 0x0000007fddda4b58  X23: 0x0000007fddda4b60
  X24: 0x0000000000000000  X25: 0x0000000000000000  X26: 0x0000000000000000  X27: 0x0000000000000000
  X28: 0x0000000000000000  FP : 0x0000007fddda4ad0  LR : 0x00000062a8f70870  SP : 0x0000007fddda4ad0
  PC : 0x00000062a8f70878  PSTATE: 0x0000000000000000
  --------------------------------
pearl:/ # /data/local/tmp/hw_bp -p 29719 -t x 0x00000062a8f70878                                                           
  X0 : 0x00000062a8f81000  X1 : 0x0000000000000000  X2 : 0x0000000000000000  X3 : 0x0000000000000000
  X4 : 0x0000000000000010  X5 : 0xb400007c330b4048  X6 : 0x000000000000000a  X7 : 0x3030303138663861
  X8 : 0x980241eb89d3fe3e  X9 : 0x980241eb89d3fe3e  X10: 0x0000000000007417  X11: 0x0000000000007417
  X12: 0xffffff80ffffffc8  X13: 0x0000000000000004  X14: 0x0000000000000000  X15: 0x0000000000000101
  X16: 0x0000007e03183ef0  X17: 0x0000007e03116ee0  X18: 0x0000007e0ac90000  X19: 0x00000062a8f70828
  X20: 0x0000000000000001  X21: 0x0000000000000001  X22: 0x0000007fddda4b58  X23: 0x0000007fddda4b60
  X24: 0x0000000000000000  X25: 0x0000000000000000  X26: 0x0000000000000000  X27: 0x0000000000000000
  X28: 0x0000000000000000  FP : 0x0000007fddda4ad0  LR : 0x00000062a8f70870  SP : 0x0000007fddda4ad0
  PC : 0x00000062a8f70878  PSTATE: 0x0000000000000000
  --------------------------------
pearl:/ # 

```
In this example, the first command installs a hardware breakpoint at PID "9394" and address "0x63c4e61000" with permissions "rw". It can be seen that after the breakpoint is triggered, the registers are successfully printed. The second command installs an execution breakpoint on the PC, which also successfully prints the register translation.

full arguments:
```
Usage: /data/local/tmp/hw_bp [OPTIONS] <address|symbol|var>

Options:
  -p, --pid <pid>        Target process ID (default: self)
  -t, --type <type>      Breakpoint type: r(ead), w(rite), x(ecute) (default: rw)
                         Allowed combinations: r, w, rw, x, rx
                         NOT allowed: rwx (rw cannot be combined with x)
  -l, --len <len>        Length: 1, 2, 4, 8 bytes (default: 4)
  -o, --output <file>    Write output to file instead of stdout
  -v, --verbose          Verbose mode
  -h, --help             Show this help message
  -V, --version          View version and copyright information

Address formats:
  0x12345678             Hexadecimal address
  symbol                 Symbol name (requires -p <pid>)
  filename:offset        File name and offset

Examples:
  hw_bp 0x7fff12340000                    # Monitor address in self
  hw_bp -p 1234 0x7fff12340000            # Monitor address in process 1234
  hw_bp -p 1234 -t w my_var               # Monitor writes to 'my_var' in process 1234
  hw_bp -p 1234 -t x 0x7fff12340000       # Monitor execute
```

## Output

![output](image/hw_output.png)

# SW-BP Usage
Under development...
