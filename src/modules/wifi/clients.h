#ifndef __CLIENTS_H__
#define __CLIENTS_H__

#include <WiFi.h>

void telnet_setup();

void ssh_setup(String host = "");
void ssh_connect_profile(String host, uint16_t port, String user);

void ssh_loop(void *pvParameters);

char *stringTochar(const String &s);

#endif
