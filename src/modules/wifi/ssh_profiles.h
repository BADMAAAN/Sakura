#pragma once
#include <Arduino.h>
#include <vector>

struct SshProfileLink { uint32_t id; String name; };
std::vector<SshProfileLink> sshProfileLinks();
bool openSshProfile(uint32_t id); // Details only; Connect is an explicit menu action.
void sshProfilesMenu();
bool sshValidPort(const char *text, uint16_t &port);
bool sshValidEndpoint(const char *host, const char *user);
