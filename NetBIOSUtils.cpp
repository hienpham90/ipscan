// NetBIOSUtils.cpp: implementation of the CNetBIOSUtils class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "NetBIOSUtils.h"
#include <nb30.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#define CONST_OWN_NETBIOS_NAME "ANGRYIPSCAN"

tNetBiosFunc pNetBiosFunc;

CNetBIOSUtils::CNetBIOSUtils()
{	
	HMODULE hDll = LoadLibrary("netapi32.dll");	

	pNetBiosFunc = (tNetBiosFunc) GetProcAddress(hDll, "Netbios");
	
	if (!hDll || !pNetBiosFunc)
	{
		MessageBox(0, "NETAPI32.DLL cannot be loaded, so NetBIOS scanning is not functional.", "Fatal Error", MB_OK | MB_ICONHAND);
		exit(1);
	}

	GetLanaNumber();
	Reset(m_nLana, 20, 30);
	AddName(m_nLana, CONST_OWN_NETBIOS_NAME);
}

CNetBIOSUtils::~CNetBIOSUtils()
{
	DeleteName(m_nLana, CONST_OWN_NETBIOS_NAME);
}

void CNetBIOSUtils::setIP(LPCSTR szIP)
{
	m_szIP = szIP;
}

void CNetBIOSUtils::setIP(DWORD nIP)
{
	in_addr in;
	in.S_un.S_addr = nIP;
	LPSTR ipa = inet_ntoa(in);	
	m_szIP = ipa;
}

void CNetBIOSUtils::MakeName(char *achDest, LPCSTR szSrc)
{
	int cchSrc;

    cchSrc = lstrlen (szSrc);
    if (cchSrc > NCBNAMSZ)
        cchSrc = NCBNAMSZ;

    memset (achDest, ' ', NCBNAMSZ);
    memcpy (achDest, szSrc, cchSrc);
}

BOOL CNetBIOSUtils::Reset(int nLana, int nSessions, int nNames)
{
	NCB ncb;

    memset (&ncb, 0, sizeof (ncb));
    ncb.ncb_command = NCBRESET;
    ncb.ncb_lsn = 0;                // Allocate new lana_num resources 
    ncb.ncb_lana_num = nLana;
    ncb.ncb_callname[0] = nSessions;  // maximum sessions 
    ncb.ncb_callname[2] = nNames;   // maximum names 

    pNetBiosFunc(&ncb);    

    return (NRC_GOODRET == ncb.ncb_retcode);
	
}

BOOL CNetBIOSUtils::AddName(int nLana, LPCSTR szName)
{

    NCB ncb;

    memset (&ncb, 0, sizeof (ncb));
    ncb.ncb_command = NCBADDNAME;
    ncb.ncb_lana_num = nLana;

    MakeName ((char*)ncb.ncb_name, szName);

    pNetBiosFunc (&ncb);    

    return (NRC_GOODRET == ncb.ncb_retcode);
}


BOOL CNetBIOSUtils::AdapterStatus(int nLana, PVOID pBuffer, int cbBuffer, LPCSTR szName)
{
	NCB ncb;

    memset (&ncb, 0, sizeof (ncb));
    ncb.ncb_command = NCBASTAT;
    ncb.ncb_lana_num = nLana;

    ncb.ncb_buffer = (PUCHAR) pBuffer;
    ncb.ncb_length = cbBuffer;

    MakeName((char*)&ncb.ncb_callname, szName);

    pNetBiosFunc(&ncb);    

    return (NRC_GOODRET == ncb.ncb_retcode);
}

BOOL CNetBIOSUtils::DeleteName(int nLana, LPCSTR szName)
{
    NCB ncb;

    memset (&ncb, 0, sizeof (ncb));
    ncb.ncb_command = NCBDELNAME;
    ncb.ncb_lana_num = nLana;

    MakeName ((char*)ncb.ncb_name, szName);

    pNetBiosFunc(&ncb);

    return (NRC_GOODRET == ncb.ncb_retcode);
}

void CNetBIOSUtils::GetLanaNumber()
{
	LANA_ENUM lan_num;
	NCB ncb;

	memset(&ncb, 0, sizeof(ncb));
	ncb.ncb_command =  NCBENUM;
	ncb.ncb_buffer = (unsigned char *) &lan_num; 
	ncb.ncb_length = sizeof(lan_num);

	pNetBiosFunc((NCB*) &ncb);

	if (lan_num.length > 1) TODO: // TODO: call options here to let the user choose the network adapter
		MessageBox(0, "Warning! You have several network adapters. NetBIOS info scanning can be very long when trying to enumerate all of them, so first of them is used", "", MB_OK | MB_ICONINFORMATION);

	m_nLana = lan_num.lana[0];
}

BOOL CNetBIOSUtils::GetNames(CString *szUserName, CString *szComputerName, CString *szGroupName, CString *szMacAddress)
{	
    int cbBuffer;
    ADAPTER_STATUS *pStatus;
    NAME_BUFFER *pNames;
    int i;
    HANDLE hHeap;

    hHeap = GetProcessHeap();

    // Allocate the largest buffer that might be needed. 
    cbBuffer = sizeof (ADAPTER_STATUS) + 255 * sizeof (NAME_BUFFER);
    pStatus = (ADAPTER_STATUS *) HeapAlloc (hHeap, 0, cbBuffer);
    if (NULL == pStatus)
        return FALSE;

    if (!AdapterStatus (m_nLana, (PVOID) pStatus, cbBuffer, m_szIP))
    {
        HeapFree (hHeap, 0, pStatus);
        return FALSE;
    }

    // The list of names follows the adapter status structure.
    pNames = (NAME_BUFFER *) (pStatus + 1);
	char szName[16];

	// get computer name
	if (szComputerName != NULL)
	{
		*szComputerName = "";
		memcpy(&szName, pNames[0].name, 15); szName[15] = 0;
		*szComputerName = szName;
		szComputerName->TrimRight(' ');
	}

	// get group name
	if (szGroupName != NULL)
	{
		*szGroupName = "";
		for (i = 0; i < pStatus->name_count; i++)
		{
			if ((pNames[i].name_flags & GROUP_NAME) == GROUP_NAME)
			{
				memcpy(&szName, pNames[i].name, 15); szName[15] = 0;
				*szGroupName = szName;
				break;
			}		
		}
		szGroupName->TrimRight(' ');
	}

	// get user name
	if (szUserName != NULL)
	{
		*szUserName = "";
		for (i = pStatus->name_count-1; i >= 0; i--)
		{
			if (pNames[i].name[15] == 3)
			{
				memcpy(&szName, pNames[i].name, 15); szName[15] = 0;
				*szUserName = szName;
				break;
			}
		}
		int nDollarIndex = szUserName->Find('$'); 
		if (nDollarIndex >= 0)
			szUserName->Delete(nDollarIndex);

		szUserName->TrimRight(' ');
	}

	// get mac address
	if (szMacAddress != NULL)
	{
   		szMacAddress->Format(_T("%02X-%02X-%02X-%02X-%02X-%02X"),
			pStatus->adapter_address[0],
			pStatus->adapter_address[1],
			pStatus->adapter_address[2],
			pStatus->adapter_address[3],
			pStatus->adapter_address[4],
			pStatus->adapter_address[5]);
	}

    HeapFree (hHeap, 0, pStatus);

    return TRUE;

}

