#pragma once
#ifndef STORE
#include <cwchar>
#include <Shlwapi.h>
#include <WinBase.h>
#include <winreg.h>

#include "app.hpp"
#else
#include <winrt/Windows.ApplicationModel.h>
#include "UWP.hpp"
#endif

#include "ttberror.hpp"

namespace Autostart {

#ifndef STORE
	enum class StartupState {
		Disabled,
		DisabledByUser,
		Enabled
	};
#else
	typedef winrt::Windows::ApplicationModel::StartupTaskState StartupState;
#endif

	StartupState GetStartupState()
	{
#ifndef STORE
		LRESULT error = RegGetValue(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", App::NAME.c_str(), RRF_RT_REG_SZ, NULL, NULL, NULL);
		if (error == ERROR_FILE_NOT_FOUND)
		{
			return StartupState::Disabled;
		}
		else if (error == ERROR_SUCCESS)
		{
			uint8_t status[12];
			DWORD size = sizeof(status);
			error = RegGetValue(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run", App::NAME.c_str(), RRF_RT_REG_BINARY, NULL, &status, &size);
			if (error != ERROR_FILE_NOT_FOUND && ErrorHandle(HRESULT_FROM_WIN32(error), Error::Level::Log, L"Querying startup disable state failed.") && status[0] == 3)
			{
				return StartupState::DisabledByUser;
			}
			else
			{
				return StartupState::Enabled;
			}
		}
		else
		{
			ErrorHandle(HRESULT_FROM_WIN32(error), Error::Level::Log, L"Querying startup state failed.");
			return StartupState::Disabled;
		}
#else
		try
		{
			return UWP::GetApplicationStartupTask().State();
		}
		catch (const winrt::hresult_error &error)
		{
			ErrorHandle(error.code(), Error::Level::Log, L"Getting startup task state failed.");
			return StartupState::Disabled;
		}
#endif
	}

	void SetStartupState(const StartupState &state)
	{
#ifndef STORE
		HKEY hkey;
		LRESULT error = RegCreateKey(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", &hkey);
		if (ErrorHandle(HRESULT_FROM_WIN32(error), Error::Level::Error, L"Opening registry key failed!")) //Creates a key
		{
			if (state == StartupState::Enabled)
			{
				DWORD exeLocation_size = LONG_PATH;
				std::vector<wchar_t> exeLocation(exeLocation_size);
				if (!QueryFullProcessImageName(GetCurrentProcess(), 0, exeLocation.data(), &exeLocation_size))
				{
					ErrorHandle(HRESULT_FROM_WIN32(GetLastError()), Error::Level::Error, L"Failed to determine executable location!");
					return;
				}

				std::wstring exeLocation_str(exeLocation.data());

				error = RegSetValueEx(hkey, App::NAME.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE *>(exeLocation_str.c_str()), exeLocation_str.length());
				ErrorHandle(HRESULT_FROM_WIN32(error), Error::Level::Error, L"Error while setting startup registry value!");
			}
			else if (state == StartupState::Disabled)
			{
				error = RegDeleteValue(hkey, App::NAME.c_str());
				ErrorHandle(HRESULT_FROM_WIN32(error), Error::Level::Error, L"Error while deleting startup registry value!");
			}
			error = RegCloseKey(hkey);
			ErrorHandle(HRESULT_FROM_WIN32(error), Error::Level::Log, L"Error closing registry key.");
		}
#else
		try
		{
			auto task = UWP::GetApplicationStartupTask();
			if (state == StartupState::Enabled)
			{
				task.RequestEnableAsync().get();
			}
			else if (state == StartupState::Disabled)
			{
				task.Disable();
			}
		}
		catch (const winrt::hresult_error &error)
		{
			ErrorHandle(error.code(), Error::Level::Error, L"Changing startup task state failed!");
			return;
		}
#endif
	}

}