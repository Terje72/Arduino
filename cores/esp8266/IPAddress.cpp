/*
 IPAddress.cpp - Base class that provides IPAddress
 Copyright (c) 2011 Adrian McEwen.  All right reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <Arduino.h>
#include <IPAddress.h>
#include <Print.h>

IPAddress::IPAddress() {
    ipv4Only();
    ipv4() = 0;
}

IPAddress::IPAddress(uint8_t first_octet, uint8_t second_octet, uint8_t third_octet, uint8_t fourth_octet) {
    ipv4Only();
    (*this)[0] = first_octet;
    (*this)[1] = second_octet;
    (*this)[2] = third_octet;
    (*this)[3] = fourth_octet;
}

IPAddress::IPAddress(uint32_t address) {
    ipv4Only();
    ipv4() = address;
}

IPAddress::IPAddress(const uint8_t *address) {
    ipv4Only();
    ipv4() = *reinterpret_cast<const uint32_t*>(address);
}

bool IPAddress::fromString(const char *address) {
    ipv4Only();
    // TODO: add support for "a", "a.b", "a.b.c" formats

    uint16_t acc = 0; // Accumulator
    uint8_t dots = 0;

    while (*address)
    {
        char c = *address++;
        if (c >= '0' && c <= '9')
        {
            acc = acc * 10 + (c - '0');
            if (acc > 255) {
                // Value out of [0..255] range
                return false;
            }
        }
        else if (c == '.')
        {
            if (dots == 3) {
                // Too much dots (there must be 3 dots)
                return false;
            }
            (*this)[dots++] = acc;
            acc = 0;
        }
        else
        {
            // Invalid char
            return false;
        }
    }

    if (dots != 3) {
        // Too few dots (there must be 3 dots)
        return false;
    }
    (*this)[3] = acc;
    return true;
}

IPAddress& IPAddress::operator=(const uint8_t *address) {
    ipv4Only();
    ipv4() = *reinterpret_cast<const uint32_t*>(address);
    return *this;
}

IPAddress& IPAddress::operator=(uint32_t address) {
    ipv4Only();
    ipv4() = address;
    return *this;
}

bool IPAddress::operator==(const uint8_t* addr) const {
    return IP_IS_V4_VAL(_ip) && ipv4() == *reinterpret_cast<const uint32_t*>(addr);
}

size_t IPAddress::printTo(Print& p) const {
    size_t n = 0;
    for(int i = 0; i < 3; i++) {
        n += p.print((*this)[i], DEC);
        n += p.print('.');
    }
    n += p.print((*this)[3], DEC);
    return n;
}

String IPAddress::toString() const
{
    char szRet[16];
    sprintf(szRet,"%u.%u.%u.%u", (*this)[0], (*this)[1], (*this)[2], (*this)[3]);
    return String(szRet);
}

bool IPAddress::isValid(const String& arg) {
	return IPAddress().fromString(arg);
}

bool IPAddress::isValid(const char* arg) {
	return IPAddress().fromString(arg);
}

const IPAddress INADDR_NONE(0, 0, 0, 0);

/* lwip */

IPAddress::IPAddress(ipv4_addr fw_addr) {
    ipv4Only();
    ipv4() = fw_addr.addr;
}

IPAddress::IPAddress(const ipv4_addr* fw_addr) {
    ipv4Only();
    ipv4() = fw_addr->addr;
}
