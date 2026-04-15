#pragma once
// WireGuardServiceHost.h
// When our executable is launched by SCM as a Windows service (via
// WireGuardManager::installService), it passes --wg-service <confPath>.
// This module handles that path: loads tunnel.dll, calls WireGuardTunnelService().

#include <QString>

// Returns true if the process was started as a service and handled it.
// Call this BEFORE constructing QApplication in main().
bool handleServiceMode(int argc, char* argv[]);
