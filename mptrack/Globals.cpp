/*
 * globals.cpp
 * -----------
 * Purpose: Implementation of various views of the tracker interface.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "Globals.h"
#include "Childfrm.h"
#include "Ctrl_com.h"
#include "Ctrl_gen.h"
#include "Ctrl_ins.h"
#include "Ctrl_pat.h"
#include "Ctrl_smp.h"
#include "ImageLists.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "TrackerSettings.h"
#include "../soundlib/mod_specifications.h"


OPENMPT_NAMESPACE_BEGIN


/////////////////////////////////////////////////////////////////////////////
// CModControlDlg

BEGIN_MESSAGE_MAP(CModControlDlg, CDialog)
	//{{AFX_MSG_MAP(CModControlDlg)
	ON_WM_SIZE()
#if !defined(MPT_BUILD_RETRO)
	ON_MESSAGE(WM_DPICHANGED, &CModControlDlg::OnDPIChanged)
#endif
	ON_MESSAGE(WM_MOD_UNLOCKCONTROLS,		&CModControlDlg::OnUnlockControls)
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTW, 0, 0xFFFF, &CModControlDlg::OnToolTipText)
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTA, 0, 0xFFFF, &CModControlDlg::OnToolTipText)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


CModControlDlg::CModControlDlg(CModControlView &parent, CModDoc &document) : m_modDoc(document), m_sndFile(document.GetSoundFile()), m_parent(parent)
{
}


CModControlDlg::~CModControlDlg()
{
	MPT_ASSERT(m_hWnd == nullptr);
}


BOOL CModControlDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	m_nDPIx = Util::GetDPIx(m_hWnd);
	m_nDPIy = Util::GetDPIy(m_hWnd);
	EnableToolTips(TRUE);
	return TRUE;
}


LRESULT CModControlDlg::OnDPIChanged(WPARAM wParam, LPARAM)
{
	m_nDPIx = LOWORD(wParam);
	m_nDPIy = HIWORD(wParam);
	return 0;
}


void CModControlDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);
	if (((nType == SIZE_RESTORED) || (nType == SIZE_MAXIMIZED)) && (cx > 0) && (cy > 0))
	{
		RecalcLayout();
	}
}


LRESULT CModControlDlg::OnModCtrlMsg(WPARAM wParam, LPARAM lParam)
{
	switch(wParam)
	{
	case CTRLMSG_SETVIEWWND:
		m_hWndView = (HWND)lParam;
		break;

	case CTRLMSG_ACTIVATEPAGE:
		OnActivatePage(lParam);
		break;

	case CTRLMSG_DEACTIVATEPAGE:
		OnDeactivatePage();
		break;

	case CTRLMSG_SETFOCUS:
		GetParentFrame()->SetActiveView(&m_parent);
		SetFocus();
		break;
	}
	return 0;
}


LRESULT CModControlDlg::SendViewMessage(UINT uMsg, LPARAM lParam) const
{
	if (m_hWndView)	return ::SendMessage(m_hWndView, WM_MOD_VIEWMSG, uMsg, lParam);
	return 0;
}


BOOL CModControlDlg::PostViewMessage(UINT uMsg, LPARAM lParam) const
{
	if (m_hWndView)	return ::PostMessage(m_hWndView, WM_MOD_VIEWMSG, uMsg, lParam);
	return FALSE;
}


INT_PTR CModControlDlg::OnToolHitTest(CPoint point, TOOLINFO* pTI) const
{
	INT_PTR nHit = CDialog::OnToolHitTest(point, pTI);
	if ((nHit >= 0) && (pTI))
	{
		if ((pTI->lpszText == LPSTR_TEXTCALLBACK) && (pTI->hwnd == m_hWnd))
		{
			CFrameWnd *pMDIParent = GetParentFrame();
			if (pMDIParent) pTI->hwnd = pMDIParent->m_hWnd;
		}
	}
	return nHit;
}


BOOL CModControlDlg::OnToolTipText(UINT nID, NMHDR* pNMHDR, LRESULT* pResult)
{
	CChildFrame *pChildFrm = (CChildFrame *)GetParentFrame();
	if (pChildFrm) return pChildFrm->OnToolTipText(nID, pNMHDR, pResult);
	if (pResult) *pResult = 0;
	return FALSE;
}


/////////////////////////////////////////////////////////////////////////////
// CModControlView

BOOL CModTabCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if (!pMainFrm) return FALSE;
	if (!CTabCtrl::Create(dwStyle, rect, pParentWnd, nID)) return FALSE;
	SendMessage(WM_SETFONT, (WPARAM)pMainFrm->GetGUIFont());
	SetImageList(&pMainFrm->m_MiscIcons);
	return TRUE;
}


BOOL CModTabCtrl::InsertItem(int nIndex, LPCTSTR pszText, LPARAM lParam, int iImage)
{
	TC_ITEM tci;
	tci.mask = TCIF_TEXT | TCIF_PARAM | TCIF_IMAGE;
	tci.pszText = const_cast<LPTSTR>(pszText);
	tci.lParam = lParam;
	tci.iImage = iImage;
	return CTabCtrl::InsertItem(nIndex, &tci);
}


LPARAM CModTabCtrl::GetItemData(int nIndex)
{
	TC_ITEM tci;
	tci.mask = TCIF_PARAM;
	tci.lParam = 0;
	if (!GetItem(nIndex, &tci)) return 0;
	return tci.lParam;
}


/////////////////////////////////////////////////////////////////////////////////
// CModControlView

IMPLEMENT_DYNCREATE(CModControlView, CView)

BEGIN_MESSAGE_MAP(CModControlView, CView)
	//{{AFX_MSG_MAP(CModControlView)
	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_NOTIFY(TCN_SELCHANGE, IDC_TABCTRL1,	&CModControlView::OnTabSelchange)
	ON_MESSAGE(WM_MOD_ACTIVATEVIEW,			&CModControlView::OnActivateModView)
	ON_MESSAGE(WM_MOD_CTRLMSG,				&CModControlView::OnModCtrlMsg)
	ON_MESSAGE(WM_MOD_GETTOOLTIPTEXT,		&CModControlView::OnGetToolTipText)
	ON_COMMAND(ID_EDIT_CUT,					&CModControlView::OnEditCut)
	ON_COMMAND(ID_EDIT_COPY,				&CModControlView::OnEditCopy)
	ON_COMMAND(ID_EDIT_PASTE,				&CModControlView::OnEditPaste)
	ON_COMMAND(ID_EDIT_MIXPASTE,			&CModControlView::OnEditMixPaste)
	ON_COMMAND(ID_EDIT_MIXPASTE_ITSTYLE,	&CModControlView::OnEditMixPasteITStyle)
	ON_COMMAND(ID_EDIT_FIND,				&CModControlView::OnEditFind)
	ON_COMMAND(ID_EDIT_FINDNEXT,			&CModControlView::OnEditFindNext)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CModDoc *CModControlView::GetDocument() const noexcept { return static_cast<CModDoc *>(m_pDocument); }

void CModControlView::OnInitialUpdate() // called first time after construct
{
	CView::OnInitialUpdate();
	CRect rect;

	CChildFrame *pParentFrame = (CChildFrame *)GetParentFrame();
	if (pParentFrame) m_hWndView = pParentFrame->GetHwndView();
	GetClientRect(&rect);
	m_TabCtrl.Create(WS_CHILD|WS_VISIBLE|TCS_FOCUSNEVER|TCS_FORCELABELLEFT, rect, this, IDC_TABCTRL1);
	UpdateView(UpdateHint().ModType());
	SetActivePage(Page::First);
}


void CModControlView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);
	if (((nType == SIZE_RESTORED) || (nType == SIZE_MAXIMIZED)) && (cx > 0) && (cy > 0))
	{
		RecalcLayout();
	}
}


void CModControlView::RecalcLayout()
{
	CRect rcClient;

	if (m_TabCtrl.m_hWnd == NULL) return;
	GetClientRect(&rcClient);
	if(CWnd *pDlg = GetCurrentControlDlg())
	{
		CRect rect = rcClient;
		m_TabCtrl.AdjustRect(FALSE, &rect);
		HDWP hdwp = BeginDeferWindowPos(2);
		DeferWindowPos(hdwp, m_TabCtrl.m_hWnd, NULL, rcClient.left, rcClient.top, rcClient.Width(), rcClient.Height(), SWP_NOZORDER);
		DeferWindowPos(hdwp, pDlg->m_hWnd, NULL, rect.left, rect.top, rect.Width(), rect.Height(), SWP_NOZORDER);
		EndDeferWindowPos(hdwp);
	} else
	{
		m_TabCtrl.MoveWindow(&rcClient);
	}
}


void CModControlView::OnUpdate(CView *, LPARAM lHint, CObject *pHint)
{
	UpdateView(UpdateHint::FromLPARAM(lHint), pHint);
}


void CModControlView::ForceRefresh()
{
	SetActivePage(GetActivePage());
}


CModControlDlg *CModControlView::GetCurrentControlDlg() const
{
	if(m_nActiveDlg >= Page::First && m_nActiveDlg < Page::MaxPages)
		return m_Pages[static_cast<size_t>(m_nActiveDlg)];
	else
		return nullptr;
}


bool CModControlView::SetActivePage(Page page, LPARAM lParam)
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	CModControlDlg *pDlg = nullptr;

	if(page == Page::Unknown)
		page = static_cast<Page>(m_TabCtrl.GetCurSel());

	const UINT nID = static_cast<UINT>(m_TabCtrl.GetItemData(static_cast<int>(page)));
	if(nID == 0)
		return false;

	switch(nID)
	{
		case IDD_CONTROL_COMMENTS:
			page = Page::Comments;
			break;
		case IDD_CONTROL_GLOBALS:
			page = Page::Globals;
			break;
		case IDD_CONTROL_PATTERNS:
			page = Page::Patterns;
			break;
		case IDD_CONTROL_SAMPLES:
			page = Page::Samples;
			break;
		case IDD_CONTROL_INSTRUMENTS:
			page = Page::Instruments;
			break;
		default:
			return false;
	}

	if(page < Page::First || page >= Page::MaxPages || !pMainFrm)
		return false;

	CModControlDlg *oldActiveDlg = GetCurrentControlDlg();
	if(oldActiveDlg)
		oldActiveDlg->GetSplitPosRef() = static_cast<CChildFrame *>(GetParentFrame())->GetSplitterHeight();

	if(page == m_nActiveDlg)
	{
		pDlg = oldActiveDlg;
		PostMessage(WM_MOD_CTRLMSG, CTRLMSG_ACTIVATEPAGE, lParam);
		return true;
	}
	if(oldActiveDlg)
	{
		OnModCtrlMsg(CTRLMSG_DEACTIVATEPAGE, 0);
		oldActiveDlg->ShowWindow(SW_HIDE);
	}
	if(m_Pages[static_cast<size_t>(page)]) // Ctrl window already created?
	{
		m_nActiveDlg = page;
		pDlg = m_Pages[static_cast<size_t>(page)];
	} else // Ctrl window is not created yet - creating one.
	{
		m_nActiveDlg = Page::Unknown;
		MPT_ASSERT_ALWAYS(GetDocument() != nullptr);
		switch(nID)
		{
		case IDD_CONTROL_COMMENTS:
			pDlg = new CCtrlComments(*this, *GetDocument());
			break;
		case IDD_CONTROL_GLOBALS:
			pDlg = new CCtrlGeneral(*this, *GetDocument());
			break;
		case IDD_CONTROL_PATTERNS:
			pDlg = new CCtrlPatterns(*this, *GetDocument());
			break;
		case IDD_CONTROL_SAMPLES:
			pDlg = new CCtrlSamples(*this, *GetDocument());
			break;
		case IDD_CONTROL_INSTRUMENTS:
			pDlg = new CCtrlInstruments(*this, *GetDocument());
			break;
		default:
			return false;
		}
		pDlg->SetViewWnd(m_hWndView);
		BOOL bStatus = pDlg->Create(nID, this);
		if(bStatus == 0) // Creation failed.
		{
			delete pDlg;
			return false;
		}
		m_nActiveDlg = page;
		m_Pages[static_cast<size_t>(page)] = pDlg;
	}
	RecalcLayout();
	pMainFrm->SetUserText(_T(""));
	pMainFrm->SetInfoText(_T(""));
	pMainFrm->SetXInfoText(_T(""));
	pDlg->ShowWindow(SW_SHOW);
	static_cast<CChildFrame *>(GetParentFrame())->SetSplitterHeight(pDlg->GetSplitPosRef());
	if (m_hWndMDI) ::PostMessage(m_hWndMDI, WM_MOD_CHANGEVIEWCLASS, (WPARAM)lParam, (LPARAM)pDlg);
	return true;
}


void CModControlView::OnDestroy()
{
	m_nActiveDlg = Page::Unknown;
	for(auto &pDlg : m_Pages)
	{
		if(pDlg)
		{
			pDlg->DestroyWindow();
			delete pDlg;
			pDlg = nullptr;
		}
	}
	CView::OnDestroy();
}


void CModControlView::UpdateView(UpdateHint lHint, CObject *pObject)
{
	CWnd *pActiveDlg = nullptr;
	CModDoc *pDoc = GetDocument();
	if(!pDoc)
		return;
	// Module type changed: update tabs
	if (lHint.GetType()[HINT_MODTYPE])
	{
		UINT nCount = 4;
		UINT mask = 1 | 2 | 4 | 16;

		if(pDoc->GetSoundFile().GetModSpecifications().instrumentsMax > 0 || pDoc->GetNumInstruments() > 0)
		{
			mask |= 8;
			//mask |= 32; //rewbs.graph
			nCount++;
		}
		if (nCount != (UINT)m_TabCtrl.GetItemCount())
		{
			UINT count = 0;
			pActiveDlg = GetCurrentControlDlg();
			if(pActiveDlg)
				pActiveDlg->ShowWindow(SW_HIDE);
			m_TabCtrl.DeleteAllItems();
			if (mask & 1) m_TabCtrl.InsertItem(count++, _T("General"), IDD_CONTROL_GLOBALS, IMAGE_GENERAL);
			if (mask & 2) m_TabCtrl.InsertItem(count++, _T("Patterns"), IDD_CONTROL_PATTERNS, IMAGE_PATTERNS);
			if (mask & 4) m_TabCtrl.InsertItem(count++, _T("Samples"), IDD_CONTROL_SAMPLES, IMAGE_SAMPLES);
			if (mask & 8) m_TabCtrl.InsertItem(count++, _T("Instruments"), IDD_CONTROL_INSTRUMENTS, IMAGE_INSTRUMENTS);
			//if (mask & 32) m_TabCtrl.InsertItem(count++, _T("Graph"), IDD_CONTROL_GRAPH, IMAGE_GRAPH); //rewbs.graph
			if (mask & 16) m_TabCtrl.InsertItem(count++, _T("Comments"), IDD_CONTROL_COMMENTS, IMAGE_COMMENTS);
		}
	}
	// Update child dialogs
	for (UINT nIndex=0; nIndex<int(Page::MaxPages); nIndex++)
	{
		CModControlDlg *pDlg = m_Pages[nIndex];
		if ((pDlg) && (pObject != pDlg)) pDlg->UpdateView(UpdateHint(lHint), pObject);
	}
	// Restore the displayed child dialog
	if (pActiveDlg) pActiveDlg->ShowWindow(SW_SHOW);
}


void CModControlView::OnTabSelchange(NMHDR*, LRESULT* pResult)
{
	SetActivePage(static_cast<Page>(m_TabCtrl.GetCurSel()));
	if(pResult)
		*pResult = 0;
}


LRESULT CModControlView::OnActivateModView(WPARAM nIndex, LPARAM lParam)
{
	if(::GetActiveWindow() != CMainFrame::GetMainFrame()->m_hWnd)
	{
		// If we are in a dialog (e.g. Amplify Sample), do not allow to switch to a different tab. Otherwise, watch the tracker crash!
		return 0;
	}

	if (m_TabCtrl.m_hWnd)
	{
		if (static_cast<Page>(nIndex) < Page::MaxPages)
		{
			m_TabCtrl.SetCurSel(static_cast<int>(nIndex));
			SetActivePage(static_cast<Page>(nIndex), lParam);
		} else
		// Might be a dialog id IDD_XXXX
		{
			int nItems = m_TabCtrl.GetItemCount();
			for (int i = 0; i < nItems; i++)
			{
				if (static_cast<WPARAM>(m_TabCtrl.GetItemData(i)) == nIndex)
				{
					m_TabCtrl.SetCurSel(i);
					SetActivePage(static_cast<Page>(i), lParam);
					break;
				}
			}
		}
	}
	return 0;
}


LRESULT CModControlView::OnModCtrlMsg(WPARAM wParam, LPARAM lParam)
{
	CModControlDlg *pActiveDlg = GetCurrentControlDlg();
	if(!pActiveDlg)
		return 0;
	switch(wParam)
	{
	case CTRLMSG_SETVIEWWND:
		m_hWndView = reinterpret_cast<HWND>(lParam);
		for(CModControlDlg *dlg : m_Pages)
		{
			if(dlg)
				dlg->SetViewWnd(m_hWndView);
		}
		break;
	}
	return pActiveDlg->OnModCtrlMsg(wParam, lParam);
}


LRESULT CModControlView::OnGetToolTipText(WPARAM uId, LPARAM pszText)
{
	CModControlDlg *pActiveDlg = GetCurrentControlDlg();
	if(!pActiveDlg)
		return 0;
	return static_cast<LRESULT>(pActiveDlg->GetToolTipText(static_cast<UINT>(uId), reinterpret_cast<LPTSTR>(pszText)));
}


void CModControlView::SampleChanged(SAMPLEINDEX smp)
{
	const CModDoc *modDoc = GetDocument();
	if(modDoc && modDoc->GetNumInstruments())
	{
		INSTRUMENTINDEX k = static_cast<INSTRUMENTINDEX>(GetInstrumentChange());
		if(!modDoc->IsChildSample(k, smp))
		{
			INSTRUMENTINDEX nins = modDoc->FindSampleParent(smp);
			if(nins != INSTRUMENTINDEX_INVALID)
			{
				InstrumentChanged(nins);
			}
		}
	} else
	{
		InstrumentChanged(smp);
	}
}


//////////////////////////////////////////////////////////////////
// CModScrollView

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x20E // Only available on Vista and newer
#endif

IMPLEMENT_SERIAL(CModScrollView, CScrollView, 0)
BEGIN_MESSAGE_MAP(CModScrollView, CScrollView)
	//{{AFX_MSG_MAP(CModScrollView)
	ON_WM_DESTROY()
	ON_WM_MOUSEWHEEL()
	ON_WM_MOUSEHWHEEL()
#if !defined(MPT_BUILD_RETRO)
	ON_MESSAGE(WM_DPICHANGED, &CModScrollView::OnDPIChanged)
#endif
	ON_MESSAGE(WM_MOD_VIEWMSG,			&CModScrollView::OnReceiveModViewMsg)
	ON_MESSAGE(WM_MOD_DRAGONDROPPING,	&CModScrollView::OnDragonDropping)
	ON_MESSAGE(WM_MOD_UPDATEPOSITION,	&CModScrollView::OnUpdatePosition)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CModDoc *CModScrollView::GetDocument() const noexcept { return static_cast<CModDoc *>(m_pDocument); }

LRESULT CModScrollView::SendCtrlMessage(UINT uMsg, LPARAM lParam) const
{
	if (m_hWndCtrl)	return ::SendMessage(m_hWndCtrl, WM_MOD_CTRLMSG, uMsg, lParam);
	return 0;
}


void CModScrollView::SendCtrlCommand(int id) const
{
	::SendMessage(m_hWndCtrl, WM_COMMAND, id, 0);
}


BOOL CModScrollView::PostCtrlMessage(UINT uMsg, LPARAM lParam) const
{
	if (m_hWndCtrl)	return ::PostMessage(m_hWndCtrl, WM_MOD_CTRLMSG, uMsg, lParam);
	return FALSE;
}


LRESULT CModScrollView::OnReceiveModViewMsg(WPARAM wParam, LPARAM lParam)
{
	return OnModViewMsg(wParam, lParam);
}


void CModScrollView::OnUpdate(CView* pView, LPARAM lHint, CObject*pHint)
{
	if (pView != this) UpdateView(UpdateHint::FromLPARAM(lHint), pHint);
}


LRESULT CModScrollView::OnModViewMsg(WPARAM wParam, LPARAM lParam)
{
	switch(wParam)
	{
	case VIEWMSG_SETCTRLWND:
		m_hWndCtrl = (HWND)lParam;
		break;

	case VIEWMSG_SETFOCUS:
	case VIEWMSG_SETACTIVE:
		GetParentFrame()->SetActiveView(this);
		SetFocus();
		break;
	}
	return 0;
}


void CModScrollView::OnInitialUpdate()
{
	CScrollView::OnInitialUpdate();
	m_nDPIx = Util::GetDPIx(m_hWnd);
	m_nDPIy = Util::GetDPIy(m_hWnd);
}


LRESULT CModScrollView::OnDPIChanged(WPARAM wParam, LPARAM)
{
	m_nDPIx = LOWORD(wParam);
	m_nDPIy = HIWORD(wParam);
	return 0;
}


void CModScrollView::UpdateIndicator(LPCTSTR lpszText)
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if (pMainFrm) pMainFrm->SetUserText((lpszText) ? lpszText : _T(""));
}


// Accumulate mouse wheel steps for laptop precision touchpads that emit wheel events < WHEEL_DELTA
static short RoundMouseWheelToWholeStep(int value, int &accum)
{
	accum += value;
	value = mpt::align_down(accum, WHEEL_DELTA);
	accum -= value;
	return mpt::saturate_cast<short>(value);
}


BOOL CModScrollView::OnMouseWheel(UINT fFlags, short zDelta, CPoint point)
{
	// we don't handle anything but scrolling just now
	if(fFlags & (MK_SHIFT | MK_CONTROL))
		return FALSE;

	// we can't get out of it--perform the scroll ourselves
	return DoMouseWheel(fFlags, RoundMouseWheelToWholeStep(zDelta, m_nScrollPosYfine), point);
}


void CModScrollView::OnMouseHWheel(UINT fFlags, short zDelta, CPoint point)
{
	zDelta = RoundMouseWheelToWholeStep(zDelta, m_nScrollPosXfine);

	// we don't handle anything but scrolling just now
	if(fFlags & (MK_SHIFT | MK_CONTROL))
	{
		CScrollView::OnMouseHWheel(fFlags, zDelta, point);
		return;
	}

	if (OnScrollBy(CSize(zDelta * m_lineDev.cx / WHEEL_DELTA, 0), TRUE))
		UpdateWindow();
}


void CModScrollView::OnDestroy()
{
	CModDoc *pModDoc = GetDocument();
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if ((pMainFrm) && (pModDoc))
	{
		if (pMainFrm->GetFollowSong(pModDoc) == m_hWnd)
		{
			pModDoc->SetNotifications(Notification::Default);
			pModDoc->SetFollowWnd(NULL);
		}
		if (pMainFrm->GetMidiRecordWnd() == m_hWnd)
		{
			pMainFrm->SetMidiRecordWnd(NULL);
		}
	}
	CScrollView::OnDestroy();
}


LRESULT CModScrollView::OnUpdatePosition(WPARAM, LPARAM lParam)
{
	Notification *pnotify = (Notification *)lParam;
	if (pnotify) return OnPlayerNotify(pnotify);
	return 0;
}


BOOL CModScrollView::OnScroll(UINT nScrollCode, UINT nPos, BOOL bDoScroll)
{
	SCROLLINFO info;
	if(LOBYTE(nScrollCode) == SB_THUMBTRACK)
	{
		if(GetScrollInfo(SB_HORZ, &info, SIF_TRACKPOS))
			nPos = info.nTrackPos;
		m_nScrollPosX = nPos;
	} else if(HIBYTE(nScrollCode) == SB_THUMBTRACK)
	{
		if(GetScrollInfo(SB_VERT, &info, SIF_TRACKPOS))
			nPos = info.nTrackPos;
		m_nScrollPosY = nPos;
	}
	if(bDoScroll)
		m_nScrollPosXfine = m_nScrollPosYfine = 0;
	return CScrollView::OnScroll(nScrollCode, nPos, bDoScroll);
}


BOOL CModScrollView::OnScrollBy(CSize sizeScroll, BOOL bDoScroll)
{
	BOOL ret = CScrollView::OnScrollBy(sizeScroll, bDoScroll);
	if(ret)
	{
		SCROLLINFO info;
		if(sizeScroll.cx)
		{
			if(GetScrollInfo(SB_HORZ, &info, SIF_POS))
				m_nScrollPosX = info.nPos;
		}
		if(sizeScroll.cy)
		{
			if(GetScrollInfo(SB_VERT, &info, SIF_POS))
				m_nScrollPosY = info.nPos;
		}
		if(bDoScroll)
			m_nScrollPosXfine = m_nScrollPosYfine = 0;
	}
	return ret;
}


int CModScrollView::SetScrollPos(int nBar, int nPos, BOOL bRedraw)
{
	if(nBar == SB_HORZ)
		m_nScrollPosX = nPos;
	else if(nBar == SB_VERT)
		m_nScrollPosY = nPos;
	return CScrollView::SetScrollPos(nBar, nPos, bRedraw);
}


void CModScrollView::SetScrollSizes(int nMapMode, SIZE sizeTotal, const SIZE& sizePage, const SIZE& sizeLine)
{
	CScrollView::SetScrollSizes(nMapMode, sizeTotal, sizePage, sizeLine);
	// Fix scroll positions
	SCROLLINFO info;
	if(GetScrollInfo(SB_HORZ, &info, SIF_POS))
		m_nScrollPosX = info.nPos;
	if(GetScrollInfo(SB_VERT, &info, SIF_POS))
		m_nScrollPosY = info.nPos;
}


BOOL CModScrollView::OnGesturePan(CPoint ptFrom, CPoint ptTo)
{
	// On Windows 8 and later, panning with touch gestures does not generate sensible WM_*SCROLL messages.
	// OnScrollBy is only ever called with a size of 0/0 in this case.
	// WM_GESTURE on the other hand gives us sensible data to work with.
	OnScrollBy(ptTo - ptFrom, TRUE);
	return TRUE;
}


////////////////////////////////////////////////////////////////////////////
// 	CModControlBar


BOOL CModControlBar::Init(CImageList &icons, CImageList &disabledIcons)
{
	const int imgSize = Util::ScalePixels(16, m_hWnd), btnSizeX = Util::ScalePixels(26, m_hWnd), btnSizeY = Util::ScalePixels(24, m_hWnd);
	SetButtonStructSize(sizeof(TBBUTTON));
	SetBitmapSize(CSize(imgSize, imgSize));
	SetButtonSize(CSize(btnSizeX, btnSizeY));

	// Add bitmaps
	SetImageList(&icons);
	SetDisabledImageList(&disabledIcons);
	UpdateStyle();
	return TRUE;
}


BOOL CModControlBar::AddButton(UINT nID, int iImage, UINT nStyle, UINT nState)
{
	TBBUTTON btn;

	btn.iBitmap = iImage;
	btn.idCommand = nID;
	btn.fsStyle = (BYTE)nStyle;
	btn.fsState = (BYTE)nState;
	btn.dwData = 0;
	btn.iString = 0;
	return AddButtons(1, &btn);
}


void CModControlBar::UpdateStyle()
{
	if (m_hWnd)
	{
		LONG lStyleOld = GetWindowLong(m_hWnd, GWL_STYLE);
		if (TrackerSettings::Instance().m_dwPatternSetup & PATTERN_FLATBUTTONS)
			lStyleOld |= TBSTYLE_FLAT;
		else
			lStyleOld &= ~TBSTYLE_FLAT;
		lStyleOld |= CCS_NORESIZE | CCS_NOPARENTALIGN | CCS_NODIVIDER | TBSTYLE_TOOLTIPS;
		SetWindowLong(m_hWnd, GWL_STYLE, lStyleOld);
		Invalidate();
	}
}


OPENMPT_NAMESPACE_END
