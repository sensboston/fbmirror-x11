#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <string.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xfixes.h>

int main(int argc, char *argv[]) {
    int fps = 30;
    if (argc > 1) fps = atoi(argv[1]);
    
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open X display\n");
        return 1;
    }
    
    Window root = DefaultRootWindow(dpy);
    XWindowAttributes attr;
    XGetWindowAttributes(dpy, root, &attr);
    
    printf("X11: %dx%d, depth=%d\n", attr.width, attr.height, attr.depth);
    
    int xfixes_event, xfixes_error;
    if (!XFixesQueryExtension(dpy, &xfixes_event, &xfixes_error)) {
        fprintf(stderr, "XFixes not available\n");
        return 1;
    }
    
    XShmSegmentInfo shminfo;
    XImage *img = XShmCreateImage(dpy, attr.visual, attr.depth, ZPixmap,
                                   NULL, &shminfo, attr.width, attr.height);
    if (!img) {
        fprintf(stderr, "XShmCreateImage failed\n");
        return 1;
    }
    
    shminfo.shmid = shmget(IPC_PRIVATE, img->bytes_per_line * img->height, IPC_CREAT | 0777);
    shminfo.shmaddr = img->data = shmat(shminfo.shmid, 0, 0);
    shminfo.readOnly = False;
    XShmAttach(dpy, &shminfo);
    
    int fd1 = open("/dev/fb1", O_RDWR);
    if (fd1 < 0) {
        perror("Cannot open /dev/fb1");
        return 1;
    }
    
    struct fb_var_screeninfo vinfo1;
    struct fb_fix_screeninfo finfo1;
    ioctl(fd1, FBIOGET_VSCREENINFO, &vinfo1);
    ioctl(fd1, FBIOGET_FSCREENINFO, &finfo1);
    
    printf("fb1: %dx%d, %d bpp, line_length=%d\n",
           vinfo1.xres, vinfo1.yres, vinfo1.bits_per_pixel, finfo1.line_length);
    
    size_t size1 = finfo1.line_length * vinfo1.yres;
    char *fb1 = mmap(NULL, size1, PROT_READ | PROT_WRITE, MAP_SHARED, fd1, 0);
    if (fb1 == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }
    
    int copy_width = (attr.width < (int)vinfo1.xres) ? attr.width : vinfo1.xres;
    int copy_height = (attr.height < (int)vinfo1.yres) ? attr.height : vinfo1.yres;
    
    long frame_ns = 1000000000L / fps;
    struct timespec ts;
    
    printf("Copying %dx%d @ %d fps (optimized cursor)\n", copy_width, copy_height, fps);
    
    while (1) {
        XShmGetImage(dpy, root, img, 0, 0, AllPlanes);
        
        // Step 1: Fast copy without cursor check
        for (int y = 0; y < copy_height; y++) {
            unsigned char *src = (unsigned char *)img->data + y * img->bytes_per_line;
            unsigned short *dst = (unsigned short *)(fb1 + y * finfo1.line_length);
            
            for (int x = 0; x < copy_width; x++) {
                unsigned char b = src[x * 4 + 0];
                unsigned char g = src[x * 4 + 1];
                unsigned char r = src[x * 4 + 2];
                dst[x] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            }
        }
        
        // Step 2: Draw cursor only in its area
        XFixesCursorImage *cursor = XFixesGetCursorImage(dpy);
        if (cursor) {
            int cur_x = cursor->x - cursor->xhot;
            int cur_y = cursor->y - cursor->yhot;
            
            int start_x = (cur_x < 0) ? 0 : cur_x;
            int start_y = (cur_y < 0) ? 0 : cur_y;
            int end_x = cur_x + cursor->width;
            int end_y = cur_y + cursor->height;
            if (end_x > copy_width) end_x = copy_width;
            if (end_y > copy_height) end_y = copy_height;
            
            for (int y = start_y; y < end_y; y++) {
                unsigned char *src = (unsigned char *)img->data + y * img->bytes_per_line;
                unsigned short *dst = (unsigned short *)(fb1 + y * finfo1.line_length);
                int cy = y - cur_y;
                
                for (int x = start_x; x < end_x; x++) {
                    int cx = x - cur_x;
                    unsigned long pixel = cursor->pixels[cy * cursor->width + cx];
                    unsigned char ca = (pixel >> 24) & 0xFF;
                    
                    if (ca > 0) {
                        unsigned char cr = (pixel >> 16) & 0xFF;
                        unsigned char cg = (pixel >> 8) & 0xFF;
                        unsigned char cb = pixel & 0xFF;
                        
                        if (ca == 255) {
                            dst[x] = ((cr >> 3) << 11) | ((cg >> 2) << 5) | (cb >> 3);
                        } else {
                            unsigned char b = src[x * 4 + 0];
                            unsigned char g = src[x * 4 + 1];
                            unsigned char r = src[x * 4 + 2];
                            r = (cr * ca + r * (255 - ca)) / 255;
                            g = (cg * ca + g * (255 - ca)) / 255;
                            b = (cb * ca + b * (255 - ca)) / 255;
                            dst[x] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                        }
                    }
                }
            }
            XFree(cursor);
        }
        
        ts.tv_sec = 0;
        ts.tv_nsec = frame_ns;
        nanosleep(&ts, NULL);
    }
    
    return 0;
}
