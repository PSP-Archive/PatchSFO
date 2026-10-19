#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <stdio.h>
#include <psppower.h>
#include <stdlib.h>
#include <pspsdk.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <malloc.h>
#include <sys/stat.h>

PSP_MODULE_INFO("PatchSFO", 0, 1, 1);

#define DEBUG_SCREEN 1
#define DEBUG_FILE   1

int OptionXloaderPatch = 1;
int OptionXloaderPatchBackups = 1;

void debugScreen(char *msg, ...);

/*****************************************************************************/
/* some PBP structs to help with parsing the SFO                             */
/*****************************************************************************/
typedef struct
{
  char signature[4];  // \0PBP
  int  version;       // 0x00010000
  int  offsets[8];
} PBP_HEADER;

typedef struct
{
  char magic[4];      // \0PSF
  char rfu000[4];
  int  label_ptr;
  int  data_ptr;
  int  nsects;
} PSF_HEADER;

typedef struct
{
  short label_off;
  char  rfu001;
  char  data_type;
  int   datafield_used;
  int   datafield_size;
  int   data_off;
} PSF_SECT;

#define SFO_TITLE_SIZE_POS  0x88
#define SFO_TITLE_POS       0x118
#define SFO_TITLE_SIZE      0x7f

PBP_HEADER default_pbphdr =
{
	"\0PBP",
	0x00010000,
	{ 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28 }
};

char g_defaultSfo[] = {
        0x00, 0x50, 0x53, 0x46, 0x01, 0x01, 0x00, 0x00, 0x94, 0x00, 0x00, 0x00, 0xe8, 0x00, 0x00, 0x00,
        0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x04, 0x02, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
        0x04, 0x00, 0x00, 0x00, 0x12, 0x00, 0x04, 0x02, 0x0a, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
        0x08, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x04, 0x02, 0x05, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
        0x18, 0x00, 0x00, 0x00, 0x27, 0x00, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
        0x20, 0x00, 0x00, 0x00, 0x36, 0x00, 0x04, 0x02, 0x05, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
        0x24, 0x00, 0x00, 0x00, 0x45, 0x00, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
        0x2c, 0x00, 0x00, 0x00, 0x4c, 0x00, 0x04, 0x02, 0x24, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
        0x30, 0x00, 0x00, 0x00, 0x42, 0x4f, 0x4f, 0x54, 0x41, 0x42, 0x4c, 0x45, 0x00, 0x43, 0x41, 0x54,
        0x45, 0x47, 0x4f, 0x52, 0x59, 0x00, 0x44, 0x49, 0x53, 0x43, 0x5f, 0x49, 0x44, 0x00, 0x44, 0x49,
        0x53, 0x43, 0x5f, 0x56, 0x45, 0x52, 0x53, 0x49, 0x4f, 0x4e, 0x00, 0x50, 0x41, 0x52, 0x45, 0x4e,
        0x54, 0x41, 0x4c, 0x5f, 0x4c, 0x45, 0x56, 0x45, 0x4c, 0x00, 0x50, 0x53, 0x50, 0x5f, 0x53, 0x59,
        0x53, 0x54, 0x45, 0x4d, 0x5f, 0x56, 0x45, 0x52, 0x00, 0x52, 0x45, 0x47, 0x49, 0x4f, 0x4e, 0x00,
        0x54, 0x49, 0x54, 0x4c, 0x45, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x4d, 0x47, 0x00, 0x00,
        0x55, 0x43, 0x4a, 0x53, 0x31, 0x30, 0x30, 0x34, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x31, 0x2e, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x31, 0x2e, 0x30, 0x30,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*****************************************************************************/
/* Patch the PARAM.SFO to be the latest version                              */
/*****************************************************************************/
void patchSFO(const char *fullpath)
{
	char outfilename[256];
	char ltitle[SFO_TITLE_SIZE];
	strcpy(ltitle ,"unknown");

	debugScreen("Working with SFO in file: %s", fullpath);

	FILE *infile = fopen(fullpath, "rb");
	if (!infile)
	{
		debugScreen("failed to open infile: %s", fullpath);
		return;
	}

	sprintf(outfilename, "%s.patched", fullpath);
	FILE *outfile = fopen(outfilename, "wb");
	if (!outfile)
	{
		debugScreen("failed to open outfile: %s", outfilename);
		return;
	}

  /***************************************************************************/
  /* EBOOT header is the PBP_HEADER structure above.                         */
  /*                                                                         */
  /* offsets should be:                                                      */
  /*   PARAM.SFO                                                             */
  /*   ICON.PNG                                                              */
  /*   ICON.PMF                                                              */
  /*   UNKNOWN.PNG                                                           */
  /*   PIC1.PNG                                                              */
  /*   SND0.AT3                                                              */
  /*   DATA.PSP                                                              */
  /*                                                                         */
  /* Based on data from www.oopo.net/consoledev/files/psp-updating.txt       */
  /***************************************************************************/
	PBP_HEADER lpbphdr;
	int lbytesread = fread(&lpbphdr, 1, sizeof(PBP_HEADER), infile);

  if (lbytesread != sizeof(PBP_HEADER))
  {
		debugScreen("didn't read full header - just %d", lbytesread);
		goto ERROR_EXIT;
  }

  /***************************************************************************/
	/* Check that this is a real PBP                                           */
	/***************************************************************************/
  if ((lpbphdr.signature[0] != '\0') ||
      (lpbphdr.signature[1] != 'P') ||
      (lpbphdr.signature[2] != 'B') ||
      (lpbphdr.signature[3] != 'P'))
  {
		debugScreen("Not a PBP");

    /*************************************************************************/
    /* It might be a plain ELF, in which case we will convert it into a      */
    /* PBP.                                                                  */
    /*************************************************************************/
		if ((lpbphdr.signature[0] == '\x7f') &&
				(lpbphdr.signature[1] == 'E') &&
				(lpbphdr.signature[2] == 'L') &&
				(lpbphdr.signature[3] == 'F'))
		{
			debugScreen("Plain ELF");

      /***********************************************************************/
			/* The conversion process works like this:                             */
      /*   Setup a default PBP header                                        */
      /*   Determine the EBOOT title                                         */
      /*   Determine the PBP offsets from filesize                           */
      /* Then later, we write the PBP, SFO and then will copy the ELF.       */
      /*                                                                     */
      /* Note that we set up the default PBP knowing that we will be         */
      /* modifying it in the standard logic below.                           */
			/***********************************************************************/
			memcpy(&lpbphdr, &default_pbphdr, sizeof(PBP_HEADER));

			fseek(infile, 0, SEEK_END);
			lpbphdr.offsets[7] = ftell(infile);
			debugScreen("file size: %d", lpbphdr.offsets[7]);

			fseek(infile, 0, SEEK_SET);

			// find the pathname between the last 2 '/'s
			const char *lslash1 = NULL;
			const char *lslash2 = NULL;
			const char *lptr = fullpath;

			do
			{
				debugScreen("lptr: %s", lptr);
				lptr = strchr(lptr, '/');

				if (lptr)
				{
					lslash1 = lslash2;
					lslash2 = lptr;
					lptr++;
				}
			}	while (lptr);

			if (!(lslash1 && lslash2))
			{
				debugScreen("slash1 or slash2 is null");
				goto ERROR_EXIT;
			}

			lslash1++;
			strncpy(ltitle, lslash1, (lslash2 - lslash1));
			ltitle[lslash2 - lslash1] = '\0';
		}
		else
		{
			debugScreen("Not an ELF");
			goto ERROR_EXIT;
		}
  }

  /***************************************************************************/
	/* The size of a modern patched SFO is 408 bytes                           */
	/***************************************************************************/
	int lsfosize = lpbphdr.offsets[1] - lpbphdr.offsets[0];
	debugScreen("sfo size: %d", lsfosize);
	if (lsfosize == 408)
	{
		debugScreen("already patched");
		goto ERROR_EXIT;
	}

  /***************************************************************************/
	/* Now we adjust the offsets to allow for the extra size we're about to    */
  /* add.                                                                    */
	/***************************************************************************/
	int ii;
	for (ii = 1; ii <= 7; ii++)
	{
		debugScreen("adjust offs %d from %d to + %d", ii, lpbphdr.offsets[ii], 408 - lsfosize);
		lpbphdr.offsets[ii] += 408 - lsfosize;
	}

	fwrite(&lpbphdr, sizeof(PBP_HEADER), 1, outfile);

  /***************************************************************************/
	/* We need to grab the title from the PARAM.SFO, we can ignore everything  */
  /* else.  Note : SFOSIZE = 0 if plainelf is true, so we won't overwrite    */
  /* ltitle.                                                                 */
	/***************************************************************************/
  if (lsfosize != 0)
  {
    char *lsfodata = (char*)malloc(lsfosize);
    if (!lsfodata)
    {
			debugScreen("failed malloc");
			goto ERROR_EXIT;
    }

		fseek(infile, lpbphdr.offsets[0], SEEK_SET);
		fread(lsfodata, lsfosize, 1, infile);

    PSF_HEADER *lpsfhdr = (PSF_HEADER*) lsfodata;

    if ((lpsfhdr->magic[0] == '\0') &&
        (lpsfhdr->magic[1] == 'P') &&
        (lpsfhdr->magic[2] == 'S') &&
        (lpsfhdr->magic[3] == 'F'))
    {
      char *lpsflabels = ((char*)lsfodata) + lpsfhdr->label_ptr;
      char *lpsfdata   = ((char*)lsfodata) + lpsfhdr->data_ptr;

      PSF_SECT *lpsfsect = (PSF_SECT*) (((char*)lsfodata) + sizeof(PSF_HEADER));

      for (ii = 0; ii < lpsfhdr->nsects; ii++)
      {
        /*********************************************************************/
        /* We're just looking for the title, which is a string type.  So we  */
        /* can ignore non-strings.                                           */
        /*********************************************************************/
        if (lpsfsect[ii].data_type == 2)
        {
          char *llabel = lpsflabels + lpsfsect[ii].label_off;
          if (stricmp(llabel, "TITLE")==0)
          {
						int llength = lpsfsect[ii].datafield_size;
						if (llength > SFO_TITLE_SIZE)
						{
							llength = SFO_TITLE_SIZE;
						}
            strncpy(ltitle,
                    lpsfdata + lpsfsect[ii].data_off,
                    llength);
            ltitle[llength] = '\0';
          }
        }
      }
    }
		else
		{
			debugScreen("not a PSF file");
			goto ERROR_EXIT;
		}
		free(lsfodata);
  }

  /***************************************************************************/
	/* Now write the new SFO template                                          */
	/***************************************************************************/
	debugScreen("title: '%s'", ltitle);

	g_defaultSfo[SFO_TITLE_SIZE_POS] = strlen(ltitle);
	strcpy(&g_defaultSfo[SFO_TITLE_POS], ltitle);

	fwrite(g_defaultSfo, sizeof(g_defaultSfo), 1, outfile);

  /***************************************************************************/
	/* And now the rest of the EBOOT.                                          */
	/***************************************************************************/

	// get a reasonable-sized buffer to copy the data
	int lbuffsize = 1048576;
	char  *lbuffer = NULL;
	while (!lbuffer && (lbuffsize > 512))
	{
    lbuffer = (char*)malloc(lbuffsize);
		if (!lbuffer)
		{
			lbuffsize /= 2;
		}
	}

	lbytesread = 1;

	while (lbytesread)
	{
		lbytesread = fread(lbuffer, 1, lbuffsize, infile);
		debugScreen("read %d bytes, now write", lbytesread);
		fwrite(lbuffer, 1, lbytesread, outfile);
	}

	int lrc;

	fclose(infile);
	fclose(outfile);

	char loldcwd[256];
	getcwd(loldcwd, 256);

	char lnewpath[256];
	strcpy(lnewpath, fullpath);
	*(strrchr(lnewpath, '/')) = '\0';

	chdir(lnewpath);

	if (OptionXloaderPatchBackups)
	{
		char backupfilename[256];
		sprintf(backupfilename, "%s.backup", fullpath);
		debugScreen("backup old file : %s", backupfilename);
		sceIoRemove(backupfilename);
		lrc = sceIoRename(fullpath, backupfilename);
		debugScreen("rename returned: %08lX", lrc);
	}
	else
	{
		debugScreen("delete file: %s", fullpath);
		sceIoRemove(fullpath);
	}

	sprintf(outfilename, "%s.patched", fullpath);
	lrc = sceIoRename(outfilename, fullpath);
	debugScreen("rename2 %s to %s returned: %08lX", outfilename, fullpath, lrc);

	debugScreen("finished");

	chdir(loldcwd);

	return;

ERROR_EXIT:
	fclose(outfile);
	fclose(infile);
}

int valid_dir(SceIoDirent *dir_ent)
{
	if (strcmp(dir_ent->d_name,".") == 0)    return 0;
	if (strcmp(dir_ent->d_name,"..") == 0)   return 0;
	if (strcmp(dir_ent->d_name,"%") == 0)    return 0;
	if (!(dir_ent->d_stat.st_attr & 0x0010)) return 0;

	return 1;
}

int main(void)
{
	pspDebugScreenInit();
#if DEBUG_FILE==1
	int file = sceIoOpen("ms0:/trace.txt", PSP_O_CREAT | PSP_O_TRUNC | PSP_O_WRONLY, 0777);
	sceIoClose(file);
#endif
	
	char temp[1000];
	SceIoDirent l_dir_ent;
	memset(&l_dir_ent, 0, sizeof(SceIoDirent));

	// Need to go through the directory structure and find the EBOOT's to patch
	int l_dh = sceIoDopen("ms0:/psp/game");
	if (l_dh >= 0)
	{
		while (sceIoDread(l_dh, &(l_dir_ent)) > 0)
		{
			if (valid_dir(&(l_dir_ent)) == 1)
			{
				sprintf(temp, "ms0:/psp/game/%%%s/EBOOT.PBP", l_dir_ent.d_name);
				FILE *l_file = fopen(temp, "rb");
				if(l_file)
				{
					fclose(l_file);
				}
				else
				{
					sprintf(temp, "ms0:/psp/game/%s%%/EBOOT.PBP", l_dir_ent.d_name);
					FILE *l_file = fopen(temp, "rb");
					if(l_file)
					{
						fclose(l_file);						
					}
					else
					{
						sprintf(temp, "ms0:/psp/game/%s/EBOOT.PBP", l_dir_ent.d_name);
					}
				}

				pspDebugScreenInit();
				patchSFO(temp);

				sprintf(temp, "%s.patched", temp);
				sceIoRemove(temp);
			}
		}

		sceIoDclose(l_dh);
	}

	sceKernelExitGame();

	return 1;
}

char buffer[512];

void debugScreen(char *msg, ...)
{
#if DEBUG_SCREEN==1 || DEBUG_FILE==1
    	va_list fmtargs;

      va_start(fmtargs,msg);
      vsnprintf(buffer,sizeof(buffer)-1,msg,fmtargs);
      va_end(fmtargs);

#if DEBUG_SCREEN==1
	  pspDebugScreenPrintf("%s\n", buffer);
#endif
#if DEBUG_FILE==1
		 {
			 int file = sceIoOpen("ms0:/trace.txt", PSP_O_APPEND | PSP_O_RDWR, 0777);
			 strcat(buffer, "\n");
			 sceIoWrite(file, buffer, strlen(buffer));
			 sceIoClose(file);
		 }
#endif
#endif
}
