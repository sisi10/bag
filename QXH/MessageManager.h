#pragma once
#include "CoreInterface\DB\DBManager.h"
#include "CoreInterface\JSON\json.h"
#include "CoreInterface\MessageManage\InstantMessage.h"
#include "CoreInterface\MessageManage\RecentContact.h"
#include "CoreInterface\UA\UserAgentCoreDef.h"
#include "afxmt.h"
#include <queue>
/// <summary>
/// ���ݿ�汾
/// <summary>
/// v102 - ���sid(��������ϢID)
///        �޸�������Ϣ�ֶ�descriptionΪsender_description
///		   �������Ŀ��������Ϣ�ֶ�target_description
///		   ��ӷ�����id(P2P��Ϣ - ��Ŀ��ID��ͬ; Ⱥ����Ϣ�͹����˺���Ϣ - ������ID)
/// v103 - ���isSyns(�Ƿ���ͬ����Ϣ)
/// <summary>
#define MSG_DB_VERSION 105

namespace MESSAGE_MANAGER
{
	//��Ϣ���ѷ�ʽ
	enum MSG_TIP_TYPE
	{
		//��������
		TIP_SOUND = 1,
		//��������
		TIP_FLICKER = 2,
		//�������
		TIP_REDPOINT = 4,
		//������Ϣ
		TIP_ROAMING = 5,
	};

/// <summary>
/// ��ʱ��Ϣ��������
/// ΪԤ�����ṩ����
/// <summary>
struct IMBasicData
{
	CString dbMID;
	CString serverMID;
	CString targetUID;
	CString targetDescription;
	CString senderUID;
	CString senderDescription;
	CString message;
	CString messageJson;
	CString messageTopic;
	MessageType messageType;
	ChatType chatType;	
	__int64 time;
	__int64 tss, tsc;
	__int64 pid;
	BOOL isReceive;		
	BOOL isSyns;
	BOOL hasRead;
	BOOL isActive;
	BOOL isRoaming;
public:
	IMBasicData()
	{
		dbMID = CreateGuid();
		messageType = MT_Text;
		chatType = CT_P2P;
		time = tss = tsc = _time64(NULL)*pow(10.0, 6);
		isReceive = FALSE;
		isSyns = FALSE;
		hasRead = FALSE;
		isActive = TRUE;
		isRoaming = FALSE;
		pid = 0;
		targetDescription = '0';
		messageTopic = _T("");
	}

	IMBasicData& operator=(const IMBasicData& t)
	{
		if (this == &t)
			return *this;
		dbMID = t.dbMID;
		serverMID = t.serverMID;
		targetUID = t.targetUID;
		targetDescription = t.targetDescription;
		senderUID = t.senderUID;
		senderDescription = t.senderDescription;
		message = t.message;
		messageJson = t.messageJson;
		messageType = t.messageType;
		chatType = t.chatType;
		time = t.time;
		tss = t.tss;
		tsc = t.tsc;
		pid = t.pid;
		isReceive = t.isReceive;
		isSyns = t.isSyns;
		hasRead = t.hasRead;
		isActive = t.isActive;
		messageTopic = t.messageTopic;
		isRoaming = t.isRoaming;
		return *this;
	}
};

/// <summary>
/// ��ʱ��Ϣ��¼��Ϣ
/// ���ڼ�ʱ��Ϣ���ӳټ�¼
/// <summary>
struct IMRecord
{
	/// <summary>
	/// ��������
	/// <summary>
	ChatType chatType;
	/// <summary>
	/// ��Ϣ�����ݿ�ID
	/// <summary>
	CString dbMID;
	/// <summary>
	/// ��������ID
	/// <summary>
	CString targetID;
	/// <summary>
	/// ��Ϣ�������ݿ����
	/// <summary>
	CString recordSQL;
	/// <summary>
	///��������, 0: �������ѣ�MSG_TIP_TYPE�е�ֵ�����
	/// <summary>	
	int nTipType;
};

/// <summary>
/// ��ʱ��Ϣ��¼��Ϣ
/// ���ڼ�ʱ��Ϣ���ӳ�ˢ��
/// <summary>
struct IMUpdate
{
	/// <summary>
	/// ��������
	/// <summary>
	ChatType chatType;
	/// <summary>
	/// ��������ID
	/// <summary>
	CString targetUID;
	/// <summary>
	/// ��Ҫ���µ���ϢID
	/// <summary>
	list<CString> dbMIDs;
	/// <summary>
	///��������, 0: �������ѣ�MSG_TIP_TYPE�е�ֵ�����
	/// <summary>	
	int nTipType;
};

/// <summary>
/// ����Ϣ���½ṹ
/// <summary>
struct SubMsgUpdate
{
#define SMF_SHOWSTYLE 1
#define SMF_SMALLURI 2
#define SMF_URI 4
#define SMF_LOCALPATH 8
#define SMF_STATE 16
#define SMF_LASTTRANSSIZE 32
#define SMF_LASTCHECKSUM 64
#define SMF_TRANSID 128
#define SMF_SESSIONID 256

	/// <summary>
	/// �Ƿ���±�־����
	/// <summary>
	DWORD dwFlag;
	/// <summary>
	/// ��ʾ��ʽ
	/// <summary>
	int showStyle;
	/// <summary>
	/// ����ͼ��������
	/// <summary>
	CString smallUri;
	/// <summary>
	/// �ļ���������
	/// <summary>
	CString uri;
	/// <summary>
	/// �ļ��洢·��
	/// <summary>
	CString fileFullPath;
	/// <summary>
	/// ����״̬
	/// <summary>
	FileTransState state;	
	/// <summary>
	/// ���һ�δ���Ĵ�С
	/// <summary>
	int lastTranSize;
	/// <summary>
	/// �ϴ��ļ���У���
	/// <summary>
	CString lastChecksum;
	/// <summary>
	/// �ļ�����ID
	/// <summary>
	int transID;
	/// <summary>
	/// �ļ�����sessionID
	/// <summary>
	CString sessionID;

	SubMsgUpdate()
	{
		dwFlag = 0;
		showStyle = SUBMSG_ME_SHOW_CH_SHOW;
		smallUri = _T("");
		uri = _T("");
		fileFullPath = _T("");
		state = Trans_Unknown;
		lastTranSize = 0;
		lastChecksum = _T("");
		transID = -1;
		sessionID = _T("");
	}
};

/// <summary>
/// ��ʱ��Ϣ��¼��ϢMap
/// <summary>
typedef map<CString, struct IMRecord> IMRecordMap;

/// <summary>
/// ��ʱ��Ϣ��¼��Ϣ����
/// <summary>
typedef queue<struct IMRecord> IMRecordQueue;

/// <summary>
/// ��ʱ��Ϣ����Map
/// <summary>
typedef map<CString, struct IMUpdate> IMUpdateMap;
/********************************************************************
*
*   Copyright (C) 2014
*
*   ժҪ: ��Ϣ������
*
*******************************************************************/
class CMessageManager : public CDBManager
{
public:
	/// <summary>
	/// ��Ϣ������
	/// <summary>
	CMessageManager();
	/// <summary>
	/// ��Ϣ������
	/// <summary>
	/// <param name="pathDB">���ݿ�·��</param>
	CMessageManager(CString pathDB);
	/// <summary>
	/// ��Ϣ������
	/// <summary>
	~CMessageManager();
private:
	/// <summary>
	/// ��Ϣ���ݿ�����
	/// <summary>
	CString nameMessageDB;
	/// <summary>
	/// ��ʱ��Ϣ��
	/// <summary>
	CCriticalSection m_csIM;
	/// <summary>
	/// ��ʱ��Ϣ�б�
	/// <summary>
	list<CInstantMessage*> m_listInstantMessage;
private:
	/// <summary>
	/// ��ʱ��Ϣ���������б�, ΪԤ�����ṩ����
	/// <summary>
	queue<IMBasicData*> queueIMData;
	/// <summary>
	/// ��ʱ��Ϣ���������б�����ٽ���
	/// <summary>
	CCriticalSection csBufferIM;
	/// <summary>
	/// ��ʱ��ϢԤ�����߳�
	/// <summary>
	HANDLE threadPreprocessIM;
	/// <summary>
	/// ��ʱ��ϢԤ�����߳��Ƿ�ִ��
	/// <summary>
	bool preprocessIMDie;

	/// <summary>
	/// ��Ҫ��¼�ļ�ʱ��Ϣ
	/// <summary>
	IMRecordQueue imRecord;
	/// <summary>
	/// ��ʱ��Ϣ��¼��Ϣ�����ٽ���
	/// <summary>
	CCriticalSection csRecordIM;
	/// <summary>
	/// ��һ����ʱ��Ϣʱ���
	/// <summary>
	SYSTEMTIME timeLastRecord;
	/// <summary>
	/// ��ʱ��Ϣ����ʱ����(����)
	/// <summary>
	int imRecordInterval;
public:
	/// <summary>
	/// ��Ҫ���µļ�ʱ��Ϣ
	/// <summary>
	IMUpdateMap imUpdate;
	/// <summary>
	/// ��ʱ��Ϣ���·����ٽ���
	/// <summary>
	CCriticalSection csUpdateIM;
	//MQTT���ӳɹ���ʱ��
	time_t m_tsMQTTConnected;
private:
	/// <summary>
	/// ��¼��Ҫ��¼�ļ�ʱ��Ϣ
	/// <summary>
	bool RecordRIM(CString dbMID, IMRecord im);
	bool RecordRIM();

	/// <summary>
	/// ��¼��Ҫ���µļ�ʱ��Ϣ
	/// <summary>
	/// <param name="dbMID">��ʱ��Ϣ���ݿ�ID</param>
	/// <param name="targetUID">��������UID</param>
	/// <param name="chatType">��������</param>
	bool RecordUIM(CString dbMID, CString targetUID, ChatType chatType, int nTipType = TIP_SOUND|TIP_FLICKER|TIP_REDPOINT);
private:
	/// <summary>
	/// �������ݿ��(�����ﶨ�����ݿ���Ϣ�洢��ṹ)
	/// <summary>
	void SetTable();
public:
	/// <summary>
	/// �������ݿ��(�ɰ汾)
	/// <summary>
	void SetOldTable();
	/// <summary>
	/// �������ݿ�·��
	/// <summary>
	/// <param name="pathDB">���ݿ�·��</param>
	void SetPath(CString pathDB);
public:
	//////////////////////////////////////////////////////////////////////////////////////////////
    //
	//  ��Ϣ��¼�ӿ�
	//  (�˽ӿ��Է��������͵Ļ�������������͵�JSON����Ϊ��Ϣ��������, ���м�ʱ��Ϣ��������ͨ������JSON����ȡ)
	//
	//////////////////////////////////////////////////////////////////////////////////////////////
	/// <summary>
	/// ��¼��Ϣ��������Ϣ�е�json��
	/// <summary>	
	void RecordMessageJson(IMBasicData imData);
	/// <summary>
	/// ��¼��Ϣ
	/// <summary>	
	void RecordMessageJson2(IMBasicData imData);

	/// <summary>
	/// ��ʼԤ����ʱ��Ϣ
	/// Ԥ�������(��Ϣ����ת��Ϊ��Ϣ��¼���ݿ����)
	/// <summary>
	void StartPreprocessIM();
	/// <summary>
	/// ֹͣԤ����ʱ��Ϣ
	/// <summary>
	/// <param name="force">�Ƿ�ǿ��ֹͣ��ϢԤ�����߳�</param>
	void StopPreprocessIM(bool force = false);

	/// <summary>
	/// ��ʼ����Ԥ����
	/// Ԥ�������(�����ݿ�ȡ�����ݲ����ս���������������ڽ������ݸ���)
	/// <summary>
	void StartUpdatePreprocess();
	/// <summary>
	/// ֹͣ����Ԥ����
	/// <summary>
	/// <param name="force">�Ƿ�ǿ��ֹͣ����Ԥ�����߳�</param>
	void StopUpdatePreprocess(bool force = false);
private:
	/// <summary>
	/// ��¼��Ϣ(SQL����������)
	/// <summary>
	/// <param name="sqls">SQL����б�</param>
	void RecordMessageTransation(list<CString> sqls);

	/// <summary>
	/// ������¼��ʱ��ϢSQL���
	/// ���ڻ�ȽϺ�ʱ��Ҫʹ���̴߳���
	/// <summary>	
	CString BuildRecodeMessageSQL2(IMBasicData imData);

	/// <summary>
	/// ��IM���ݴӷ���������תΪ�������ݿ������
	/// <summary>	
	bool ConvertIMFromServToDB(IMBasicData *pIMData);
	
	/// <summary>
	/// ����Ҫ��¼�ļ�ʱ��Ϣ����Ԥ����
	/// <summary>
	static DWORD WINAPI PreporcessIM(LPVOID param);
public:
	//////////////////////////////////////////////////////////////////////////////////////////////
    //
	//  ��Ϣɾ���ӿ�
	//
	//////////////////////////////////////////////////////////////////////////////////////////////
	/// <summary>
	/// ɾ����Ϣ(������Ϣ���ݿ�IDɾ����Ϣ)
	/// <summary>
	/// <param name="dbMID">��Ϣ�����ݿ�ID</param>
	void DeleteMessage(CString dbMID);
	/// <summary>
	/// �����Ϣ(ɾ��������Ϣ)
	/// <summary>
	void ClearMessage();
	/// <summary>
	/// �����Ϣ(���ĳ��Ŀ��ID��Ӧ��������Ϣ)
	/// <summary>
	/// <param name="targetUID">�������ID</param>
	void ClearMessage(CString targetUID);
	/// <summary>
	/// ɾ��ĳ���Ự��һ��ʱ��㿪ʼ֮���ȫ����Ϣ
	/// <summary>
	/// <param name="targetId">Ŀ��ID</param>
	/// <param name="time">��ʼʱ��</param>
	void DeleteMessageFrom(CString targetId, __int64 time);
public:
	//////////////////////////////////////////////////////////////////////////////////////////////
    //
	//  ��ȡ�����ϵ�˽ӿ�
	//
	//////////////////////////////////////////////////////////////////////////////////////////////
	/// <summary>
	/// ��ȡ�����ϵ��(����Ŀ��ID��ȡ�����ϵ����Ϣ)
	/// <summary>
	/// <param name="targetUID">�������ID</param>
	/// <param name="ret">�����ϵ����Ϣ</param>
	void GetRecentContact(CString targetUID, CRecentContact& ret);
	/// <summary>
	/// ��ȡ�����ϵ���б�
	/// **���øú�������Ҫ���е���MESSAGE_MANAGER::RealseContacts(contacts)�����ͷŻ�ȡ���������ϵ����Ϣ
	/// <summary>
	/// <param name="ret">�����ϵ����Ϣ</param>
	void GetRecentContacts(list<CRecentContact*>& ret);
	//�����ݿ�ɾ�������Ϣ�б��ĳ����¼
	void DeleteRecordFrom(CString uid);

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
    //
	//  ��ȡ��Ϣ�ӿ�
	//
	//////////////////////////////////////////////////////////////////////////////////////////////
	/// <summary>
	/// ��ȡ��Ϣ����(������Ϣ���ݿ�ID��ȡ��Ϣ����)
	/// <summary>
	/// <param name="dbMID">��Ϣ�����ݿ�ID</param>
	/// <param name="ret">��ʱ��Ϣ</param>
	void GetMessage(CString dbMID, CInstantMessage& ret, bool setRead=false);
	/// <summary>
	/// ��ȡ��Ϣ����(������Ϣ���ݿ�ID��ȡ��Ϣ����)
	/// <summary>
	/// <param name="dbMID">��Ϣ�����ݿ�ID</param>
	/// <param name="ret">��ʱ��Ϣ</param>
	void GetMessage(CString dbMID, CInstantMessage* &ret);
	/// <summary>
	/// ��ȡ��ʱ��Ϣ(�����������ID��ȡδ����ʱ��Ϣ, ��ҳ������)
	/// **���øú�������Ҫ���е���MESSAGE_MANAGER::RealseMessages(messages)�����ͷŻ�ȡ������Ϣ
	/// <summary>
	/// <param name="targetUID">�������ID</param>
	/// <param name="ret">��ʱ��Ϣ</param>
	void GetUnreadMessages(CString targetUID, list<CInstantMessage*>& ret);
	/// <summary>
	/// ��ȡ��ʱ��Ϣ(�����������ID��ȡ��ʱ��Ϣ, ��ҳ������)
	/// **���øú�������Ҫ���е���MESSAGE_MANAGER::RealseMessages(messages)�����ͷŻ�ȡ������Ϣ
	/// <summary>
	/// <param name="targetUID">�������ID</param>
	/// <param name="ret">��ʱ��Ϣ</param>
	/// <param name="setRead">�Ƿ���Ҫ�����Ѷ�</param>
	void GetMessages(CString targetUID, list<CInstantMessage*>& ret, bool setRead = false);
	/// <summary>
	/// ��ȡָ��������ʱ��Ϣ(��ҳ������)
	/// **���øú�������Ҫ���е���MESSAGE_MANAGER::RealseMessages(messages)�����ͷŻ�ȡ������Ϣ
	/// <summary>
	/// <param name="ret">��ʱ��Ϣ</param>
	/// <param name="count">��ʱ��Ϣ����</param>
	/// <param name="setRead">�Ƿ���Ҫ�����Ѷ�</param>
	void GetMessages(list<CInstantMessage*>& ret, int count, bool setRead = false);
	/// <summary>
	/// ��ȡ��ʱ��Ϣ(����ҳ��������ȡ��ʱ��Ϣ)
	/// **���øú�������Ҫ���е���MESSAGE_MANAGER::RealseMessages(messages)�����ͷŻ�ȡ������Ϣ
	/// <summary>
	/// <param name="page">ҳ��</param>
	/// <param name="count">ÿҳ��Ϣ����</param>
	/// <param name="ret">��ʱ��Ϣ</param>
	/// <param name="setRead">�Ƿ���Ҫ�����Ѷ�</param>
	void GetMessages(int page, int count, list<CInstantMessage*>& ret, bool setRead = false);
	/// <summary>
	/// ��ȡ��ʱ��Ϣ(�����������ID��ҳ��������ȡĳ��ļ�ʱ��Ϣ)
	/// **���øú�������Ҫ���е���MESSAGE_MANAGER::RealseMessages(messages)�����ͷŻ�ȡ������Ϣ
	/// <summary>
	/// <param name="targetUID">�������ID</param>
	/// <param name="page">ҳ��</param>
	/// <param name="count">ÿҳ��Ϣ����</param>
	/// <param name="ret">��ʱ��Ϣ</param>
	/// <param name="date">����</param>
	/// <param name="isUnread">�Ƿ�δ��</param>
	/// <param name="isAsc">�Ƿ�����</param>
	/// <param name="setRead">�Ƿ���Ҫ�����Ѷ�</param>
	void GetMessages(CString targetUID, int page, int count, list<CInstantMessage*>& ret, CString date = _T(""), bool isUnread = false, bool isAsc = true, bool setRead = false);
	/// <summary>
	/// ��ȡ��ʱ��Ϣ
	/// <summary>
	/// <param name="targetUID">�������ID</param>
	/// <param name="first">������Ϣ���</param>
	/// <param name="count">ÿҳ��Ϣ����</param>
	/// <param name="ret">��ʱ��Ϣ</param>
	/// <param name="date">����</param>
	/// <param name="isUnread">�Ƿ�δ��</param>
	/// <param name="isAsc">�Ƿ�����</param>
	/// <param name="setRead">�Ƿ���Ҫ�����Ѷ�</param>
	void GetMessagesEx(CString targetUID, int first, int count, list<CInstantMessage*>& ret, CString date = _T(""), bool isUnread = false, bool isAsc = true, bool setRead = false);
	/// <summary>
	/// ��ȡ��ʱ��Ϣ(����ҳ��������ȡĳ��ļ�ʱ��Ϣ)
	/// **���øú�������Ҫ���е���MESSAGE_MANAGER::RealseMessages(messages)�����ͷŻ�ȡ������Ϣ
	/// <summary>
	/// <param name="date">����</param>
	/// <param name="page">ҳ��</param>
	/// <param name="count">ÿҳ��Ϣ����</param>
	/// <param name="ret">��ʱ��Ϣ</param>
	/// <param name="setRead">�Ƿ���Ҫ�����Ѷ�</param>
	void GetMessagesD(CString date, int page, int count, list<CInstantMessage*>& ret, bool setRead = false);
	/// <summary>
	/// ��ȡ��ʱ��Ϣ(����ҳ��������ȡ����ĳ���ؼ��ֵļ�ʱ��Ϣ)
	/// **���øú�������Ҫ���е���MESSAGE_MANAGER::RealseMessages(messages)�����ͷŻ�ȡ������Ϣ
	/// <summary>
	/// <param name="key">��Ϣ���ݹؼ���</param>
	/// <param name="page">ҳ��</param>
	/// <param name="count">ÿҳ��Ϣ����</param>
	/// <param name="ret">��ʱ��Ϣ</param>
	/// <param name="targetUID">�������ID</param>
	/// <param name="setRead">�Ƿ���Ҫ�����Ѷ�</param>
	void GetMessagesK(CString key, int page, int count, list<CInstantMessage*>& ret, CString targetUID = _T(""), bool setRead = false);

	//��Ϣ���ݿ⸴�ƺ�����
	void CopyAndUpdateMessage(CMessageManager *pMessageMan);

	//��ȡָ������ָ����������Ϣ
	void GetMessages(int offset, int limit, vector<IMBasicData*> &vecIMData);

	//��ȡ��Ϣ��Ӧ��Sql���
	CString GetMessageSql(IMBasicData *pIMData = NULL);
public:
	//////////////////////////////////////////////////////////////////////////////////////////////
    //
	//  ��ȡ��Ϣ�����ӿ�
	//
	//////////////////////////////////////////////////////////////////////////////////////////////
	/// <summary>
	/// ��ȡ��ʱ��Ϣ����
	/// <summary>
	/// <param name="onlyUnread">�Ƿ�ֻ��ȡδ����</param>
	int GetMessageCount(bool onlyUnread = false);
	/// <summary>
	/// ��ȡ��ʱ��Ϣ����(�����������ID��ȡĳ��ļ�ʱ��Ϣ����)
	/// <summary>
	/// <param name="targetUID">�������ID</param>
	/// <param name="date">����</param>
	/// <param name="onlyUnread">�Ƿ�ֻ��ȡδ����</param>
	int GetMessageCount(CString targetUID, CString date = _T(""), bool onlyUnread = false);
	/// <summary>
	/// ��ȡ��ʱ��Ϣ����(��ȡĳ��ļ�ʱ��Ϣ����)
	/// <summary>
	/// <param name="date">����</param>
	/// <param name="onlyUnread">�Ƿ�ֻ��ȡδ����</param>
	int GetMessageCountD(CString date, bool onlyUnread = false);
	/// <summary>
	/// ��ȡ��ʱ��Ϣ����(��ȡĳ�����ĳ���ؼ��ֵļ�ʱ��Ϣ)
	/// <summary>
	/// <param name="key">���ݹؼ���</param>
	/// <param name="targetUID">�������ID</param>
	/// <param name="onlyUnread">�Ƿ�ֻ��ȡδ����</param>
	int GetMessageCountK(CString key, CString targetUID = _T(""), bool onlyUnread = false);
public:
	//////////////////////////////////////////////////////////////////////////////////////////////
    //
	//  �޸���Ϣ���ݽӿ�
	//
	//////////////////////////////////////////////////////////////////////////////////////////////
	/// <summary>
	/// ������Ϣ�Ѷ�(�����������ID������Ϣ�Ѷ�)
	/// <summary>
	/// <param name="targetUID">�������ID</param>
	void SetMessageReadByTargetID(CString targetUID);
	void SetMessageReadByTargetID(CString targetUID,char* tss);
	//ͨ��targetID����target_description��ֵΪ2 ˵������Ϣ�Ѿ�����Ϣ�б�ɾ��
	void SetTargetDescriptionDeleteByTargetID(CString targetUID);
	//ͨ��targetID����target_description��ֵΪ1 ˵�������ϵ�˵ĶԻ��Ѿ�������Ϊ�ö�
	void SetTargetDescriptionTopByTargetID(CString targetUID);
	/// <summary>
	/// ������Ϣ�Ѷ�(������Ϣ���ݿ�ID������Ϣ�Ѷ�)
	/// <summary>
	/// <param name="dbMID">��Ϣ�����ݿ�ID</param>
	void SetMessageReadByMessageID(CString dbMID);
	/// <summary>
	/// ������Ϣ��չ����(������Ϣ���ݿ�ID�޸���Ϣ��չ����)
	/// <summary>
	/// <param name="dbMID">��Ϣ�����ݿ�ID</param>
	/// <param name="messageExten">��Ϣ��չ����</param>
	void SetMessageExtenByMessageID(CString dbMID, CString messageExten);
	/// <summary>
	/// ������Ϣ�Ƿ����
	/// <summary>
	/// <param name="dbMID">��Ϣ�����ݿ�ID</param>
	/// <param name="isActive">�Ƿ񼤻�</param>
	void SetMessageActive(CString dbMID, bool isActive);	
	/// <summary>
	/// ��������Ϣ����
	/// <summary>
	/// <param name="dbMID">��Ϣ�����ݿ�ID</param>
	/// <param name="subMessageID">����Ϣ��ϢID</param>
	/// <param name="type">����Ϣ����</param>
	/// <param name="smUpdate">����Ϣ��Ҫ���µĲ���</param>
	void SetSubMsgParam(CString dbMID, CString subMessageID, SubMessageType type, SubMsgUpdate smUpdate);

	/// <summary>
	/// ������Ϣtsc
	/// <summary>
	/// <param name="targetUID">�������ID</param>
	void CMessageManager::SetMessageTscByMID(CString mid, __int64 tsc);
public:
	/// <summary>
	/// ��ȡ��ʱ��Ϣ
	/// <summary>
	/// <param name="ret">��ʱ��Ϣ</param>
	/// <param name="page">ҳ��</param>
	/// <param name="count">ÿҳ��Ϣ����</param>
	/// <param name="conditions">��ȡ����</param>
	/// <param name="isAsc">�Ƿ�����</param>
	/// <param name="setRead">�Ƿ���Ҫ�����Ѷ�</param>
	void GetMessagesC(list<CInstantMessage*>& ret, int page, int count, CString conditions = _T(""), bool isAsc = true, bool setRead = false);

	/// <summary>
	/// ��ȡ��ʱ��Ϣ
	/// <summary>
	/// <param name="ret">��ʱ��Ϣ</param>
	/// <param name="first">������Ϣ���</param>
	/// <param name="count">ÿҳ��Ϣ����</param>
	/// <param name="conditions">��ȡ����</param>
	/// <param name="isAsc">�Ƿ�����</param>
	/// <param name="setRead">�Ƿ���Ҫ�����Ѷ�</param>
	void GetMessagesCEx(list<CInstantMessage*>& ret, int first, int count, CString conditions = _T(""), bool isAsc = true, bool setRead = false);

	/// <summary>
	/// ��ȡ��ʱ��Ϣ����
	/// <summary>
	/// <param name="conditions">��ȡ����</param>
	/// <param name="onlyUnread">�Ƿ�ֻ��ȡδ����</param>
	int GetMessageCountC(CString conditions = _T(""), bool onlyUnread = false);
private:
	/// <summary>
	/// ������Ϣ�Ѷ�
	/// �Ӳ�ѯ�ֶ���id
	/// <summary>
	/// <param name="condition">��������</param>
	void SetMessageRead(CString condition);
	/// <summary>
	/// ����������Ϣ�Ѷ�
	/// <summary>
	void SetAllMessageRead();
	/// <summary>
	/// ɾ��ת���ַ�
	/// <summary>
	/// <param name="text">�ı�(��������ת���ַ�)</param>
	CString RemoveFace(CString text);

public:
	/// <summary>
	/// ��ʱ��Ϣ���µĻص�����
	/// <summary>
	static pfunIMUpdateCallBack m_pfunIMUpdateCallBack;

	/// <summary>
	/// ��ȡ��Ϣ��������
	/// <summary>
	int GetMessageTipType(time_t nMsgTime, bool isSync = false, bool isRoaming = false);

public:
	/// <summary>
	/// ��ȡtscStart ��tscEndʱ�����ȫ����tsc
	/// <summary>
	int GetTscTimes(CString targetUID, __int64 tscStart, __int64 tscEnd, vector<__int64> &times);
	/// <summary>
	/// ��ȡ���ID
	/// <summary>
	int GetRecvMaxTimePro(char* targetUID,CString& outMaxProTime);
	/// <summary>
	/// ��ȡ���ظ�����Ϣ����
	/// <summary>
	int GetIdByTsc(__int64 tsc,CString &dbID);
	/// <summary>
	/// ��Ϣ�����Ƿ�Ϊ��
	/// <summary>
	bool QueueIMDataIsEmpty();
};

/// <summary>
/// �ͷż�ʱ��Ϣ
/// <summary>
/// <param name="messages">��ʱ��Ϣ�б�</param>
void RealseMessages(list<CInstantMessage*> &messages);
/// <summary>
/// �ͷ������ϵ��
/// <summary>
/// <param name="contacts">�����ϵ���б�</param>
void RealseContacts(list<CRecentContact*> &contacts);

}
/////////////////////////////////////////////////////////////////////////////////////////////////////