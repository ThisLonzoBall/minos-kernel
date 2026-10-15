#include "kernel.h"

// tiny printf. supports %d %i %u %x %X %p %s %c %% with optional
// zero padding / width and the l / ll / z modifiers

typedef void (*putc_fn)(char c, void *arg);

static int print_num(putc_fn out, void *arg, uint64_t n, int base, bool upper,
                     int width, char pad, bool neg)
{
    char tmp[32];
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int len = 0, count = 0;

    do {
        tmp[len++] = digits[n % base];
        n /= base;
    } while (n);

    if (neg) {
        if (pad == '0') {
            out('-', arg);
            count++;
            width--;
        } else {
            tmp[len++] = '-';
        }
    }

    for (int i = len; i < width; i++) {
        out(pad, arg);
        count++;
    }
    while (len--) {
        out(tmp[len], arg);
        count++;
    }
    return count;
}

static int do_printf(putc_fn out, void *arg, const char *fmt, va_list ap)
{
    int count = 0;

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            out(*fmt, arg);
            count++;
            continue;
        }
        fmt++;

        char pad = ' ';
        int width = 0;
        int longs = 0;

        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        while (*fmt == 'l') {
            longs++;
            fmt++;
        }
        if (*fmt == 'z') {
            longs = 2;
            fmt++;
        }

        switch (*fmt) {
        case 'd':
        case 'i': {
            int64_t v = longs ? va_arg(ap, int64_t) : va_arg(ap, int);
            bool neg = v < 0;
            count += print_num(out, arg, neg ? -(uint64_t)v : (uint64_t)v, 10, false, width, pad, neg);
            break;
        }
        case 'u': {
            uint64_t v = longs ? va_arg(ap, uint64_t) : va_arg(ap, unsigned int);
            count += print_num(out, arg, v, 10, false, width, pad, false);
            break;
        }
        case 'x':
        case 'X': {
            uint64_t v = longs ? va_arg(ap, uint64_t) : va_arg(ap, unsigned int);
            count += print_num(out, arg, v, 16, *fmt == 'X', width, pad, false);
            break;
        }
        case 'p': {
            uint64_t v = (uint64_t)va_arg(ap, void *);
            out('0', arg);
            out('x', arg);
            count += 2 + print_num(out, arg, v, 16, false, 16, '0', false);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s)
                s = "(null)";
            int len = strlen(s);
            for (int i = len; i < width; i++) {
                out(' ', arg);
                count++;
            }
            while (*s) {
                out(*s++, arg);
                count++;
            }
            break;
        }
        case 'c':
            out((char)va_arg(ap, int), arg);
            count++;
            break;
        case '%':
            out('%', arg);
            count++;
            break;
        case 0:
            return count;
        default:
            // don't know this one, just print it as is
            out('%', arg);
            out(*fmt, arg);
            count += 2;
            break;
        }
    }
    return count;
}

static void console_out(char c, void *arg)
{
    (void)arg;
    console_putc(c);
}

int vkprintf(const char *fmt, va_list ap)
{
    return do_printf(console_out, NULL, fmt, ap);
}

int kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vkprintf(fmt, ap);
    va_end(ap);
    return n;
}

struct strbuf {
    char *buf;
    size_t size;
    size_t pos;
};

static void buf_out(char c, void *arg)
{
    struct strbuf *sb = arg;
    if (sb->pos + 1 < sb->size)
        sb->buf[sb->pos] = c;
    sb->pos++;
}

int ksnprintf(char *buf, size_t size, const char *fmt, ...)
{
    struct strbuf sb = { buf, size, 0 };
    va_list ap;
    va_start(ap, fmt);
    int n = do_printf(buf_out, &sb, fmt, ap);
    va_end(ap);
    if (size)
        buf[sb.pos < size ? sb.pos : size - 1] = 0;
    return n;
}

void panic(const char *fmt, ...)
{
    cli();
    kprintf("\n*** KERNEL PANIC: ");
    va_list ap;
    va_start(ap, fmt);
    vkprintf(fmt, ap);
    va_end(ap);
    kprintf("\n");
    for (;;)
        hlt();
}
