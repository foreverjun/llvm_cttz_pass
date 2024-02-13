import subprocess

def runcommand (cmd):
    proc = subprocess.Popen(cmd,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            shell=True,
                            universal_newlines=True)
    std_out, std_err = proc.communicate()
    return proc.returncode, std_out, std_err

def main():

    for i in range(-128,128):
        code1, out, err = runcommand(f"~/qemu-8.2.1/build/qemu-riscv64 -L /usr/riscv64-linux-gnu/ optimized.out {i}");
        code2, out, err = runcommand(f"~/qemu-8.2.1/build/qemu-riscv64 -L /usr/riscv64-linux-gnu/ not_optimized.out {i}");
        if code1 != code2:
            print(f"Error: {i} optimized.out: {code1} not_optimized.out: {code2}");

    code1, out, err = runcommand(f"~/qemu-8.2.1/build/qemu-riscv64 -L /usr/riscv64-linux-gnu/ optimized.out 2147483647");
    code2, out, err = runcommand(f"~/qemu-8.2.1/build/qemu-riscv64 -L /usr/riscv64-linux-gnu/ not_optimized.out 2147483647");
    if code1 != code2:
        print(f"Error: 2147483647 optimized.out: {code1} non_optimized.out: {code2}");
    code1, out, err = runcommand(f"~/qemu-8.2.1/build/qemu-riscv64 -L /usr/riscv64-linux-gnu/ optimized.out –2147483648 ");
    code2, out, err = runcommand(f"~/qemu-8.2.1/build/qemu-riscv64 -L /usr/riscv64-linux-gnu/ not_optimized.out –2147483648");
    if code1 != code2:
        print(f"Error: –2147483648 optimized.out: {code1} non_optimized.out: {code2}");

if __name__ == '__main__':
    main()