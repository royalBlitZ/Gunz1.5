#include "stdafx.h"
#include <windows.h>
#include "MDebug.h"
#include "RealSpace2.h"
#include "RParticleSystem.h"
#include "RFont.h"
#include "RMeshUtil.h"
#include <chrono>
#include <thread>
#pragma comment(lib,"winmm.lib")

#define RTOOLTIP_GAP 700
static DWORD g_last_mouse_move_time = 0;
static bool g_tool_tip = false;

bool IsToolTipEnable() {
	return g_tool_tip;
}

_NAMESPACE_REALSPACE2_BEGIN

//RMODEPARAMS g_ModeParams={ 640,480,false,RPIXELFORMAT_565 };

extern HWND g_hWnd;

bool g_bActive;
extern bool g_bFixedResolution;

RECT g_rcWindowBounds;
WNDPROC	g_WinProc=NULL;
RFFUNCTION g_pFunctions[RF_ENDOFRFUNCTIONTYPE] = {NULL, };
//LPD3DXFONT g_lpFont=NULL;

//extern LPDIRECT3DTEXTURE9 g_lpTexture ;
//extern LPDIRECT3DSURFACE9 g_lpSurface ;

extern int gNumTrisRendered;

#ifdef _USE_GDIPLUS		// GDI Plus 
	#include "unknwn.h"
	#include "gdiplus.h"

	Gdiplus::GdiplusStartupInput	g_gdiplusStartupInput;
	ULONG_PTR 						g_gdiplusToken = NULL;
#endif

void RSetFunction(RFUNCTIONTYPE ft,RFFUNCTION pfunc)
{
	g_pFunctions[ft]=pfunc;
}

bool RIsActive()
{
	return GetActiveWindow()==g_hWnd;
//	return g_bActive;
}

void RFrame_Create()
{
#ifdef _USE_GDIPLUS
	Gdiplus::GdiplusStartup(&g_gdiplusToken, &g_gdiplusStartupInput, NULL);
#endif
	GetWindowRect(g_hWnd,&g_rcWindowBounds);
}

/*
void RFrame_InitFont()
{
	LOGFONT lFont;
	
	ZeroMemory(&lFont, sizeof(LOGFONT));
	lFont.lfHeight = 16;
	lFont.lfWidth = 0;
	lFont.lfWeight = FW_BOLD; 
	lFont.lfCharSet = SHIFTJIS_CHARSET;
	lFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
	lFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	lFont.lfQuality = PROOF_QUALITY;
	lFont.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
	strcpy(lFont.lfFaceName, "FONTa9");
	
	SAFE_RELEASE(g_lpFont);
	HRESULT hr=D3DXCreateFontIndirect(RGetDevice(), &lFont, &g_lpFont);
	_ASSERT(hr==D3D_OK);
	mlog("font restored.\n");
}
*/

void RFrame_Init()
{
//	RFrame_InitFont();
}

void RFrame_Restore()
{
	//if( IsFixedResolution() )
	//	FixedResolutionRenderStart();

//	RFrame_InitFont();
	RParticleSystem::Restore();
	if(g_pFunctions[RF_RESTORE])
		g_pFunctions[RF_RESTORE](NULL);
}

void RFrame_Destroy()
{
	//if( IsFixedResolution() )
	//{
	//	FixedResolutionRenderEnd();
	//	FixedResolutionRenderInvalidate();
	//}

//	SAFE_RELEASE(g_lpFont);
	if(g_pFunctions[RF_DESTROY])
		g_pFunctions[RF_DESTROY](NULL);

	mlog("Rframe_destory::closeDisplay\n");
	RCloseDisplay();

#ifdef _USE_GDIPLUS
	Gdiplus::GdiplusShutdown(g_gdiplusToken);
#endif
}

void RFrame_Invalidate()
{
	//if( IsFixedResolution() )
	//{
	//	FixedResolutionRenderEnd();
	//	FixedResolutionRenderInvalidate();
	//}
//	SAFE_RELEASE(g_lpFont);

//	RFontTexture::m_dwStateBlock=NULL;
	RParticleSystem::Invalidate();
	if(g_pFunctions[RF_INVALIDATE])
		g_pFunctions[RF_INVALIDATE](NULL);
}

void RFrame_Update()
{
	if (g_pFunctions[RF_UPDATE])
		g_pFunctions[RF_UPDATE](NULL);
}


void RFrame_Render()
{
	if (!RIsActive() && RIsFullScreen()) return;


	RRESULT isOK = RIsReadyToRender();
	if (isOK == R_NOTREADY)
		return;
	else if (isOK == R_RESTORED)
	{
		RMODEPARAMS ModeParams = { RGetScreenWidth(),RGetScreenHeight(),RIsFullScreen(),RGetPixelFormat() };
		RResetDevice(&ModeParams);
		mlog("devices Restored. \n");
	}

	if (timeGetTime() > g_last_mouse_move_time + RTOOLTIP_GAP)
		g_tool_tip = true;

	if (g_pFunctions[RF_RENDER])
		g_pFunctions[RF_RENDER](NULL);

	RGetDevice()->SetStreamSource(0, NULL, 0, 0);
	RGetDevice()->SetIndices(0);
	RGetDevice()->SetTexture(0, NULL);
	RGetDevice()->SetTexture(1, NULL);
}

void RFrame_ToggleFullScreen()
{
	RMODEPARAMS ModeParams = { RGetScreenWidth(),RGetScreenHeight(),RIsFullScreen(),RGetPixelFormat() };

	if (!ModeParams.bFullScreen)									// 윈도우 -> 풀스크린일때 저장하고..
		GetWindowRect(g_hWnd, &g_rcWindowBounds);

	ModeParams.bFullScreen = !ModeParams.bFullScreen;
	RResetDevice(&ModeParams);

	if (!ModeParams.bFullScreen)									// 풀스크린 -> 윈도우로 갈때 복구한다.
	{
		SetWindowPos(g_hWnd, HWND_NOTOPMOST,
			g_rcWindowBounds.left, g_rcWindowBounds.top,
			(g_rcWindowBounds.right - g_rcWindowBounds.left),
			(g_rcWindowBounds.bottom - g_rcWindowBounds.top),
			SWP_SHOWWINDOW);
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	
    // Handle messages
    switch (message)
    {

#ifndef _PUBLISH
		case WM_SYSCHAR:
			if(wParam==VK_RETURN)
				RFrame_ToggleFullScreen();
			return 0;
#endif
		case WM_SYSCOMMAND:
			{
				switch (wParam)
				{
					// Trap ALT so it doesn't pause the app
					case SC_PREVWINDOW :
					case SC_NEXTWINDOW :
					case SC_KEYMENU :
					{
						return 0;
					}
					break;
				}
			}
			break;
        
		case WM_ACTIVATEAPP:
		{
			if (wParam == TRUE) {
				if (g_pFunctions[RF_ACTIVATE])
					g_pFunctions[RF_ACTIVATE](NULL);
				g_bActive = TRUE;
			} else {
				if (g_pFunctions[RF_DEACTIVATE])
					g_pFunctions[RF_DEACTIVATE](NULL);

				if (RIsFullScreen()) {
					ShowWindow(hWnd, SW_MINIMIZE);
					UpdateWindow(hWnd);
				}
				g_bActive = FALSE;
			}
		}
		break;

		case WM_MOUSEMOVE:
			{
				g_last_mouse_move_time = timeGetTime();
				g_tool_tip = false;
			}
			break;

		case WM_CLOSE:
		{
			RFrame_Destroy();
            PostQuitMessage(0);
			return 0;
		}
		break;
    }
    return g_WinProc(hWnd, message, wParam, lParam);
}

#ifndef _PUBLISH

#define __BP(i,n)	MBeginProfile(i,n);
#define __EP(i)		MEndProfile(i);

#else

#define __BP(i,n) ;
#define __EP(i) ;

#endif

int RMain(const char *AppName, HINSTANCE this_inst, HINSTANCE prev_inst, LPSTR cmdline, int cmdshow, RMODEPARAMS *pModeParams, WNDPROC winproc, WORD nIconResID )
{
	g_WinProc=winproc ? winproc : DefWindowProc;

	// make a window
    WNDCLASS    wc;
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = sizeof(DWORD);
    wc.hInstance = this_inst;
	wc.hIcon = LoadIcon( this_inst, MAKEINTRESOURCE(nIconResID));
    wc.hCursor = 0;//LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "RealSpace2";
	if(!RegisterClass(&wc)) return FALSE;

	DWORD dwStyle = pModeParams->bFullScreen ? WS_POPUP | WS_SYSMENU : WS_POPUP | WS_CAPTION | WS_SYSMENU;
	g_hWnd = CreateWindow("RealSpace2", AppName, dwStyle, CW_USEDEFAULT, CW_USEDEFAULT,
		pModeParams->nWidth, pModeParams->nHeight, NULL, NULL, this_inst, NULL);

	// initialize realspace2

	RAdjustWindow(pModeParams);

	while(ShowCursor(FALSE)>0);
//	ShowCursor(TRUE);	// RAONHAJE Mouse Cursor HardwareDraw

//	RFrame_Create();
//
//	ShowWindow(g_hWnd,SW_SHOW);
//	if(!RInitDisplay(g_hWnd,pModeParams))
//	{
//		mlog("can't init display\n");
//		return -1;
//	}
//
//	//RBeginScene();
//	RGetDevice()->Clear(0 , NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0 );
////	REndScene();
//	RFlip();

	
//	RGetDevice()->ShowCursor( TRUE );	// RAONHAJE Mouse Cursor HardwareDraw

	return 0;
}

void RFrame_UpdateRender()
{
	__BP(5006,"RMain::Run");


	RFrame_Update();
	RFrame_Render();

	__BP(5007,"RMain::RFlip");
	
	if (!RFlip())
	{
		RIsReadyToRender();
	}
	__EP(5007);

	__EP(5006);
}

int RRun()
{
	if(g_pFunctions[RF_CREATE])
	{
		if(g_pFunctions[RF_CREATE](NULL)!=R_OK)
		{
			RFrame_Destroy();
			return -1;
		}
	}

	RFrame_Init();

	// message loop
    // Now we're ready to recieve and process Windows messages.
    BOOL bGotMsg;
    MSG  msg;
//    PeekMessage( &msg, NULL, 0U, 0U, PM_NOREMOVE );
    do
    {
        // Use PeekMessage() if the app is active, so we can use idle time to
        // render the scene. Else, use GetMessage() to avoid eating CPU time.
//        if( g_bActive )
            bGotMsg = PeekMessage( &msg, NULL, 0U, 0U, PM_REMOVE );

//        else
//            bGotMsg = GetMessage( &msg, NULL, 0U, 0U );

		if( bGotMsg )
		{
			// WM_USER 이상의 메시지는 처리하지 않는다. 이것은 일본어 IME의 팝업메뉴 호출을 막기 위한 처리다.
			// (팝업메뉴가 코드진행을 멈추게하는 특성을 유저들이 무적어뷰즈로 악용하기 때문)
			// WM_USER+25는 건즈내부에서 사용하고 있으므로 WM_USER+25가 넘어간 메시지부터는 버린다.
			if (msg.message <= WM_USER +25)
			{
				TranslateMessage( &msg );
				DispatchMessage( &msg );
			}
        }
		else
		{
			///TODO: decouple gameinput
			RFrame_UpdateRender();
	/*		if (g_nFrameLimitValue != 0)
			{
				auto currTime = std::chrono::high_resolution_clock::now();
				auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(currTime - startTime).count();
				while(static_cast<double>(timeDiff) < static_cast<double>(1000) / static_cast<double>(g_nFrameLimitValue))
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					++timeDiff;
				}
				startTime = currTime;
			}*/
		}

		if(!g_bActive)
			Sleep(10);
    }while( WM_QUIT != msg.message  );
    return (INT)msg.wParam;
}


int RInitD3D( RMODEPARAMS* pModeParams )
{
	RFrame_Create();

	ShowWindow(g_hWnd, SW_SHOW);
	if(!RInitDisplay(g_hWnd,pModeParams))
	{
		mlog("can't init display\n");
		return -1;
	}

	//RBeginScene();
	RGetDevice()->Clear(0 , NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0 );
//	REndScene();
	RFlip();

	return 0;
}

_NAMESPACE_REALSPACE2_END
