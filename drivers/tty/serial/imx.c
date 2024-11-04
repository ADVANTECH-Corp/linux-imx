...
/* in rs485 mode disable transmitter if shifter is empty */
if (port->rs485.flags & SER_RS485_ENABLED &&
    readl(port->membase + USR2) & USR2_TXDC) {
    temp = readl(port->membase + UCR2);
    if (port->rs485.flags & SER_RS485_RTS_AFTER_SEND)
        imx_port_rts_active(sport, &temp);
    else
        imx_port_rts_inactive(sport, &temp);
    temp |= UCR2_RXEN;
    writel(temp, port->membase + UCR2);

    temp = readl(port->membase + UCR4);
    temp &= ~UCR4_TCEN;
    writel(temp, port->membase + UCR4);
}
...
