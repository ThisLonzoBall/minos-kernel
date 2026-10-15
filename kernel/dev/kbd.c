#include "arch/arch.h"

// ps/2 keyboard, scancode set 1 (what qemu gives us by default)

static const char normal_map[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ',
};

static const char shift_map[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ',
};

#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_CTRL   0x1D
#define SC_CAPS   0x3A

static bool shift, ctrl, caps;

// ring buffer of typed chars
#define KBD_BUF 256
static char buf[KBD_BUF];
static volatile int head, tail;

static void push_char(char c)
{
    int next = (head + 1) % KBD_BUF;
    if (next == tail)
        return;   // full, just drop it
    buf[head] = c;
    head = next;
}

static void kbd_irq(struct trap_frame *tf)
{
    (void)tf;
    uint8_t sc = inb(0x60);

    // 0xE0 means an extended key (arrows etc), ignore those for now
    if (sc == 0xE0)
        return;

    bool released = sc & 0x80;
    sc &= 0x7F;

    if (sc == SC_LSHIFT || sc == SC_RSHIFT) {
        shift = !released;
        return;
    }
    if (sc == SC_CTRL) {
        ctrl = !released;
        return;
    }
    if (released)
        return;
    if (sc == SC_CAPS) {
        caps = !caps;
        return;
    }

    char c = shift ? shift_map[sc] : normal_map[sc];
    if (!c)
        return;

    // caps lock only flips letters
    if (caps && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
        c ^= 0x20;

    // ctrl+letter -> control code (ctrl+c = 3 etc)
    if (ctrl && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
        c = (c & 0x1F);

    push_char(c);
}

void kbd_init(void)
{
    // throw away anything sitting in the buffer from before
    while (inb(0x64) & 1)
        inb(0x60);
    irq_register(IRQ_KBD, kbd_irq);
}

int kbd_getc(void)
{
    if (head == tail)
        return -1;
    char c = buf[tail];
    tail = (tail + 1) % KBD_BUF;
    return (uint8_t)c;
}
