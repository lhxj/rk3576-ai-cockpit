/* SPDX-License-Identifier: MIT
 * One fixed vendor read command via Linux TEE UAPI. No OTP write commands,
 * arbitrary MMIO, firmware loading, M0 controls, or persistent file writes.
 * Host-built diagnostic: execution on a board requires separate approval.
 */
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/tee.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/* Fixed SDK OpteeClientInterface.c:637 uses this TA, command 5, output 4B. */
static const unsigned char flag_ta[16] = {
    0x2d, 0x26, 0xd8, 0xa8, 0x51, 0x34, 0x4d, 0xd8,
    0xb3, 0x2f, 0xb3, 0x4b, 0xce, 0xeb, 0xc4, 0x71
};
enum { READ_ENABLE_FLAG = 5, PARAMS = 4 };

static int call_buf(int fd, unsigned long request, void *data, size_t size)
{
    struct tee_ioctl_buf_data buf = {
        .buf_ptr = (uintptr_t)data, .buf_len = size
    };
    return ioctl(fd, request, &buf);
}

int main(int argc, char **argv)
{
    int fd = -1, shmfd = -1, result = 1;
    bool session_open = false;
    uint32_t *word = MAP_FAILED;
    struct stat st;
    struct tee_ioctl_version_data version = {0};
    struct tee_ioctl_shm_alloc_data shm = { .size = sizeof(uint32_t) };
    size_t open_size = sizeof(struct tee_ioctl_open_session_arg) +
                       PARAMS * sizeof(struct tee_ioctl_param);
    size_t invoke_size = sizeof(struct tee_ioctl_invoke_arg) +
                         PARAMS * sizeof(struct tee_ioctl_param);
    struct tee_ioctl_open_session_arg *open_arg = calloc(1, open_size);
    struct tee_ioctl_invoke_arg *invoke = calloc(1, invoke_size);

    if (argc != 2 || strcmp(argv[1], "--read-vboot-flag")) {
        fprintf(stderr, "Usage: %s --read-vboot-flag\n", argv[0]);
        result = 2;
        goto out;
    }
    if (!open_arg || !invoke) {
        fprintf(stderr, "calloc: out of memory\n");
        goto out;
    }
    /* Best-effort process deadline; does not guarantee stopping an EL3 hang. */
    alarm(12);
    fd = open("/dev/tee0", O_RDWR | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0 || fstat(fd, &st) || !S_ISCHR(st.st_mode)) {
        fprintf(stderr, "TEE device open/type check failed: %s\n", strerror(errno));
        goto out;
    }
    if (ioctl(fd, TEE_IOC_VERSION, &version)) {
        fprintf(stderr, "TEE_IOC_VERSION: %s\n", strerror(errno));
        goto out;
    }
    printf("tee_impl=%" PRIu32 " gen_caps=0x%08" PRIx32 "\n",
           version.impl_id, version.gen_caps);
    if (version.impl_id != TEE_IMPL_ID_OPTEE || !(version.gen_caps & TEE_GEN_CAP_GP)) {
        fprintf(stderr, "Not a GlobalPlatform OP-TEE device; stop\n");
        goto out;
    }
    memcpy(open_arg->uuid, flag_ta, sizeof(flag_ta));
    open_arg->clnt_login = TEE_IOCTL_LOGIN_PUBLIC;
    open_arg->num_params = PARAMS;
    if (call_buf(fd, TEE_IOC_OPEN_SESSION, open_arg, open_size)) {
        fprintf(stderr, "TEE_IOC_OPEN_SESSION: %s\n", strerror(errno));
        goto out;
    }
    printf("open_session_ret=0x%08" PRIx32 " origin=%" PRIu32 "\n",
           open_arg->ret, open_arg->ret_origin);
    if (open_arg->ret)
        goto out;
    session_open = true;
    shmfd = ioctl(fd, TEE_IOC_SHM_ALLOC, &shm);
    if (shmfd < 0 || shm.size < sizeof(uint32_t) || shm.id < 0) {
        fprintf(stderr, "TEE_IOC_SHM_ALLOC failed: %s\n", strerror(errno));
        goto out;
    }
    word = mmap(NULL, sizeof(uint32_t), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (word == MAP_FAILED) {
        fprintf(stderr, "TEE shared buffer mmap: %s\n", strerror(errno));
        goto out;
    }
    *word = UINT32_C(0x5a5a5a5a); /* Output buffer only, not an OTP/MMIO write. */
    invoke->func = READ_ENABLE_FLAG;
    invoke->session = open_arg->session;
    invoke->num_params = PARAMS;
    invoke->params[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
    invoke->params[0].a = 0;
    invoke->params[0].b = sizeof(uint32_t);
    invoke->params[0].c = (uint32_t)shm.id;
    if (call_buf(fd, TEE_IOC_INVOKE, invoke, invoke_size)) {
        fprintf(stderr, "TEE_IOC_INVOKE command 5: %s\n", strerror(errno));
        goto out;
    }
    printf("invoke_ret=0x%08" PRIx32 " origin=%" PRIu32 " output_size=%" PRIu64 "\n",
           invoke->ret, invoke->ret_origin, (uint64_t)invoke->params[0].b);
    if (invoke->ret || invoke->params[0].b != sizeof(uint32_t))
        goto out;
    if (*word == UINT32_C(0x5a5a5a5a)) {
        fprintf(stderr, "Output not written; no valid flag evidence\n");
        goto out;
    }
    printf("read_enable_flag_raw=0x%08" PRIx32 "\n", *word);
    printf("vendor_rk3576_required_flag=%u\n", *word == UINT32_C(0xff));
    printf("scope=Linux_OPTEE_command5; not proof of U-Boot call or M0 mapping\n");
    result = 0;
out:
    if (word != MAP_FAILED)
        munmap(word, sizeof(uint32_t));
    if (shmfd >= 0)
        close(shmfd);
    if (session_open) {
        struct tee_ioctl_close_session_arg close_arg = { .session = open_arg->session };
        if (ioctl(fd, TEE_IOC_CLOSE_SESSION, &close_arg)) {
            fprintf(stderr, "TEE_IOC_CLOSE_SESSION: %s\n", strerror(errno));
            result = 1;
        }
    }
    if (fd >= 0)
        close(fd);
    free(invoke);
    free(open_arg);
    return result;
}
