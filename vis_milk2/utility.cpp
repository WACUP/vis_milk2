/*
  LICENSE
  -------
Copyright 2005-2013 Nullsoft, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer. 

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution. 

  * Neither the name of Nullsoft nor the names of its contributors may be used to 
    endorse or promote products derived from this software without specific prior written permission. 
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR 
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND 
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR 
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT 
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "api.h"
#include "utility.h"
#include <math.h>
#include <locale.h>
#include <windows.h>
#ifdef _DEBUG
    #define D3D_DEBUG_INFO  // declare this before including d3d9.h
#endif
#include <d3d9.h>
#include <Winamp/wa_ipc.h>
#include "resource.h"
#include <shellapi.h>
#include <shlwapi.h>
#include <loader/loader/utils.h>

intptr_t myOpenURL(HWND hwnd, LPCWSTR loc)
{
	if (loc && *loc)
	{
		// changed not to try & show it in the in-skin browser
		// as the cases we generally need work better in the
		// user's default browser instead of the old IE mode!
		/*bool override=false;
		WASABI_API_SYSCB->syscb_issueCallback(SysCallback::BROWSER, BrowserCallback::ONOPENURL,
											  reinterpret_cast<intptr_t>(loc),
											  reinterpret_cast<intptr_t>(&override));
		if (!override)*/
			return (intptr_t)ShellExecute(hwnd, L"open", loc, NULL, NULL, SW_SHOWNORMAL);
		/*else
			return 33;*/
	}
	return 33;
}

float PowCosineInterp(float x, float pow)
{
    // input (x) & output should be in range 0..1.
    // pow > 0: tends to push things toward 0 and 1
    // pow < 0: tends to push things toward 0.5.

    if (x<0) 
        return 0;
    if (x>1)
        return 1;

    int bneg = (pow < 0) ? 1 : 0;
    if (bneg)
        pow = -pow;

    if (pow>1000) pow=1000;

    int its = (int)pow;
    for (int i=0; i<its; i++)
    {
        if (bneg)
            x = InvCosineInterp(x);
        else
            x = CosineInterp(x);
    }
    float x2 = (bneg) ? InvCosineInterp(x) : CosineInterp(x);
    float dx = pow - its;
    return ((1-dx)*x + (dx)*x2);
}

float AdjustRateToFPS(float per_frame_decay_rate_at_fps1, float fps1, float actual_fps)
{
    // returns the equivalent per-frame decay rate at actual_fps

    // basically, do all your testing at fps1 and get a good decay rate;
    // then, in the real application, adjust that rate by the actual fps each time you use it.
    
    float per_second_decay_rate_at_fps1 = powf(per_frame_decay_rate_at_fps1, fps1);
    float per_frame_decay_rate_at_fps2 = powf(per_second_decay_rate_at_fps1, 1.0f/actual_fps);

    return per_frame_decay_rate_at_fps2;
}

float GetPrivateProfileFloatW(const wchar_t *szSectionName, const wchar_t *szKeyName, const float fDefault, const wchar_t *szIniFile)
{
    wchar_t string[64] = {0};
    wchar_t szDefault[64] = {0};
    float ret = fDefault;

	WASABI_API_LNG->SafePrintfL(szDefault, ARRAYSIZE(szDefault), L"%f", fDefault);

    if (GetPrivateProfileStringW(szSectionName, szKeyName, szDefault, string, 64, szIniFile) > 0)
    {
        WASABI_API_LNG->SafeWtoF(string, &ret);
    }
    return ret;
}

void WritePrivateProfileFloatW(const float f, const float fDefault, const wchar_t *szKeyName,
							   const wchar_t *szIniFile, const wchar_t *szSectionName)
{
    wchar_t szValue[32] = {0};
	if (f != fDefault)
	{
		WASABI_API_LNG->SafePrintfL(szValue, ARRAYSIZE(szValue), L"%f", f);
	}
    WritePrivateProfileStringW(szSectionName, szKeyName, (szValue[0] ? szValue : NULL), szIniFile);
}

void WritePrivateProfileIntW(const int d, const int dDefault, const wchar_t *szKeyName,
							 const wchar_t *szIniFile, const wchar_t *szSectionName)
{
	wchar_t szValue[32] = {0};
	if (d != dDefault)
	{
		_itow_s(d, szValue, ARRAYSIZE(szValue), 10);
	}
    WritePrivateProfileStringW(szSectionName, szKeyName, (szValue[0] ? szValue : NULL), szIniFile);
}

void SetScrollLock(int bNewState, bool bPreventHandling)
{
	if(bPreventHandling) return;

    if (bNewState != (GetKeyState(VK_SCROLL) & 1))
    {
        // Simulate a key press
        keybd_event( VK_SCROLL,
                      0x45,
                      KEYEVENTF_EXTENDEDKEY | 0,
                      0 );

        // Simulate a key release
        keybd_event( VK_SCROLL,
                      0x45,
                      KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP,
                      0);
    }
}

void RemoveExtension(wchar_t *str)
{
    wchar_t *p = wcsrchr(str, L'.');
    if (p) *p = 0;
}

static void ShiftDown(wchar_t *str)
{
	while (*str)
	{
		str[0] = str[1];
		++str;
	}
}

void RemoveSingleAmpersands(wchar_t *str)
{
	while (*str)
	{
		if (str[0] == L'&')
		{
			if (str[1] == L'&') // two in a row: replace with single ampersand, move on
				++str;

			ShiftDown(str);
		}
		else
			str = CharNextW(str);
	}
}

void TextToGuid(char *str, GUID *pGUID)
{
    if (!str) return;
    if (!pGUID) return;

    DWORD d[11] = {0};
    
    (void)sscanf(str, "%X %X %X %X %X %X %X %X %X %X %X", &d[0], &d[1],
				 &d[2], &d[3], &d[4], &d[5], &d[6], &d[7], &d[8], &d[9], &d[10]);

    pGUID->Data1 = (DWORD)d[0];
    pGUID->Data2 = (WORD)d[1];
    pGUID->Data3 = (WORD)d[2];
    pGUID->Data4[0] = (BYTE)d[3];
    pGUID->Data4[1] = (BYTE)d[4];
    pGUID->Data4[2] = (BYTE)d[5];
    pGUID->Data4[3] = (BYTE)d[6];
    pGUID->Data4[4] = (BYTE)d[7];
    pGUID->Data4[5] = (BYTE)d[8];
    pGUID->Data4[6] = (BYTE)d[9];
    pGUID->Data4[7] = (BYTE)d[10];
}

void GuidToText(GUID *pGUID, char *str, int nStrLen)
{
    // note: nStrLen should be set to sizeof(str).
    if (!str) return;
    if (!nStrLen) return;
    str[0] = 0;
    if (!pGUID) return;
    
    DWORD d[11];
    d[0]  = (DWORD)pGUID->Data1;
    d[1]  = (DWORD)pGUID->Data2;
    d[2]  = (DWORD)pGUID->Data3;
    d[3]  = (DWORD)pGUID->Data4[0];
    d[4]  = (DWORD)pGUID->Data4[1];
    d[5]  = (DWORD)pGUID->Data4[2];
    d[6]  = (DWORD)pGUID->Data4[3];
    d[7]  = (DWORD)pGUID->Data4[4];
    d[8]  = (DWORD)pGUID->Data4[5];
    d[9]  = (DWORD)pGUID->Data4[6];
    d[10] = (DWORD)pGUID->Data4[7];

    _snprintf(str, nStrLen, "%08X %04X %04X %02X %02X %02X %02X %02X %02X %02X %02X", 
			  d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9], d[10]);
}

#ifdef _DEBUG
    void OutputDebugMessage(char *szStartText, HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
    {
        // note: this function does NOT log WM_MOUSEMOVE, WM_NCHITTEST, or WM_SETCURSOR
        //        messages, since they are so frequent.
        // note: these identifiers were pulled from winuser.h

        //if (msg == WM_MOUSEMOVE || msg == WM_NCHITTEST || msg == WM_SETCURSOR)
        //    return;

        #ifdef _DEBUG
            char buf[64] = {0};
            int matched = 1;

            _snprintf(buf, ARRAYSIZE(buf), "WM_");
    
            switch(msg)
            {
            case 0x0001: strcat(buf, "CREATE"); break;                      
            case 0x0002: strcat(buf, "DESTROY"); break;
            case 0x0003: strcat(buf, "MOVE"); break;
            case 0x0005: strcat(buf, "SIZE"); break;
            case 0x0006: strcat(buf, "ACTIVATE"); break;
            case 0x0007: strcat(buf, "SETFOCUS"); break;
            case 0x0008: strcat(buf, "KILLFOCUS"); break;
            case 0x000A: strcat(buf, "ENABLE"); break;
            case 0x000B: strcat(buf, "SETREDRAW"); break;
            case 0x000C: strcat(buf, "SETTEXT"); break;
            case 0x000D: strcat(buf, "GETTEXT"); break;
            case 0x000E: strcat(buf, "GETTEXTLENGTH"); break;
            case 0x000F: strcat(buf, "PAINT"); break;
            case 0x0010: strcat(buf, "CLOSE"); break;
            case 0x0011: strcat(buf, "QUERYENDSESSION"); break;
            case 0x0012: strcat(buf, "QUIT"); break;
            case 0x0013: strcat(buf, "QUERYOPEN"); break;
            case 0x0014: strcat(buf, "ERASEBKGND"); break;
            case 0x0015: strcat(buf, "SYSCOLORCHANGE"); break;
            case 0x0016: strcat(buf, "ENDSESSION"); break;
            case 0x0018: strcat(buf, "SHOWWINDOW"); break;
            case 0x001A: strcat(buf, "WININICHANGE"); break;
            case 0x001B: strcat(buf, "DEVMODECHANGE"); break;
            case 0x001C: strcat(buf, "ACTIVATEAPP"); break;
            case 0x001D: strcat(buf, "FONTCHANGE"); break;
            case 0x001E: strcat(buf, "TIMECHANGE"); break;
            case 0x001F: strcat(buf, "CANCELMODE"); break;
            case 0x0020: strcat(buf, "SETCURSOR"); break;
            case 0x0021: strcat(buf, "MOUSEACTIVATE"); break;
            case 0x0022: strcat(buf, "CHILDACTIVATE"); break;
            case 0x0023: strcat(buf, "QUEUESYNC"); break;
            case 0x0024: strcat(buf, "GETMINMAXINFO"); break;
            case 0x0026: strcat(buf, "PAINTICON"); break;
            case 0x0027: strcat(buf, "ICONERASEBKGND"); break;
            case 0x0028: strcat(buf, "NEXTDLGCTL"); break;
            case 0x002A: strcat(buf, "SPOOLERSTATUS"); break;
            case 0x002B: strcat(buf, "DRAWITEM"); break;
            case 0x002C: strcat(buf, "MEASUREITEM"); break;
            case 0x002D: strcat(buf, "DELETEITEM"); break;
            case 0x002E: strcat(buf, "VKEYTOITEM"); break;
            case 0x002F: strcat(buf, "CHARTOITEM"); break;
            case 0x0030: strcat(buf, "SETFONT"); break;
            case 0x0031: strcat(buf, "GETFONT"); break;
            case 0x0032: strcat(buf, "SETHOTKEY"); break;
            case 0x0033: strcat(buf, "GETHOTKEY"); break;
            case 0x0037: strcat(buf, "QUERYDRAGICON"); break;
            case 0x0039: strcat(buf, "COMPAREITEM"); break;
            case 0x0041: strcat(buf, "COMPACTING"); break;
            case 0x0044: strcat(buf, "COMMNOTIFY"); break;
            case 0x0046: strcat(buf, "WINDOWPOSCHANGING"); break;
            case 0x0047: strcat(buf, "WINDOWPOSCHANGED"); break;
            case 0x0048: strcat(buf, "POWER"); break;
            case 0x004A: strcat(buf, "COPYDATA"); break;
            case 0x004B: strcat(buf, "CANCELJOURNAL"); break;

            #if(WINVER >= 0x0400)
            case 0x004E: strcat(buf, "NOTIFY"); break;
            case 0x0050: strcat(buf, "INPUTLANGCHANGEREQUEST"); break;
            case 0x0051: strcat(buf, "INPUTLANGCHANGE"); break;
            case 0x0052: strcat(buf, "TCARD"); break;
            case 0x0053: strcat(buf, "HELP"); break;
            case 0x0054: strcat(buf, "USERCHANGED"); break;
            case 0x0055: strcat(buf, "NOTIFYFORMAT"); break;
            case 0x007B: strcat(buf, "CONTEXTMENU"); break;
            case 0x007C: strcat(buf, "STYLECHANGING"); break;
            case 0x007D: strcat(buf, "STYLECHANGED"); break;
            case 0x007E: strcat(buf, "DISPLAYCHANGE"); break;
            case 0x007F: strcat(buf, "GETICON"); break;
            case 0x0080: strcat(buf, "SETICON"); break;
            #endif 

            case 0x0081: strcat(buf, "NCCREATE"); break;
            case 0x0082: strcat(buf, "NCDESTROY"); break;
            case 0x0083: strcat(buf, "NCCALCSIZE"); break;
            case 0x0084: strcat(buf, "NCHITTEST"); break;
            case 0x0085: strcat(buf, "NCPAINT"); break;
            case 0x0086: strcat(buf, "NCACTIVATE"); break;
            case 0x0087: strcat(buf, "GETDLGCODE"); break;
            case 0x0088: strcat(buf, "SYNCPAINT"); break;
            case 0x00A0: strcat(buf, "NCMOUSEMOVE"); break;
            case 0x00A1: strcat(buf, "NCLBUTTONDOWN"); break;
            case 0x00A2: strcat(buf, "NCLBUTTONUP"); break;
            case 0x00A3: strcat(buf, "NCLBUTTONDBLCLK"); break;
            case 0x00A4: strcat(buf, "NCRBUTTONDOWN"); break;
            case 0x00A5: strcat(buf, "NCRBUTTONUP"); break;
            case 0x00A6: strcat(buf, "NCRBUTTONDBLCLK"); break;
            case 0x00A7: strcat(buf, "NCMBUTTONDOWN"); break;
            case 0x00A8: strcat(buf, "NCMBUTTONUP"); break;
            case 0x00A9: strcat(buf, "NCMBUTTONDBLCLK"); break;
            case 0x0100: strcat(buf, "KEYDOWN"); break;
            case 0x0101: strcat(buf, "KEYUP"); break;
            case 0x0102: strcat(buf, "CHAR"); break;
            case 0x0103: strcat(buf, "DEADCHAR"); break;
            case 0x0104: strcat(buf, "SYSKEYDOWN"); break;
            case 0x0105: strcat(buf, "SYSKEYUP"); break;
            case 0x0106: strcat(buf, "SYSCHAR"); break;
            case 0x0107: strcat(buf, "SYSDEADCHAR"); break;
            case 0x0108: strcat(buf, "KEYLAST"); break;

            #if(WINVER >= 0x0400)
            case 0x010D: strcat(buf, "IME_STARTCOMPOSITION"); break;
            case 0x010E: strcat(buf, "IME_ENDCOMPOSITION"); break;
            case 0x010F: strcat(buf, "IME_COMPOSITION"); break;
            //case 0x010F: strcat(buf, "IME_KEYLAST"); break;
            #endif 

            case 0x0110: strcat(buf, "INITDIALOG"); break;
            case 0x0111: strcat(buf, "COMMAND"); break;
            case 0x0112: strcat(buf, "SYSCOMMAND"); break;
            case 0x0113: strcat(buf, "TIMER"); break;
            case 0x0114: strcat(buf, "HSCROLL"); break;
            case 0x0115: strcat(buf, "VSCROLL"); break;
            case 0x0116: strcat(buf, "INITMENU"); break;
            case 0x0117: strcat(buf, "INITMENUPOPUP"); break;
            case 0x011F: strcat(buf, "MENUSELECT"); break;
            case 0x0120: strcat(buf, "MENUCHAR"); break;
            case 0x0121: strcat(buf, "ENTERIDLE"); break;
            #if(WINVER >= 0x0500)
            case 0x0122: strcat(buf, "MENURBUTTONUP"); break;
            case 0x0123: strcat(buf, "MENUDRAG"); break;
            case 0x0124: strcat(buf, "MENUGETOBJECT"); break;
            case 0x0125: strcat(buf, "UNINITMENUPOPUP"); break;
            case 0x0126: strcat(buf, "MENUCOMMAND"); break;
            #endif 

            case 0x0132: strcat(buf, "CTLCOLORMSGBOX"); break;
            case 0x0133: strcat(buf, "CTLCOLOREDIT"); break;
            case 0x0134: strcat(buf, "CTLCOLORLISTBOX"); break;
            case 0x0135: strcat(buf, "CTLCOLORBTN"); break;
            case 0x0136: strcat(buf, "CTLCOLORDLG"); break;
            case 0x0137: strcat(buf, "CTLCOLORSCROLLBAR"); break;
            case 0x0138: strcat(buf, "CTLCOLORSTATIC"); break;

            //case 0x0200: strcat(buf, "MOUSEFIRST"); break;
            case 0x0200: strcat(buf, "MOUSEMOVE"); break;
            case 0x0201: strcat(buf, "LBUTTONDOWN"); break;
            case 0x0202: strcat(buf, "LBUTTONUP"); break;
            case 0x0203: strcat(buf, "LBUTTONDBLCLK"); break;
            case 0x0204: strcat(buf, "RBUTTONDOWN"); break;
            case 0x0205: strcat(buf, "RBUTTONUP"); break;
            case 0x0206: strcat(buf, "RBUTTONDBLCLK"); break;
            case 0x0207: strcat(buf, "MBUTTONDOWN"); break;
            case 0x0208: strcat(buf, "MBUTTONUP"); break;
            case 0x0209: strcat(buf, "MBUTTONDBLCLK"); break;

            #if (_WIN32_WINNT >= 0x0400) || (_WIN32_WINDOWS > 0x0400)
            case 0x020A: strcat(buf, "MOUSEWHEEL"); break;
            case 0x020E: strcat(buf, "MOUSELAST"); break;
            #else
            //case 0x0209: strcat(buf, "MOUSELAST"); break;
            #endif 

            case 0x0210: strcat(buf, "PARENTNOTIFY"); break;
            case 0x0211: strcat(buf, "ENTERMENULOOP"); break;
            case 0x0212: strcat(buf, "EXITMENULOOP"); break;

            #if(WINVER >= 0x0400)
            case 0x0213: strcat(buf, "NEXTMENU"); break;
            case 0x0214: strcat(buf, "SIZING"); break;
            case 0x0215: strcat(buf, "CAPTURECHANGED"); break;
            case 0x0216: strcat(buf, "MOVING"); break;
            case 0x0218: strcat(buf, "POWERBROADCAST"); break;
            case 0x0219: strcat(buf, "DEVICECHANGE"); break;
            #endif 

            /*
            case 0x0220: strcat(buf, "MDICREATE"); break;
            case 0x0221: strcat(buf, "MDIDESTROY"); break;
            case 0x0222: strcat(buf, "MDIACTIVATE"); break;
            case 0x0223: strcat(buf, "MDIRESTORE"); break;
            case 0x0224: strcat(buf, "MDINEXT"); break;
            case 0x0225: strcat(buf, "MDIMAXIMIZE"); break;
            case 0x0226: strcat(buf, "MDITILE"); break;
            case 0x0227: strcat(buf, "MDICASCADE"); break;
            case 0x0228: strcat(buf, "MDIICONARRANGE"); break;
            case 0x0229: strcat(buf, "MDIGETACTIVE"); break;
            */

            case 0x0230: strcat(buf, "MDISETMENU"); break;
            case 0x0231: strcat(buf, "ENTERSIZEMOVE"); break;
            case 0x0232: strcat(buf, "EXITSIZEMOVE"); break;
            case 0x0233: strcat(buf, "DROPFILES"); break;
            case 0x0234: strcat(buf, "MDIREFRESHMENU"); break;


            /*
            #if(WINVER >= 0x0400)
            case 0x0281: strcat(buf, "IME_SETCONTEXT"); break;
            case 0x0282: strcat(buf, "IME_NOTIFY"); break;
            case 0x0283: strcat(buf, "IME_CONTROL"); break;
            case 0x0284: strcat(buf, "IME_COMPOSITIONFULL"); break;
            case 0x0285: strcat(buf, "IME_SELECT"); break;
            case 0x0286: strcat(buf, "IME_CHAR"); break;
            #endif 
            #if(WINVER >= 0x0500)
            case 0x0288: strcat(buf, "IME_REQUEST"); break;
            #endif 
            #if(WINVER >= 0x0400)
            case 0x0290: strcat(buf, "IME_KEYDOWN"); break;
            case 0x0291: strcat(buf, "IME_KEYUP"); break;
            #endif 
            */

            #if(_WIN32_WINNT >= 0x0400)
            case 0x02A1: strcat(buf, "MOUSEHOVER"); break;
            case 0x02A3: strcat(buf, "MOUSELEAVE"); break;
            #endif 

            case 0x0300: strcat(buf, "CUT"); break;
            case 0x0301: strcat(buf, "COPY"); break;
            case 0x0302: strcat(buf, "PASTE"); break;
            case 0x0303: strcat(buf, "CLEAR"); break;
            case 0x0304: strcat(buf, "UNDO"); break;
            case 0x0305: strcat(buf, "RENDERFORMAT"); break;
            case 0x0306: strcat(buf, "RENDERALLFORMATS"); break;
            case 0x0307: strcat(buf, "DESTROYCLIPBOARD"); break;
            case 0x0308: strcat(buf, "DRAWCLIPBOARD"); break;
            case 0x0309: strcat(buf, "PAINTCLIPBOARD"); break;
            case 0x030A: strcat(buf, "VSCROLLCLIPBOARD"); break;
            case 0x030B: strcat(buf, "SIZECLIPBOARD"); break;
            case 0x030C: strcat(buf, "ASKCBFORMATNAME"); break;
            case 0x030D: strcat(buf, "CHANGECBCHAIN"); break;
            case 0x030E: strcat(buf, "HSCROLLCLIPBOARD"); break;
            case 0x030F: strcat(buf, "QUERYNEWPALETTE"); break;
            case 0x0310: strcat(buf, "PALETTEISCHANGING"); break;
            case 0x0311: strcat(buf, "PALETTECHANGED"); break;
            case 0x0312: strcat(buf, "HOTKEY"); break;

            #if(WINVER >= 0x0400)
            case 0x0317: strcat(buf, "PRINT"); break;
            case 0x0318: strcat(buf, "PRINTCLIENT"); break;

            case 0x0358: strcat(buf, "HANDHELDFIRST"); break;
            case 0x035F: strcat(buf, "HANDHELDLAST"); break;

            case 0x0360: strcat(buf, "AFXFIRST"); break;
            case 0x037F: strcat(buf, "AFXLAST"); break;
            #endif 

            case 0x0380: strcat(buf, "PENWINFIRST"); break;
            case 0x038F: strcat(buf, "PENWINLAST"); break;

            default: 
                _snprintf(buf, ARRAYSIZE(buf), "unknown"); 
                matched = 0;
                break;
            }

            int n = strlen(buf);
            int desired_len = 24;
            int spaces_to_append = desired_len-n;
            if (spaces_to_append>0)
            {
                for (int i=0; i<spaces_to_append; i++)
                    buf[n+i] = ' ';
                buf[desired_len] = 0;
            }      

            char buf2[256] = {0};
            if (matched)
                _snprintf(buf2, ARRAYSIZE(buf2), "%shwnd=%08x, msg=%s, w=%08x, l=%08x\n", szStartText, hwnd, buf, wParam, lParam);
            else
                _snprintf(buf2, ARRAYSIZE(buf2), "%shwnd=%08x, msg=unknown/0x%08x, w=%08x, l=%08x\n", szStartText, hwnd, msg, wParam, lParam);
            OutputDebugStringA(buf2);
        #endif
    }
#endif

extern "C" __declspec(dllimport) bool SetupFile(LPSTR setup_url, LPCWSTR filename);
extern "C" __declspec(dllimport) void ExtractLZMA(LPCWSTR filename, LPCWSTR folder);
void DownloadDirectX(HWND hwnd)
{
	wchar_t folder[MAX_PATH] = { 0 };
	if (ExpandEnvironmentStrings(L"%localappdata%\\WACUP\\DirectX\\",
								 folder, ARRAYSIZE(folder)) && folder[0])
	{
		CreateDirectory(folder, NULL);
		if (PathIsDirectory(folder))
		{
			DWORD exit_code = (DWORD)-1;
			wchar_t file[MAX_PATH] = { 0 };
			PathCombine(file, folder, L"WACUP_DirectX_9_0c_Specific_Setup.7z");
			SetupFile("https://getwacup.com/files/wacup_d3dx9_31_43_x86_embed.7z", file);
			ExtractLZMA(file, folder);

			PathCombine(file, folder, L"dxsetup.exe");
			if (PathFileExists(file))
			{
				SHELLEXECUTEINFO lpExecInfo = {0};
				lpExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
				lpExecInfo.lpFile = file;
				lpExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_UNICODE |
								   SEE_MASK_FLAG_NO_UI | SEE_MASK_DOENVSUBST;
				lpExecInfo.nShow = SW_SHOWNORMAL;

				// build an appropriate set of command-line
				// options to have the installer install in
				// portable mode local to the wacup install
				wchar_t cl[MAX_PATH] = {L"/silent"};
				lpExecInfo.lpParameters = cl;
				lpExecInfo.lpDirectory = folder;

				ShellExecuteEx(&lpExecInfo);
				if (lpExecInfo.hProcess != NULL)
				{
					WaitForSingleObject(lpExecInfo.hProcess, INFINITE);
					GetExitCodeProcess(lpExecInfo.hProcess, &exit_code);
					::CloseHandle(lpExecInfo.hProcess);
				}
			}
			else
			{
				exit_code = (DWORD)-2;
			}

			wchar_t message[256] = { 0 };
			if (exit_code)
			{
				_snwprintf(message, ARRAYSIZE(message), WASABI_API_LNGSTRINGW(IDS_DIRECTX_INSTALL_FAILED), exit_code);
			}
			else
			{
				WASABI_API_LNGSTRINGW_BUF(IDS_DIRECTX_INSTALL_SUCCEEDED, message, ARRAYSIZE(message));
			}
			TimedMessageBox(hwnd, message, WASABI_API_LNGSTRINGW(IDS_DIRECTX_INSTALL_ISSUE),
							MB_OK|MB_SETFOREGROUND|MB_TOPMOST|MB_TASKMODAL|(exit_code ?
							MB_ICONEXCLAMATION : MB_ICONINFORMATION), 5000);

			SHFILEOPSTRUCT FileStruct = { hwnd, FO_DELETE, folder, 0,
										 FOF_ALLOWUNDO | FOF_NOCONFIRMATION |
										 FOF_SILENT | FOF_RENAMEONCOLLISION, 0 };
			SHFileOperation(&FileStruct);
		}
	}

#if 0
    const wchar_t szUrl[] = L"https://www.microsoft.com/download/details.aspx?id=35";
    const intptr_t ret = myOpenURL(NULL, szUrl);
    if (ret <= 32)
    {
        wchar_t buf[1024] = {0};
        switch(ret)
        {
        case SE_ERR_FNF:
        case SE_ERR_PNF:
            _snwprintf(buf, ARRAYSIZE(buf), WASABI_API_LNGSTRINGW(IDS_URL_COULD_NOT_OPEN), szUrl);
            break;
        case SE_ERR_ACCESSDENIED:
        case SE_ERR_SHARE:
            _snwprintf(buf, ARRAYSIZE(buf), WASABI_API_LNGSTRINGW(IDS_ACCESS_TO_URL_WAS_DENIED), szUrl);
            break;
        case SE_ERR_NOASSOC:
            _snwprintf(buf, ARRAYSIZE(buf), WASABI_API_LNGSTRINGW(IDS_ACCESS_TO_URL_FAILED_DUE_TO_NO_ASSOC), szUrl);
            break;
        default:
            _snwprintf(buf, ARRAYSIZE(buf), WASABI_API_LNGSTRINGW(IDS_ACCESS_TO_URL_FAILED_CODE_X), szUrl, ret);
            break;
        }
        TimedMessageBox(hwnd, buf, WASABI_API_LNGSTRINGW(IDS_ERROR_OPENING_URL),
						MB_OK|MB_SETFOREGROUND|MB_TOPMOST|MB_TASKMODAL, 2000);
    }
#endif
}

void MissingDirectX(HWND hwnd)
{
    // DIRECTX MISSING OR CORRUPT -> PROMPT TO GO TO WEB.
	wchar_t title[128] = {0};
    if (TimedMessageBox(hwnd, 
        #ifndef D3D_SDK_VERSION
            --- error; you need to #include <d3d9.h> ---
        #endif
        #if (D3D_SDK_VERSION==120)
            // plugin was *built* using the DirectX 9.0 sdk, therefore, 
            // the dx9.0 runtime is missing or corrupt
            "Failed to initialize DirectX 9.0 or later.\n"
            "Milkdrop requires d3dx9_31.dll to be installed.\n"
            "\n"
            "Would you like to be taken to:\n"
			"https://www.microsoft.com/download/details.aspx?id=35,\n"
            "where you can update DirectX 9.0?\n"
            XXXXXXX
        #else
            // plugin was *built* using some other version of the DirectX9 sdk, such as 
            // 9.1b; therefore, we don't know exactly what version to tell them they need
            // to install; so we ask them to go get the *latest* version.
            WASABI_API_LNGSTRINGW(IDS_DIRECTX_MISSING_OR_CORRUPT_TEXT)
        #endif
        ,
		WASABI_API_LNGSTRINGW_BUF(IDS_DIRECTX_MISSING_OR_CORRUPT,
		title, ARRAYSIZE(title)), MB_YESNO|MB_SETFOREGROUND|MB_TOPMOST|
		MB_TASKMODAL|MB_DEFBUTTON1|MB_ICONEXCLAMATION, 5000) == IDYES)
	{
        DownloadDirectX(hwnd);
	}
}

#ifdef LEGACY_DESKTOP_MODE
void GetDesktopFolder(wchar_t *szDesktopFolder) // should be MAX_PATH len.
{
    // returns the path to the desktop folder, WITHOUT a trailing backslash.
    szDesktopFolder[0] = 0;
	ITEMIDLIST pidl = {0};
    if (!SHGetPathFromIDList(&pidl, szDesktopFolder))
        szDesktopFolder[0] = 0;
}

void ExecutePidl(LPITEMIDLIST pidl, wchar_t *szPathAndFile, wchar_t *szWorkingDirectory, HWND hWnd)
{
    // This function was based on code by Jeff Prosise.

    // Note: for some reason, ShellExecuteEx fails when executing
    // *shortcuts* (.lnk files) from the desktop, using their PIDLs.  
    // So, if that fails, we try again w/the plain old text filename 
    // (szPathAndFile).

    wchar_t szVerb[] = L"open", szFilename2[MAX_PATH] = {0};
    _snwprintf(szFilename2, ARRAYSIZE(szFilename2), L"%s.lnk", szPathAndFile);

    // -without the "no-verb" pass,
    //   certain icons still don't work (like shortcuts
    //   to IE, VTune...)
    // -without the "context menu" pass,
    //   certain others STILL don't work (Netscape...) 
    // -without the 'ntry' pass, shortcuts (to folders/files)
    //   don't work
    for (int verb_pass=0; verb_pass<2; verb_pass++)
    {
        for (int ntry=0; ntry<3; ntry++)
        {
            for (int context_pass=0; context_pass<2; context_pass++)
            {
                SHELLEXECUTEINFO sei = { sizeof(sei) };
                sei.hwnd = hWnd;
                sei.fMask = SEE_MASK_FLAG_NO_UI;
                if (context_pass==1)
                    sei.fMask |= SEE_MASK_INVOKEIDLIST;
                sei.lpVerb = (verb_pass) ? NULL : szVerb;
                sei.lpDirectory = szWorkingDirectory;
                sei.nShow = SW_SHOWNORMAL;
        
                if (ntry==0)
                {
                    // this case works for most non-shortcuts
                    sei.fMask |= SEE_MASK_IDLIST;
                    sei.lpIDList = pidl;
                }
                else if (ntry==1)
                {
                    // this case is required for *shortcuts to folders* to work
                    sei.lpFile = szPathAndFile;
                }
                else if (ntry==2)
                {
                    // this case is required for *shortcuts to files* to work
                    sei.lpFile = szFilename2;
                }

                if (ShellExecuteEx(&sei))
                    return;
            }
        }
    }
}

LPCONTEXTMENU2 g_pIContext2or3;

LRESULT CALLBACK HookWndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp,
							 UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
   //UINT uItem;
   //TCHAR szBuf[MAX_PATH] = {0};

   switch (msg) 
   { 
   case WM_DRAWITEM:
   case WM_MEASUREITEM:
      if(wp) break; // not menu related
   case WM_INITMENUPOPUP:
      g_pIContext2or3->HandleMenuMsg(msg, wp, lp);
      return (msg==WM_INITMENUPOPUP ? 0 : TRUE); // handled

   /*case WM_MENUSELECT:
      // if this is a shell item, get its descriptive text
      uItem = (UINT) LOWORD(wp);   
      if(0 == (MF_POPUP & HIWORD(wp)) && uItem >= 1 && uItem <= 0x7fff) 
      {
         g_pIContext2or3->GetCommandString(uItem-1, GCS_HELPTEXT,
            NULL, szBuf, sizeof(szBuf)/sizeof(szBuf[0]) );

         // set the status bar text
         ((CFrameWnd*)(AfxGetApp()->m_pMainWnd))->SetMessageText(szBuf);
         return 0;
      }
      break;*/

	default:
		break;
	}

	// for all untreated messages, call the original wndproc
	return DefSubclass(hWnd, msg, wp, lp);
}

BOOL DoExplorerMenu (HWND hwnd, LPITEMIDLIST pidlMain, POINT point)
{
    LPMALLOC pMalloc;
    LPSHELLFOLDER psfFolder, psfNextFolder;
    LPITEMIDLIST pidlItem, pidlNextItem;
    LPCONTEXTMENU pContextMenu;
    CMINVOKECOMMANDINFO ici;
    UINT nCount;
    BOOL bResult;

    //
    // Get pointers to the shell's IMalloc interface and the desktop's
    // IShellFolder interface.
    //
    bResult = FALSE;

    if (!SUCCEEDED (SHGetMalloc (&pMalloc)))
		return bResult;

    if (!SUCCEEDED (SHGetDesktopFolder (&psfFolder))) {
        pMalloc->Release();
        return bResult;
    }

    if (nCount = GetItemCount (pidlMain)) // nCount must be > 0
    {
        //
        // Initialize psfFolder with a pointer to the IShellFolder
        // interface of the folder that contains the item whose context
        // menu we're after, and initialize pidlItem with a pointer to
        // the item's item ID. If nCount > 1, this requires us to walk
        // the list of item IDs stored in pidlMain and bind to each
        // subfolder referenced in the list.
        //
        pidlItem = pidlMain;

        while (--nCount) {
            //
            // Create a 1-item item ID list for the next item in pidlMain.
            //
            pidlNextItem = DuplicateItem (pMalloc, pidlItem);
            if (pidlNextItem == NULL) {
                psfFolder->Release();
                pMalloc->Release();
                return bResult;
            }

            //
            // Bind to the folder specified in the new item ID list.
            //
            if (!SUCCEEDED (psfFolder->BindToObject(pidlNextItem, NULL, IID_IShellFolder, (void**)&psfNextFolder)))  // modified by RG
            {
                pMalloc->Free(pidlNextItem);
                psfFolder->Release();
                pMalloc->Release();
                return bResult;
            }

            //
            // Release the IShellFolder pointer to the parent folder
            // and set psfFolder equal to the IShellFolder pointer for
            // the current folder.
            //
            psfFolder->Release();
            psfFolder = psfNextFolder;

            //
            // Release the storage for the 1-item item ID list we created
            // just a moment ago and initialize pidlItem so that it points
            // to the next item in pidlMain.
            //
            pMalloc->Free(pidlNextItem);
            pidlItem = GetNextItem (pidlItem);
        }

        //
        // Get a pointer to the item's IContextMenu interface and call
        // IContextMenu::QueryContextMenu to initialize a context menu.
        //
        LPITEMIDLIST *ppidl = &pidlItem;
        if (SUCCEEDED (psfFolder->GetUIObjectOf(hwnd, 1, (LPCITEMIDLIST*)ppidl, IID_IContextMenu, NULL, (void**)&pContextMenu)))   // modified by RG
        {
            // try to see if we can upgrade to an IContextMenu3 
            // or IContextMenu2 interface pointer:
            int level = 1;
            void *pCM = NULL;
            if (pContextMenu->QueryInterface(IID_IContextMenu3, &pCM) == NOERROR)
            {
                pContextMenu->Release();
                pContextMenu = (LPCONTEXTMENU)pCM;
                level = 3;
            }
            else if (pContextMenu->QueryInterface(IID_IContextMenu2, &pCM) == NOERROR)
            {
                pContextMenu->Release();
                pContextMenu = (LPCONTEXTMENU)pCM;
                level = 2;
            }

            HMENU hMenu = CreatePopupMenu ();
            if (SUCCEEDED (pContextMenu->QueryContextMenu(hMenu, 0, 1, 0x7FFF, CMF_EXPLORE))) 
            {
                ClientToScreen (hwnd, &point);

                // install the subclassing "hook", for versions 2 or 3
                if (level >= 2) 
                {
					Subclass(hwnd, HookWndProc);
                    g_pIContext2or3 = (LPCONTEXTMENU2)pContextMenu; // cast ok for ICMv3
                }
                else 
                {
                    g_pIContext2or3 = NULL;
                }

                //
                // Display the context menu.
                //
                UINT nCmd = TrackPopupMenu (hMenu, TPM_LEFTALIGN |
                    TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD,
                    point.x, point.y, 0, hwnd, NULL);

                // restore old wndProc
				UnSubclass(hwnd, HookWndProc);

                //
                // If a command was selected from the menu, execute it.
                //
                if (nCmd >= 1 && nCmd <= 0x7fff) 
                {
                    memset(&ici, 0, sizeof(ici));
                    ici.cbSize          = sizeof (CMINVOKECOMMANDINFO);
                    //ici.fMask           = 0;
                    ici.hwnd            = hwnd;
                    ici.lpVerb          = MAKEINTRESOURCEA(nCmd - 1);
                    //ici.lpParameters    = NULL;
                    //ici.lpDirectory     = NULL;
                    ici.nShow           = SW_SHOWNORMAL;
                    //ici.dwHotKey        = 0;
                    //ici.hIcon           = NULL;

                    if (SUCCEEDED ( pContextMenu->InvokeCommand (&ici)))
                        bResult = TRUE;
                }
                /*else if (nCmd) 
                {
                    PostMessage(hwnd, WM_COMMAND, nCmd, NULL); // our command
                }*/
            }
            DestroyMenu (hMenu);
            pContextMenu->Release();
        }
    }

    //
    // Clean up and return.
    //
    psfFolder->Release();
    pMalloc->Release();

    return bResult;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  Note: a special thanks goes out to Jeff Prosise for writing & publishing 
//        the following code!
//  
//  FUNCTION:       GetItemCount
//
//  DESCRIPTION:    Computes the number of item IDs in an item ID list.
//
//  INPUT:          pidl = Pointer to an item ID list.
//
//  RETURNS:        Number of item IDs in the list.
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

UINT GetItemCount (LPITEMIDLIST pidl)
{
    USHORT nLen;
    UINT nCount;

    nCount = 0;
    while ((nLen = pidl->mkid.cb) != 0) {
        pidl = GetNextItem (pidl);
        ++nCount;
    }
    return nCount;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  Note: a special thanks goes out to Jeff Prosise for writing & publishing 
//        the following code!
//  
//  FUNCTION:       GetNextItem
//
//  DESCRIPTION:    Finds the next item in an item ID list.
//
//  INPUT:          pidl = Pointer to an item ID list.
//
//  RETURNS:        Pointer to the next item.
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

LPITEMIDLIST GetNextItem (LPITEMIDLIST pidl)
{
    USHORT nLen;

    if ((nLen = pidl->mkid.cb) == 0)
        return NULL;
    
    return (LPITEMIDLIST) (((LPBYTE) pidl) + nLen);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  Note: a special thanks goes out to Jeff Prosise for writing & publishing 
//        the following code!
//  
//  FUNCTION:       DuplicateItem
//
//  DESCRIPTION:    Makes a copy of the next item in an item ID list.
//
//  INPUT:          pMalloc = Pointer to an IMalloc interface.
//                  pidl    = Pointer to an item ID list.
//
//  RETURNS:        Pointer to an ITEMIDLIST containing the copied item ID.
//
//  NOTES:          It is the caller's responsibility to free the memory
//                  allocated by this function when the item ID is no longer
//                  needed. Example:
//
//                    pidlItem = DuplicateItem (pMalloc, pidl);
//                      .
//                      .
//                      .
//                    pMalloc->lpVtbl->Free (pMalloc, pidlItem);
//
//                  Failure to free the ITEMIDLIST will result in memory
//                  leaks.
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

LPITEMIDLIST DuplicateItem (LPMALLOC pMalloc, LPITEMIDLIST pidl)
{
    USHORT nLen;
    LPITEMIDLIST pidlNew;

    nLen = pidl->mkid.cb;
    if (nLen == 0)
        return NULL;

    pidlNew = (LPITEMIDLIST) pMalloc->Alloc (
        nLen + sizeof (USHORT));
    if (pidlNew == NULL)
        return NULL;

    CopyMemory (pidlNew, pidl, nLen);
    *((USHORT*) (((LPBYTE) pidlNew) + nLen)) = 0;

    return pidlNew;
}
#endif

//----------------------------------------------------------------------
// A special thanks goes out to Jeroen-bart Engelen (Yeep) for providing
// his source code for getting the position & label information for all 
// the icons on the desktop, as found below.  See his article at 
// http://www.digiwar.com/scripts/renderpage.php?section=2&subsection=2
//----------------------------------------------------------------------

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
	HWND p = FindWindowEx(hwnd, NULL, L"SHELLDLL_DefView", NULL);
	HWND* ret = (HWND*)lParam;

	if (p)
	{
		// Gets the WorkerW Window after the current one.
		*ret = FindWindowEx(NULL, hwnd, L"WorkerW", NULL);
	}
	return true;
}

HWND GetDesktopWorkerWindow(HWND progman)
{
	// this will cause a WorkerW to be created (if not already) behind the desktop
	// icons so we don't have to mess around with the likes of hooks to do it all!
	if (IsWindow(progman))
	{
		DWORD_PTR ret = 0;
		SendMessageTimeout(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, &ret);
	}

	// the WorkerW to then be used will be the next sibling after the parent of the
	// SHELLDLL_DefView so we need to enumerate the windows to find what we need :)
	HWND wallpaper_hwnd = NULL;
	EnumWindows(EnumWindowsProc, (LPARAM)&wallpaper_hwnd);
	return wallpaper_hwnd;
}

#ifdef LEGACY_DESKTOP_MODE
void FindDesktopWindows(HWND *desktop_progman, HWND *desktopview_wnd, HWND *listview_wnd)
#else
void FindDesktopWindows(HWND *desktop_progman, HWND *desktopview_wnd)
#endif
{
	*desktopview_wnd = NULL;
#ifdef LEGACY_DESKTOP_MODE
	*listview_wnd = NULL;
#endif

	*desktop_progman = FindWindow(NULL, (TEXT("Program Manager")));
	if(*desktop_progman == NULL)
	{
		//MessageBox(NULL, "Unable to get the handle to the Program Manager.", "Fatal error", MB_OK|MB_ICONERROR);
		return;
	}

	*desktopview_wnd = GetDesktopWorkerWindow(*desktop_progman);
	if (IsWindow(*desktopview_wnd))
	{
		return;
	}

#ifdef LEGACY_DESKTOP_MODE
	// if we didn't get a window above then we're going to have to use the older method
	*desktopview_wnd = FindWindowEx(*desktop_progman, NULL, TEXT("SHELLDLL_DefView"), NULL);
	if(*desktopview_wnd == NULL)
	{
		//MessageBox(NULL, "Unable to get the handle to the desktopview.", "Fatal error", MB_OK|MB_ICONERROR);
		return;
	}
	
	// Thanks ef_ef_ef@yahoo.com for pointing out this works in NT 4 and not the way I did it originally.
	*listview_wnd = FindWindowEx(*desktopview_wnd, NULL, TEXT("SysListView32"), NULL);
	if(*listview_wnd == NULL)
	{
		//MessageBox(NULL, "Unable to get the handle to the folderview.", "Fatal error", MB_OK|MB_ICONERROR);
		return;
	}
#endif
}

//----------------------------------------------------------------------

int GetDesktopIconSize()
{
    int ret = 32;

    // reads the key: HKEY_CURRENT_USER\Control Panel, Desktop\WindowMetrics\Shell Icon Size
    unsigned char buf[64] = {0};
    unsigned long len = sizeof(buf);
    DWORD type;
    HKEY key;

    if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Control Panel\\Desktop\\WindowMetrics"), 0, KEY_READ, &key))
    {
        if (ERROR_SUCCESS == RegQueryValueEx(key, TEXT("Shell Icon Size"), NULL, &type, (unsigned char*)buf, &len) &&
            type == REG_SZ)
        {
            int x = atoi((char*)buf);
            if (x>0 && x<=128)
                ret = x;
        }

        RegCloseKey(key);
    }

    return ret;
}

//----------------------------------------------------------------------

// handy functions for populating Combo Boxes:
int SelectItemByValue(HWND ctrl, DWORD value)
{
    int count = SendMessage(ctrl, CB_GETCOUNT, 0, 0);
	for (int i=0; i<count; i++)
	{
		DWORD value_i = SendMessage( ctrl, CB_GETITEMDATA, i, 0);
		if (value_i == value)
        {
			SendMessage( ctrl, CB_SETCURSEL, i, 0);
            return i;
        }
	}
    return -1;
}

bool ReadCBValue(HWND hwnd, DWORD ctrl_id, int* pRetValue)
{
    if (!pRetValue)
        return false;
    HWND ctrl = GetDlgItem( hwnd, ctrl_id );
	int t = SendMessage( ctrl, CB_GETCURSEL, 0, 0);
	if (t == CB_ERR) 
        return false;
    *pRetValue = (int)SendMessage( ctrl, CB_GETITEMDATA, t, 0);
    return true;
}

D3DXCREATEFONTW pCreateFontW=0;
D3DXMATRIXMULTIPLY pMatrixMultiply=0;
D3DXMATRIXTRANSLATION pMatrixTranslation=0;
D3DXMATRIXSCALING pMatrixScaling=0;
D3DXMATRIXROTATION pMatrixRotationX=0, pMatrixRotationY=0, pMatrixRotationZ=0;
D3DXCREATETEXTUREFROMFILEEXW pCreateTextureFromFileExW=0;
D3DXMATRIXORTHOLH pMatrixOrthoLH = 0;
D3DXCOMPILESHADER pCompileShader=0;
D3DXMATRIXLOOKATLH pMatrixLookAtLH=0;
D3DXCREATETEXTURE pCreateTexture=0;
//----------------------------------------------------------------------
HINSTANCE FindD3DX9(HWND winamp)
{
	HINSTANCE d3dx9 = GetD3DX9();///*NULL;/*/(HINSTANCE)SendMessage(winamp, WM_WA_IPC, 0, IPC_GET_D3DX9)/**/;
	if (d3dx9)
	{
		pCreateFontW = (D3DXCREATEFONTW) GetProcAddress(d3dx9,"D3DXCreateFontW");
		pMatrixMultiply = (D3DXMATRIXMULTIPLY) GetProcAddress(d3dx9,"D3DXMatrixMultiply");
		pMatrixTranslation = (D3DXMATRIXTRANSLATION)GetProcAddress(d3dx9,"D3DXMatrixTranslation");
		pMatrixScaling = (D3DXMATRIXSCALING)GetProcAddress(d3dx9,"D3DXMatrixScaling");
		pMatrixRotationX = (D3DXMATRIXROTATION)GetProcAddress(d3dx9,"D3DXMatrixRotationX");
		pMatrixRotationY = (D3DXMATRIXROTATION)GetProcAddress(d3dx9,"D3DXMatrixRotationY");
		pMatrixRotationZ = (D3DXMATRIXROTATION)GetProcAddress(d3dx9,"D3DXMatrixRotationZ");
		pCreateTextureFromFileExW = (D3DXCREATETEXTUREFROMFILEEXW)GetProcAddress(d3dx9,"D3DXCreateTextureFromFileExW");
		pMatrixOrthoLH = (D3DXMATRIXORTHOLH)GetProcAddress(d3dx9,"D3DXMatrixOrthoLH");
		pCompileShader = (D3DXCOMPILESHADER)GetProcAddress(d3dx9,"D3DXCompileShader");
		pMatrixLookAtLH = (D3DXMATRIXLOOKATLH)GetProcAddress(d3dx9,"D3DXMatrixLookAtLH");
		pCreateTexture = (D3DXCREATETEXTURE)GetProcAddress(d3dx9,"D3DXCreateTexture");
	}

	return d3dx9;
}

/*LRESULT GetWinampVersion(HWND winamp)
{
	static LRESULT version=0;
	if (!version)
		version=SendMessage(winamp,WM_WA_IPC,0,0);
	return version;
}*/

/*void* GetTextResource(UINT id, int no_fallback){
	void* data = 0;
	HINSTANCE hinst = WASABI_API_LNG_HINST;
	HRSRC rsrc = FindResource(hinst,MAKEINTRESOURCE(id),TEXT("TEXT"));
	if(!rsrc && !no_fallback) rsrc = FindResource((hinst = WASABI_API_ORIG_HINST),MAKEINTRESOURCE(id),TEXT("TEXT"));
	if(rsrc){
	HGLOBAL resourceHandle = LoadResource(hinst,rsrc);
		data = LockResource(resourceHandle);
	}
	return data;
}*/