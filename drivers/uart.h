#pragma once

void init_uart();
void uart_putc(char c);
void uart_puts(const char *s);
int uart_getc(void);
