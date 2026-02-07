file build/zurichos.elf

set architecture i386

target remote localhost:1234

define hook-stop
    info registers
end

define print-stack
    x/20x $esp
end

define print-page-dir
    x/1024x 0xFFFFF000
end

echo \n
echo ZurichOS GDB Commands:\n
echo   print-stack     - Display stack contents\n
echo   print-page-dir  - Display page directory\n
echo \n
echo Set breakpoints with: break <function>\n
echo Continue execution with: continue\n
echo \n
