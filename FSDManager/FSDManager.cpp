// FSDManager.cpp : Defines the entry point for the console application.
//
#include "CFSDPortConnector.h"
#include "FSDCommonInclude.h"
#include "FSDCommonDefs.h"
#include "stdio.h"
#include "AutoPtr.h"
#include "FSDThreadUtils.h"
#include "Shlwapi.h"
#include <math.h>
#include <fstream>
#include <vector>
#include "LZJD.h"
#include "MurmurHash3.h"
#include "CFSDDynamicByteBuffer.h"
#include <unordered_map>

using namespace std;

HRESULT HrMain();

#define MAX_COMMAND_LENGTH 10
#define MAX_PARAMETER_LENGTH 256

#define FSD_INPUT_THREADS_COUNT 8

uint64_t digest_size = 1024;


// 0.0 is an absolute order, 8.0 is an absolute chaos.
double shannonEntropy(const char* filename)
{
	long			alphabet[256];
	unsigned char   buffer[1024];
	double			entropy = 0.0, temp;
	int				i, n, size = 0;
	errno_t			err;
	FILE*			pFile;


	memset(alphabet, 0, sizeof(long) * 256);

	err = fopen_s(&pFile, filename, "rb");
	if (err != 0)
	{
		printf("Couldn't open file. Error: %d \n", err);
		return -1.0;
	}

	while ((n = (int)fread(buffer, 1, 1024, pFile)) != 0)
	{
		for (i = 0; i < n; i++)
		{
			alphabet[(int)buffer[i]]++;
			size++;
		}
	}
	fclose(pFile);

	for (i = 0; i < 256; i++)
	{
		if (alphabet[i] != 0)
		{
			temp = (double)alphabet[i] / (double)size;
			entropy += (-1) * temp * log2(temp);
		}
	}
	
	return entropy;
}

static void readAllBytes(char const* filename, vector<char>& result)
{
	ifstream ifs(filename, ios::binary | ios::ate);
	ifstream::pos_type pos = ifs.tellg();

	result.clear();//empty out
	result.resize(pos); //make sure we have enough space
	if (result.size() == 0) return; // empty file!
	ifs.seekg(0, ios::beg);
	ifs.read(&result[0], pos);
}

void compareDigests(pair<vector<const char*>, vector<vector<int32_t>>>& A, pair<vector<const char*>, vector<vector<int32_t>>>& B)
{
	vector<const char*> A_s = A.first;
	vector<const char*> B_s = B.first;
	vector<vector<int32_t>> A_i = A.second;
	vector<vector<int32_t>> B_i = B.second;

	for (size_t i = 0; i < A_i.size(); i++)
	{
		const char* hAiN = A_s[i];
		vector<int32_t> hAiH = A_i[i];

		size_t j_start;
		if (&A == &B)
			j_start = i + 1; //don't self compare / repeat comparisons
		else
			j_start = 0;


		for (size_t j = j_start; j < B_i.size(); j++)
		{
			int32_t sim = similarity(hAiH, B_i[j]);
			if (sim >= 0)//threshold)
			{
				cout << hAiN << "|" << B_s[j] << "|";
				//make the similarity output formated as 00X, 0XX, or 100, depending on value
				if (sim < 100)
				{
					cout << "0";
					if (sim < 10)
						cout << "0";
				}
				cout << sim << "\n";
			}
		}
	}
}


int ljzd(const char* filename1, const char* filename2)
{
	vector<char> all_bytes;
	vector<const char* > digestNewName;
	vector<vector<int32_t>> digesstNewInts;

	readAllBytes(filename1, all_bytes);

	vector<int32_t> di = digest(digest_size, all_bytes);

	digesstNewInts.push_back(di);
	digestNewName.push_back(filename1);

	readAllBytes(filename2, all_bytes);

	di = digest(digest_size, all_bytes);

	digesstNewInts.push_back(di);
	digestNewName.push_back(filename2);

	pair<vector<const char*>, vector<vector<int32_t>>> digestNew = make_pair(digestNewName, digesstNewInts);
	compareDigests(digestNew, digestNew);

	return 0;
}

int main(int argc, char **argv)
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    HRESULT hr = HrMain();
    if (FAILED(hr))
    {
        printf("Main failed with status 0x%x\n", hr);
        return 1;
    }

    return 0;
}

HRESULT OnChangeDirectoryCmd(CFSDPortConnector* pConnector)
{
    HRESULT hr = S_OK;

    CAutoStringW wszParameter = new WCHAR[MAX_PARAMETER_LENGTH];
    RETURN_IF_FAILED_ALLOC(wszParameter);

    wscanf_s(L"%ls[/]", wszParameter.LetPtr(), MAX_FILE_NAME_LENGTH);

    if (!PathFileExistsW(wszParameter.LetPtr()))
    {
        printf("Directory: %ls is not valid\n", wszParameter.LetPtr());
        return S_OK;
    }

    CAutoStringW wszVolumePath = new WCHAR[50];
    hr = GetVolumePathNameW(wszParameter.LetPtr(), wszVolumePath.LetPtr(), 50);
    RETURN_IF_FAILED(hr);

    size_t cVolumePath = wcslen(wszVolumePath.LetPtr());

    FSD_MESSAGE_FORMAT aMessage;
    aMessage.aType = MESSAGE_TYPE_SET_SCAN_DIRECTORY;
    wcscpy_s(aMessage.wszFileName, MAX_FILE_NAME_LENGTH, wszParameter.LetPtr() + cVolumePath);

    printf("Changing directory to: %ls\n", wszParameter.LetPtr());

    hr = pConnector->SendMessage((LPVOID)&aMessage, sizeof(aMessage), NULL, NULL);
    RETURN_IF_FAILED(hr);

    return S_OK;
}

HRESULT OnSendMessageCmd(CFSDPortConnector* pConnector)
{
    HRESULT hr = S_OK;

    CAutoStringW wszParameter = new WCHAR[MAX_PARAMETER_LENGTH];
    RETURN_IF_FAILED_ALLOC(wszParameter);

    wscanf_s(L"%ls", wszParameter.LetPtr(), MAX_FILE_NAME_LENGTH);

    FSD_MESSAGE_FORMAT aMessage;
    aMessage.aType = MESSAGE_TYPE_PRINT_STRING;
    wcscpy_s(aMessage.wszFileName, MAX_FILE_NAME_LENGTH, wszParameter.LetPtr());

    printf("Sending message: %ls\n", wszParameter.LetPtr());

    BYTE pReply[MAX_STRING_LENGTH];
    DWORD dwReplySize = sizeof(pReply);
    hr = pConnector->SendMessage((LPVOID)&aMessage, sizeof(aMessage), pReply, &dwReplySize);
    RETURN_IF_FAILED(hr);

    if (dwReplySize > 0)
    {
        printf("Recieved response: %ls\n", (WCHAR*)pReply);
    }

    return S_OK;
}

struct THREAD_CONTEXT
{
    bool               fExit;
    CFSDPortConnector* pConnector;
};

class CFileInformation
{
public:
    CFileInformation(LPCWSTR wszFileName)
        : wszFileName(wszFileName)
    {}

public:
    wstring wszFileName;
};

class CProcess
{
public:
    CProcess(ULONG uPid)
        : uPid(uPid)
    {}

    void ProcessIrp(FSD_OPERATION_DESCRIPTION* pOperation);

public:
    ULONG uPid;
    unordered_map<wstring, CFileInformation> aFiles;
};

void CProcess::ProcessIrp(FSD_OPERATION_DESCRIPTION* pOperation)
{
    aFiles.insert({ (LPCWSTR)pOperation->pData, CFileInformation((LPCWSTR)pOperation->pData) });
}

unordered_map<ULONG, CProcess> aProcesses;

void ProcessIrp(FSD_OPERATION_DESCRIPTION* pOperation)
{
    //printf("PID: %u MJ: %u MI: %u %ls\n", pOpDescription->uPid, pOpDescription->uMajorType, pOpDescription->uMinorType, (LPCWSTR)pOpDescription->pData);
    auto res = aProcesses.insert({ pOperation->uPid, CProcess(pOperation->uPid) });
    res.first->second.ProcessIrp(pOperation);

}

HRESULT FSDIrpSniffer(PVOID pvContext)
{
    HRESULT hr = S_OK;

    THREAD_CONTEXT* pContext = static_cast<THREAD_CONTEXT*>(pvContext);
    RETURN_IF_FAILED_ALLOC(pContext);

    CFSDPortConnector* pConnector = pContext->pConnector;
    ASSERT(pConnector != NULL);

    CFSDDynamicByteBuffer pBuffer;
    hr = pBuffer.Initialize(1024);
    RETURN_IF_FAILED(hr);

    size_t cTotalIrpsRecieved = 0;
    while (!pContext->fExit)
    {
        FSD_MESSAGE_FORMAT aMessage;
        aMessage.aType = MESSAGE_TYPE_QUERY_NEW_OPS;

        BYTE* pResponse = pBuffer.Get();
        DWORD dwReplySize = numeric_cast<DWORD>(pBuffer.ReservedSize());
        hr = pConnector->SendMessage((LPVOID)&aMessage, sizeof(aMessage), pBuffer.Get(), &dwReplySize);
        RETURN_IF_FAILED(hr);

        if (dwReplySize == 0)
        {
            continue;
        }

        FSD_OPERATION_DESCRIPTION* pOpDescription = ((FSD_QUERY_NEW_OPS_RESPONSE_FORMAT*)(PVOID)pResponse)->GetFirst();
        size_t cbData = 0;
        size_t cCurrentIrpsRecieved = 0;
        for (;;)
        {
            if (cbData >= dwReplySize)
            {
                ASSERT(cbData == dwReplySize);
                break;
            }

            try
            {
                ProcessIrp(pOpDescription);
            }
            CATCH_ALL_AND_RETURN_FAILED_HR

            cbData += pOpDescription->PureSize();
            cCurrentIrpsRecieved++;
            pOpDescription = pOpDescription->GetNext();
        }

        cTotalIrpsRecieved += cCurrentIrpsRecieved;

        printf("Total IRPs: %Iu Current recieve: %Iu Recieves size: %Iu Buffer size: %Iu\n", cTotalIrpsRecieved, cCurrentIrpsRecieved, cbData, pBuffer.ReservedSize());

        if (cbData >= pBuffer.ReservedSize() / 2)
        {
            pBuffer.Grow();
        }

        if (cbData < pBuffer.ReservedSize() / 20)
        {
            Sleep(1000);
        }
    }

    return S_OK;
}

/*
HRESULT FSDInputParser(PVOID pvContext)
{
    HRESULT hr = S_OK;

    THREAD_CONTEXT* pContext = static_cast<THREAD_CONTEXT*>(pvContext);
    RETURN_IF_FAILED_ALLOC(pContext);
    
    CFSDPortConnector* pConnector = pContext->pConnector;
    ASSERT(pConnector != NULL);

    while (!pContext->fExit)
    {
        DWORD dwMessageSize;
        ULONG64 uCompletionKey;
        LPOVERLAPPED pOverlapped;

        bool res = GetQueuedCompletionStatus(pContext->hCompletionPort, &dwMessageSize, &uCompletionKey, &pOverlapped, INFINITE);
        if (!res)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            RETURN_IF_FAILED(hr);
        }

        CFSDPortConnectorMessage* pConnectorMessage = CFSDPortConnectorMessage::CastFrom(pOverlapped);
        
        FSD_MESSAGE_FORMAT* pMessage = (FSD_MESSAGE_FORMAT*)pConnectorMessage->pBuffer;
        if (pMessage->aType != MESSAGE_TYPE_SNIFFER_NEW_IRP)
        {
            printf("pMessage->aType == %d\n", pMessage->aType);
        }

        memset(&pConnectorMessage->aOverlapped, 0, sizeof(pConnectorMessage->aOverlapped));

        hr = pConnector->RecieveMessage(pConnectorMessage);
        HR_IGNORE_ERROR(STATUS_IO_PENDING);
        if (FAILED(hr))
        {
            printf("Recieve message failed with status 0x%x\n", hr);
            continue;
        }        
        
        /*
        switch (pMessage->aType)
        {
            case MESSAGE_TYPE_SNIFFER_NEW_IRP:
            {
                //printf("[Sniffer] %ls\n", pMessage->wszFileName);
                break;
            }
            default:
            {
                printf("Unknown message type recieved %d", pMessage->aType);
                ASSERT(false);
            }
        }
    }

    return S_OK;
}
*/

HRESULT UserInputParser(PVOID pvContext)
{
    HRESULT hr = S_OK;

    THREAD_CONTEXT* pContext = static_cast<THREAD_CONTEXT*>(pvContext);
    RETURN_IF_FAILED_ALLOC(pContext);

    CFSDPortConnector* pConnector = pContext->pConnector;
    ASSERT(pConnector != NULL);

    CAutoStringW wszCommand = new WCHAR[MAX_COMMAND_LENGTH];
    RETURN_IF_FAILED_ALLOC(wszCommand);

    while (!pContext->fExit)
    {
        printf("Input a command: ");
        wscanf_s(L"%ls", wszCommand.LetPtr(), MAX_COMMAND_LENGTH);
        if (wcscmp(wszCommand.LetPtr(), L"chdir") == 0)
        {
            hr = OnChangeDirectoryCmd(pConnector);
            RETURN_IF_FAILED(hr);
        } 
        else
        if (wcscmp(wszCommand.LetPtr(), L"message") == 0)
        {
            hr = OnSendMessageCmd(pConnector);
            RETURN_IF_FAILED(hr);
        }
        else
        if (wcscmp(wszCommand.LetPtr(), L"exit") == 0)
        {
            pContext->fExit = true;
            printf("Exiting FSDManager\n");
        }
        else
        {
            printf("Invalid command: %ls\n", wszCommand.LetPtr());
        }
    }

    return S_OK;
}

HRESULT HrMain()
{
    HRESULT hr = S_OK;

    CAutoPtr<CFSDPortConnector> pConnector;
    hr = NewInstanceOf<CFSDPortConnector>(&pConnector, g_wszFSDPortName);
    if (hr == E_FILE_NOT_FOUND)
    {
        printf("Failed to connect to FSDefender Kernel module. Try to load it.\n");
    }
    RETURN_IF_FAILED(hr);

    THREAD_CONTEXT aContext = {};
    aContext.fExit           = false;
    aContext.pConnector      = pConnector.LetPtr();

    CAutoHandle hFSDIrpSnifferThread;
    hr = UtilCreateThreadSimple(&hFSDIrpSnifferThread, (LPTHREAD_START_ROUTINE)FSDIrpSniffer, (PVOID)&aContext);
    RETURN_IF_FAILED(hr);
    
    CAutoHandle hUserInputParserThread;
    hr = UtilCreateThreadSimple(&hUserInputParserThread, (LPTHREAD_START_ROUTINE)UserInputParser, (PVOID)&aContext);
    RETURN_IF_FAILED(hr);

    hr = WaitForSingleObject(hFSDIrpSnifferThread.LetPtr(), INFINITE);
    RETURN_IF_FAILED(hr);

    hr = WaitForSingleObject(hUserInputParserThread.LetPtr(), INFINITE);
    RETURN_IF_FAILED(hr);

    return S_OK;
}