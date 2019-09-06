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
#include <assert.h>

#include "../TFTP.h"


static CLIENT g_Clients[MAX_TFTP_CLIENTS];
#define TFTP_SESSION_TIMEOUT 15000			// 15 seconds

static CLIENT *FindClient(TFTP_OPCODE eOpcode, const sockaddr_in *pAddr)
{
	int i;
	sockaddr_in nulladdr;
	memset(&nulladdr,0x00,sizeof(nulladdr));
	
	for(i=0;i<MAX_TFTP_CLIENTS;i++)
	{
		// is this the session we are looking for?
		if(memcmp(&g_Clients[i].addr, pAddr, sizeof(sockaddr_in))==0) {
			g_Clients[i].dwLastSignOfLife = GetTickCount();
			return &g_Clients[i];
		} 

		// if not, has this one timed out?
		DWORD dwNow = GetTickCount();
		DWORD dwExpiration = g_Clients[i].dwLastSignOfLife+TFTP_SESSION_TIMEOUT;
		
		if(g_Clients[i].eOpcode && g_Clients[i].dwLastSignOfLife<dwNow && g_Clients[i].dwLastSignOfLife<dwExpiration)
		{
			//printf("Session %d timed out\r\n",i);
			ReleaseClient(&g_Clients[i]);
		}
	}

	if((eOpcode == TFTP_RRQ) || (eOpcode == TFTP_WRQ)) {
		// allocate a new one		
		for(i=0;i<MAX_TFTP_CLIENTS;i++)
		{
			if(memcmp(&g_Clients[i].addr, &nulladdr, sizeof(sockaddr_in))==0) {
				memcpy(&g_Clients[i].addr, pAddr, sizeof(sockaddr_in));
				//printf("New client %d.%d.%d.%d:%d\r\n",pAddr->sin_addr.S_un.S_un_b.s_b1,pAddr->sin_addr.S_un.S_un_b.s_b2,pAddr->sin_addr.S_un.S_un_b.s_b3,pAddr->sin_addr.S_un.S_un_b.s_b4,ntohs(pAddr->sin_port));
				g_Clients[i].dwLastSignOfLife = GetTickCount();
				return &g_Clients[i];
			}
		}
	}
	return NULL;
}

static void ReleaseClient(CLIENT *pClient)
{
	if(!pClient)
		return;
	if(pClient->hFile != INVALID_HANDLE_VALUE)
		CloseHandle(pClient->hFile);
	if(pClient->pCache)
		free(pClient->pCache);
	memset(pClient,0x00,sizeof(pClient));
}

void StartTransfer(int nSocket, CLIENT *pClient, TFTP_OPCODE eOpcode, const BYTE *pBuf, int nLen)
{
	int i;	
	int nFileNameLen = 0;
	int nModeLen = 0;
	char *pszOptName[MAX_OPTIONS];
	char *pszOptVal[MAX_OPTIONS];
	int nOptCount = 0;
	char *pszOutOptNames[MAX_OPTIONS];
	char *pszOutOptValues[MAX_OPTIONS];	
	char pszTSize[20];
	int nOutOptCount = 0;

	// clean up
	memset(pszOptName,0x00,sizeof(pszOptName));
	memset(pszOptVal,0x00,sizeof(pszOptVal));

	// decode fixed header
	for(i=2;i<nLen;i++)
	{
		if(nFileNameLen<sizeof(pClient->pszFileName))
			pClient->pszFileName[nFileNameLen++] = pBuf[i];
		if(pBuf[i] == 0)
			break;
	}
	i++;
	for(;i<nLen;i++)
	{
		if(nModeLen<sizeof(pClient->pszMode))
			pClient->pszMode[nModeLen++] = pBuf[i];
		if(pBuf[i] == 0)
			break;
	}
	i++;

	//printf("File: %s\r\nMode: %s\r\n",pClient->pszFileName,pClient->pszMode);
	pClient->eOpcode = eOpcode;
	pClient->hFile = INVALID_HANDLE_VALUE;	
	pClient->nBlockSize = 512;
	pClient->wBlockNumHigh = 0;
	pClient->nNextBlockNum = 1;

	if(!IsValidFileName(pClient->pszFileName)) {
		SendErr(nSocket, pClient,TFTPERR_ACCESSVIOLATION,"Invalid filename / security violation");
		ReleaseClient(pClient);
		return;
	} 

	if(eOpcode == TFTP_WRQ)
	{
		pClient->hFile = CreateFile(pClient->pszFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
	} else {
		pClient->hFile = CreateFile(pClient->pszFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if(pClient->hFile != INVALID_HANDLE_VALUE)
		{
			DWORD dwFSHigh;
			pClient->nTSize = GetFileSize(pClient->hFile, &dwFSHigh);
			if(dwFSHigh || (pClient->nTSize<=0))
			{
				SendErr(nSocket, pClient, TFTPERR_UNDEF, "File size zero or too large (>2GB)");
				ReleaseClient(pClient);
				return;
			}
#define FILE_CACHE_LIMIT 32*1024*1024
			if(pClient->nTSize < FILE_CACHE_LIMIT) {
				pClient->pCache = (BYTE*)malloc(pClient->nTSize);
				assert(pClient->pCache);
				if(!pClient->pCache) {
					SendErr(nSocket, pClient, TFTPERR_UNDEF, "Out of memory for file cache");
					ReleaseClient(pClient);
					return;
				}
				DWORD dwRead = 0;
				ReadFile(pClient->hFile, pClient->pCache, pClient->nTSize, &dwRead, NULL);
				if(dwRead != (DWORD)pClient->nTSize) {
					SendErr(nSocket, pClient, TFTPERR_UNDEF, "File read error");
					ReleaseClient(pClient);
					return;
				}
				CloseHandle(pClient->hFile);
			}
		}
	}
	if(pClient->hFile == INVALID_HANDLE_VALUE)
	{
		SendErr(nSocket, pClient, TFTPERR_FNOTFOUND, "Unable to open file");
		ReleaseClient(pClient);
		return;
	}
	

	//
	// parse TFTP options
	//

	for(;i<nLen;i++)
	{
		pszOptName[nOptCount] = (char*)&pBuf[i];
		for(;i<nLen;i++)
		{
			if(pBuf[i] == 0)
				break;
		}
		i++;
		if(i<nLen)
			pszOptVal[nOptCount] = (char*)&pBuf[i];
		for(;i<nLen;i++)
		{
			if(pBuf[i] == 0)
				break;
		}

		if(!pszOptName[nOptCount] || !pszOptVal[nOptCount])
			break;

		if(_stricmp(pszOptName[nOptCount],"blksize") == 0) {
			// RFC2348
			//printf("Found blksize option\r\n");
			pszOutOptNames[nOutOptCount] = pszOptName[nOptCount];
			pszOutOptValues[nOutOptCount] = pszOptVal[nOptCount];
			nOutOptCount++;
			pClient->nBlockSize = atoi(pszOptVal[nOptCount]);
			if((pClient->nBlockSize<8) || (pClient->nBlockSize>65464)) {
				SendErr(nSocket, pClient,TFTPERR_UNDEF,"Invalid blocksize");
				ReleaseClient(pClient);
				return;
			}

		} else if(_stricmp(pszOptName[nOptCount],"timeout") == 0) {
			// RFC2349
			//printf("Found timeout option\r\n");
			pszOutOptNames[nOutOptCount] = pszOptName[nOptCount];
			pszOutOptValues[nOutOptCount] = pszOptVal[nOptCount];
			nOutOptCount++;
			pClient->nTimeout = atoi(pszOptVal[nOptCount]);
			if((pClient->nTimeout<1) || (pClient->nTimeout>255)) {
				SendErr(nSocket, pClient,TFTPERR_UNDEF,"Invalid timeout");
				ReleaseClient(pClient);
				return;
			}


		} else if(_stricmp(pszOptName[nOptCount],"tsize") == 0) {
			// RFC2349
			//printf("Found tsize option\r\n");
			pszOutOptNames[nOutOptCount] = pszOptName[nOptCount];
			pszOutOptValues[nOutOptCount] = pszOptVal[nOptCount];
			if(eOpcode == TFTP_RRQ) 
			{
				if(atoi(pszOptVal[nOptCount])!=0) {
					SendErr(nSocket, pClient,TFTPERR_UNDEF,"Invalid transfer size");
					ReleaseClient(pClient);
					return;
				} else {
					sprintf(pszTSize,"%d",pClient->nTSize);
					pszOutOptValues[nOutOptCount] = pszTSize;
				}
			} else {
				pClient->nTSize = atoi(pszOptVal[nOptCount]);
				if(pClient->nTSize<=0) {
					SendErr(nSocket, pClient,TFTPERR_UNDEF,"Invalid transfer size");
					ReleaseClient(pClient);
					return;
				}
			}
			nOutOptCount++;

		} else {
			printf("\r\nOption %s=%s not recognized\r\n",pszOptName[nOptCount],pszOptVal[nOptCount]);

		}
		
		//printf("%s = %s\r\n",pszOptName[nOptCount],pszOptVal[nOptCount]);
		nOptCount++;
	}
	
	pClient->nNumBlocks = pClient->nTSize/pClient->nBlockSize;
	if(pClient->nTSize % pClient->nBlockSize)
		pClient->nNumBlocks++;
	
	if(nOutOptCount)
	{
		SendOACK(nSocket, pClient, pszOutOptNames, pszOutOptValues, nOutOptCount);
	}
	if(eOpcode == TFTP_WRQ) {
		SendAck(nSocket, pClient,0);
	}
	else {
		SendNextDataBlock(nSocket, 1,pClient);
	}
	
}


int main(int argc, char *argv[])
{
	WSADATA wsa;
	CLIENT *pClient;
	memset(&wsa,0x00,sizeof(wsa));

	printf("TFTP Server V1.12 - Copyright 2013-2018 GkWare e.K. - http://www.gkware.com\r\n");
	printf("This application is freeware. If you paid for it, ask for a refund.\r\n");
	printf("\r\n");
	printf("Waiting for TFTP traffic on port %d, press CTRL-C to stop the server\r\n",TFTP_PORT);

	if(WSAStartup(0x0202,&wsa)!=0)
	{
		printf("Unable to initialize windows sockets\r\n");
		return -1;
	}

	memset(g_Clients,0x00,sizeof(g_Clients));	
	int nSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if(nSocket == INVALID_SOCKET) {
		printf("Unable to create socket\r\n");
		return -1;
	}
	sockaddr_in addr;
	memset(&addr,0x00,sizeof(addr));
	addr.sin_port = htons(TFTP_PORT);
	addr.sin_family = AF_INET;
	
	int ret;
	ret = bind(nSocket, (sockaddr*)&addr, sizeof(addr));
	if(ret == SOCKET_ERROR) {
		printf("Unable to bind to port %d\r\n",TFTP_PORT);
		return -1;
	}


	for(;;)
	{
		BYTE bBuf[MAX_UDP_PACKETSIZE];
		int addrlen = sizeof(addr);
		ret = recvfrom(nSocket, (char*)bBuf, sizeof(bBuf), 0, (sockaddr*)&addr, &addrlen);
		if(ret > 0)
		{
			TFTP_OPCODE eOpcode = (TFTP_OPCODE)((bBuf[0]<<8)|bBuf[1]);
			pClient = FindClient(eOpcode, &addr);
			if(!pClient) {
				printf("No free session\r\n");
				continue;
			}
			switch(eOpcode)
			{
			case TFTP_RRQ:
			case TFTP_WRQ:
				StartTransfer(nSocket, pClient, eOpcode, bBuf, ret);
				break;
			case TFTP_DATA:
				WORD wBlockNum;
				wBlockNum = (WORD)((bBuf[2]<<8)|bBuf[3]);
				if(pClient->nNextBlockNum != ((pClient->wBlockNumHigh<<16)|wBlockNum))
					break;
				pClient->nNextBlockNum++;
				if(wBlockNum == 0xFFFF)
					pClient->wBlockNumHigh++;

				if(pClient->eOpcode == TFTP_WRQ)
				{
					DWORD dwWritten = 0;
					//printf("Data %d len %d\r\n",wBlockNum, ret-4);
					WriteFile(pClient->hFile, &bBuf[4],ret-4,&dwWritten, NULL);
					PrintProgress(pClient);
					if((int)dwWritten == (ret-4))
					{
						SendAck(nSocket, pClient,wBlockNum);
						if((ret-4)<pClient->nBlockSize)
						{
							pClient->nTSize = GetFileSize(pClient->hFile,NULL);
							PrintProgress(pClient);
							printf("[complete]\r\n");
							CloseHandle(pClient->hFile);
						}
					}
					else {
						SendErr(nSocket, pClient,TFTPERR_DISKFULL,"Write failed");
						ReleaseClient(pClient);
					}
				} else
					SendErr(nSocket, pClient,TFTPERR_ILLEGALOP,"Illegal operation");
				break;
			case TFTP_ACK:
				wBlockNum = (WORD)((bBuf[2]<<8)|bBuf[3]);
				//printf("Ack %d / %d\r\n",wBlockNum,pClient->nNumBlocks);
				pClient->nNextBlockNum = ((pClient->wBlockNumHigh<<16)|wBlockNum);
				if((pClient->nNextBlockNum<pClient->nNumBlocks) && (pClient->nNumBlocks!=1)) {
					if(wBlockNum == 0xFFFF)
						SendNextDataBlock(nSocket, ((pClient->wBlockNumHigh+1)<<16)|(0),pClient);
					else
						SendNextDataBlock(nSocket, ((pClient->wBlockNumHigh)<<16)|(wBlockNum+1),pClient);
					PrintProgress(pClient);
				}
				else {
					if(wBlockNum != 0) {
					pClient->fComplete = true;
					PrintProgress(pClient);
					printf("[complete]\r\n");
					
					ReleaseClient(pClient);
					}
				}
				if(wBlockNum == 0xFFFF)
					pClient->wBlockNumHigh++;
				break;
			case TFTP_ERROR:
				bBuf[ret] = '\0';
				printf("Received error message \"%s\"\r\n",&bBuf[4]);
				ReleaseClient(pClient);
				break;
			case TFTP_OACK:
				SendErr(nSocket, pClient,TFTPERR_ILLEGALOP,"Unexpected OACK opcode");
				ReleaseClient(pClient);
				break;
			}
			
		}
		else {
			printf("recvfrom error %d\r\n",WSAGetLastError());
		}
	}

	closesocket(nSocket);
	WSACleanup();
	return 0;
}

