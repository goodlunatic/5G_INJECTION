#!/usr/bin/env python3

import click
import ipaddress
import subprocess
from pyroute2 import IPRoute
from pyroute2.netlink import NetlinkError


def handle_ip_string(ctx, param, value):
    try:
        ret = ipaddress.ip_network(value)
        return ret
    except ValueError:
        raise click.BadParameter(f'{value} is not a valid IP range.')


@click.command()
@click.option("--if_name", default="ogstun", help="TUN interface name.")
@click.option("--ip_range", default='10.45.0.0/24', callback=handle_ip_string,
              help="IP range of the TUN interface.")
def main(if_name, ip_range):
    # Get the gateway IP (first host address in range) and prefix length
    gateway_ip = str(next(ip_range.hosts()))
    ip_netmask = ip_range.prefixlen

    ipr = IPRoute()

    # Create the TUN interface
    try:
        ipr.link('add', ifname=if_name, kind='tuntap', mode='tun')
    except NetlinkError as e:
        if e.code != 17:  # 17 = EEXIST, ignore if already exists
            raise

    # Look up the interface index
    dev = ipr.link_lookup(ifname=if_name)[0]
    # Bring it down before configuring
    ipr.link('set', index=dev, state='down')
    # Add the gateway IP address
    try:
        ipr.addr('add', index=dev, address=gateway_ip, mask=ip_netmask)
    except NetlinkError as e:
        if e.code != 17:  # EEXIST
            raise
    # Bring it up
    ipr.link('set', index=dev, state='up')

    # Add route for the UE subnet
    try:
        ipr.route('add', dst=ip_range.with_prefixlen, gateway=gateway_ip)
    except NetlinkError:
        pass

    # Allow INPUT on the TUN interface
    subprocess.run([
        'iptables', '-A', 'INPUT', '-i', if_name, '-j', 'ACCEPT'
    ], check=True)

    # Allow FORWARD through the TUN interface in both directions
    subprocess.run([
        'iptables', '-A', 'FORWARD', '-i', if_name, '-j', 'ACCEPT'
    ], check=True)
    subprocess.run([
        'iptables', '-A', 'FORWARD', '-o', if_name, '-j', 'ACCEPT'
    ], check=True)

    # Masquerade UE traffic going out to the internet (NOT back through ogstun)
    subprocess.run([
        'iptables', '-t', 'nat', '-A', 'POSTROUTING',
        '-s', ip_range.with_prefixlen,
        '!', '-o', if_name,
        '-j', 'MASQUERADE'
    ], check=True)


if __name__ == "__main__":
    main()
