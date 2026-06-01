#pragma once

void init_uart(void);
void uart_putc(char c);
void uart_puts(const char *s);
int uart_getc(void);
void uart_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
