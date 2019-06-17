#pragma once

// CMainRecentChatDlg �Ի���
class CMainRecentChatDlg : public CDialog
{
	DECLARE_DYNAMIC(CMainRecentChatDlg)

public:
	CMainRecentChatDlg(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CMainRecentChatDlg();
	static CMainRecentChatDlg* GetInstanceDlg();
// �Ի�������
	enum { IDD = IDD_DLG_MAINRECENTCHAT };

protected:
	//virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage (MSG* pMsg);//��ϢԤ����
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnMouseHover(UINT nFlags, CPoint point);
	afx_msg LRESULT OnListNotify(WPARAM wParam,LPARAM lParam);//����¼�
	afx_msg LRESULT OnNotifyFromSDK(WPARAM wParam,LPARAM lParam);//�¼� ����SDK
	afx_msg LRESULT OnNotifyByMQTT(WPARAM wParam,LPARAM lParam);//�¼� ����SDK��MQTT
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
	//Ƥ���ؼ�
	IDirectUI	*m_pDirectUI;
	IDUIListView *m_plstRecentChat;
	IDUITVItem *m_pCurItem;
	int m_cntUnReadItem; //δ������������ֵΪ0ʱ����������Tab��ť�ϵĺ��
};

