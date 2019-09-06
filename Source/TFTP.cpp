// ********************************************************
//
//   Author/Copyright 	Gero Kuehn / GkWare e.K.
//						Hatzper Strasse 172B
//						D-45149 Essen, Germany
//						Tel: +49 174 520 8026
//						Email: support@gkware.com
//						Web: http://www.gkware.com
//
//
// ********************************************************
#include <windows.h>
#include <stdio.h>
#include <process.h>

#include "TFTP.h"



void PrintHexDump(const BYTE *pAddr,DWORD dwLen)
{
	DWORD i,j;
	for(i=0;i<dwLen;i+=16)
	{
		printf("%08X:",(int)(pAddr+i));
		for(j=0;j<16;j++) {
			if(i+j<dwLen)
				printf("%02X ",pAddr[i+j]);
			else
				printf("   ");
		}
		for(j=0;j<16;j++) 
		{
			if(i+j<dwLen) {
				BYTE c;
				c = pAddr[i+j];
				if(c>32)
					printf("%c",c);
				else
					printf(".");
			} else 
				printf(" ");
		}
		printf("\r\n");
	}
}




void SendAck(int nSocket, const CLIENT *pClient, int nBlockNum)
{
	BYTE bData[4];
	int ret;
	bData[0] = (BYTE)(TFTP_ACK>>8);
	bData[1] = (BYTE)(TFTP_ACK);
	bData[2] = (BYTE)(nBlockNum>>8);
	bData[3] = (BYTE)(nBlockNum);
	ret = sendto(nSocket, (char*)bData, 4, 0, (sockaddr*)&pClient->addr, sizeof(pClient->addr));
}

void SendErr(int nSocket, const CLIENT *pClient, int nErrorCode, const char *pszErrorMsg)
{
	BYTE bData[MAX_PATH];
	int ret;
	if((strlen(pszErrorMsg)+5) > sizeof(bData))
		return;
	bData[0] = (BYTE)(TFTP_ERROR>>8);
	bData[1] = (BYTE)(TFTP_ERROR);
	bData[2] = (BYTE)(nErrorCode>>8);
	bData[3] = (BYTE)(nErrorCode);
	strcpy((char*)&bData[4], pszErrorMsg);
	ret = sendto(nSocket, (char*)bData, (strlen(pszErrorMsg)+5), 0, (sockaddr*)&pClient->addr, sizeof(pClient->addr));
}

void SendData(int nSocket, const CLIENT *pClient, int nBlockNum, const BYTE *pData, int nDataLen)
{
	BYTE bData[MAX_UDP_PACKETSIZE];
	int ret;
	if((nDataLen+4) > sizeof(bData))
		return;
	bData[0] = (BYTE)(TFTP_DATA>>8);
	bData[1] = (BYTE)(TFTP_DATA);
	bData[2] = (BYTE)(nBlockNum>>8);
	bData[3] = (BYTE)(nBlockNum);
	memcpy((char*)&bData[4], pData, nDataLen);
	ret = sendto(nSocket, (char*)bData, (nDataLen+4), 0, (sockaddr*)&pClient->addr, sizeof(pClient->addr));
}

void SendOACK(int nSocket, const CLIENT *pClient, char **pOptNames, char **pOptValues, int nOptions)
{
	BYTE bData[MAX_UDP_PACKETSIZE];
	int ret;
	int nOffset = 2;
	int i;

	bData[0] = (BYTE)(TFTP_OACK>>8);
	bData[1] = (BYTE)(TFTP_OACK);
	for(i=0;i<nOptions;i++)
	{
		int nNameLen = strlen(pOptNames[i]);
		int nValLen = strlen(pOptValues[i]);
		if((nOffset+nNameLen+nValLen+2)>sizeof(bData))
			return;
		strcpy((char*)&bData[nOffset],pOptNames[i]);
		nOffset+=(nNameLen+1);

		strcpy((char*)&bData[nOffset],pOptValues[i]);
		nOffset+=(nValLen+1);
	}
	ret = sendto(nSocket, (char*)bData, nOffset, 0, (sockaddr*)&pClient->addr, sizeof(pClient->addr));

}

BOOL IsValidFileName(const char *pszFName)
{
	// "./" at the beginning is ok
	if(pszFName[0]=='.' && pszFName[1]=='/')
		pszFName+=2;

	// prevent escape from the root directory
	if(strstr(pszFName,".."))
		return FALSE;

	// prevent absolute paths
	if((pszFName[0] == '/') || (pszFName[0] == '\\'))
		return FALSE;
	
	// prevent paths including a drive letter
	if(strchr(pszFName,':'))
		return FALSE;
	return TRUE;
}

void SendNextDataBlock(int nSocket, int nBlockNum, CLIENT *pClient)
{
	DWORD dwRead = 0;
	BYTE bData[MAX_UDP_PACKETSIZE];
	if(nBlockNum<1)
		return;
	if(pClient->pCache) {
		dwRead = min(pClient->nBlockSize, pClient->nTSize-(nBlockNum-1)*pClient->nBlockSize);
		if(dwRead)
			memcpy(bData, &pClient->pCache[(nBlockNum-1)*pClient->nBlockSize], dwRead);
	} else {
		SetFilePointer(pClient->hFile, (nBlockNum-1)*pClient->nBlockSize, NULL, FILE_BEGIN);
		ReadFile(pClient->hFile,bData,pClient->nBlockSize, &dwRead, NULL);
	}
	SendData(nSocket, pClient, nBlockNum, bData, dwRead);
}

void PrintProgress(const CLIENT *pClient)
{
	
	int nPercent;
	const sockaddr_in *pAddr = &pClient->addr;
	char pszIPString[100];
	char pszLine[200];
	if(pClient->nNumBlocks)
		nPercent = pClient->nNextBlockNum*100/pClient->nNumBlocks;
	else 
		nPercent = 0;
	// for small files
	if(nPercent>100)
		nPercent = 100;
	sprintf(pszIPString,"%d.%d.%d.%d:%-5d ",pAddr->sin_addr.S_un.S_un_b.s_b1,pAddr->sin_addr.S_un.S_un_b.s_b2,pAddr->sin_addr.S_un.S_un_b.s_b3,pAddr->sin_addr.S_un.S_un_b.s_b4,ntohs(pAddr->sin_port));
	
	if(pClient->eOpcode == TFTP_WRQ)
		strcpy(pszLine,"\rUp ");
	else
		strcpy(pszLine,"\rDn ");
	
	
	int i;

	if(pClient->nTSize && nPercent) 
	{
		if(nPercent<100)
			sprintf(pszLine+strlen(pszLine),"%s  %02d%% |",pszIPString, nPercent);
		else
			sprintf(pszLine+strlen(pszLine),"%s %02d%% |",pszIPString, nPercent);

		for(i=0;i<100;i+=3)
		{
			if(i>nPercent)
				strcat(pszLine," ");
			else
				strcat(pszLine,"=");
		}
	} else {
		char pszText[100];
		sprintf(pszLine+strlen(pszLine),"%19s      |",pszIPString);

		if(pClient->fComplete)
			sprintf(pszText," %d bytes",pClient->nTSize);
		else
			sprintf(pszText," %d bytes",pClient->nBlockSize*(pClient->nNextBlockNum-1));
		sprintf(pszLine+strlen(pszLine),"%-33s",pszText);
	}
	sprintf(pszLine+strlen(pszLine),"| %s  ",pClient->pszFileName);
	printf("%s",pszLine);
}
