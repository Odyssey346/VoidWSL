//
//    Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

#include "stdafx.h"

bool DistributionInfo::CreateUser(std::wstring_view userName)
{
    // Create the user account.
    DWORD exitCode;
    std::wstring commandLine = L"/usr/sbin/useradd -m ";
    commandLine += userName;
    HRESULT hr = g_wslApi.WslLaunchInteractive(commandLine.c_str(), true, &exitCode);
    if ((FAILED(hr)) || (exitCode != 0)) {
        return false;
    }
        
    // Ask the user to enter a password for the new user account.
    commandLine = L"/usr/bin/passwd ";
    commandLine += userName;
    hr = g_wslApi.WslLaunchInteractive(commandLine.c_str(), true, &exitCode);
    if ((FAILED(hr)) || (exitCode != 0)) {
		// Delete the user if the password set command failed.
		commandLine = L"/usr/sbin/userdel ";
		commandLine += userName;
		g_wslApi.WslLaunchInteractive(commandLine.c_str(), true, &exitCode);
		return false;
	}

    // Add the user account to any relevant groups.
    commandLine = L"/usr/sbin/usermod -aG adm,cdrom,plugdev,wheel ";
    commandLine += userName;
    hr = g_wslApi.WslLaunchInteractive(commandLine.c_str(), true, &exitCode);
    if ((FAILED(hr)) || (exitCode != 0)) {

        // Delete the user if the group add command failed.
        commandLine = L"/usr/sbin/userdel ";
        commandLine += userName;
        g_wslApi.WslLaunchInteractive(commandLine.c_str(), true, &exitCode);
        return false;
    }

    // Uncomment wheel group in /etc/sudoers
    commandLine = L"sed -i 's/^# %wheel ALL=(ALL:ALL) ALL/%wheel ALL=(ALL:ALL) ALL/' /etc/sudoers";
    hr = g_wslApi.WslLaunchInteractive(commandLine.c_str(), true, &exitCode);
    if ((FAILED(hr)) || (exitCode != 0)) {
		// Delete the user if the group add command failed.
		commandLine = L"/usr/bin/echo Could not allow wheel group to use sudo. ";
		g_wslApi.WslLaunchInteractive(commandLine.c_str(), true, &exitCode);
		return false;
	}

    return true;
}

ULONG DistributionInfo::QueryUid(std::wstring_view userName)
{
    // Create a pipe to read the output of the launched process.
    HANDLE readPipe;
    HANDLE writePipe;
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, true};
    ULONG uid = UID_INVALID;
    if (CreatePipe(&readPipe, &writePipe, &sa, 0)) {
        // Query the UID of the supplied username.
        std::wstring command = L"/usr/bin/id -u ";
        command += userName;
        int returnValue = 0;
        HANDLE child;
        HRESULT hr = g_wslApi.WslLaunch(command.c_str(), true, GetStdHandle(STD_INPUT_HANDLE), writePipe, GetStdHandle(STD_ERROR_HANDLE), &child);
        if (SUCCEEDED(hr)) {
            // Wait for the child to exit and ensure process exited successfully.
            WaitForSingleObject(child, INFINITE);
            DWORD exitCode;
            if ((GetExitCodeProcess(child, &exitCode) == false) || (exitCode != 0)) {
                hr = E_INVALIDARG;
            }

            CloseHandle(child);
            if (SUCCEEDED(hr)) {
                char buffer[64];
                DWORD bytesRead;

                // Read the output of the command from the pipe and convert to a UID.
                if (ReadFile(readPipe, buffer, (sizeof(buffer) - 1), &bytesRead, nullptr)) {
                    buffer[bytesRead] = ANSI_NULL;
                    try {
                        uid = std::stoul(buffer, nullptr, 10);

                    } catch( ... ) { }
                }
            }
        }

        CloseHandle(readPipe);
        CloseHandle(writePipe);
    }

    return uid;
}

bool DistributionInfo::SetupFiles(std::wstring_view userName) 
{
    DWORD exitCode;
    // create /etc/motd
    std::wstring commandLine = L"/usr/bin/echo 'Welcome to Void Linux running on WSL!\nThe default password of the root user is voidlinux.\nYou also have access to sudo as the account you created.\nTo remove this message, edit /etc/motd.\nEnjoy!' > /etc/motd";
    g_wslApi.WslLaunchInteractive(commandLine.c_str(), true, &exitCode);
    // add /etc/motd to bashrc
    commandLine = L"/usr/bin/echo cat /etc/motd >> /home/";
    commandLine += userName;
    commandLine += L"/.bashrc";
    g_wslApi.WslLaunchInteractive(commandLine.c_str(), true, &exitCode);

    return true;
}
