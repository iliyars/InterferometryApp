
// InterferometryAppView.cpp : implementation of the CInterferometryAppView class
//

#include "pch.h"
#include "framework.h"
// SHARED_HANDLERS can be defined in an ATL project implementing preview, thumbnail
// and search filter handlers and allows sharing of document code with that project.
#ifndef SHARED_HANDLERS
#include "InterferometryApp.h"
#endif

#include "InterferometryAppDoc.h"
#include "InterferometryAppView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CInterferometryAppView

IMPLEMENT_DYNCREATE(CInterferometryAppView, CView)

BEGIN_MESSAGE_MAP(CInterferometryAppView, CView)
	// Standard printing commands
	ON_COMMAND(ID_FILE_PRINT, &CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, &CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, &CInterferometryAppView::OnFilePrintPreview)
	ON_WM_CONTEXTMENU()
	ON_WM_RBUTTONUP()
END_MESSAGE_MAP()

// CInterferometryAppView construction/destruction

CInterferometryAppView::CInterferometryAppView() noexcept
{
	// TODO: add construction code here

}

CInterferometryAppView::~CInterferometryAppView()
{
}

BOOL CInterferometryAppView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return CView::PreCreateWindow(cs);
}

// CInterferometryAppView drawing

void CInterferometryAppView::OnDraw(CDC* /*pDC*/)
{
	CInterferometryAppDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return;

	// TODO: add draw code for native data here
}


// CInterferometryAppView printing


void CInterferometryAppView::OnFilePrintPreview()
{
#ifndef SHARED_HANDLERS
	AFXPrintPreview(this);
#endif
}

BOOL CInterferometryAppView::OnPreparePrinting(CPrintInfo* pInfo)
{
	// default preparation
	return DoPreparePrinting(pInfo);
}

void CInterferometryAppView::OnBeginPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: add extra initialization before printing
}

void CInterferometryAppView::OnEndPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: add cleanup after printing
}

void CInterferometryAppView::OnRButtonUp(UINT /* nFlags */, CPoint point)
{
	ClientToScreen(&point);
	OnContextMenu(this, point);
}

void CInterferometryAppView::OnContextMenu(CWnd* /* pWnd */, CPoint point)
{
#ifndef SHARED_HANDLERS
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
#endif
}


// CInterferometryAppView diagnostics

#ifdef _DEBUG
void CInterferometryAppView::AssertValid() const
{
	CView::AssertValid();
}

void CInterferometryAppView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CInterferometryAppDoc* CInterferometryAppView::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CInterferometryAppDoc)));
	return (CInterferometryAppDoc*)m_pDocument;
}
#endif //_DEBUG


// CInterferometryAppView message handlers
