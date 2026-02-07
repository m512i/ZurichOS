#!/bin/bash
# ZurichOS QEMU Run Script

# Default options
KERNEL="../build/zurichos.elf"
MEMORY="256M"
DEBUG=0
SERIAL="stdio"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            DEBUG=1
            shift
            ;;
        -m|--memory)
            MEMORY="$2"
            shift 2
            ;;
        -k|--kernel)
            KERNEL="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  -d, --debug     Start with GDB server on port 1234"
            echo "  -m, --memory    Set memory size (default: 256M)"
            echo "  -k, --kernel    Path to kernel ELF (default: ../build/zurichos.elf)"
            echo "  -h, --help      Show this help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Check if kernel exists
if [ ! -f "$KERNEL" ]; then
    echo "Error: Kernel not found at $KERNEL"
    echo "Run 'make' first to build the kernel."
    exit 1
fi

# Build QEMU command
QEMU_CMD="qemu-system-i386"
QEMU_ARGS="-kernel $KERNEL -m $MEMORY -serial $SERIAL -no-reboot -no-shutdown"

# Add debug options
if [ $DEBUG -eq 1 ]; then
    QEMU_ARGS="$QEMU_ARGS -s -S"
    echo "Starting QEMU with GDB server on port 1234..."
    echo "Connect with: gdb $KERNEL -ex 'target remote localhost:1234'"
fi

# Run QEMU
$QEMU_CMD $QEMU_ARGS
