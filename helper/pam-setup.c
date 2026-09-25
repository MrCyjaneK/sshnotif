#define _GNU_SOURCE
/* Setuid helper for /etc/pam.d/sshd. argv: install | uninstall */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PAMFILE "/etc/pam.d/sshd"
#define TMPFILE "/etc/pam.d/sshd.sshnotif.tmp"
#define MARKER "/usr/libexec/sshnotif/pam-hook"
#define AUTH_LINE "auth       optional     pam_exec.so quiet /usr/libexec/sshnotif/pam-hook\n"
#define SESSION_LINE "session    optional     pam_exec.so quiet /usr/libexec/sshnotif/pam-hook\n"
#define MAX_PAM (64 * 1024)

static void die(const char *msg)
{
    fprintf(stderr, "sshnotif-setup: %s\n", msg);
    unlink(TMPFILE);
    exit(1);
}

static char *read_pam(struct stat *st, ssize_t *n)
{
    int fd = open(PAMFILE, O_RDONLY | O_NOFOLLOW);
    if (fd < 0)
        die("cannot open /etc/pam.d/sshd");
    if (fstat(fd, st) != 0) {
        close(fd);
        die("fstat failed");
    }
    if (!S_ISREG(st->st_mode) || st->st_size < 1 || st->st_size > MAX_PAM) {
        close(fd);
        die("unexpected /etc/pam.d/sshd");
    }
    char *buf = malloc((size_t)st->st_size + 1);
    if (!buf) {
        close(fd);
        die("out of memory");
    }
    *n = read(fd, buf, (size_t)st->st_size);
    close(fd);
    if (*n != st->st_size) {
        free(buf);
        die("short read");
    }
    buf[*n] = '\0';
    return buf;
}

static void write_pam(const char *data, size_t len, const struct stat *st)
{
    int tfd = open(TMPFILE, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600);
    if (tfd < 0)
        die("cannot create temp file");
    if (fchmod(tfd, st->st_mode & 0777) != 0 || fchown(tfd, st->st_uid, st->st_gid) != 0) {
        close(tfd);
        die("cannot set temp file perms");
    }
    if (write(tfd, data, len) != (ssize_t)len || fsync(tfd) != 0) {
        close(tfd);
        die("write failed");
    }
    close(tfd);
    if (rename(TMPFILE, PAMFILE) != 0)
        die("rename failed");
}

static void do_install(char *buf, ssize_t n, const struct stat *st)
{
    if (strstr(buf, MARKER)) {
        free(buf);
        return;
    }

    const char *insert_at = buf;
    char *p = buf;
    while (p && *p) {
        if (strncmp(p, "auth", 4) == 0 && (p[4] == ' ' || p[4] == '\t')) {
            insert_at = p;
            break;
        }
        p = strchr(p, '\n');
        if (!p)
            break;
        p++;
        insert_at = p;
    }

    size_t head = (size_t)(insert_at - buf);
    size_t tail = (size_t)n - head;
    size_t extra = strlen(AUTH_LINE) + strlen(SESSION_LINE);
    char *out = malloc(head + extra + tail);
    if (!out) {
        free(buf);
        die("out of memory");
    }
    memcpy(out, buf, head);
    memcpy(out + head, AUTH_LINE, strlen(AUTH_LINE));
    memcpy(out + head + strlen(AUTH_LINE), SESSION_LINE, strlen(SESSION_LINE));
    memcpy(out + head + extra, insert_at, tail);
    free(buf);
    write_pam(out, head + extra + tail, st);
    free(out);
}

static void do_uninstall(char *buf, ssize_t n, const struct stat *st)
{
    if (!strstr(buf, MARKER)) {
        free(buf);
        return;
    }

    char *out = malloc((size_t)n + 1);
    if (!out) {
        free(buf);
        die("out of memory");
    }
    size_t o = 0;
    char *line = buf;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        size_t len = nl ? (size_t)(nl - line) + 1 : strlen(line);
        if (!memmem(line, len, MARKER, strlen(MARKER))) {
            memcpy(out + o, line, len);
            o += len;
        }
        if (!nl)
            break;
        line = nl + 1;
    }
    free(buf);
    if (o < 1)
        die("refusing to write empty pam file");
    write_pam(out, o, st);
    free(out);
}

int main(int argc, char **argv)
{
    const char *cmd = (argc == 2) ? argv[1] : NULL;
    if (argc != 2 || (strcmp(cmd, "install") != 0 && strcmp(cmd, "uninstall") != 0))
        die("usage: pam-setup install|uninstall");

    if (setuid(0) != 0 || seteuid(0) != 0)
        die("setuid(0) failed (is the binary setuid root?)");

    struct stat st;
    ssize_t n = 0;
    char *buf = read_pam(&st, &n);
    if (strcmp(cmd, "install") == 0)
        do_install(buf, n, &st);
    else
        do_uninstall(buf, n, &st);
    return 0;
}
