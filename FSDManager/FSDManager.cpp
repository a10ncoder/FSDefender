// FSDManager.cpp : Defines the entry point for the console application.
//
#include "CFSDPortConnector.h"
#include "FSDCommon.h"
#include "stdio.h"
#include "AutoPtr.h"

HRESULT HrMain();

int main()
{
	HRESULT hr = HrMain();
	if (FAILED(hr))
	{
		printf("Main failed with status 0x%x\n", hr);
		return 1;
	}
	
    return 0;
}

HRESULT HrMain()
{
	HRESULT hr = S_OK;

	CFSDPortConnector aConnector;
	hr = aConnector.Initialize(g_wszFSDPortName);
	RETURN_IF_FAILED(hr);

	
	{
		LPCWSTR wszMessage = L"Test message";

		FSD_MESSAGE_FORMAT aMessage;
		aMessage.aType = MESSAGE_TYPE_PRINT_STRING;
		wcscpy_s(aMessage.wszFileName, MAX_FILE_NAME_LENGTH, wszMessage);

		printf("Sending message: %ls\n", wszMessage);

		BYTE pReply[MAX_STRING_LENGTH];
		DWORD dwReplySize = sizeof(pReply);
		hr = aConnector.SendMessage((LPVOID)&aMessage, sizeof(aMessage), pReply, &dwReplySize);
		RETURN_IF_FAILED(hr);

		if (dwReplySize > 0)
		{
			printf("Recieved response: %ls", (WCHAR*)pReply);
		}

	}

	{
		LPCWSTR wszDirectory = L"\\Device\\HarddiskVolume4\\Users\\User\\Documents\\";

		FSD_MESSAGE_FORMAT aMessage;
		aMessage.aType = MESSAGE_TYPE_SET_SCAN_DIRECTORY;
		wcscpy_s(aMessage.wszFileName, MAX_FILE_NAME_LENGTH, wszDirectory);

		printf("Sending message: %ls\n", wszDirectory);

		BYTE pReply[MAX_STRING_LENGTH];
		DWORD dwReplySize = sizeof(pReply);
		hr = aConnector.SendMessage((LPVOID)&aMessage, sizeof(aMessage), pReply, &dwReplySize);
		RETURN_IF_FAILED(hr);

		if (dwReplySize > 0)
		{
			printf("Recieved response: %ls", (WCHAR*)pReply);
		}
	}

	/*CFSDPortConnectorMessage aMessage = {};
	hr = aConnector.RecieveMessage(&aMessage);
	RETURN_IF_FAILED(hr);

	if (aMessage.aRecieveHeader.ReplyLength)
	{
		printf("New message recieved: %s\n", aMessage.pBuffer);
	}
	*/
	return S_OK;
}

