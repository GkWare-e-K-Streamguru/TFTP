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

#include "../TFTP.h"

static BOOL fBinaryTransfer = FALSE;
static int nPortNum = TFTP_PORT;
static const char *pszHost;
static const char *pszPort;
static const char *pszHostFile;
static const char *pszTFTPFile;
static BOOL fGotOACK = FALSE;

static CLIENT Client;
static CLIENT *pClient = &Client;


BOOL ParseParameters(int argc, char *argv[])
{
	int i;
	for(i=1;i<argc;i++)
	{
		//printf("param %d:%s\r\n",i,argv[i]);

		if((argv[i][0] == '-') || (argv[i][0] == '/'))
		{
			if(stricmp(&argv[i][1],"i")==0) {
				//printf("Binary transfer selected\r\n");
				fBinaryTransfer = TRUE;
			} else 
			/*if(stricmp(&argv[i][1],"v")==0) {
				printf("Verbose mode selected\r\n");				
			} else */
			if((stricmp(&argv[i][1],"b")==0) && (argc>(i+1)) && isdigit(argv[i+1][0]))
			{				
				pClient->nBlockSize = atol(argv[i+1]);
				//printf("Blocksize %d\r\n", pClient->nBlockSize);
				i++;
				continue;
			} else 
			if((stricmp(&argv[i][1],"p")==0) && (argc>(i+1)) && isdigit(argv[i+1][0]))
			{
				nPortNum = atol(argv[i+1]);
				pszPort = argv[i];
				//printf("Port %d\r\n", nPortNum);
				i++;
				continue;
			} else 
			if((stricmp(&argv[i][1],"t")==0) && (argc>(i+1)) && isdigit(argv[i+1][0]))
			{
				pClient->nTimeout = atol(argv[i+1]);				
				//printf("Timeout %d\r\n", pClient->nTimeout);
				i++;
				continue;
			} else 
			{
				printf("Unrecognized option %s\r\n",argv[i]);
				return FALSE;
			}
		} else
			break;
	}

	if(argc<=i)
		return FALSE;
	pszHost = argv[i++];

	if(stricmp(argv[i],"get")==0)
		pClient->eOpcode = TFTP_RRQ;
	else if(stricmp(argv[i],"put")==0)
		pClient->eOpcode = TFTP_WRQ;
	else
		return FALSE;
	i++;

	if(argc<=i)
		return FALSE;
	pszHostFile = argv[i++];
	if(argc<=i)
		pszTFTPFile = pszHostFile;
	else
		pszTFTPFile = argv[i++];

	//printf("%s %s\r\n",pszHostFile,pszTFTPFile);


	if(pClient->nBlockSize<8 || pClient->nBlockSize>65464) {
		printf("Invalid block size\r\n");
		return FALSE;
	}

	if(nPortNum<1 || nPortNum>65535) {
		printf("Invalid port number\r\n");
		return FALSE;
	}


	return TRUE;
}

BOOL SendRRQWRQ(int nSocket, TFTP_OPCODE eOpcode, CLIENT *pClient)
{
	BYTE bData[MAX_UDP_PACKETSIZE];
	int ret;
	int nOffset = 2;
	bData[0] = (BYTE)(eOpcode>>8);
	bData[1] = (BYTE)(eOpcode);
	strcpy((char*)&bData[nOffset],pszTFTPFile);
	nOffset += (strlen(pszTFTPFile)+1);

	char *pszModeString;
	if(fBinaryTransfer)
		pszModeString = "octet";
	else		
		pszModeString = "netascii";
	strcpy((char*)&bData[nOffset],pszModeString);
	nOffset += (strlen(pszModeString)+1);

	strcpy(pClient->pszFileName,pszHostFile);
	pClient->eOpcode = eOpcode;
	pClient->hFile = INVALID_HANDLE_VALUE;		
	pClient->nNextBlockNum = 1;

	if(eOpcode == TFTP_WRQ)
	{
		pClient->hFile = CreateFile(pClient->pszFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		DWORD dwFSHigh;
		pClient->nTSize = GetFileSize(pClient->hFile, &dwFSHigh);
		if(dwFSHigh || (pClient->nTSize<=0))
		{
			printf("File size zero or too large (>2GB)\r\n");			
			return FALSE;
		}
	} else {
		pClient->hFile = CreateFile(pClient->pszFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
		pClient->nTSize = 0;
	}
	if(pClient->hFile == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	pClient->nNumBlocks = pClient->nTSize/pClient->nBlockSize;



	char *pszOptName[MAX_OPTIONS];
	char pszOptVal[MAX_OPTIONS][100];
	int nOptCount = 0;
	if(pClient->nBlockSize != 512) {
		pszOptName[nOptCount] = "blksize";
		sprintf(pszOptVal[nOptCount],"%d",pClient->nBlockSize);
		nOptCount++;
	}

	pszOptName[nOptCount] = "tsize";
	sprintf(pszOptVal[nOptCount],"%d",pClient->nTSize);
	nOptCount++;
	
	if(pClient->nTimeout != 1) {
		pszOptName[nOptCount] = "timeout";
		sprintf(pszOptVal[nOptCount],"%d",pClient->nTimeout);
		nOptCount++;
	}

	int i;
	for(i=0;i<nOptCount;i++)
	{
		int nNameLen = strlen(pszOptName[i]);
		int nValLen = strlen(pszOptVal[i]);
		if((nOffset+nNameLen+nValLen+2)>sizeof(bData))
			return FALSE;
		strcpy((char*)&bData[nOffset],pszOptName[i]);
		nOffset+=(nNameLen+1);
		
		strcpy((char*)&bData[nOffset],pszOptVal[i]);
		nOffset+=(nValLen+1);
	}
	
	ret = send(nSocket, (char*)bData, nOffset, 0);
	//ret = sendto(nSocket, (char*)bData, nOffset, 0, (sockaddr*)&pClient->addr, sizeof(pClient->addr));
	return TRUE;
}

static BOOL ParseOACK(int nSocket, const BYTE *pBuf, int nLen)
{
	char *pszOptName[MAX_OPTIONS];
	char *pszOptVal[MAX_OPTIONS];
	int nOptCount = 0;
	//
	// parse TFTP options
	//
	int i=2;
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
			pClient->nBlockSize = atoi(pszOptVal[nOptCount]);
			if((pClient->nBlockSize<8) || (pClient->nBlockSize>65464)) {
				SendErr(nSocket, pClient,TFTPERR_UNDEF,"Invalid blocksize");
				return FALSE;
			}
			
		} else if(_stricmp(pszOptName[nOptCount],"timeout") == 0) {
			// RFC2349
			//printf("Found timeout option\r\n");
			pClient->nTimeout = atoi(pszOptVal[nOptCount]);
			if((pClient->nTimeout<1) || (pClient->nTimeout>255)) {
				SendErr(nSocket, pClient,TFTPERR_UNDEF,"Invalid timeout");
				return FALSE;
			}
			
			
		} else if(_stricmp(pszOptName[nOptCount],"tsize") == 0) {
			// RFC2349
			//printf("Found tsize option\r\n");
			pClient->nTSize = atoi(pszOptVal[nOptCount]);			
			if(pClient->eOpcode == TFTP_RRQ) 
			{
				if(atoi(pszOptVal[nOptCount])==0) {
					SendErr(nSocket, pClient,TFTPERR_UNDEF,"Invalid transfer size");					
					return FALSE;
				} else {
					//sprintf(pszTSize,"%d",pClient->nTSize);
					//pszOutOptValues[nOutOptCount] = pszTSize;
				}
			} else {
				if(pClient->nTSize<=0) {
					SendErr(nSocket, pClient,TFTPERR_UNDEF,"Invalid transfer size");
					return FALSE;
				}
			}
		} else {
			printf("\r\nOption %s=%s not recognized\r\n",pszOptName[nOptCount],pszOptVal[nOptCount]);
			
		}
		
		//printf("%s = %s\r\n",pszOptName[nOptCount],pszOptVal[nOptCount]);
		nOptCount++;
	}
	return TRUE;
}

void PrintStatistics(DWORD dwStartTicks)
{
	DWORD dwTicks = GetTickCount()-dwStartTicks;
	float bps = (float)pClient->nTSize*1000/(float)(dwTicks);								
	printf("\r\n%d bytes transferred in %.3f seconds (%d bytes per second)\r\n",pClient->nTSize, (float)dwTicks/1000,(int)bps);
}


int main(int argc, char *argv[])
{
	WSADATA wsa;
	memset(&wsa,0x00,sizeof(wsa));
	
	printf("TFTP Client V1.10 - Copyright 2013-2018 GkWare e.K. - http://www.gkware.com\r\n");
	printf("This application is freeware. If you paid for it, ask for a refund.\r\n");
	
	if(WSAStartup(0x0202,&wsa)!=0)
	{
		printf("Unable to initialize windows sockets\r\n");
		return -1;
	}
	
	int nSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if(nSocket == INVALID_SOCKET) {
		printf("Unable to create socket\r\n");
		return -1;
	}

	memset(&Client,0x00,sizeof(Client));
	Client.nBlockSize = 512;
	Client.nTimeout = 1;


	if(!ParseParameters(argc,argv))
	{
		printf("Usage: TFTPClient [-i] [-b blocksize] [-t timeout] [-p portnumber] host [GET | PUT] local_file [destination_file]\r\n");
		return -1;
	}

	struct hostent *he;
	he = gethostbyname(pszHost);
	if(!he || he->h_addr==NULL) {
		printf("Unable to resolve %s\r\n",pszHost);
		return -1;
	}
	if(he->h_length != 4) {
		printf("Invalid adress length\r\n");
		return -1;
	}

	struct sockaddr_in addr;
	int ret;
	memset(&addr,0x00,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(nPortNum);
	memcpy(&addr.sin_addr,he->h_addr,sizeof(sockaddr_in));
	ret = connect(nSocket,(sockaddr*)&addr, sizeof(sockaddr_in));

	memcpy(&Client.addr,&addr,sizeof(addr));
	
	BOOL fStop = FALSE;	

	if(!SendRRQWRQ(nSocket, pClient->eOpcode,&Client))
		fStop = TRUE;
	
	int nTotalretries = 0;
	DWORD dwStartTicks = GetTickCount();
	while(!fStop)
	{
		
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(nSocket, &fds);
		int nRetryCount = 0;
		struct timeval timeout;
		timeout.tv_sec  = pClient->nTimeout;
		timeout.tv_usec = 0;
		
		ret = select(0, &fds, 0, 0, &timeout);
		if(ret == 0)
		{
			printf("\r\nTimeout\r\n");
			if(nRetryCount == 3)
				break;
			else {
				nRetryCount++;
				nTotalretries++;
				SendNextDataBlock(nSocket, pClient->nNextBlockNum,pClient);
			}
			
		} else {
			BYTE bBuf[MAX_UDP_PACKETSIZE];
			int addrlen = sizeof(addr);			   
			ret = recvfrom(nSocket, (char*)bBuf, sizeof(bBuf), 0, (sockaddr*)&addr, &addrlen);
			if(ret > 0)
			{
				TFTP_OPCODE eOpcode = (TFTP_OPCODE)((bBuf[0]<<8)|bBuf[1]);
				WORD wBlockNum;
				switch(eOpcode)
				{
				case TFTP_RRQ:
				case TFTP_WRQ:
					SendErr(nSocket, pClient,TFTPERR_ILLEGALOP,"Unexpected RRQ/WRQ opcode");
					fStop = TRUE;
					break;
				case TFTP_DATA:
					if(pClient->eOpcode == TFTP_RRQ) {
						if(!fGotOACK) {
							pClient->nBlockSize = 512;
						}
						
						wBlockNum = (WORD)((bBuf[2]<<8)|bBuf[3]);
						if(pClient->nNextBlockNum != ((pClient->wBlockNumHigh<<16)|wBlockNum))
							break;
						pClient->nNextBlockNum++;
						if(wBlockNum == 0xFFFF)
							pClient->wBlockNumHigh++;
						
						DWORD dwWritten = 0;
						//printf("Data %d len %d\r\n",wBlockNum, ret-4);
						WriteFile(pClient->hFile, &bBuf[4],ret-4,&dwWritten, NULL);
						PrintProgress(pClient);
						if((int)dwWritten == (ret-4))
						{
							SendAck(nSocket, pClient,wBlockNum);
							if((ret-4)<pClient->nBlockSize)
							{
								printf("[complete]\r\n");
								PrintStatistics(dwStartTicks);
								
								fStop = TRUE;
							}
						}
						else {
							SendErr(nSocket, pClient,TFTPERR_DISKFULL,"Write failed");
							//	ReleaseClient(pClient);
							fStop = TRUE;
						}
					} else {
						SendErr(nSocket, pClient,TFTPERR_ILLEGALOP,"Illegal operation");
						fStop = TRUE;
					}
					break;
				case TFTP_OACK:
					fGotOACK = TRUE;
					if(!ParseOACK(nSocket, bBuf,ret))
						fStop = TRUE;
					pClient->nNumBlocks = pClient->nTSize/pClient->nBlockSize;
					// intentionally drop through
				case TFTP_ACK:
					if(!fGotOACK) {
						pClient->nBlockSize = 512;
					}
					if(eOpcode == TFTP_OACK)
						wBlockNum = 0;
					else
						wBlockNum = (WORD)((bBuf[2]<<8)|bBuf[3]);
					//printf("Ack %d\r\n",wBlockNum);
					pClient->nNextBlockNum = (pClient->wBlockNumHigh<<16)|wBlockNum;
					PrintProgress(pClient);
					if(((pClient->wBlockNumHigh<<16)|wBlockNum)<=pClient->nNumBlocks) {
						if(wBlockNum == 0xFFFF) {
							pClient->nNextBlockNum = ((pClient->wBlockNumHigh+1)<<16)|(0);
							pClient->wBlockNumHigh++;
						} else {
							pClient->nNextBlockNum = ((pClient->wBlockNumHigh)<<16)|(wBlockNum+1);
						}
						SendNextDataBlock(nSocket, pClient->nNextBlockNum,pClient);
					} else {
						printf("[complete]\r\n");
						fStop = TRUE;
						PrintStatistics(dwStartTicks);
					}
					break;
				case TFTP_ERROR:
					bBuf[ret] = '\0';
					printf("Received error message \"%s\"\r\n",&bBuf[4]);
					fStop = TRUE;
					break;
				}
			}
		}
	}
	if(nTotalretries) {
		printf("%d retries\r\n", nTotalretries);
	}


	if(Client.hFile != INVALID_HANDLE_VALUE)
		CloseHandle(Client.hFile);

	closesocket(nSocket);
	WSACleanup();
	return 0;
}