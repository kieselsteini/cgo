/*
 * cgo - a simple terminal based gopher client
 * Copyright (c) 2014 Sebastian Steinhauer <s.steinhauer@yahoo.de>
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
#include <ctype.h>

/* some "configuration" */
#define START_URI           "gopher://gopher.floodgap.com:70"
#define CMD_TEXT            "less"
#define CMD_IMAGE           "display"
#define CMD_BROWSER         "firefox"
#define CMD_PLAYER          "mplayer"
#define CMD_TELNET          "telnet"
#define COLOR_PROMPT        "1;34"
#define COLOR_SELECTOR      "1;32"
#define HEAD_CHECK_LEN      5
#define GLOBAL_CONFIG_FILE  "/etc/cgorc"
#define LOCAL_CONFIG_FILE   "/.cgorc"
#define NUM_BOOKMARKS       20

/* some internal defines */
#define KEY_RANGE   ('z' - 'a')

/* structs */
typedef struct link_s link_t;
struct link_s {
    link_t  *next;
    char    which;
    short   key;
    char    *host;
    char    *port;
    char    *selector;
};

typedef struct config_s config_t;
struct config_s {
    char    start_uri[512];
    char    cmd_text[512];
    char    cmd_image[512];
    char    cmd_browser[512];
    char    cmd_player[512];
    char    color_prompt[512];
    char    color_selector[512];
};

char        tmpfilename[256];
link_t      *links = NULL;
link_t      *history = NULL;
int         link_key;
char        current_host[512], current_port[64], current_selector[1024];
char        parsed_host[512], parsed_port[64], parsed_selector[1024];
char        bookmarks[NUM_BOOKMARKS][512];
config_t    config;

/* function prototypes */
int parse_uri(const char *uri);

/* implementation */
void usage()
{
    fputs("usage: cgo [-v] [-H] [gopher URI]\n",
            stderr);
    exit(EXIT_SUCCESS);
}

void banner(FILE *f)
{
    fputs("cgo 0.4.1  Copyright (c) 2014  Sebastian Steinhauer\n", f);
}

void parse_config_line(const char *line)
{
    char    token[1024];
    char    bkey[128];
    char    *value = NULL;
    int     i, j;

    while (*line == ' ' || *line == '\t') line++;
    for (i = 0; *line && *line != ' ' && *line != '\t'; line++)
        if (i < sizeof(token) - 1) token[i++] = *line;
    token[i] = 0;

    if (! strcmp(token, "start_uri")) value = &config.start_uri[0];
    else if (! strcmp(token, "cmd_text")) value = &config.cmd_text[0];
    else if (! strcmp(token, "cmd_browser")) value = &config.cmd_browser[0];
    else if (! strcmp(token, "cmd_image")) value = &config.cmd_image[0];
    else if (! strcmp(token, "cmd_player")) value = &config.cmd_player[0];
    else if (! strcmp(token, "color_prompt")) value = &config.color_prompt[0];
    else if (! strcmp(token, "color_selector")) value = &config.color_selector[0];
    else {
        for (j = 0; j < NUM_BOOKMARKS; j++) {
            snprintf(bkey, sizeof(bkey), "bookmark%d", j+1);
            if (! strcmp(token, bkey)) {
                value = &bookmarks[j][0];
                break;
            }
        }
        if (! value) return;
    };

    while (*line == ' ' || *line == '\t') line++;
    for (i = 0; *line; line++)
        if (i < 512-1) value[i++] = *line;
    for (i--; i > 0 && (value[i] == ' ' || value[i] == '\t'); i--) ;
    value[++i] = 0;

}

void load_config(const char *filename)
{
    FILE    *fp;
    int     ch, i;
    char    line[1024];

    fp = fopen(filename, "r");
    if (! fp) return;

    memset(line, 0, sizeof(line));
    i = 0;
    ch = fgetc(fp);
    while (1) {
        switch (ch) {
            case '#':
                while (ch != '\n' && ch != -1)
                    ch = fgetc(fp);
                break;
            case -1:
                parse_config_line(line);
                fclose(fp);
                return;
            case '\r':
                ch = fgetc(fp);
                break;
            case '\n':
                parse_config_line(line);
                memset(line, 0, sizeof(line));
                i = 0;
                ch = fgetc(fp);
                break;
            default:
                if (i < sizeof(line) - 1)
                    line[i++] = ch;
                ch = fgetc(fp);
                break;
        }
    }
}

void init_config()
{
    char        filename[1024];
    const char  *home;
    int         i;

    /* copy defaults */
    snprintf(config.start_uri, sizeof(config.start_uri), START_URI);
    snprintf(config.cmd_text, sizeof(config.cmd_text), "%s", CMD_TEXT);
    snprintf(config.cmd_image, sizeof(config.cmd_image), "%s", CMD_IMAGE);
    snprintf(config.cmd_browser, sizeof(config.cmd_browser), "%s", CMD_BROWSER);
    snprintf(config.cmd_player, sizeof(config.cmd_player), "%s", CMD_PLAYER);
    snprintf(config.color_prompt, sizeof(config.color_prompt), "%s", COLOR_PROMPT);
    snprintf(config.color_selector, sizeof(config.color_selector), "%s", COLOR_SELECTOR);
    for (i = 0; i < NUM_BOOKMARKS; i++) bookmarks[i][0] = 0;
    /* read configs */
    load_config(GLOBAL_CONFIG_FILE);
    home = getenv("HOME");
    if (home) {
        snprintf(filename, sizeof(filename), "%s%s", home, LOCAL_CONFIG_FILE);
        load_config(filename);
    }
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
    int             srvfd, len;
    unsigned long   total = 0;
    char            buffer[4096];

    printf("downloading [%s]...\r", selector);
    srvfd = dial(host, port, selector);
    if (srvfd == -1) {
        printf("\033[2Kerror: downloading [%s] failed\n", selector);
        close(fd);
        return 0;
    }
    while ((len = read(srvfd, buffer, sizeof(buffer))) > 0) {
        write(fd, buffer, len);
        total += len;
        printf("downloading [%s] (%ld kb)...\r", selector, total / 1024);
    }
    close(fd);
    close(srvfd);
    printf("\033[2Kdownloading [%s] complete\n", selector);
    return 1;
}

int download_temp(const char *host, const char *port, const char *selector)
{
    int     tmpfd;

#if defined(__OpenBSD__)
    strlcpy(tmpfilename, "/tmp/cgoXXXXXX", sizeof(tmpfilename));
#else
    strcpy(tmpfilename, "/tmp/cgoXXXXXX");
#endif
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
            config.color_selector, a, b, name);
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

void handle_directory_line(char *line)
{
    int     i;
    char    *lp, *last, *fields[4];

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
        case '3':
            printf("   %s\n", fields[0]);
            break;
        case '.':   /* some gopher servers use this */
            puts("");
            break;
        case '0':
        case '1':
        case '5':
        case '7':
        case '8':
        case '9':
        case 'g':
        case 'I':
        case 'p':
        case 'h':
        case 's':
            add_link(line[0], fields[0], fields[2], fields[3], fields[1]);
            break;
        default:
            printf("miss [%c]: %s\n", line[0], fields[0]);
            break;
    }
}

int is_valid_directory_entry(const char *line)
{
    switch (line[0]) {
        case 'i':
        case '3':
        case '.':   /* some gopher servers use this */
        case '0':
        case '1':
        case '5':
        case '7':
        case '8':
        case '9':
        case 'g':
        case 'I':
        case 'p':
        case 'h':
        case 's':
            return 1;
        default:
            return 0;
    }
}

void view_directory(const char *host, const char *port,
        const char *selector, int make_current)
{
    int     is_dir;
    int     srvfd, i, head_read;
    char    line[1024];
    char    head[HEAD_CHECK_LEN][1024];

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
    head_read = 0;
    is_dir = 1;
    while (head_read < HEAD_CHECK_LEN && read_line(srvfd, line, sizeof(line))) {
        strcpy(head[head_read], line);
        if (!is_valid_directory_entry(head[head_read])) {
            is_dir = 0;
            break;
        }
        head_read++;
    }
    if (!is_dir) {
        puts("error: Not a directory.");
        close(srvfd);
        return;
    }
    for (i = 0; i < head_read; i++) {
        handle_directory_line(head[i]);
    }
    while (read_line(srvfd, line, sizeof(line))) {
        handle_directory_line(line);
    }
    close(srvfd);
}

void view_file(const char *cmd, const char *host,
        const char *port, const char *selector)
{
    pid_t   pid;
    int     status, i, j;
    char    buffer[1024], *argv[32], *p;

    printf("h(%s) p(%s) s(%s)\n", host, port, selector);

    if (! download_temp(host, port, selector))
        return;

    /* parsed command line string */
    argv[0] = &buffer[0];
    for (p = (char*) cmd, i = 0, j = 1; *p && i < sizeof(buffer) - 1 && j < 30; ) {
        if (*p == ' ' || *p == '\t') {
            buffer[i++] = 0;
            argv[j++] = &buffer[i];
            while (*p == ' ' || *p == '\t') p++;
        } else buffer[i++] = *p++;
    }
    buffer[i] = 0;
    argv[j++] = tmpfilename;
    argv[j] = NULL;

    /* fork and execute */
    printf("executing: %s %s\n", cmd, tmpfilename);
    pid = fork();
    if (pid == 0) {
        if (execvp(argv[0], argv) == -1)
            puts("error: execvp() failed!");
    } else if (pid == -1) puts("error: fork() failed");
    sleep(1); /* to wait for browsers etc. that return immediatly */
    waitpid(pid, &status, 0);
    unlink(tmpfilename);
}

void view_telnet(const char *host, const char *port)
{
    pid_t   pid;
    int     status;

    printf("executing: %s %s %s\n", CMD_TELNET, host, port);
    pid = fork();
    if (pid == 0) {
        if (execlp(CMD_TELNET, CMD_TELNET, host, port, NULL) == -1)
            puts("error: execlp() failed!");
    } else if (pid == -1) puts("error: fork() failed!");
    waitpid(pid, &status, 0);
    puts("(done)");
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
#if defined(__OpenBSD__)
        strlcpy(filename, line, sizeof(filename));
#else
        strcpy(filename, line);
#endif
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

void view_bookmarks(int key)
{
    int     i;
    char    a, b;

    if (key < 0) {
        puts("(bookmarks)");
        for (i = 0; i < NUM_BOOKMARKS; i++) {
            if (bookmarks[i][0]) {
                make_key_str(i, &a, &b);
                printf("\033[%sm%c%c\033[0m \033[1m%s\033[0m\n",
                    COLOR_SELECTOR, a, b, &bookmarks[i][0]);
            }
        }
    } else {
        for (i = 0; i < NUM_BOOKMARKS; i++) {
            if (bookmarks[i][0] && i == key) {
                if (parse_uri(&bookmarks[i][0])) view_directory(parsed_host, parsed_port, parsed_selector, 0);
                else printf("invalid gopher URI: %s", &bookmarks[i][0]);
                return;
            }
        }
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
                view_file(&config.cmd_text[0], link->host, link->port, link->selector);
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
            case '8':
                view_telnet(link->host, link->port);
                break;
            case 'g':
            case 'I':
            case 'p':
                view_file(&config.cmd_image[0], link->host, link->port, link->selector);
                break;
            case 'h':
                view_file(&config.cmd_browser[0], link->host, link->port, link->selector);
                break;
            case 's':
                view_file(&config.cmd_player[0], link->host, link->port, link->selector);
                break;
            default:
                printf("missing handler [%c]\n", link->which);
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

int parse_uri(const char *uri)
{
    int     i;

    /* strip gopher:// */
    if (! strncmp(uri, "gopher://", 9))
        uri += 9;
    /* parse host */
    for (i = 0; *uri && *uri != ':' && *uri != '/'; uri++) {
        if (*uri != ' ' && i < sizeof(parsed_host) - 1)
            parsed_host[i++] = *uri;
    }
    if (i > 0) parsed_host[i] = 0;
    else return 0;
    /* parse port */
    if (*uri == ':') {
        uri++;
        for (i = 0; *uri && *uri != '/'; uri++)
            if (*uri != ' ' && i < sizeof(parsed_port) - 1)
                parsed_port[i++] = *uri;
        parsed_port[i] = 0;
    } else snprintf(parsed_port, sizeof(parsed_port), "%d", 70);
    /* parse selector */
    if (*uri == '/') {
        for (i = 0; *uri; uri++)
            if (i < sizeof(parsed_selector) - 1)
                parsed_selector[i++] = *uri;
        parsed_selector[i++] = *uri;
    } else snprintf(parsed_selector, sizeof(parsed_selector), "%s", "/");
    return 1;
}

int main(int argc, char *argv[])
{
    int     i;
    char    line[1024], *uri;

    /* copy defaults */
    init_config();
    uri = &config.start_uri[0];

    /* parse command line */
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') switch(argv[i][1]) {
            case 'H':
                usage();
                break;
            case 'v':
                banner(stderr);
                exit(EXIT_FAILURE);
            default:
                usage();
        } else {
            uri = argv[i];
        }
    }

    /* parse uri */
    banner(stdout);
    if (! parse_uri(uri)) {
        banner(stderr);
        fprintf(stderr, "invalid gopher URI: %s", argv[i]);
        exit(EXIT_FAILURE);
    }

    /* main loop */
    view_directory(parsed_host, parsed_port, parsed_selector, 0);
    for (;;) {
        printf("\033[%sm%s:%s%s\033[0m ", config.color_prompt,
                current_host, current_port, current_selector);
        fflush(stdout); /* to display the prompt */
        if (! read_line(0, line, sizeof(line))) {
            puts("QUIT");
            return EXIT_SUCCESS;
        }
        i = strlen(line);
        switch (line[0]) {
            case '?':
                puts(
                    "?          - help\n"
                    "*          - reload directory\n"
                    "<          - go back in history\n"
                    ".LINK      - download the given link\n"
                    "h          - show history\n"
                    "hLINK      - jump to the specified history item\n"
                    "gURI       - jump to the given gopher URI\n"
                    "b          - show bookmarks\n"
                    "bLINK      - jump to the specified bookmark item\n"
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
                if (i == 1 || i == 3) view_history(make_key(line[1], line[2]));
                else follow_link(make_key(line[0], line[1]));
                break;
            case 'g':
                if (i != 2) {
                    if (parse_uri(&line[1])) view_directory(parsed_host, parsed_port, parsed_selector, 1);
                    else puts("invalid gopher URI");
                } else follow_link(make_key(line[0], line[1]));
                break;
            case 'b':
                if (i == 1 || i == 3) view_bookmarks(make_key(line[1], line[2]));
                else follow_link(make_key(line[0], line[1]));
                break;
            default:
                follow_link(make_key(line[0], line[1]));
                break;
        }
    }
    return EXIT_SUCCESS; /* never get's here but stops cc complaining */
}
