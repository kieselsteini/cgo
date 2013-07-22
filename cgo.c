/*
 * cgo - a simple terminal based gopher client
 * Copyright (c) 2013 Sebastian Steinhauer <s.steinhauer@yahoo.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* some "configuration" */
#define DEFAULT_HOST        "gopher.floodgap.com"
#define DEFAULT_PORT        "70"
#define DEFAULT_SELECTOR    "/"
#define CMD_TEXT            "less"
#define CMD_IMAGE           "display"
#define CMD_BROWSER         "firefox"
#define CMD_PLAYER          "mplayer"
#define COLOR_PROMT         "1;34"
#define COLOR_SELECTOR      "1;32"

/* some internal defines */
#define KEY_RANGE   ('z' - 'a')

typedef struct link_s link_t;
struct link_s {
    link_t  *next;
    char    which;
    short   key;
    char    *host;
    char    *port;
    char    *selector;
};

char        tmpfilename[256];
link_t      *links = NULL;
link_t      *history = NULL;
int         link_key;
char        current_host[512], current_port[64], current_selector[1024];


void usage()
{
    fputs("usage: cgo [-h host] [-p port] [-s selector] [-v] [-H]\n",
            stderr);
    exit(EXIT_SUCCESS);
}

void banner(FILE *f)
{
    fputs("cgo 0.1.0  Copyright (C) 2012  Sebastian Steinhauer\n", f);
}

int dial(const char *host, const char *port, const char *selector)
{
    struct addrinfo hints;
    struct addrinfo *res, *r;
    int             srv = -1, l;
    char            request[512];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &res) != 0) {
        fprintf(stderr, "error: cannot resolve hostname '%s:%s': %s\n",
                host, port, strerror(errno));
        return -1;
    }
    for (r = res; r; r = r->ai_next) {
        srv = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
        if (srv == -1)
            continue;
        if (connect(srv, r->ai_addr, r->ai_addrlen) == 0)
            break;
        close(srv);
    }
    freeaddrinfo(res);
    if (! r) {
        fprintf(stderr, "error: cannot connect to host '%s:%s'\n",
                host, port);
        return -1;
    }
    snprintf(request, sizeof(request), "%s\r\n", selector);
    l = strlen(request);
    if (write(srv, request, l) != l) {
        fprintf(stderr, "error: cannot complete request\n");
        close(srv);
        return -1;
    }
    return srv;
}

int read_line(int fd, char *buf, size_t buf_len)
{
    size_t  i = 0;
    char    c = 0;

    do {
        if (read(fd, &c, sizeof(char)) != sizeof(char))
            return 0;
        if (c != '\r')
            buf[i++] = c;
    } while (c != '\n' && i < buf_len);
    buf[i - 1] = '\0';
    return 1;
}

int download_file(const char *host, const char *port,
        const char *selector, int fd)
{
    int     srvfd, len;
    char    buffer[1024];

    printf("downloading [%s]...\r", selector);
    srvfd = dial(host, port, selector);
    if (srvfd == -1) {
        printf("\033[2Kerror: downloading [%s] failed\n", selector);
        close(fd);
        return 0;
    }
    while ((len = read(srvfd, buffer, sizeof(buffer))) > 0) {
        write(fd, buffer, len);
    }
    close(fd);
    close(srvfd);
    printf("\033[2Kdownloading [%s] complete\n", selector);
    return 1;
}

int download_temp(const char *host, const char *port, const char *selector)
{
    int     tmpfd;

    strcpy(tmpfilename, "/tmp/cgoXXXXXX");
    tmpfd = mkstemp(tmpfilename);
    if (tmpfd == -1) {
        fputs("error: unable to create tmp file\n", stderr);
        return 0;
    }
    if (! download_file(host, port, selector, tmpfd)) {
        unlink(tmpfilename);
        return 0;
    }
    return 1;
}

int make_key(char c1, char c2)
{
    if (! c1 || ! c2)
        return -1;
    return ((c1 - 'a') * KEY_RANGE) + (c2 - 'a');
}

void make_key_str(int key, char *c1, char *c2)
{
    *c1 = 'a' + (key / KEY_RANGE);
    *c2 = 'a' + (key % KEY_RANGE);
}

void add_link(char which, const char *name,
        const char *host, const char *port, const char *selector)
{
    link_t  *link;
    char    a = 0, b = 0;

    if (! host || ! port || ! selector)
        return; /* ignore incomplete selectors */
    link = calloc(1, sizeof(link_t));
    link->which = which;
    link->key = link_key;
    link->host = strdup(host);
    link->port = strdup(port);
    link->selector = strdup(selector);
    if (! links)
        link->next = NULL;
    else
        link->next = links;
    links = link;

    make_key_str(link_key++, &a, &b);    
    printf("\033[%sm%c%c\033[0m \033[1m%s\033[0m\n",
            COLOR_SELECTOR, a, b, name);
}

void clear_links()
{
    link_t  *link, *next;
    
    for (link = links; link; ) {
        next = link->next;
        free(link->host);
        free(link->port);
        free(link->selector);
        free(link);
        link = next;
    }
    links = NULL;
    link_key = 0;
}

void add_history()
{
    link_t  *link;

    link = calloc(1, sizeof(link_t));
    link->host = strdup(current_host);
    link->port = strdup(current_port);
    link->selector = strdup(current_selector);
    link->which = 0;    /* not needed for history...just clear them */
    link->key = 0;
    if (! history)
        link->next = NULL;
    else
        link->next = history;
    history = link;
}

void view_directory(const char *host, const char *port,
        const char *selector, int make_current)
{
    int     srvfd, i;
    char    line[1024];
    char    *lp, *last, *fields[4];

    srvfd = dial(host, port, selector);
    if (srvfd != -1) {  /* only adapt current prompt when successful */
        /* make history entry */
        if (make_current)
            add_history();
        /* don't overwrite the current_* things... */
        if (host != current_host)
            snprintf(current_host, sizeof(current_host), "%s", host);
        if (port != current_port)
            snprintf(current_port, sizeof(current_port), "%s", port);
        if (selector != current_selector)
            snprintf(current_selector, sizeof(current_selector),
                    "%s", selector);
    }
    clear_links();  /* clear links *AFTER* dialing out!! */
    if (srvfd == -1)
        return; /* quit if not successful */
    while (read_line(srvfd, line, sizeof(line))) {
        /* tokenize */
        for (i = 0; i < 4; i++)
            fields[i] = NULL;
        last = &line[1];
        for (lp = last, i = 0; i < 4; lp++) {
            if (*lp == '\t' || *lp == '\0') {
                fields[i] = last;
                last = lp + 1;
                if (*lp == '\0')
                    break;
                *lp = '\0';
                i++;
            }
        }
        /* determine listing type */
        switch (line[0]) {
            case 'i':
                printf("   %s\n", fields[0]);
                break;
            case '.':   /* some gopher servers use this */
                puts("");
                break;
            case '0':
            case '1':
            case '5':
            case '7':
            case '9':
            case 'g':
            case 'I':
            case 'h':
            case 's':
                add_link(line[0], fields[0], fields[2], fields[3], fields[1]);
                break;
            default:
                printf("miss [%c]: %s\n", line[0], fields[0]);
                break;
        }
    }
    close(srvfd);
}

void view_file(const char *cmd, const char *host,
        const char *port, const char *selector)
{
    pid_t   pid;
    int     status;

    if (! download_temp(host, port, selector))
        return;
    printf("executing: %s %s\n", cmd, tmpfilename);
    pid = fork();
    if (pid == 0) {
        if (execlp(cmd, cmd, tmpfilename, NULL) == -1)
            puts("error: execlp() failed!");
    } else if (pid == -1) {
        puts("error: fork() failed");
    }
    sleep(1); /* to wait for browsers etc. that return immediatly */
    waitpid(pid, &status, 0);
    unlink(tmpfilename);
}

void view_download(const char *host, const char *port, const char *selector)
{
    int     fd;
    char    filename[1024], line[1024];

    snprintf(filename, sizeof(filename), "%s", strrchr(selector, '/') + 1);
    printf("enter filename for download [%s]: ", filename);
    fflush(stdout);
    if (! read_line(0, line, sizeof(line))) {
        puts("download aborted");
        return;
    }
    if (strlen(line) > 0)
        strcpy(filename, line);
    fd = open(filename, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        printf("error: unable to create file [%s]: %s\n",
                filename, strerror(errno));
        return;
    }
    if (! download_file(host, port, selector, fd)) {
        printf("error: unable to download [%s]\n", selector);
        unlink(filename);
        return;
    }
}

void view_search(const char *host, const char *port, const char *selector)
{
    char    search_selector[1024];
    char    line[1024];

    printf("enter search string: ");
    fflush(stdout);
    if (! read_line(0, line, sizeof(line))) {
        puts("search aborted");
        return;
    }
    snprintf(search_selector, sizeof(search_selector), "%s\t%s",
            selector, line);
    view_directory(host, port, search_selector, 1);
}

void view_history(int key)
{
    int     history_key = 0;
    char    a, b;
    link_t  *link;

    if (! history) {
        puts("(empty history)");
        return;
    }
    if ( key < 0 ) {
        puts("(history)");
        for ( link = history; link; link = link->next ) {
            make_key_str(history_key++, &a, &b);
            printf("\033[%sm%c%c\033[0m \033[1m%s:%s%s\033[0m\n",
                COLOR_SELECTOR, a, b, link->host, link->port, link->selector);
        }
    } else {
        /* traverse history list */
        for ( link = history; link; link = link->next, ++history_key ) {
            if ( history_key == key ) {
                view_directory(link->host, link->port, link->selector, 0);
                return;
            }
        }
        puts("history item not found");
    }
}

void pop_history()
{
    link_t  *next;

    if (! history) {
        puts("(empty history)");
        return;
    }
    /* reload page from history (and don't count as history) */
    view_directory(history->host, history->port, history->selector, 0);
    /* history is history... :) */
    next = history->next;
    free(history->host);
    free(history->port);
    free(history->selector);
    free(history);
    history = next;
}

int follow_link(int key)
{
    link_t  *link;

    for (link = links; link; link = link->next) {
        if (link->key != key)
            continue;
        switch (link->which) {
            case '0':
                view_file(CMD_TEXT, link->host, link->port, link->selector);
                break;
            case '1':
                view_directory(link->host, link->port, link->selector, 1);
                break;
            case '7':
                view_search(link->host, link->port, link->selector);
                break;
            case '5':
            case '9':
                view_download(link->host, link->port, link->selector);
                break;
            case 'g':
            case 'I':
                view_file(CMD_IMAGE, link->host, link->port, link->selector);
                break;
            case 'h':
                view_file(CMD_BROWSER, link->host, link->port,
                        link->selector);
                break;
            case 's':
                view_file(CMD_PLAYER, link->host, link->port, link->selector);
                break;
            default:
                printf("mssing handler [%c]\n", link->which);
                break;
        }
        return 1; /* return the array is broken after view! */
    }
    return 0;
}

void download_link(int key)
{
    link_t  *link;

    for (link = links; link; link = link->next) {
        if (link->key != key)
            continue;
        view_download(link->host, link->port, link->selector);
        return;
    }
    puts("link not found");
}

int main(int argc, char *argv[])
{
    int     i;
    char    line[1024];
    char    *host, *port, *selector;

    /* copy defaults */
    host = DEFAULT_HOST;
    port = DEFAULT_PORT;
    selector = DEFAULT_SELECTOR;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') switch(argv[i][1]) {
            case 'H':
                usage();
                break;
            case 'v':
                banner(stderr);
                exit(EXIT_FAILURE);
            case 'h':
                if (++i >= argc) usage();
                host = argv[i];
                break;
            case 'p':
                if (++i >= argc) usage();
                port = argv[i];
                break;
            case 's':
                if (++i >= argc) usage();
                selector = argv[i];
                break;
            default:
                usage();
        }
    }

    banner(stdout);
    view_directory(host, port, selector, 0);
    for (;;) {  /* main loop */
        printf("\033[%sm%s:%s%s\033[0m ", COLOR_PROMT,
                current_host, current_port, current_selector);
        fflush(stdout); /* to display the prompt */
        if (! read_line(0, line, sizeof(line))) {
            puts("QUIT");
            return EXIT_SUCCESS;
        }
        switch (line[0]) {
            case '?':
                puts(
                    "?          - help\n"
                    "*          - reload directory\n"
                    "<          - go back in history\n"
                    ".LINK      - download the given link\n"
                    "h[LINk]    - show history\n"
                    "             jump to specific history item\n"
                    "C^d        - quit");
                break;
            case '<':
                pop_history();
                break;
            case '*':
                view_directory(current_host, current_port,
                        current_selector, 0);
                break;
            case '.':
                download_link(make_key(line[1], line[2]));
                break;
            case 'h':
                view_history(make_key(line[1], line[2]));
                break;
            default:
                follow_link(make_key(line[0], line[1]));
                break;
        }
    }
    return EXIT_SUCCESS; /* never get's here but stops cc complaining */
}

