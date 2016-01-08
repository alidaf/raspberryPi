//  ===========================================================================
/*
    testDT:

    Tests parsing the device tree for the BCM2835 peripherals.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
//  ===========================================================================
/*
    Compile with:

    gcc testDT.c mcp42x1.c -Wall -o testDT

    Also use the following flags for Raspberry Pi optimisation:
        -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3
*/
//  ===========================================================================
/*
    Authors:        D.Faulke    04/01/2016

    Contributors:
*/
//  ===========================================================================

#define VERSION 10000 // v1.0

//  ===========================================================================
/*
    Changelog:

        v0.1    Original version.
*/
//  ===========================================================================


//  Information. --------------------------------------------------------------
/*
    Sources:

    https://www.raspberrypi.org/documentation/configuration/device-tree.md
    https://en.wikipedia.org/wiki/Device_tree
    http://elinux.org/Device_Tree
    http://devicetree.org/Linux

    Description:

    A device tree is a data structure for describing hardware, implemented as a
    tree of nodes, child nodes and properties.

    It is intended as a means to allow compilation of kernels that are fairly
    hardware agnostic within an architecture family, i.e. between different
    hardware revisions with slightly different peripherals. A significant part
    of the hardware description may be moved out of the kernel binary and into
    a compiled device tree blob, which is passed to the kernel by the boot
    loader.

    This is especially important for ARM-based Linux distributions such as the
    Raspberry Pi where traditionally, the boot loader has been customised for
    specific boards, creating many variants of kernel for the different boards.

    Intent:

    The Raspberry Pi has a Broadcom BCM2835 or BCM2836 chip (depending on
    version) that provides peripheral support. The latest Raspbian kernel now
    has device tree support. This program experiments with the BCM2835 device
    tree to test parsing and information retrieval to avoid typically using
    lots of unwieldly macro definitions.

    Device trees are written as .dts files in textual form and compiled into
    a flattened device tree or device tree blob (.dtb) file. Additional trees
    with the .dtsi extension may be referenceded via the /include/ card and are
    analogous to .h header files in C.

    Usage:

    RPi device tree blobs should be located in /boot/overlays/
    e.g. /boot/overlays/spi-bcm2835-overlay.dtb



*/
//  ---------------------------------------------------------------------------

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

struct dt
{
    struct device *device;
    void __iomem *base;
    void __iomem *base_of;
    void __iomem *base_node;

int main()
{

}
