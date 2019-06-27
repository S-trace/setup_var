// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include <fcntl.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

//#define VARIABLE "/sys/firmware/efi/efivars/Setup-ec87d643-eba4-4bb5-a1e5-3f3e36b20da9"
#define VARIABLE "/home/s-trace/test2"

struct new_efi_variable {
    u_int32_t attributes;
    u_int8_t data[0];
};

static void
print_value_at_offset(const struct new_efi_variable *const efivar, const size_t offset, const size_t variable_size) {
    if (efivar == NULL) {
        fprintf(stderr, "%s: efivar == null!\n", __func__);
        exit(EXIT_FAILURE);
    }
    u_int8_t value = efivar->data[offset];
    printf("8-bit value at 0x%zx: 0x%02x (%u)\n", offset, value, value);
    if (variable_size - 1 > offset) {
        u_int16_t value16 = (u_int16_t) (efivar->data[offset + 1] << 8u | efivar->data[offset]);
        printf("16-bit value at 0x%zx: 0x%04x (%u)\n", offset, value16, value16);
    }
    if (variable_size - 3 > offset) {
        u_int32_t value32 = (u_int32_t) (efivar->data[offset + 3] << 24u | efivar->data[offset + 2] << 16u
                                         | efivar->data[offset + 1] << 8u | efivar->data[offset]);
        printf("32-bit value at 0x%zx: 0x%08x (%u)\n", offset, value32, value32);
    }
}

static void print_usage(const char *name) {
    printf("Usage: %s <offset> [new_value [8|16|32]]\n", name);
    printf("offset: offset to read/write value\n");
    printf("new_value: new value (if not specified - just read and show current value)\n");
    printf("8|16|32: new value size (if not specified - autodetect from new value)\n");
}

static bool is_file_immutable(const char *const name) {
    int fd = open(name, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "ERROR: Cannot open \"%s\" for reading: %s\n", name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    unsigned long attr;
    int ioctl_result = ioctl(fd, FS_IOC_GETFLAGS, &attr);
    if (ioctl_result == -1) {
        perror("ERROR: FS_IOC_GETFLAGS ioctl failed, cannot read immutable flag");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
    return (attr & FS_IMMUTABLE_FL) != 0;
}

static void toggle_file_immutable(const char *const name) {
    int fd = open(name, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "ERROR: Cannot open \"%s\" for reading: %s\n", name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    unsigned long attr = 0;
    int ioctl_result = 0;
    ioctl_result = ioctl(fd, FS_IOC_GETFLAGS, &attr);
    if (ioctl_result == -1) {
        perror("ERROR: FS_IOC_GETFLAGS ioctl failed, cannot read flags");
        close(fd);
        exit(EXIT_FAILURE);
    }
    bool was_immutable = (attr ^ FS_IMMUTABLE_FL) != 0;
    ioctl_result = ioctl(fd, FS_IOC_SETFLAGS, &attr);
    if (ioctl_result == -1) {
        perror("ERROR: FS_IOC_SETFLAGS ioctl failed, cannot write changed flags");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // check current status of immutable immutable_flag again to make sure it was really changed
    ioctl_result = ioctl(fd, FS_IOC_GETFLAGS, &attr);
    if (ioctl_result == -1) {
        perror("ERROR: FS_IOC_GETFLAGS ioctl failed, cannot re-read flags after change");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
    bool now_immutable = (attr ^ FS_IMMUTABLE_FL) != 0;
    if (was_immutable == now_immutable) {
        fprintf(stderr, "ERROR: Can't change immutable immutable_flag state due to unknown reason, current state: %d",
                now_immutable);
        exit(EXIT_FAILURE);
    }
}

static bool is_read_only(const int argc) {
    return argc < 3;
}

static size_t get_file_size(const char *const name) {
    struct stat st;
    if (stat(name, &st) == -1) {
        fprintf(stderr, "ERROR: unable to stat '%s' file: %s", name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return (uint32_t) st.st_size;
}

static struct new_efi_variable *read_variable(const char *const name) {
    size_t file_size = get_file_size(name);
    size_t data_len = file_size - sizeof(((struct new_efi_variable *) NULL)->attributes);
    printf("Variable \"%s\" data length = %zu (0x%zX)\n",
           name, data_len, data_len);
    struct new_efi_variable *efivar = calloc(1, file_size);
    if (efivar == NULL) {
        fprintf(stderr, "ERROR: Cannot allocate %ld bytes for efivar\n", file_size);
        exit(EXIT_FAILURE);
    }
    int fd = open(name, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "ERROR: Cannot open \"%s\" for reading: %s\n",
                name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    ssize_t read_result = read(fd, efivar, file_size);
    if (read_result == -1) {
        fprintf(stderr, "ERROR: Failed to read %zu bytes from '%s' file: %s",
                file_size, VARIABLE, strerror(errno));
        exit(EXIT_FAILURE);
    }
    close(fd);
    size_t read_bytes = (size_t) read_result; // read() returns -1..0x7ffff000 (see man read (2) for details)

    if (read_bytes != file_size) {
        fprintf(stderr, "ERROR: Cannot read variable from \"%s\" (%zd of %ld bytes read) : %s\n",
                name, read_bytes, file_size, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return efivar;
}

static void write_variable(const char *const name, const struct new_efi_variable *const variable, const size_t size) {
    bool was_immutable = is_file_immutable(name);
    if (was_immutable) {
        toggle_file_immutable(name);
    }
    int fd = open(name, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "ERROR: Cannot open \"%s\" for writing: %s\n",
                name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    ssize_t write_result = write(fd, variable, size);
    if (write_result == -1) {
        fprintf(stderr, "ERROR: Cannot write variable data to \"%s\": %s\n",
                name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    size_t written_bytes = (size_t) write_result; // write() returns -1..0x7ffff000 (see man write (2) for details)
    int close_result = close(fd);
    if (written_bytes != size) {
        fprintf(stderr, "ERROR: Cannot write variable to \"%s\" (%zu of %zu bytes written) : %s\n",
                name, written_bytes, size, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (close_result == -1) {
        perror("ERROR: can't close variable file after write");
        exit(EXIT_FAILURE);
    }
    if (was_immutable) {
        toggle_file_immutable(name);
    }
}

static void
set_value_at_offset(struct new_efi_variable *const efivar, const size_t offset, const unsigned int new_value,
                    const size_t new_value_size, const size_t variable_size) {
    size_t available_bits = 8 * (variable_size - offset);
    if (available_bits < new_value_size) {
        fprintf(stderr, "ERROR: Cannot set %zu-bit value - there is only %zd bits available at offset 0x%zX\n",
                new_value_size, available_bits, offset);
        exit(EXIT_FAILURE);
    }
    if (new_value_size == 8) {
        printf("Setting 8-bit value\n");
        efivar->data[offset] = (u_int8_t) new_value;
    } else if (new_value_size == 16) {
        printf("Setting 16-bit value\n");
        efivar->data[offset] = (u_int8_t) new_value;
        efivar->data[offset + 1] = (u_int8_t) (new_value >> 8u);
    } else if (new_value_size == 32) {
        printf("Setting 32-bit value\n");
        efivar->data[offset] = (u_int8_t) new_value;
        efivar->data[offset + 1] = (u_int8_t) (new_value >> 8u);
        efivar->data[offset + 2] = (u_int8_t) (new_value >> 16u);
        efivar->data[offset + 3] = (u_int8_t) (new_value >> 24u);
    }
}

static uint32_t get_new_value_size(const int argc, const char **const argv, const uint32_t value) {
    uint32_t size = 8;
    if (argc == 4) {
        unsigned long forced_size = strtoul(argv[3], NULL, 0);
        if (!(forced_size == 8 || forced_size == 16 || forced_size == 32)) {
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
        size = (uint32_t) forced_size;
        printf("Forced value size == %d\n", size);
    } else {
        if (value > UINT8_MAX) {
            size = 16;
        }
        if (value > UINT16_MAX) {
            size = 32;
        }
    }
    return size;
}

int main(const int argc, const char **const argv) {
    char *filename = VARIABLE;
    int read_only = is_read_only(argc);
    if (argc < 2) {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    size_t offset = strtoul(argv[1], NULL, 0);
    size_t variable_file_size = get_file_size(filename);
    size_t variable_data_size = variable_file_size - sizeof(((struct new_efi_variable *) NULL)->attributes);
    if (variable_data_size <= 0) {
        fprintf(stderr, "ERROR: Variable data size <= 0: %ld - can't read/write any data from/to it!\n",
                variable_data_size);
        exit(EXIT_FAILURE);
    }
    if (offset >= variable_data_size) {
        fprintf(stderr, "ERROR: Offset 0x%zX is beyound variable size (0x%lX) - please use 0x0..0x%lX offsets only\n",
                offset, variable_data_size, variable_data_size - 1);
        exit(EXIT_FAILURE);
    }
    struct new_efi_variable *variable = read_variable(filename);
    print_value_at_offset(variable, offset, variable_data_size);
    if (!read_only) {
        unsigned long new_value_long = strtoul(argv[2], NULL, 0);
        if (new_value_long > UINT32_MAX) {
            fprintf(stderr, "ERROR: new value is too big to fit into 32 bits: %0lx\n", new_value_long);
            exit(EXIT_FAILURE);
        }
        uint32_t new_value = (uint32_t) new_value_long;
        size_t new_value_size = get_new_value_size(argc, argv, new_value);
        set_value_at_offset(variable, offset, new_value, new_value_size, variable_data_size);
        write_variable(filename, variable, variable_file_size);
        free(variable);
        // Re-read it to make sure
        variable = read_variable(filename);
        printf("New value:\n");
        print_value_at_offset(variable, offset, variable_data_size);
    }
    free(variable);
    exit(EXIT_SUCCESS);
}
