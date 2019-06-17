// LoginDlg.cpp : 实现文件
//
#include "stdafx.h"
#include "BCSClient.h"
#include "BCSClientDlg.h"
#include "IMFrameDlg.h"
#include "MainRecentChatDlg.h"
#include "..\UIUtil.h"
#include "..\..\Skin\HeadPicManager.h"
#include "AccountMan.h"
#include "CoreInterface\Common\SIPHelloUtil.h"
#include "im.h"
#include "..\OtherForm\OtherNoticeDlg.h"
#include "contacts.h"
#include "..\Model\ContactMan.h"
#include "IMCefDlg.h"
#include <algorithm>//add by zhangpeng 
//#include "CoreInterface\MessageManage\SubInstantMessage.h"
//#include "CoreInterface\UA\include\CXXXHandlers.h"
using namespace UA_UTIL;
//using namespace CallBack;

#define IDM_DEL_SEL_RECORD 102
#define IDM_TOP_SEL_RECORD 103

// CMainRecentChatDlg 对话框
IMPLEMENT_DYNAMIC(CMainRecentChatDlg, CDialog)
CMainRecentChatDlg::CMainRecentChatDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CMainRecentChatDlg::IDD, pParent)
	,m_pDirectUI(NULL)
	,m_cntUnReadItem(0)
{
	//初始化指针
}

CMainRecentChatDlg::~CMainRecentChatDlg()
{
}

//暂时不用
// void CMainRecentChatDlg::DoDataExchange(CDataExchange* pDX)
// {
// 	CDialog::DoDataExchange(pDX);
// }

CMainRecentChatDlg* CMainRecentChatDlg::GetInstanceDlg()
{
	return (CMainRecentChatDlg*)CBCSClientDlg::GetInstanceDlg()->m_pDlgRecentChat;
}
//自定义消息
BEGIN_MESSAGE_MAP(CMainRecentChatDlg, CDialog)
	ON_WM_CREATE()
	//ON_WM_MOUSEHOVER()
	ON_MESSAGE(DUILV_NOTIFY,OnListNotify)
	ON_MESSAGE(WM_NOTIFYFROMSDK,OnNotifyFromSDK)
	ON_MESSAGE(WM_NOTIFY_MQTT, OnNotifyByMQTT)
END_MESSAGE_MAP()


// ====== 下面 CMainRecentChatDlg 消息处理程序
//系统事件 消息预处理屏蔽Enter和Escape
BOOL CMainRecentChatDlg::PreTranslateMessage (MSG* pMsg)
{
	switch (pMsg->message)
	{
	case WM_LBUTTONDOWN:
		::SetFocus(m_hWnd);
		break;
	case WM_KEYDOWN:
		switch (pMsg->wParam)
		{
		case VK_RETURN:
		case VK_ESCAPE:
			return TRUE;
		}
	}
	return CDialog::PreTranslateMessage(pMsg);
}

//系统事件 皮肤控件初始化
int CMainRecentChatDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_pDirectUI = (IDirectUI*)theApp.m_pDUIRes->CreateDirectUI(_T("duiMainRecentChat"),HandleToLong(m_hWnd));
	m_plstRecentChat = (IDUIListView *) DIRECTUI_GETCONTROL(_T("lstRecentChat"));

	return 0;
}

//系统事件 create 之后做的初始化，一般不需要，除非引入windows标准控件，只有在这里才能获取
BOOL CMainRecentChatDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetWindowText(_T("MainRecentChatDlg"));

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

//事件 UI控件消息处理
LRESULT CMainRecentChatDlg::OnListNotify(WPARAM wParam,LPARAM lParam)
{
	DUILVNotifyInfo *pLvInfo = (DUILVNotifyInfo*)wParam;
	IDUIListView *pListView = (IDUIListView*)pLvInfo->lParam1;
	IDUITVItemBase* pItem = (IDUITVItemBase*)pLvInfo->lParam2;

	switch( pLvInfo->eDUILVMsgId )
	{
	case DUI_LV_MOSEMOVE:
	case DUI_LV_HOTCHANGE:
		if (pItem)
		{
			IUIFormObj* pfrm = (IUIFormObj*)pItem->GetCustomObj();
			IRadioBox* pRdx = (IRadioBox*)CUIUtil::GetDUIControl(pfrm, _T("rdxBack"), FALSE);
			BOOL bHot = (pLvInfo->eDUILVMsgId == DUI_LV_MOSEMOVE);
			if (bHot && !CUIUtil::PtInSkin(pRdx))
				break;
			CHeadImageManager::PUI_IM pUiIm = (CHeadImageManager::PUI_IM)pItem->GetData();
			if (pUiIm)
			{
				if (pUiIm->chattype == ICT_CT_P2P && pLvInfo->eDUILVMsgId == DUI_LV_MOSEMOVE) {
					m_pCurItem = (IDUITVItem *)pItem;
					CUIUtil::DealHeadLogoMouseMove(CBCSClientDlg::GetInstanceDlg()->m_pDlgPersonInfo, m_pCurItem);
				}
			}
			pRdx->SetBackImage(DUI_RADIOBOX_NORMAL, FALSE, pRdx->GetBackImage(bHot ? DUI_RADIOBOX_HOT : DUI_RADIOBOX_DISABLE, FALSE));
			pListView->RedrawWindow(FALSE);
		}
		break;
	case DUI_LV_LBUTTONDOWN:
	case DUI_LV_RBUTTONDOWN:
		if (pItem) {
			CUIUtil::SetItemRadioBackCheck(pItem);
		}
		break;
	case DUI_LV_LBUTTONUP:
		break;
	case DUI_LV_DBLBUTTONUP:
		if (pItem)
		{
			//双击显示聊天窗口
			CHeadImageManager::PUI_IM pUiIm = (CHeadImageManager::PUI_IM)pItem->GetData();
			if (pUiIm)
			{
				if (pUiIm->chattype == ICT_CT_LAPP)
				{
					IDUIControlBase *pfrm = pItem->GetCustomObj();
					COtherNoticeDlg *pDlg = COtherNoticeDlg::GetInstance();
					char8 uid[256], url[8192] = {0};
					CString strUid = pItem->GetText().c_str();
					CString2Char(strUid, uid);
					GetLAPPUrl(uid, url, TRUE);
					if (pDlg && pDlg->LoadUrl(url))
					{
						tstring strText = CUIUtil::GetDUIStaticText(pfrm, _T("stcName"));
						pDlg->SetWindowText(strText.c_str());
						pDlg->ShowWindow(SW_SHOWNORMAL);

						if (SetItemRead(pItem))
						{
							::SendMessage(CBCSClientDlg::GetInstanceDlg()->m_pDlgMore->GetSafeHwnd(), WM_NOTIFYFROMSDK, WM_UNREADSTATECHANGE, (LPARAM)strUid.GetString());
						}
					}
				}
				else
				{
					CBCSClientDlg::GetInstanceDlg()->m_pDlgIMFrame->PostMessage(WM_NOTIFYFROMSDK, (WPARAM)WM_NOTIFY_IM_CREATE, (LPARAM)pUiIm);
				}
			}
		}
		break;
 	case DUI_LV_RBUTTONUP:
 		{
 			IDUIPopupMenu *ppm = CUIUtil::GetDefPopupMenu();
 			ppm->AppendMenu(IDM_DEL_SEL_RECORD, _T(""), _T("从会话列表移除"), 0, MENUITEMSTYLE_NORMAL, FALSE, TRUE);
			ppm->AppendMenu(IDM_TOP_SEL_RECORD, _T(""), _T("会话置顶"), 0, MENUITEMSTYLE_NORMAL, FALSE, TRUE);
 
 			POINT ptCursor;
 			::GetCursorPos( &ptCursor );
			long nRet = ppm->TrackPopupMenuEx(DUI_TPM_LEFTALIGN,(short)ptCursor.x,(short)ptCursor.y,0);
 			if (nRet == IDM_DEL_SEL_RECORD)
 			{
				IDUITVItemBase *pItem = m_plstRecentChat->GetFirstSelItem();
				while (pItem)
				{
					IDUITVItemBase *pItemNext = m_plstRecentChat->GetNextSelItem(pItem);
					CString strUid = pItem->GetText().c_str();
					SetTargetDescriptionDelete(CT2A(strUid));
					if (DelRecord(CT2A(strUid)) == 0)
					{
						m_plstRecentChat->RemoveItem((IDUITVItem *)pItem);
					}
					pItem = pItemNext;
				}
 			}
			else if (nRet == IDM_TOP_SEL_RECORD)
			{
				PGList plst = (PGList)lParam;
				CString uid;
				CHeadImageManager::PUI_IM pUiIm = NULL;
				PRECENT_CONTACT_UI_DATA pData = NULL;

				IDUITVItemBase *pItem = m_plstRecentChat->GetFirstSelItem();
				while (pItem)
				{
					IDUITVItemBase *pItemNext = m_plstRecentChat->GetNextSelItem(pItem);
					CString strUid = pItem->GetText().c_str();
					SetTargetDescriptionTop(CT2A(strUid));

					m_plstRecentChat->DeleteAllItems();
					m_plstRecentChat->RefreshView();

					if (!plst)
					{
						GetLastIM(CT_Other, &plst);
					}
					while (plst)
					{
						pData = (PRECENT_CONTACT_UI_DATA) plst->data;
						if(pData)
						{
							Char2CString(pData->uid, uid);
							if (uid.IsEmpty())
							{
								continue;
							}
							pUiIm = (CHeadImageManager::PUI_IM)CHeadImageManager::GetInstanceDlg()->GetImData(uid);
							pUiIm->chattype = pData->chatType; //群组、个人聊天标志
							pUiIm->unreadNum = pData->unreadNum;
							pUiIm->msgTime = pData->msgTime;
							Char2CString(pData->name, pUiIm->name);
							CString msgContent;
							Char2CString(pData->msgContent, msgContent);
							CIMCefDlg::HTMLDecode(msgContent);
							pUiIm->msgContent = msgContent;
							AddItem((LPARAM)pUiIm);
							if (pUiIm->chattype == ICT_CT_P2P)
							{
								FastUpdateUserInfo(pData->uid);
							}
						}//end pData
						plst = plst->next;
					}//end while

					ReleaseGList((PGList)lParam);
					m_plstRecentChat->RefreshView();

					pItem = pItemNext;
				}
				//m_plstRecentChat->RefreshView();
			}
 		}
 		break;
	}

	return 0;
}

void CMainRecentChatDlg::OnMouseHover(UINT nFlags, CPoint point)
{
	CDialog::OnMouseHover(nFlags, point);
}

//事件 来自SDK
LRESULT CMainRecentChatDlg::OnNotifyFromSDK(WPARAM wParam,LPARAM lParam)
{
	DEBUGCODE(Tracer::LogToConsole(_T("[CMainRecentChatDlg::OnNotifyFromSDK] - wParam:%d"), wParam);)

	if (wParam == WM_NOTIFY_LOGOFF)
	{
		m_plstRecentChat->DeleteAllItems();
	}
	//modified by zhangpeng on 20160420 begin
	else if (wParam == WM_PRESONALSYNC)
	{//同步消息
		CHeadImageManager::PUI_IM pUiIm = (CHeadImageManager::PUI_IM)(lParam);
		IDUITVItemBase *pItem = m_plstRecentChat->GetItem((LPCTSTR)(pUiIm->uid));
		if (pItem)
		{
			SetItemRead(pItem);
		}
	}
	//modified by zhangpeng on 20160420 end
	else if (wParam == WM_UNREADSTATECHANGE)  //来自发现界面
	{
		IDUITVItemBase *pItem = m_plstRecentChat->GetItem((LPCTSTR)lParam);
		if (pItem)
		{
			SetItemRead(pItem);
		}
	}
	else
	{
		int cntUnReadItemOld = m_cntUnReadItem;
		switch (wParam)
		{
		case LOGIN_UPDATE_REMSG_UI:
			cntUnReadItemOld = (m_cntUnReadItem = 0);
			InitListView(lParam);
			break;
		case WM_NOTIFY_IM_CREATE:
			UpdateListView(lParam);
			break;
		case WM_NOTIFY_IM_UPDATE:
			UpdateListView(lParam, TRUE);
			break;
		}
		if (cntUnReadItemOld != m_cntUnReadItem)
		{
			AfxGetMainWnd()->PostMessage(WM_UNREADSTATECHANGE, 0, (m_cntUnReadItem != 0));
		}
	}

	return 0L;
}

//事件 来自MQTT
LRESULT CMainRecentChatDlg::OnNotifyByMQTT(WPARAM wParam,LPARAM lParam)
{
	if(wParam == REQ_PresenceUpdate)//更新在线状态
	{
		IUIFormObj *pfrm,*pfrmState;
		PGList pNode = (PGList)lParam;
		CString uid;
		while(pNode)
		{
			SDK_Presence* pData = (SDK_Presence*)pNode->data;
			Char2CString(pData->uid,uid);
			pfrm = (IUIFormObj *)DIRECTUI_GETCONTROL(uid.GetString());
			if(pfrm)
			{
				//从GetPresenceUpdateData得到的状态还是不准，换回GetPresence
				int nPresence = GetPresence(pData->uid, NULL);

				//在线或离线头像
				CUIUtil::SetDUILogoHead(pfrm, uid, nPresence);

				//在线离线终端类型图标
				pfrmState = (IUIFormObj *)pfrm->GetObjectByCaption(DUIOBJTYPE_PLUGIN,_T("frmState"),TRUE);
				CUIUtil::SetPresenceImage(pfrmState, nPresence);
				//pfrmState->SetVisible(CHeadImageManager::GetTermType(pData->uid) != CHeadImageManager::TERM_PC,TRUE,FALSE);
			}
			pNode = pNode->next;
		}

		//ReleaseGList((PGList)lParam);
	}
	else if (wParam == REQ_PresenceClear || wParam == REQ_PresenceFullUpdate)
	{
		IDUITVItemBase *pItem;
		int nCount = m_plstRecentChat->GetItemCount();
		int nPresence = SDK_PRE_PC_OFFLINE;
		if(wParam == REQ_PresenceFullUpdate) nPresence = SDK_PRE_UNKNOWN;
		for (int i = 0; i < nCount; ++i)
		{
			 pItem = m_plstRecentChat->GetAt(i);
			 CHeadImageManager::PUI_IM pUiIm = (CHeadImageManager::PUI_IM)pItem->GetData();
			 if (pUiIm && pUiIm->chattype == ICT_CT_P2P)
			 {
				 IUIFormObj *pfrm = (IUIFormObj *)pItem->GetCustomObj();
				 IUIFormObj *pfrmState = (IUIFormObj *)pfrm->GetObjectByCaption(DUIOBJTYPE_PLUGIN,_T("frmState"),TRUE);
				 //终端类型
				 int pres = nPresence;
				 CString strUid = pUiIm->uid;
				 char description[128];
				 if (pres == SDK_PRE_UNKNOWN)
				 {
					 char uid[128];
					 CString2Char(strUid,uid);
					 pres = GetPresence(uid, description);
					 CUIUtil::SetPresenceImage(pfrmState, pres);
				 }

				 CUIUtil::SetDUILogoHead(pfrm, pUiIm->uid, pres);
			 }
		}
	}
	else if (wParam == REQ_PBNodeChangeNotice)
	{
		RES_PBNodeChangeNotice *pData = (RES_PBNodeChangeNotice *)lParam;
		CString strUid;
		Char2CString(pData->uid, strUid);

		IDUITVItemBase *pItem = m_plstRecentChat->GetItem(strUid.GetString());
		if (pItem)
		{
			IDUIControlBase  *pfrm = pItem->GetCustomObj();
			int nPresence = GetPresence(pData->uid, NULL);
			CUIUtil::SetDUILogoHead(pfrm, strUid, nPresence);
			//在线离线终端类型图标
			IUIFormObj *pfrmState= (IUIFormObj *)pfrm->GetObjectByCaption(DUIOBJTYPE_PLUGIN,_T("frmState"),TRUE);
			CUIUtil::SetPresenceImage(pfrmState, nPresence);
			//更新姓名 
			CUIUtil::SetDUIStaticText(pfrm, _T("stcName"), ContactMan::GetNameByUid(pData->uid));
		}

		free(pData);
	}
	else if (wParam == REQ_FriendHeadPortraitChange)
	{
		RES_FriendHeadPortraitChange *pData = (RES_FriendHeadPortraitChange *)lParam;
		CString strUid;
		Char2CString(pData->uid, strUid);

		IDUITVItemBase *pItem = m_plstRecentChat->GetItem(strUid.GetString());
		if (pItem)
		{
			CUIUtil::SetDUILogoHead(pItem->GetCustomObj(), strUid);
		}
	}
	else if (wParam == REQ_LAPPUSERNOTICE)
	{
		RES_LAPPUSERNotice *pData = (RES_LAPPUSERNotice *)lParam;
		CString strUid;
		Char2CString(pData->lappid, strUid);
		CHeadImageManager::PUI_IM pUiIm = (CHeadImageManager::PUI_IM)CHeadImageManager::GetInstanceDlg()->GetImData(strUid);
		Char2CString(pData->name, pUiIm->name);
		Char2CString(pData->title, pUiIm->msgContent);

		MSG_RECEIVE_NEW contact;
		GetRecentContact(pData->lappid, &contact);
		pUiIm->msgTime = contact.msgTime;
		pUiIm->chattype = ICT_CT_LAPP;
		++pUiIm->unreadNum;
		OnNotifyFromSDK(WM_NOTIFY_IM_UPDATE, (LPARAM)pUiIm);
	}

	return 0;
}

void CMainRecentChatDlg::InitListView(LPARAM lParam)
{
	PGList plst = (PGList)lParam;
	CString uid;
	CHeadImageManager::PUI_IM pUiIm = NULL;
	PRECENT_CONTACT_UI_DATA pData = NULL;
	m_plstRecentChat->DeleteAllItems();
	m_plstRecentChat->RefreshView();
	DEBUGCODE(Tracer::Log(_T("[CMainRecentChatDlg::InitListView] - plst:0x%x"), lParam);)
	//modified by zhangpeng on 20160709 begin
	//有可能出现plst为空，导致刚启动时候最近联系人列表为空，确保plst有数据

	if (!plst)
	{
		GetLastIM(CT_Other, &plst);
		DEBUGCODE(Tracer::Log(_T("[CMainRecentChatDlg::InitListView]:this plst is null"), lParam);)
	}
	//modified by zhangpeng on 20160709 end
	while (plst)
	{
		pData = (PRECENT_CONTACT_UI_DATA) plst->data;
		if(pData)
		{
			Char2CString(pData->uid, uid);
			//add by zhangpeng on 20160530 begin
			if (uid.IsEmpty())
			{
				continue;
			}
			//add by zhangpeng on 20160530 end
			pUiIm = (CHeadImageManager::PUI_IM)CHeadImageManager::GetInstanceDlg()->GetImData(uid);
			//pUiIm->uid = uid;
			pUiIm->chattype = pData->chatType; //群组、个人聊天标志
			pUiIm->unreadNum = pData->unreadNum;
			pUiIm->msgTime = pData->msgTime;
			Char2CString(pData->name, pUiIm->name);
			//HTML编解码
			CString msgContent;
			Char2CString(pData->msgContent, msgContent);
			CIMCefDlg::HTMLDecode(msgContent);
			pUiIm->msgContent = msgContent;
			AddItem((LPARAM)pUiIm);
			DEBUGCODE(Tracer::Log(_T("[CMainRecentChatDlg::InitListView] -AddItem uid:%s"),pUiIm->uid);)
			//pUiIm->name.Empty();
			//pUiIm->msgContent.Empty();
			//pUiIm->msgTime.Empty();
			if (pUiIm->chattype == ICT_CT_P2P)
			{
				FastUpdateUserInfo(pData->uid);
			}
		}//end pData
		plst = plst->next;
	}//end while

	ReleaseGList((PGList)lParam);
	m_plstRecentChat->RefreshView();
	DEBUGCODE(Tracer::Log(_T("[CMainRecentChatDlg::InitListView] -exit"));)
}
//modified by zhangpeng on 20160526 for bug 185 begin 
vector<CHeadImageManager::PUI_IM> CMainRecentChatDlg::vc_pOldUiIm(0);
//自定义排序函数  
bool CMainRecentChatDlg::SortByTime( const CHeadImageManager::PUI_IM &v1, const CHeadImageManager::PUI_IM &v2)
{
	return v1->msgTime < v2->msgTime;//↑排列  
}  
//modified by zhangpeng on 20160526 for bug 185 end
void CMainRecentChatDlg::UpdateListView(LPARAM lParam, BOOL bMoveItem)
{
	CHeadImageManager::PUI_IM pUiIm = (CHeadImageManager::PUI_IM)lParam;
	IDUITVItemBase *pItem = m_plstRecentChat->GetItem(pUiIm->uid.GetString());
	//modified by zhangpeng on 20160525 for bug 301 begin
	pUiIm->uid.ReleaseBuffer();
	if ((pUiIm->ownerName.IsEmpty()) && (pUiIm->chattype == 1))
	{//创建者为空，说明群主已经将该主群删除
		if (vc_pOldUiIm.size() > 0)
		{
			for (vector<CHeadImageManager::PUI_IM>::iterator itr = vc_pOldUiIm.begin();itr!=vc_pOldUiIm.end();)
			{
				if (((*itr)->uid == pUiIm->uid) ||((*itr)->msgContent.IsEmpty()))
				{//找到相同组群ID,删除过期的
					IDUITVItemBase *pItem = m_plstRecentChat->GetItem((*itr)->uid.GetString());
					int unreadNumPre = 0;
					if (pItem)
					{
						unreadNumPre = pItem->GetCustomObj()->GetUserData();
						m_plstRecentChat->RemoveItem((IDUITVItem *)pItem);
					}
					if (unreadNumPre > 0) --m_cntUnReadItem;
					itr = vc_pOldUiIm.erase(itr);
				}
				else
				{
					itr++;
				}
			}
		}
		
		return;
	}
	//modified by zhangpeng on 20160525 for bug 301 end
	if (pItem)
	{
		int unreadNumPre = pItem->GetCustomObj()->GetUserData();
		if (bMoveItem)
		{
			m_plstRecentChat->RemoveItem((IDUITVItem *)pItem);
		}
		else
		{
			UpdateItem(pItem);
		}
		if (unreadNumPre > 0)
			--m_cntUnReadItem;
	}
	//modified by zhangpeng on 20160526 for bug 185 begin
	//去掉重复的数据
	if (vc_pOldUiIm.size() > 0)
	{
		for (vector<CHeadImageManager::PUI_IM>::iterator itr = vc_pOldUiIm.begin();itr!=(vc_pOldUiIm.end());)
		{
			if (((*itr)->msgTime >0 )&& ((*itr)->uid == pUiIm->uid) && ((*itr)->msgTime <= pUiIm->msgTime))
			{//同一个账号，历史消息小于本次消息，去除历史消息显示
				itr = vc_pOldUiIm.erase(itr);
			}
			else
			{
				itr++;
			}
		}
	}
	//modified by zhangpeng on 20160526 for bug 185 end
	if (!pUiIm->msgContent.IsEmpty() && bMoveItem)
	{
		//modified by zhangpeng on 20160526 for bug 185 and bug 301 begin 
		//if (pUiIm->unreadNum >0)
		{//如果有未读消息
			vc_pOldUiIm.push_back(pUiIm);//保存当前未读消息选项
		
			std::sort(vc_pOldUiIm.begin(),vc_pOldUiIm.end(),SortByTime);  
			for (int i =0;i< vc_pOldUiIm.size();++i)
			{
				if ((vc_pOldUiIm[i]->msgTime > 0) && !vc_pOldUiIm[i]->uid.IsEmpty())
				{
					IDUITVItemBase *pItem = m_plstRecentChat->GetItem(vc_pOldUiIm[i]->uid.GetString());
					int unreadNumPre = 0;
					if (pItem)
					{
						int unreadNumPre = pItem->GetCustomObj()->GetUserData();
						if (bMoveItem)
						{
							m_plstRecentChat->RemoveItem((IDUITVItem *)pItem);
						}
						else
						{
							UpdateItem(pItem);
						}
						if (unreadNumPre > 0)
							--m_cntUnReadItem;
					}

					if (unreadNumPre > 0) --m_cntUnReadItem;
					InsertItem(0, (LPARAM)vc_pOldUiIm[i]);					
				}
			}	
		}	
		//else
		//{
		//	InsertItem(0, lParam);
		//	//DEBUGCODE(Tracer::LogA("[CCMainRecentChatDlg::UpdateListView] [before ScrollOver]");)
		//	m_plstRecentChat->ScrollOver(TRUE);
		//	//DEBUGCODE(Tracer::LogA("[CCMainRecentChatDlg::UpdateListView] [after ScrollOver]");)
		//}
		//modified by zhangpeng on 20160526 for bug 185 end
		
	}
	
	m_plstRecentChat->RefreshView();
}

IDUITVItemBase * CMainRecentChatDlg::InsertItem(int nIndex, LPARAM lParam)
{
	CHeadImageManager::PUI_IM pUiIm = (CHeadImageManager::PUI_IM)lParam;
	//long nID = CUIUtil::UidToId(pUiIm->uid, pUiIm->chattype);
	nIndex = m_plstRecentChat->InsertItem(nIndex, pUiIm->uid.GetString(), 0, TRUE); //nID都为0好像没问题，待观察 
	IDUITVItemBase *pItem = m_plstRecentChat->GetAt(nIndex);
	if (pItem)
	{
		pItem->SetData(lParam);
		UpdateItem(pItem);
		if (pUiIm->unreadNum > 0)
			++m_cntUnReadItem;
	}

	return pItem;
}

void CMainRecentChatDlg::UpdateItem(IDUITVItemBase *pItem)
{
	ASSERT(pItem);
	IDUIControlBase* pfrm = (IUIFormObj *)pItem->GetCustomObj();
	CHeadImageManager::PUI_IM pUiIm = (CHeadImageManager::PUI_IM)pItem->GetData();

	ASSERT(pUiIm);
	ASSERT(pfrm);
	if (pfrm)
	{
		IUIFormObj *pfrmState = (IUIFormObj *)pfrm->GetObjectByCaption(DUIOBJTYPE_PLUGIN,_T("frmState"),TRUE);
		IDUIStatic *pstcName = (IDUIStatic *)pfrm->GetObjectByCaption(DUIOBJTYPE_PLUGIN, _T("stcName"), TRUE);
		IDUIStatic *pstcDate = (IDUIStatic *)pfrm->GetObjectByCaption(DUIOBJTYPE_PLUGIN, _T("stcDate"), TRUE);
		IDUIStatic *pstcDescribe = (IDUIStatic *)pfrm->GetObjectByCaption(DUIOBJTYPE_PLUGIN, _T("stcDescribe"), TRUE);
		IDUIStatic *pstcNum= (IDUIStatic *)pfrm->GetObjectByCaption(DUIOBJTYPE_PLUGIN, _T("stcNum"), TRUE);

		CString strMsgTime(CUIUtil::FormatMsgTime(pUiIm->msgTime));
		pstcDate->SetText(strMsgTime.GetString());
		pstcName->SetText(pUiIm->name.IsEmpty() ? _T("陌生人") : pUiIm->name.GetString());

		IDUIRect *pRectNum = pstcNum->GetRect();
		IDUIRect *pRectDescribe = pstcDescribe->GetRect();
		UINT nMaxWidth = (pUiIm->unreadNum ? pRectNum->left - 4 : pRectNum->right) - pRectDescribe->left;
		pstcDescribe->SetAutoResize(TRUE, 16, 16, 80, nMaxWidth);
		pstcDescribe->SetText(pUiIm->msgContent.GetString());
		pstcDescribe->SetTooltip(pUiIm->msgContent.GetString());

		CUIUtil::SetUnReadNum(pstcNum, pUiIm->unreadNum);
		pfrm->SetUserData(pUiIm->unreadNum);

		SIZE size;
		IDUITextStyle *pTextStyle = pstcDate->GetTextStyle(DUI_STATIC_ACTIVE);
		CUIUtil::CalcTextSize(strMsgTime, pTextStyle, size);
		CUIUtil::SetDUIControlRightWidth(pstcName, size.cx + 10);

		//在线或离线头像
		CUIUtil::SetDUILogoHead(pfrm, pUiIm->uid, (pUiIm->chattype == ICT_CT_P2P) ? 0 : SDK_PRE_PC_ONLINE);

		if (pUiIm->chattype == ICT_CT_P2P)
		{
			//在线离线终端类型图标
			char cuid[256];
			CString2Char(pUiIm->uid, cuid);
			CUIUtil::SetPresenceImage(pfrmState, cuid);
			//pfrmState->SetVisible(CHeadImageManager::GetTermType(cuid) != CHeadImageManager::TERM_PC,TRUE,FALSE);
		}
		else
		{
			pfrmState->SetVisible(FALSE, TRUE, FALSE);
			if (pUiIm->chattype == ICT_CT_GROUP && pUiIm->grpType == 3)
			{
				//解决讨论组没消息时间问题
				::PostMessage(CBCSClientDlg::GetInstanceDlg()->GetSafeHwnd(), WM_NOTIFYFROMSDK, WM_NOTIFY_GROUP_UPDATE, _ttoi(pUiIm->uid));
			}
		}
	}
}

BOOL CMainRecentChatDlg::SetItemRead(IDUITVItemBase *pItem)
{
	CHeadImageManager::PUI_IM pUiIm = (CHeadImageManager::PUI_IM)pItem->GetData();
	IDUIControlBase *pfrm = pItem->GetCustomObj();
	IDUIStatic *pstcNum = (IDUIStatic *)pfrm->GetObjectByCaption(DUIOBJTYPE_PLUGIN, _T("stcNum"), TRUE);
	//if (pstcNum->IsVisible())
	{
		if (pUiIm->unreadNum > 0)
		{
			pUiIm->unreadNum = 0;
			--m_cntUnReadItem;
		}
		pfrm->SetUserData(0);
		pstcNum->SetVisible(FALSE, TRUE, FALSE);

		AfxGetMainWnd()->PostMessage(WM_UNREADSTATECHANGE, 0, (m_cntUnReadItem != 0));

		char uid[256];
		CString2Char(pUiIm->uid, uid);
		SetMessageRead(uid);

		return TRUE;
	}
	return FALSE;
}

