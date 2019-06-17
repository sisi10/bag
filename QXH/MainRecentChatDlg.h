#pragma once

// CMainRecentChatDlg 对话框
class CMainRecentChatDlg : public CDialog
{
	DECLARE_DYNAMIC(CMainRecentChatDlg)

public:
	CMainRecentChatDlg(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CMainRecentChatDlg();
	static CMainRecentChatDlg* GetInstanceDlg();
// 对话框数据
	enum { IDD = IDD_DLG_MAINRECENTCHAT };

protected:
	//virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage (MSG* pMsg);//消息预过滤
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnMouseHover(UINT nFlags, CPoint point);
	afx_msg LRESULT OnListNotify(WPARAM wParam,LPARAM lParam);//点击事件
	afx_msg LRESULT OnNotifyFromSDK(WPARAM wParam,LPARAM lParam);//事件 来自SDK
	afx_msg LRESULT OnNotifyByMQTT(WPARAM wParam,LPARAM lParam);//事件 来自SDK的MQTT
	DECLARE_MESSAGE_MAP()
private:
	void InitListView(LPARAM lParam);
	void UpdateListView(LPARAM lParam, BOOL bMoveItem = FALSE);
	//modified by zhangpeng on 20160526 for bug 185 begin
	static bool SortByTime( const CHeadImageManager::PUI_IM &v1, const CHeadImageManager::PUI_IM &v2);
public:
	static vector<CHeadImageManager::PUI_IM> vc_pOldUiIm;
private:
	//modified by zhangpeng on 20160526 for bug 185 end
	IDUITVItemBase * InsertItem(int nIndex, LPARAM lParam);
	IDUITVItemBase * AddItem(LPARAM lParam) {	return InsertItem(m_plstRecentChat->GetItemCount(), lParam); }
	void UpdateItem(IDUITVItemBase *pItem);
	BOOL SetItemRead(IDUITVItemBase *pItem);

private:
	//皮肤控件
	IDirectUI	*m_pDirectUI;
	IDUIListView *m_plstRecentChat;
	IDUITVItem *m_pCurItem;
	int m_cntUnReadItem; //未读项数，当此值为0时隐藏主界面Tab按钮上的红点
};

