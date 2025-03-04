#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>

int main(void)
{
    int fd = open("/dev/input/event31", O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        perror("open(/dev/input/event31)");
        return EXIT_FAILURE;
    }

    struct input_id id;
    if (ioctl(fd, EVIOCGID, &id) < 0) {
        perror("ioctl(EVIOCGID)");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("id: {\n"
        "    .bustype = 0x%x\n"
        "    .vendor = 0x%x\n"
        "    .product = 0x%x\n"
        "    .version = 0x%x\n"
        "};\n",
        id.bustype, id.version, id.product, id.version);

    char name[256] = { 0 };
    if (ioctl(fd, EVIOCGNAME(256), name) < 0) {
        perror("ioctl(EVIOCGNAME)");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("name: \"%s\"\n", name);

    struct input_absinfo abs[ABS_CNT];
    for (unsigned int i = 0; i < ABS_CNT; i++) {
        if (ioctl(fd, EVIOCGABS(i), &abs[i]) < 0) {
            perror("ioctl(EVIOCGABS)");
            close(fd);
            return EXIT_FAILURE;
        }
    }

    printf("absmax: {\n    ");
    for (unsigned int i = 0; i < ABS_CNT; i++) {
        printf("0x%x, ", abs[i].maximum);
    }
    printf("\n};\n");

    printf("absmin: {\n    ");
    for (unsigned int i = 0; i < ABS_CNT; i++) {
        printf("0x%x, ", abs[i].minimum);
    }
    printf("\n};\n");

    printf("absfuzz: {\n    ");
    for (unsigned int i = 0; i < ABS_CNT; i++) {
        printf("0x%x, ", abs[i].fuzz);
    }
    printf("\n};\n");

    printf("absflat: {\n    ");
    for (unsigned int i = 0; i < ABS_CNT; i++) {
        printf("0x%x, ", abs[i].flat);
    }
    printf("\n};\n");

    close(fd);
    return EXIT_SUCCESS;
}
