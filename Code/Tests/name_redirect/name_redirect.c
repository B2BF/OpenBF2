/*
 * [TEMP] [CHOSEN, not recovered] Test-only host-name redirect for locally built macOS images.
 * Never linked into a game image and never packaged; see README.md.
 *
 * Loaded with DYLD_INSERT_LIBRARIES into bf2 or bf2_client. With OPENBF2_TEST_REDIRECT_DOMAIN set
 * (for example test.openbf2), every host name equal to that domain or ending in ".<domain>" resolves
 * to 127.0.0.1. Every other name goes to the system resolver unchanged. Started with
 * +gamespy <domain>, the game then reaches all of its online services (BF2Web.<domain>,
 * gpcm.<domain>, battlefield2.master.<domain>, ...) on loopback without a hosts-file or DNS change.
 *
 * OPENBF2_TEST_REDIRECT_HTTP_PORT (optional) moves TCP connections to 127.0.0.1:80 to that loopback
 * port, because an unprivileged process cannot listen on 127.0.0.1:80. The stats web requests use
 * port 80, which is fixed in the game.
 *
 * OPENBF2_TEST_REDIRECT_LOG=1 writes one stderr line per redirected name or connection.
 */
#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

static int redirect_log(void)
{
    const char* value = getenv("OPENBF2_TEST_REDIRECT_LOG");
    return value && *value && strcmp(value, "0") != 0;
}

/* True when name is the configured domain or one of its subdomains (case-insensitive). */
static int redirected_name(const char* name)
{
    const char* domain = getenv("OPENBF2_TEST_REDIRECT_DOMAIN");
    size_t name_length, domain_length;
    if (!name || !domain || !*domain)
        return 0;
    name_length = strlen(name);
    domain_length = strlen(domain);
    /* A trailing dot is the same fully qualified name. */
    if (name_length && name[name_length - 1] == '.')
        --name_length;
    if (name_length < domain_length || strncasecmp(name + name_length - domain_length, domain, domain_length) != 0)
        return 0;
    return name_length == domain_length || name[name_length - domain_length - 1] == '.';
}

static int redirect_http_port(void)
{
    const char* value = getenv("OPENBF2_TEST_REDIRECT_HTTP_PORT");
    int port;
    if (!value || !*value)
        return 0;
    port = atoi(value);
    return port > 0 && port < 65536 && port != 80 ? port : 0;
}

static int redirect_getaddrinfo(const char* node, const char* service, const struct addrinfo* hints,
                                struct addrinfo** result)
{
    if (redirected_name(node))
    {
        /* Answered by the system's numeric parser, so freeaddrinfo releases it as usual. */
        struct addrinfo numeric;
        if (hints)
            numeric = *hints;
        else
        {
            memset(&numeric, 0, sizeof(numeric));
            numeric.ai_family = AF_UNSPEC;
        }
        if (numeric.ai_family == AF_UNSPEC)
            numeric.ai_family = AF_INET;
        numeric.ai_flags = (numeric.ai_flags & ~AI_CANONNAME) | AI_NUMERICHOST;
        if (redirect_log())
            fprintf(stderr, "[name-redirect] %s -> 127.0.0.1\n", node);
        return getaddrinfo("127.0.0.1", service, &numeric, result);
    }
    return getaddrinfo(node, service, hints, result);
}

static struct hostent* redirect_gethostbyname(const char* name)
{
    if (redirected_name(name))
    {
        /* One static entry, as gethostbyname's own result is static. */
        static struct in_addr address;
        static char* addresses[2];
        static char* aliases[1];
        static char canonical[256];
        static struct hostent entry;
        address.s_addr = htonl(INADDR_LOOPBACK);
        addresses[0] = (char*)&address;
        addresses[1] = 0;
        aliases[0] = 0;
        strncpy(canonical, name, sizeof(canonical) - 1);
        canonical[sizeof(canonical) - 1] = 0;
        entry.h_name = canonical;
        entry.h_aliases = aliases;
        entry.h_addrtype = AF_INET;
        entry.h_length = 4;
        entry.h_addr_list = addresses;
        if (redirect_log())
            fprintf(stderr, "[name-redirect] %s -> 127.0.0.1\n", name);
        return &entry;
    }
    return gethostbyname(name);
}

static int redirect_connect(int fd, const struct sockaddr* address, socklen_t length)
{
    const int port = redirect_http_port();
    if (port && address && address->sa_family == AF_INET && length >= (socklen_t)sizeof(struct sockaddr_in))
    {
        struct sockaddr_in changed;
        memcpy(&changed, address, sizeof(changed));
        if (changed.sin_port == htons(80) && changed.sin_addr.s_addr == htonl(INADDR_LOOPBACK))
        {
            changed.sin_port = htons((unsigned short)port);
            if (redirect_log())
                fprintf(stderr, "[name-redirect] 127.0.0.1:80 -> 127.0.0.1:%d\n", port);
            return connect(fd, (const struct sockaddr*)&changed, sizeof(changed));
        }
    }
    return connect(fd, address, length);
}

/* dyld's interposing table: calls from every other image go to the replacement; calls made inside
 * this library reach the original functions. */
struct interpose
{
    const void* replacement;
    const void* original;
};

__attribute__((used, section("__DATA,__interpose"))) static const struct interpose interposers[] = {
    { (const void*)&redirect_getaddrinfo, (const void*)&getaddrinfo },
    { (const void*)&redirect_gethostbyname, (const void*)&gethostbyname },
    { (const void*)&redirect_connect, (const void*)&connect },
};
