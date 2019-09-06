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
#ifndef __TFTP_H
#define __TFTP_H

#define TFTP_PORT 69
#define MAX_UDP_PACKETSIZE 65535

typedef enum {
	TFTP_RRQ  =1, //!< Read request (RRQ)
	TFTP_WRQ  =2, //!< Write request (WRQ)
	TFTP_DATA =3, //!< Data (DATA)
	TFTP_ACK  =4, //!< Acknowledgment (ACK)
	TFTP_ERROR=5, //!< Error (ERROR)
	TFTP_OACK =6  //!< RFC 2347 
} TFTP_OPCODE;


typedef enum {
	TFTPERR_UNDEF=0,			//!< Not defined, see error message (if any).
	TFTPERR_FNOTFOUND=1,		//!< File not found.
	TFTPERR_ACCESSVIOLATION=2,	//!< Access violation.
	TFTPERR_DISKFULL=3,			//!< Disk full or allocation exceeded.
	TFTPERR_ILLEGALOP=4,		//!< Illegal TFTP operation.
	TFTPERR_UNKNOWNID=5,		//!< Unknown transfer ID.
	TFTPERR_FILEEXISTS=6,		//!< File already exists.
	TFTPERR_NOUSER=7			//!< No such user.
} TFTPERR;

typedef struct {
	TFTP_OPCODE eOpcode;			// read or write
	sockaddr_in addr;
	char pszFileName[MAX_PATH];
	char pszMode[MAX_PATH];
	HANDLE hFile;
	BYTE *pCache;					// if non-null, complete file cached in memory
	bool fComplete;					// if true, download is complete, only used for printprogress
	WORD wBlockNumHigh;
	int nNextBlockNum;
	int nNumBlocks;
	int nBlockSize;	// default 512, otherwise extension
	int nTimeout;	// extension
	int nTSize;		// extension
	DWORD dwLastSignOfLife;				//!< tick count of last traffic for this session
} CLIENT;

#define MAX_OPTIONS 16
#define MAX_TFTP_CLIENTS 100


void PrintHexDump(const BYTE *pAddr,DWORD dwLen);


CLIENT *FindClient(TFTP_OPCODE eOpcode, const sockaddr_in *pAddr);
void ReleaseClient(CLIENT *pClient);
void SendWRQ(int nSocket, const CLIENT *pClient);
void SendAck(int nSocket, const CLIENT *pClient, int nBlockNum);
void SendErr(int nSocket, const CLIENT *pClient, int nErrorCode, const char *pszErrorMsg);
void SendData(int nSocket, const CLIENT *pClient, int nBlockNum, const BYTE *pData, int nDataLen);
void SendOACK(int nSocket, const CLIENT *pClient, char **pOptNames, char **pOptValues, int nOptions);
BOOL IsValidFileName(const char *pszFName);
void SendNextDataBlock(int nSocket, int nBlockNum, CLIENT *pClient);
void PrintProgress(const CLIENT *pClient);


#endif // __TFTP_H
