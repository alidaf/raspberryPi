//  ===========================================================================
/*
    fb-test2:

    This is a framebuffer test program taken from the blog at ...

    http://raspberrycompote.blogspot.co.uk.

    It displays the size and colour depth in bpp (bits per pixel) and fills
    half of the display with colour.

    The only change to the code is the explicit reference to the fbtft
    framebuffer device /dev/fb1 rather than the default /dev/fb0.
*/
//  ===========================================================================
/*
    Compile with:

    make fb-test2

*/


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>

// application entry point
int main(int argc, char* argv[])
{
    int fbfd = 0;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    long int screensize = 0;
    char *fbp = 0;
    uint i;

    // Open the file for reading and writing
    fbfd = open("/dev/fb1", O_RDWR);
    if (!fbfd)
    {
        printf("Error: cannot open framebuffer device.\n");
        return(1);
    }
    printf("The framebuffer device was opened successfully.\n");

    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo))
    {
        printf("Error reading fixed information.\n");
    }

    // Get variable screen information
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo))
    {
        printf("Error reading variable information.\n");
    }
    printf("%dx%d, %d bpp\n", vinfo.xres, vinfo.yres,
        vinfo.bits_per_pixel );

    // map framebuffer to user memory
    screensize = finfo.smem_len;

    fbp = (char*)mmap(0,
                      screensize,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED,
                      fbfd, 0);

    if ((int)fbp == -1)
    {
        printf("Failed to mmap.\n");
    }
    else
    {
        // draw...
        // just fill upper half of the screen with something
        for ( i = 0; i < 0xff; i++ )
        {
            memset(fbp, i, screensize/2);
        // and lower half with something else
            memset(fbp + screensize/2, 0xff - i, screensize/2);
        }
    }

    // cleanup
    munmap(fbp, screensize);
    close(fbfd);
    return 0;
}
