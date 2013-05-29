/***************************************************************************
 *   Copyright (C) 2013 by James Holodnak                                  *
 *   jamesholodnak@gmail.com                                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <prsht.h>
#include "system/win32/resource.h"
#include "system/win32/mainwnd.h"
#include "misc/config.h"

static const char progid[] = "nesemu2.image.1";

static void showerror(int err)
{
	LPVOID lpMsgBuf;
	DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
	DWORD langid = MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT);

	if(FormatMessage(flags,NULL,err,langid,(LPTSTR)&lpMsgBuf,0,NULL) == 0) {
		MessageBox(0,"Error getting the error message","nesemu2",MB_OK | MB_ICONINFORMATION);
		return;
	}
	MessageBox(NULL,(LPCTSTR)lpMsgBuf,"Error",MB_OK | MB_ICONINFORMATION);
	LocalFree(lpMsgBuf);
}

//works with xp
static int registerprogid()
{
	HKEY hKey;
	DWORD dis = 0;
	int ret = 1;
	char keyname[512];
	char cmd[1024];
	char filename[1024];
	LONG err;

	//get executable filename
	GetModuleFileName(GetModuleHandle(0),filename,1024);

	//register progid
	sprintf(cmd,"\"%s\" \"%%1\"",filename);
	sprintf(keyname,"Software\\Classes\\%s\\shell\\open\\command",progid);
	if((err = RegCreateKeyEx(HKEY_CURRENT_USER,keyname,0,NULL,0,KEY_READ | KEY_WRITE,NULL,&hKey,&dis)) == ERROR_SUCCESS) {
		log_printf("registerprogid:  setting default value to '%s'\n",cmd);
		RegSetValueEx(hKey,NULL,0,REG_EXPAND_SZ,(LPBYTE)cmd,(DWORD)strlen(cmd) + 1);
		RegCloseKey(hKey);
	}
	else {
		showerror(err);
	}

	//register default icon
	sprintf(cmd,"%s,0",filename);
	sprintf(keyname,"Software\\Classes\\%s\\defaulticon",progid);
	log_printf("registerprogid:  creating key '%s'\n",keyname);
	if((err = RegCreateKeyEx(HKEY_CURRENT_USER,keyname,0,NULL,0,KEY_READ | KEY_WRITE,NULL,&hKey,&dis)) == ERROR_SUCCESS) {
		RegSetValueEx(hKey,NULL,0,REG_EXPAND_SZ,(LPBYTE)cmd,(DWORD)strlen(cmd) + 1);
		RegCloseKey(hKey);
	}
	else {
		showerror(err);
	}
	return(ret);
}

static int registerassociation(char *extension)
{
	HKEY hKey;
	DWORD dis = 0;
	int ret = 1;
	char keyname[128];
	char filename[1024];
	LONG err;

	GetModuleFileName(GetModuleHandle(0),filename,1024);

	//register progid
	sprintf(keyname,"Software\\Classes\\%s",extension);
	log_printf("registerassociation:  creating key '%s'\n",keyname);
	if((err = RegCreateKeyEx(HKEY_CURRENT_USER,keyname,0,NULL,0,KEY_READ | KEY_WRITE,NULL,&hKey,&dis)) == ERROR_SUCCESS) {
		RegSetValueEx(hKey,NULL,0,REG_SZ,(LPBYTE)progid,(DWORD)strlen(progid) + 1);
		RegCloseKey(hKey);
	}
	else {
		showerror(err);
	}
	return(ret);
}

static int unregisterassociation(char *extension)
{
	char keyname[128];

	sprintf(keyname,"Software\\Classes\\%s",extension);
	RegDeleteKey(HKEY_CURRENT_USER,keyname);
	return(0);
}

/* returns 0 if already associated */
static int checkassociation(char *extension)
{
	int ret = 1;
	HKEY hKey;
	char str[256];
	char keyname[512];
	DWORD type = REG_SZ;
	int len = 256;

	//open key
	sprintf(keyname,"Software\\Classes\\%s",extension);
	if(RegOpenKeyEx(HKEY_CURRENT_USER,keyname,0,KEY_QUERY_VALUE,&hKey) == ERROR_SUCCESS) {

		//get default entry
		if(RegQueryValueEx(hKey,"",NULL,&type,(LPBYTE)str,&len) == ERROR_SUCCESS) {

			//compare with our progid
			if(strcmp(str,progid) == 0) {
				ret = 0;
			}
		}

		//close key
		RegCloseKey(hKey);
	}

	//return
	return(ret);
}

static void modifyassociations(DWORD mask)
{
	char *extensions[4] = {".nes",".unf",".unif",".fds"};
	int i,a;

	//if we need the progid, register ir
	if(mask)
		registerprogid();

	//find out what we are associating with
	for(i=0;i<4;i++) {

		//see if we are already associated with this extension
		a = checkassociation(extensions[i]);

		//if this extension is to be registered, then do it
		if(mask & 1) {

			//if it isnt registered already
			if(a == 1)
				registerassociation(extensions[i]);
		}

		//else see if we are already associated with it, then unregister
		else if(a == 0) {
			unregisterassociation(extensions[i]);
		}

		mask >>= 1;
	}
}

//PSN_KILLACTIVE validated the changes
LRESULT CALLBACK GeneralProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	LPNMHDR nmhdr;
	DWORD mask;

	switch(message) {
		case WM_INITDIALOG:
			CheckDlgButton(hDlg,IDC_NESCHECK,checkassociation(".nes") ? BST_UNCHECKED : BST_CHECKED);
			CheckDlgButton(hDlg,IDC_UNFCHECK,checkassociation(".unf") ? BST_UNCHECKED : BST_CHECKED);
			CheckDlgButton(hDlg,IDC_FDSCHECK,checkassociation(".fds") ? BST_UNCHECKED : BST_CHECKED);
			return TRUE;

		case WM_NOTIFY:
			nmhdr = (LPNMHDR)lParam;
			switch(nmhdr->code) {
				case PSN_APPLY:
					mask =  IsDlgButtonChecked(hDlg,IDC_NESCHECK) ? 1 : 0;
					mask |= IsDlgButtonChecked(hDlg,IDC_UNFCHECK) ? 2 : 0;
					mask |= IsDlgButtonChecked(hDlg,IDC_UNFCHECK) ? 4 : 0;
					mask |= IsDlgButtonChecked(hDlg,IDC_FDSCHECK) ? 8 : 0;
					modifyassociations(mask);
					return(TRUE);
			}
			break;
	}
	return(FALSE);
}

#define GetDlgItemText_SetConfig(hwnd,ctrlid,cfgvar)	\
	GetDlgItemText(hwnd,ctrlid,tmpstr,1024);				\
	config_set_string(cfgvar,tmpstr);
LRESULT CALLBACK PathsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	LPNMHDR nmhdr;
	static char tmpstr[1024];

	switch(message) {
		case WM_INITDIALOG:
			SetDlgItemText(hDlg,IDC_DATAPATHEDIT,config_get_string("path.data"));
			SetDlgItemText(hDlg,IDC_BIOSPATHEDIT,config_get_string("path.bios"));
			SetDlgItemText(hDlg,IDC_SRAMPATHEDIT,config_get_string("path.save"));
			SetDlgItemText(hDlg,IDC_STATEPATHEDIT,config_get_string("path.state"));
			SetDlgItemText(hDlg,IDC_PATCHPATHEDIT,config_get_string("path.patch"));
			SetDlgItemText(hDlg,IDC_PALETTEPATHEDIT,config_get_string("path.palette"));
			SetDlgItemText(hDlg,IDC_CHEATPATHEDIT,config_get_string("path.cheat"));
			return(TRUE);

		case WM_NOTIFY:
			nmhdr = (LPNMHDR)lParam;
			switch(nmhdr->code) {
				case PSN_APPLY:
					GetDlgItemText_SetConfig(hDlg,IDC_DATAPATHEDIT,"path.data");
					GetDlgItemText_SetConfig(hDlg,IDC_BIOSPATHEDIT,"path.bios");
					GetDlgItemText_SetConfig(hDlg,IDC_SRAMPATHEDIT,"path.save");
					GetDlgItemText_SetConfig(hDlg,IDC_STATEPATHEDIT,"path.state");
					GetDlgItemText_SetConfig(hDlg,IDC_PATCHPATHEDIT,"path.patch");
					GetDlgItemText_SetConfig(hDlg,IDC_PALETTEPATHEDIT,"path.palette");
					GetDlgItemText_SetConfig(hDlg,IDC_CHEATPATHEDIT,"path.cheat");
					return(TRUE);
			}
			break;
	}
	return(FALSE);
}

LRESULT CALLBACK NesProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	LPNMHDR nmhdr;
	static char tmpstr[1024];

	switch(message) {
		case WM_INITDIALOG:
			CheckDlgButton(hDlg,IDC_PAUSEAFTERLOADCHECK,config_get_bool("nes.pause_on_load") ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hDlg,IDC_LOGUNHANDLEDIOCHECK,config_get_bool("nes.log_unhandled_io") ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hDlg,IDC_FDSHLECHECK,config_get_bool("nes.fds.hle") ? BST_CHECKED : BST_UNCHECKED);
			SetDlgItemText(hDlg,IDC_FDSBIOSEDIT,config_get_string("nes.fds.bios"));
			CheckDlgButton(hDlg,IDC_GENIECHECK,config_get_bool("nes.gamegenie.enabled") ? BST_CHECKED : BST_UNCHECKED);
			SetDlgItemText(hDlg,IDC_GENIEBIOSEDIT,config_get_string("nes.gamegenie.bios"));
			return(TRUE);

		case WM_NOTIFY:
			nmhdr = (LPNMHDR)lParam;
			switch(nmhdr->code) {
				case PSN_APPLY:
					config_set_bool("nes.pause_on_load",IsDlgButtonChecked(hDlg,IDC_PAUSEAFTERLOADCHECK) ? 1 : 0);
					config_set_bool("nes.log_unhandled_io",IsDlgButtonChecked(hDlg,IDC_LOGUNHANDLEDIOCHECK) ? 1 : 0);
					config_set_bool("nes.fds.hle",IsDlgButtonChecked(hDlg,IDC_FDSHLECHECK) ? 1 : 0);
					GetDlgItemText_SetConfig(hDlg,IDC_FDSBIOSEDIT,"nes.fds.bios");
					config_set_bool("nes.gamegenie.enabled",IsDlgButtonChecked(hDlg,IDC_GENIECHECK) ? 1 : 0);
					GetDlgItemText_SetConfig(hDlg,IDC_GENIEBIOSEDIT,"nes.gamegenie.bios");
					return(TRUE);
			}
			break;
	}
	return(FALSE);
}

VOID ConfigurationPropertySheet(HWND hWnd)
{
	PROPSHEETPAGE psp[3];
	PROPSHEETHEADER psh;
	int i;

	memset(psp,0,sizeof(PROPSHEETPAGE) * 3);
	memset(&psh,0,sizeof(PROPSHEETHEADER));

	for(i=0;i<3;i++) {
		psp[i].dwSize = sizeof(PROPSHEETPAGE);
		psp[i].hInstance = hInst;
	}

	psp[0].pszTemplate = MAKEINTRESOURCE(IDD_CONFIG_GENERAL1);
	psp[0].pfnDlgProc = GeneralProc;

	psp[1].pszTemplate = MAKEINTRESOURCE(IDD_CONFIG_GENERAL2);
	psp[1].pfnDlgProc = PathsProc;

	psp[2].pszTemplate = MAKEINTRESOURCE(IDD_CONFIG_GENERAL3);
	psp[2].pfnDlgProc = NesProc;

	psh.dwSize = sizeof(PROPSHEETHEADER);
	psh.dwFlags = PSH_USEICONID | PSH_PROPSHEETPAGE;
	psh.hwndParent = hWnd;
	psh.hInstance = hInst;
	psh.pszCaption = (LPSTR) "Configuration";
	psh.nPages = sizeof(psp) / sizeof(PROPSHEETPAGE);
	psh.ppsp = (LPCPROPSHEETPAGE)&psp;

	PropertySheet(&psh);
}
