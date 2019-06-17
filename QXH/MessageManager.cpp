#include "stdafx.h"
#include "CoreInterface\MessageManage\MessageManager.h"
#include "CoreInterface\Common\SIPHelloUtil.h"
#include "CoreInterface\UA\UserAgentCore.h"
#include "CoreInterface\MessageManage\UserHrefHelper.h"
#include "CoreInterface\Common\Tracer.h"

namespace MESSAGE_MANAGER
{
//#define MESSAGEM_LOG

#define ICT_IMTABLE					_T("IM")
//add by zhangpeng on 20160531 begin
#define ICT_IMFIELD_AUTO_ADD		_T("auto_id")
//add by zhangpeng on 20160531 end
#define ICT_IMFIELD_ID				_T("id")
#define ICT_IMFIELD_SID				_T("sid")//��ֵ
#define ICT_IMFIELD_PID				_T("pid")
#define ICT_IMFIELD_TARGERTID		_T("target_id")
#define ICT_IMFIELD_TARGETDES		_T("target_description")
#define ICT_IMFIELD_SENDERID		_T("sender_id")
#define ICT_IMFIELD_SENDERDES		_T("sender_description")
#define ICT_IMFIELD_MESSAGE			_T("message")
#define ICT_IMFIELD_MESSAGEEXTEN	_T("message_exten")
#define ICT_IMFIELD_MESSAGETYPE		_T("message_type")
#define ICT_IMFIELD_CHATTYPE		_T("chat_type")
#define ICT_IMFIELD_TIME			_T("time_pro")
#define ICT_IMFIELD_TSS				_T("time_serv")
#define ICT_IMFIELD_TSC				_T("time_client")
#define ICT_IMFIELD_RECEIVE			_T("isReceive")
#define ICT_IMFIELD_SYNS			_T("isSyns")
#define ICT_IMFIELD_READ			_T("hasRead")
#define ICT_IMFIELD_ACTIVE			_T("isActive")
#define ICT_IMFIELD_UNREADNUM		_T("unReadNum")
#define ICT_IMFIELD_TOPIC		    _T("message_topic")//��ֵ

pfunIMUpdateCallBack CMessageManager::m_pfunIMUpdateCallBack = NULL;

/// <summary>
/// ��Ϣ������
/// <summary>
CMessageManager::CMessageManager()
{		
	//��ʼ��MQTT���ӳɹ���ʱ��
	m_tsMQTTConnected = -1;
	//��ʱ��ϢԤ�����߳�
	this->threadPreprocessIM = NULL;
	//��ʱ��ϢԤ�����߳��Ƿ�ֹͣ
	this->preprocessIMDie = false;
	//��ʼ��ʱ��
	GetLocalTime(&this->timeLastRecord);
	if(this->timeLastRecord.wSecond+10 > 60)
	{
		if(this->timeLastRecord.wMinute+1>60)
		{
			if(this->timeLastRecord.wHour+1<60)
			{
				this->timeLastRecord.wHour+=1;
			}
		}
		else
		{
			this->timeLastRecord.wMinute+=1;
		}
	}
	else
	{
		this->timeLastRecord.wSecond += 10;
	}
	//��ʼ������״̬����ʱ����
	this->imRecordInterval = 500;

	//������Ϣ���ݿ�����
	this->nameMessageDB.Format(_T("MessageV%d.db"), MSG_DB_VERSION);
	//���ò���
	SetParams(_T(""), this->nameMessageDB, _T(""));
	//�������ݱ��
	SetTable();
}

/// <summary>
/// ��Ϣ������
/// <summary>
CMessageManager::~CMessageManager()
{
	DEBUGCODE(Tracer::Log(_T("CMessageManager::~CMessageManager"));)
	CSingleLock lock(&m_csIM);
	lock.Lock();
	for(list<CInstantMessage*>::iterator iter = m_listInstantMessage.begin(); iter != m_listInstantMessage.end(); iter++)
	{
		CInstantMessage* instantMessage = (*iter);
		if(instantMessage)
			delete instantMessage;
	}
	m_listInstantMessage.clear();
	lock.Unlock();
	DEBUGCODE(Tracer::Log(_T("CMessageManager::~CMessageManager EXIT"));)
}

/// <summary>
/// ��Ϣ������
/// <summary>
/// <param name="path">���ݿ�·��</param>
CMessageManager::CMessageManager(CString path)
{
	//��ʼ��MQTT���ӳɹ���ʱ��
	m_tsMQTTConnected = -1;
	//������Ϣ���ݿ�����
	this->nameMessageDB.Format(_T("MessageV%d.db"), MSG_DB_VERSION);
	//���ò���
	SetParams(path, this->nameMessageDB, _T(""));
	//�������ݱ��
	SetTable();
}

/// <summary>
/// ��¼��Ҫ��¼�ļ�ʱ��Ϣ
/// <summary>
/// <param name="dbMID">��ʱ��Ϣ���ݿ�ID</param>
/// <param name="im">��Ҫ��¼�ļ�ʱ��Ϣ��Ϣ</param>
bool CMessageManager::RecordRIM()
{
	//�洢��ʱ����
	bool toRecord = false;
	CSingleLock lockRecordIM(&this->csRecordIM);
	lockRecordIM.Lock();
	//����״̬���¼�¼ʱ��
	SYSTEMTIME timeNow;
	GetLocalTime(&timeNow);
	ULARGE_INTEGER fTimeNow;
	ULARGE_INTEGER fTimeLast;
	SystemTimeToFileTime(&timeNow, (FILETIME*)&fTimeNow);
	SystemTimeToFileTime(&this->timeLastRecord, (FILETIME*)&fTimeLast);
	unsigned __int64 spanT = fTimeNow.QuadPart - fTimeLast.QuadPart;
	unsigned __int64 span = spanT / 10000;
	//DEBUGCODE(Tracer::LogToConsole(_T("[CMessageManager::RecordRIM] [span]:%d [dbMID]:%s"), span, dbMID);)
	
	list<CString> sqls;	
	if (span >= this->imRecordInterval || this->imRecord.size() >= 500)
	{
		//(ʱ������洢)����(��������洢), ������Ҫ����, ����ʱ��
		GetLocalTime(&this->timeLastRecord);
		if (this->imRecord.size() > 0)
		{
			//���������ʱ�����Ԥ�������ݼ�¼���ݿ�������
			while (!this->imRecord.empty())
	        {
				//�������ݿ�������
				IMRecord record = this->imRecord.front();
				sqls.push_back(record.recordSQL);
				//��¼���²���
				if(record.nTipType != TIP_ROAMING)
					RecordUIM(record.dbMID, record.targetID, record.chatType, record.nTipType);
				this->imRecord.pop();
	        }
	
			toRecord = true;
		}
	}
	lockRecordIM.Unlock();
	
	//��¼
	if (toRecord && sqls.size() > 0)
	{
		//�����������д洢
		//DEBUGCODE(Tracer::LogToConsole(_T("[CMessageManager::RecordRIM] 1.1 [sql size]:%d"), sqls.size());)
		this->RecordMessageTransation(sqls);
		DEBUGCODE(Tracer::LogToConsole(_T("[CMessageManager::RecordRIM] 1"));)
	
		if (this->m_pfunIMUpdateCallBack)
		{
			//�����������и���
			CSingleLock lockUpdateIM(&this->csUpdateIM);
			lockUpdateIM.Lock();
			if (this->imUpdate.size() > 0)
			{
				for (IMUpdateMap::iterator iter = this->imUpdate.begin(); iter != this->imUpdate.end(); iter++)
				{
					DEBUGCODE(Tracer::LogToConsole(_T("[MessageManager::RecordRIM] [targetUID]:%s [countM]:%d"), iter->second.targetUID, iter->second.dbMIDs.size());)
				}
			}
			lockUpdateIM.Unlock();
	
			this->m_pfunIMUpdateCallBack();
		}
		return true;
	}
	return false;
}

/// <summary>
/// ��¼��Ҫ��¼�ļ�ʱ��Ϣ
/// <summary>
/// <param name="dbMID">��ʱ��Ϣ���ݿ�ID</param>
/// <param name="im">��Ҫ��¼�ļ�ʱ��Ϣ��Ϣ</param>
bool CMessageManager::RecordRIM(CString dbMID, IMRecord im)
{
	if (dbMID != _T(""))
	{
		//�洢��ʱ����
		bool toRecord = false;
		CSingleLock lockRecordIM(&this->csRecordIM);
		lockRecordIM.Lock();
		//���ں�������״̬�仯״̬�����, �����������
		this->imRecord.push(im);
		//����״̬���¼�¼ʱ��
		SYSTEMTIME timeNow;
		GetLocalTime(&timeNow);
		ULARGE_INTEGER fTimeNow;
		ULARGE_INTEGER fTimeLast;
		SystemTimeToFileTime(&timeNow, (FILETIME*)&fTimeNow);
		SystemTimeToFileTime(&this->timeLastRecord, (FILETIME*)&fTimeLast);
		unsigned __int64 spanT = fTimeNow.QuadPart - fTimeLast.QuadPart;
		int span = spanT / 10000;
		//DEBUGCODE(Tracer::LogToConsole(_T("[CMessageManager::RecordRIM] [span]:%d [dbMID]:%s"), span, dbMID);)

		list<CString> sqls;	
		if (span >= this->imRecordInterval || this->imRecord.size() >= 500)
		{
			//(ʱ������洢)����(��������洢), ������Ҫ����, ����ʱ��
			GetLocalTime(&this->timeLastRecord);
			if (this->imRecord.size() > 0)
			{
				//���������ʱ�����Ԥ�������ݼ�¼���ݿ�������
				while (!this->imRecord.empty())
                {
					//�������ݿ�������
					IMRecord record = this->imRecord.front();
					sqls.push_back(record.recordSQL);
					//��¼���²���
					RecordUIM(record.dbMID, record.targetID, record.chatType, record.nTipType);
					this->imRecord.pop();
                }

				toRecord = true;
			}
		}
		lockRecordIM.Unlock();

		//��¼
		if (toRecord && sqls.size() > 0)
		{
			//�����������д洢
			//DEBUGCODE(Tracer::LogToConsole(_T("[CMessageManager::RecordRIM] 1.1 [sql size]:%d"), sqls.size());)
			this->RecordMessageTransation(sqls);
			DEBUGCODE(Tracer::LogToConsole(_T("[CMessageManager::RecordRIM] 1"));)

			if (this->m_pfunIMUpdateCallBack)
			{
				//�����������и���
				CSingleLock lockUpdateIM(&this->csUpdateIM);
				lockUpdateIM.Lock();
				if (this->imUpdate.size() > 0)
				{
					for (IMUpdateMap::iterator iter = this->imUpdate.begin(); iter != this->imUpdate.end(); iter++)
					{
						DEBUGCODE(Tracer::LogToConsole(_T("[MessageManager::RecordRIM] [targetUID]:%s [countM]:%d"), iter->second.targetUID, iter->second.dbMIDs.size());)
					}
				}
				lockUpdateIM.Unlock();

				this->m_pfunIMUpdateCallBack();
			}
			return true;
		}
	}
	else
	{
		//�洢ǿ�ƴ���
		bool toRecord = false;
		CSingleLock lockRecordIM(&this->csRecordIM);
		lockRecordIM.Lock();
		SYSTEMTIME timeNow;
		GetLocalTime(&timeNow);
		ULARGE_INTEGER fTimeNow;
		ULARGE_INTEGER fTimeLast;
		SystemTimeToFileTime(&timeNow, (FILETIME*)&fTimeNow);
		SystemTimeToFileTime(&this->timeLastRecord, (FILETIME*)&fTimeLast);
		unsigned __int64 spanT = fTimeNow.QuadPart - fTimeLast.QuadPart;
		int span = spanT / 10000;

		list<CString> sqls;	
		if (span >= this->imRecordInterval && this->imRecord.size() > 0)
		{
			if (this->imRecord.size() > 0)
			{
				//����Ԥ�������ݼ�¼���ݿ�������
				while (!this->imRecord.empty())
                {
					//�������ݿ�������
					IMRecord record = this->imRecord.front();
					sqls.push_back(record.recordSQL);
					//��¼���²���
					RecordUIM(record.dbMID, record.targetID, record.chatType, record.nTipType);
					this->imRecord.pop();
                }

				toRecord = true;
			}
		}
		lockRecordIM.Unlock();

		//��¼
		if (toRecord && sqls.size() > 0)
		{
			//�����������д洢
			//DEBUGCODE(Tracer::LogToConsole(_T("[MessageManager::RecordRIM] 2.1 [sql size]:%d"), sqls.size());)
			this->RecordMessageTransation(sqls);
			DEBUGCODE(Tracer::LogToConsole(_T("[MessageManager::RecordRIM] 2"));)

			if (this->m_pfunIMUpdateCallBack)
			{
				//�����������и���
				CSingleLock lockUpdateIM(&this->csUpdateIM);
				lockUpdateIM.Lock();
				if (this->imUpdate.size() > 0)
				{
					for (IMUpdateMap::iterator iter = this->imUpdate.begin(); iter != this->imUpdate.end(); iter++)
					{
						DEBUGCODE(Tracer::LogToConsole(_T("[MessageManager::RecordRIM] [targetUID]:%s [countM]:%d"), iter->second.targetUID, iter->second.dbMIDs.size());)
					}
				}
				lockUpdateIM.Unlock();
				this->m_pfunIMUpdateCallBack();
			}
			return true;
		}
	}
	return false;
}

/// <summary>
/// ��¼��Ҫ���µļ�ʱ��Ϣ
/// <summary>
/// <param name="dbMID">��ʱ��Ϣ���ݿ�ID</param>
/// <param name="targetUID">��������UID</param>
/// <param name="chatType">��������</param>
bool CMessageManager::RecordUIM(CString dbMID, CString targetUID, ChatType chatType, int nTipType)
{
	if (dbMID == _T("") || targetUID == _T(""))
		return false;

	CSingleLock lockUpdateIM(&this->csUpdateIM);
	lockUpdateIM.Lock();
	//�����Ҫ���µļ�ʱ��Ϣ
	//���ս���UID��������
	IMUpdateMap::iterator iter = this->imUpdate.find(targetUID);
	if(iter != this->imUpdate.end())
	{
		//�ҵ���Ҫ��¼����Ϣ��Ӧ�Ľ�������, ��¼��Ϣ���ݿ�ID������������
		iter->second.dbMIDs.push_back(dbMID);
	}
	else
	{
		IMUpdate imUpdate;
		list<CString> dbMIDs;
		dbMIDs.push_back(dbMID);
		imUpdate.chatType = chatType;
		imUpdate.targetUID = targetUID;
		imUpdate.dbMIDs = dbMIDs;
		imUpdate.nTipType = nTipType;
		this->imUpdate[targetUID] = imUpdate;
	}
	lockUpdateIM.Unlock();
	return true;
}

/// <summary>
/// �������ݿ��(�����ﶨ�����ݿ���Ϣ�洢��ṹ)
/// <summary>
void CMessageManager::SetTable()
{
	//���ݿ�������
	DBTable table(ICT_IMTABLE);

	//������ݿ����ֶ�
	//add by zhangpeng on 20160531 begin
	DBTableValue valueAutoID;
	valueAutoID.name = ICT_IMFIELD_AUTO_ADD;
	valueAutoID.type = "INTEGER";
	valueAutoID.Fun = " PRIMARY KEY AUTOINCREMENT";//��������
	table.values.push_back(valueAutoID);
	//add by zhangpeng on 20160531 end

	DBTableValue valueID;
	valueID.name = ICT_IMFIELD_ID;
	valueID.type = "TEXT";
	table.values.push_back(valueID);

	DBTableValue valueSID;
	valueSID.name = ICT_IMFIELD_SID;
	valueSID.type = "TEXT";
	table.values.push_back(valueSID);

	DBTableValue valueTargetID;
	valueTargetID.name = ICT_IMFIELD_TARGERTID;
	valueTargetID.type = "TEXT";
	table.values.push_back(valueTargetID);

	DBTableValue valueTargetDescription;
	valueTargetDescription.name = ICT_IMFIELD_TARGETDES;
	valueTargetDescription.type = "TEXT";
	table.values.push_back(valueTargetDescription);

	DBTableValue valueSenderID;
	valueSenderID.name = ICT_IMFIELD_SENDERID;
	valueSenderID.type = "TEXT";
	table.values.push_back(valueSenderID);

	DBTableValue valueSenderDescription;
	valueSenderDescription.name = ICT_IMFIELD_SENDERDES;
	valueSenderDescription.type = "TEXT";
	table.values.push_back(valueSenderDescription);

	DBTableValue valueMessage;
	valueMessage.name = ICT_IMFIELD_MESSAGE;
	valueMessage.type = "TEXT";
	table.values.push_back(valueMessage);

	DBTableValue valueMessageExten;
	valueMessageExten.name = ICT_IMFIELD_MESSAGEEXTEN;
	valueMessageExten.type = "TEXT";
	table.values.push_back(valueMessageExten);

	DBTableValue valueMessageType;
	valueMessageType.name = ICT_IMFIELD_MESSAGETYPE;
	valueMessageType.type = "TEXT";
	table.values.push_back(valueMessageType);

	DBTableValue valueChatType;
	valueChatType.name = ICT_IMFIELD_CHATTYPE;
	valueChatType.type = "TEXT";
	table.values.push_back(valueChatType);

	DBTableValue valueTimePro;
	valueTimePro.name = ICT_IMFIELD_TIME;
	valueTimePro.type = "INT64";
	table.values.push_back(valueTimePro);

	DBTableValue valueTSS;
	valueTSS.name = ICT_IMFIELD_TSS;
	valueTSS.type = "INT64";
	table.values.push_back(valueTSS);

	DBTableValue valueTSC;
	valueTSC.name = ICT_IMFIELD_TSC;
	valueTSC.type = "INT64";
	table.values.push_back(valueTSC);

	DBTableValue valuePID;
	valuePID.name = ICT_IMFIELD_PID;
	valuePID.type = "INT64";
	table.values.push_back(valuePID);

	DBTableValue valueIsReceive;
	valueIsReceive.name = ICT_IMFIELD_RECEIVE;
	valueIsReceive.type = "BOOL";
	table.values.push_back(valueIsReceive);

	DBTableValue valueIsSyns;
	valueIsSyns.name = ICT_IMFIELD_SYNS;
	valueIsSyns.type = "BOOL";
	table.values.push_back(valueIsSyns);
	
	DBTableValue valueHasRead;
	valueHasRead.name = ICT_IMFIELD_READ;
	valueHasRead.type = "BOOL";
	table.values.push_back(valueHasRead);

	DBTableValue valueIsActive;
	valueIsActive.name = ICT_IMFIELD_ACTIVE;
	valueIsActive.type = "BOOL";
	table.values.push_back(valueIsActive);

	DBTableValue valueTopic;
	valueTopic.name = ICT_IMFIELD_TOPIC;
	valueTopic.type = "TEXT";
	table.values.push_back(valueTopic);

	//��ӱ��
	AddDBTable(table);
}

void CMessageManager::SetOldTable()
{
	//���ݿ�������
	DBTable table(ICT_IMTABLE);

	//������ݿ����ֶ�
	DBTableValue valueID;
	valueID.name = ICT_IMFIELD_ID;
	valueID.type = "TEXT";
	table.values.push_back(valueID);

	DBTableValue valueSID;
	valueSID.name = ICT_IMFIELD_SID;
	valueSID.type = "TEXT";
	table.values.push_back(valueSID);

	DBTableValue valueTargetID;
	valueTargetID.name = ICT_IMFIELD_TARGERTID;
	valueTargetID.type = "TEXT";
	table.values.push_back(valueTargetID);

	DBTableValue valueTargetDescription;
	valueTargetDescription.name = ICT_IMFIELD_TARGETDES;
	valueTargetDescription.type = "TEXT";
	table.values.push_back(valueTargetDescription);

	DBTableValue valueSenderID;
	valueSenderID.name = ICT_IMFIELD_SENDERID;
	valueSenderID.type = "TEXT";
	table.values.push_back(valueSenderID);

	DBTableValue valueSenderDescription;
	valueSenderDescription.name = ICT_IMFIELD_SENDERDES;
	valueSenderDescription.type = "TEXT";
	table.values.push_back(valueSenderDescription);

	DBTableValue valueMessage;
	valueMessage.name = ICT_IMFIELD_MESSAGE;
	valueMessage.type = "TEXT";
	table.values.push_back(valueMessage);

	DBTableValue valueMessageExten;
	valueMessageExten.name = ICT_IMFIELD_MESSAGEEXTEN;
	valueMessageExten.type = "TEXT";
	table.values.push_back(valueMessageExten);

	DBTableValue valueMessageType;
	valueMessageType.name = ICT_IMFIELD_MESSAGETYPE;
	valueMessageType.type = "TEXT";
	table.values.push_back(valueMessageType);

	DBTableValue valueChatType;
	valueChatType.name = ICT_IMFIELD_CHATTYPE;
	valueChatType.type = "TEXT";
	table.values.push_back(valueChatType);

	DBTableValue valueTimePro;
	valueTimePro.name = ICT_IMFIELD_TIME;
	valueTimePro.type = "REAL";
	table.values.push_back(valueTimePro);

	DBTableValue valueIsReceive;
	valueIsReceive.name = ICT_IMFIELD_RECEIVE;
	valueIsReceive.type = "BOOL";
	table.values.push_back(valueIsReceive);

	DBTableValue valueIsSyns;
	valueIsSyns.name = ICT_IMFIELD_SYNS;
	valueIsSyns.type = "BOOL";
	table.values.push_back(valueIsSyns);

	DBTableValue valueHasRead;
	valueHasRead.name = ICT_IMFIELD_READ;
	valueHasRead.type = "BOOL";
	table.values.push_back(valueHasRead);

	DBTableValue valueIsActive;
	valueIsActive.name = ICT_IMFIELD_ACTIVE;
	valueIsActive.type = "BOOL";
	table.values.push_back(valueIsActive);

	DBTableValue valueTopic;
	valueChatType.name = ICT_IMFIELD_TOPIC;
	valueChatType.type = "TEXT";
	table.values.push_back(valueTopic);

	//��ӱ��
	AddDBTable(table);
}

/// <summary>
/// ����·��
/// <summary>
/// <param name="">���ݿ�·��</param>
void CMessageManager::SetPath(CString path)
{
	//�Ͽ���ǰ���ݿ�����
	DisconnectDB();
	this->pathDB = path;
}

void CMessageManager::RecordMessageJson2(IMBasicData imData)
{
	IMBasicData *im = new IMBasicData;
	*im = imData;

	//��¼��Ϣ����
	CSingleLock lock(&this->csBufferIM);
	lock.Lock();
	this->queueIMData.push(im);
	lock.Unlock();	
}

/// <summary>
/// ��¼��Ϣ(SQL����������)
/// <summary>
/// <param name="sqls">SQL����б�</param>
void CMessageManager::RecordMessageTransation(list<CString> sqls)
{
	this->DBTransaction(sqls);
}

CString CMessageManager::BuildRecodeMessageSQL2(IMBasicData imData)
{	
	//ת�����ݣ�serv->db
	bool bRet = ConvertIMFromServToDB(&imData);

	//�������ݿ����
	CString sql;
	if(bRet)
	{
		CString fields;
		fields.Format(
			_T("(%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)"),
			ICT_IMFIELD_ID, ICT_IMFIELD_SID, ICT_IMFIELD_TARGERTID, ICT_IMFIELD_TARGETDES, ICT_IMFIELD_SENDERID, 
			ICT_IMFIELD_SENDERDES, ICT_IMFIELD_MESSAGE, ICT_IMFIELD_MESSAGEEXTEN, ICT_IMFIELD_MESSAGETYPE, 
			ICT_IMFIELD_CHATTYPE, ICT_IMFIELD_TIME, ICT_IMFIELD_TSS, ICT_IMFIELD_TSC,ICT_IMFIELD_PID, ICT_IMFIELD_RECEIVE, 
			ICT_IMFIELD_SYNS, ICT_IMFIELD_READ, ICT_IMFIELD_ACTIVE, ICT_IMFIELD_TOPIC
			);

		CString values;
		values.Format(
			_T("VALUES('%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%d', '%d','%I64d', '%I64d', '%I64d', '%I64d', '%d', '%d', '%d', '%d', '%s')"), 
			imData.dbMID, imData.serverMID, imData.targetUID, imData.targetDescription, imData.senderUID, 
			imData.senderDescription, imData.message, imData.messageJson, (int)(imData.messageType), (int)(imData.chatType),
			imData.time, imData.tss, imData.tsc, imData.pid, imData.isReceive, imData.isSyns, imData.hasRead, imData.isActive, imData.messageTopic
			);
		
		sql.Format(_T("insert into %s %s %s;"), ICT_IMTABLE, fields, values);	
	}	
	return sql;
}

bool CMessageManager::ConvertIMFromServToDB(IMBasicData *pIMData)
{
	if(!pIMData)
		return false;
	int idSubMessage = 1;
	bool bRet = false;
	int len = 0;
	char *pMessageJson = UA_UTIL::ConvertStringToUTF8(pIMData->messageJson, len);
	if(pMessageJson && strlen(pMessageJson))
	{		
		string strJson;
		strJson.assign(pMessageJson);
		Json::Reader reader;
		Json::Value root;
		if (pIMData->messageType == MT_Text)
		{				
			//���ı���Ϣ
			/*
				{
					"co": "message oh"
				}
			*/
			if(!reader.parse(strJson, root))
			{
				DEBUGCODE(Tracer::LogA("[CMessageManager::RecordMessageJson] [json]:[%s] parse failed", strJson.c_str());)				
			}
			else
			{
				CString message;
				string sMessage;
				if (root.type() == Json::objectValue && root.isMember("co") && root["co"].isString())
				{
					//��ȡco�е�������Ϊ�ı�����
					sMessage  = root["co"].asString();		
					Char2CString(sMessage.c_str(), message);
				}

				//������Ϣ��չ����				
				Json::Value rootBuild;
				Json::FastWriter fast_writer;
				//���ı���Ϣ����
				//���촿�ı���������
				string sValueName;
				char cValueName[64] = {0};
				sprintf(cValueName, "%d_text", idSubMessage);
				sValueName.assign(cValueName);				
				//���촿�ı���Ϣ���ݽڵ�
				rootBuild[sValueName] = sMessage;
				string temp = fast_writer.write(rootBuild);							
				Char2CString(temp.c_str(), pIMData->messageJson);

				//��ǰ����һ����Ϣ, ��Ϣ�еı���ת���ַ�����������Ϣ����֮��, ֻ��������Ϣ��չ��Ϣ��
				pIMData->message = RemoveFace(message);				
				bRet = true;
			}
		}
		else if(pIMData->messageType == MT_Map)
		{
			//��ͼ��Ϣ
			pIMData->message = _T("[λ��]");		
			bRet = true;
		}
		else if (pIMData->messageType == MT_Image || pIMData->messageType == MT_Audio || pIMData->messageType == MT_Video || pIMData->messageType == MT_Document || pIMData->messageType == MT_Unknown)
		{
			//�ļ�
			/*
				{
					"co": "http:\\10.0.0.1\ima.jpg",
					"si": "211",
					"fn": "t.txt"
				}
			*/
			CString fileName;			
			if(!reader.parse(strJson, root))
			{
				DEBUGCODE(Tracer::LogA("[CMessageManager::RecordMessageJson] [json]:[%s] parse failed", strJson.c_str());)				
			}
			else
			{
				string sUri, sFilePath, sFileName, sFileSize;
				if (root.type() == Json::objectValue && root.isMember("co"))
				{
					//�ļ��������洢��ַ
					sUri = root["co"].asString();
					if (pIMData->isReceive || pIMData->isSyns)
						//��������¼�¼��Ϣ��ʱ�򱾵ش洢·������Ϊ��, ���ļ�������������ñ���·��
						sFilePath = "";
					else
						//��������¼�¼��Ϣ��ʱ�򱾵ش洢·����uri��ͬ
						sFilePath = sUri;
				}

				if (root.type() == Json::objectValue && root.isMember("si"))
				{
					//�ļ���С
					if (root["si"].type() == Json::stringValue)
						sFileSize = root["si"].asString();
					else if (root["si"].type() == Json::intValue)
					{
						char si[32];
						sprintf(si,"%d",root["si"].asInt());
						sFileSize.assign(si);
					}

					if (sFileSize == "")
						sFileSize = "0";
				}

				if(root.type() == Json::objectValue && root.isMember("fn"))
				{
					//�ļ���ʾ����
					sFileName = root["fn"].asString();
					Char2CString(sFileName.c_str(), fileName);
				}

				//������Ϣ��չ����				
				Json::Value rootBuild;
				Json::Value rootFile;
				Json::FastWriter fast_writer;
				//��ʾ��ʽ
				rootFile["showstyle"] = Json::Value(SUBMSG_ME_SHOW_CH_SHOW);
				//����ͼ��ַ
				rootFile["smalluri"] = "";
				//���ص�ַ
				rootFile["uri"] = sUri;
				//���ش洢·��
				rootFile["localpath"] = sFilePath;				
				//����״̬
				rootFile["TransState"] = Json::Value(Trans_Unknown);
				//���һ�δ���Ĵ�С
				rootFile["lastTranSize"] = Json::Value(0);				
				//У���
				rootFile["lastChecksum"] = "";
				//����id
				rootFile["transID"] = Json::Value(-1);
				//����sessionID
				rootFile["sessionID"] = "";
				if (pIMData->messageType != MT_Image)
				{
					//��ͼƬ�ļ������ļ������ļ���С
					//�ļ���
					rootFile["name"] = sFileName;
					//�ļ���С
					rootFile["size"] = sFileSize;
					//���쵥�ļ���������
					string sValueName;
					char cValueName[64];
					if (pIMData->messageType == MT_Audio)
						sprintf(cValueName, "%d_audio", idSubMessage);
					else
						sprintf(cValueName, "%d_file", idSubMessage);
					sValueName.assign(cValueName);
					//���촿�ı���Ϣ���ݽڵ�
					rootBuild[sValueName] = rootFile;
				}
				else
				{
					//���쵥�ļ���������
					string sValueName;
					char cValueName[64];
					sprintf(cValueName, "%d_image", idSubMessage);
					sValueName.assign(cValueName);
					//���촿�ı���Ϣ���ݽڵ�
					rootBuild[sValueName] = rootFile;
				}

				string temp = fast_writer.write(rootBuild);								
				Char2CString(temp.c_str(), pIMData->messageJson);
				pIMData->message = fileName;
				
				bRet = true;
			}
		}
		else if (pIMData->messageType == MT_RichMedia)
		{
			//��ý��(�������)
			/*
				{
					"co": {
						"ma": {
							"ti": "mat",
							"ab": "maa",
							"pl": "mainjpg",
							"wl": "maincom"
						},
						"op": [
							{
								"te": "t1",
								"pl": "t1jpg",
								"wl": "t1com"
							}
						]
					},
					"si": "",
					"fn": ""
				}
			*/
			CString fileName;			
			if(!reader.parse(strJson, root))
			{
				DEBUGCODE(Tracer::LogA("[CMessageManager::RecordMessageJson] [json]:[%s] parse failed", strJson.c_str());)				
			}
			else
			{
				//����JSONʹ�õĸ��ڵ�
				Json::Value rootBuild;
				Json::Value co;
				if (root.type() == Json::objectValue && root.isMember("co"))
				{
					//��ý����Ϣ���ݽڵ�
					co = root["co"];
				}

				string sMaTi, sMaAb, sMaPl, sMaWl;
				if (co.type() == Json::objectValue && co.isMember("ma"))
				{
					//��ý��������ڵ�
					Json::Value ma = co["ma"];

					//������������Ϣ
					if(ma.type() == Json::objectValue && ma.isMember("ti"))
						//������
						sMaTi = ma["ti"].asString();

					if(ma.type() == Json::objectValue && ma.isMember("ab"))
						//��ժҪ
						sMaAb = ma["ab"].asString();

					if(ma.type() == Json::objectValue && ma.isMember("pl"))
						//������ͼƬUri
						sMaPl = ma["pl"].asString();

					if(ma.type() == Json::objectValue && ma.isMember("wl"))
						//����������
						sMaWl = ma["wl"].asString();

					//����JSONʹ�õ���ʱ�ڵ�
					Json::Value temp;
					string sValueName;
					char cValueName[64];
					////////////////////////////////////////////////////
					temp["text"] = sMaTi;
					temp["uri"] = sMaWl;
					//���������������������
					sprintf(cValueName, "%d_title", idSubMessage);
					sValueName.assign(cValueName);
					rootBuild[sValueName] = temp;
					///////////////////////////////////////////////////
					//����������ժҪ��������
					sprintf(cValueName, "%d_text", ++idSubMessage);
					sValueName.assign(cValueName);
					rootBuild[sValueName] = sMaAb;
					///////////////////////////////////////////////////
					temp.clear();
					temp["uri"] = sMaPl;
					temp["localpath"] = "";
					//����������ͼƬ��������
					sprintf(cValueName, "%d_image", ++idSubMessage);
					sValueName.assign(cValueName);
					rootBuild[sValueName] = temp;
				}

				if (co.type() == Json::objectValue && co.isMember("op"))
				{
					//������������Ϣ, �������и�������Ϣ
					string sOpTe, sOpPl, sOpWl;
					Json::Value ops = co["op"];
					for (int i = 0; i < (int)ops.size(); i++)
					{
						Json::Value op = ops[i];
						if(op.type() == Json::objectValue && op.isMember("te"))
							//������
							sOpTe = op["te"].asString();

						if(op.type() == Json::objectValue && op.isMember("pl"))
							//������ͼƬUri
							sOpPl = op["pl"].asString();

						if(op.type() == Json::objectValue && op.isMember("wl"))
							//����������
							sOpWl = op["wl"].asString();

						//����JSONʹ�õ���ʱ�ڵ�
						Json::Value temp;
						string sValueName;
						char cValueName[64];
						////////////////////////////////////////////////////
						temp["text"] = sOpTe;
						temp["uri"] = sOpWl;
						//���������������������
						sprintf(cValueName, "%d_title", ++idSubMessage);
						sValueName.assign(cValueName);
						rootBuild[sValueName] = temp;
						///////////////////////////////////////////////////
						temp.clear();
						temp["uri"] = sOpPl;
						temp["localpath"] = "";
						//����������ͼƬ��������
						sprintf(cValueName, "%d_image", ++idSubMessage);
						sValueName.assign(cValueName);
						rootBuild[sValueName] = temp;
					}
				}

				//������Ϣ��չ����				
				Json::FastWriter fast_writer;
				string temp = fast_writer.write(rootBuild);				
				Char2CString(temp.c_str(), pIMData->messageJson);
				pIMData->message = fileName;

				//��¼��Ϣ����
				bRet = true;
			}
		}
		else if(pIMData->messageType == MT_GroupNotify || pIMData->messageType == MT_SystemNotify)
		{			
			pIMData->message = pIMData->messageJson;
			pIMData->messageJson = _T("");
			bRet = true;
		}
		else if(pIMData->messageType == MT_LAPPAnnounce)
		{			
			if(reader.parse(strJson, root))
			{							
				if (root.type() == Json::objectValue && root.isMember("title"))
				{
					//��ȡco�е�������Ϊ�ı�����
					string sMessage  = root["title"].asString();		
					Char2CString(sMessage.c_str(), pIMData->message);
				}				
				bRet = true;				
			}
		}		
		else if(pIMData->messageType == MT_Reply)
		{
			//�ظ�����Ϣ
			/*
				{
					"co": "���յ�",
					"from": "PC"
				}
			*/
			if(!reader.parse(strJson, root))
			{
				DEBUGCODE(Tracer::LogA("[CMessageManager::RecordMessageJson] [json]:[%s] parse failed", strJson.c_str());)				
			}
			else
			{								
				if (root.type() == Json::objectValue && root.isMember("co") && root["co"].isString())
				{
					//��ȡco�е�������Ϊ�ı�����
					string sMessage  = root["co"].asString();		
					Char2CString(sMessage.c_str(), pIMData->message);
				}

				//������Ϣ��չ����				
				Json::Value rootBuild;
				Json::FastWriter fast_writer;

				string sValueName;
				char cValueName[64] = {0};
				sprintf(cValueName, "%d_reply", idSubMessage);
				sValueName.assign(cValueName);				

				//���촿�ı���Ϣ���ݽڵ�
				rootBuild[sValueName] = root;
				string temp = fast_writer.write(rootBuild);							
				Char2CString(temp.c_str(), pIMData->messageJson);
						
				bRet = true;
			}
		}

		delete pMessageJson, pMessageJson = NULL;
	}

	pIMData->targetDescription.Replace(_T("\'"), _T("\'\'"));
	pIMData->senderDescription.Replace(_T("\'"), _T("\'\'"));
	pIMData->message.Replace(_T("\'"), _T("\'\'"));
	pIMData->messageJson.Replace(_T("\'"), _T("\'\'"));

	return bRet;
}

/// <summary>
/// ��ʼ����ʱ��Ϣ
/// <summary>
void CMessageManager::StartPreprocessIM()
{
	m_tsMQTTConnected = time(NULL);
	this->preprocessIMDie = false;
	this->threadPreprocessIM = NULL;

	if (!this->threadPreprocessIM)
		this->threadPreprocessIM = CreateThread(NULL, 0, CMessageManager::PreporcessIM, (void *)this, 0, NULL);	
}

/// <summary>
/// ֹͣԤ����ʱ��Ϣ
/// <summary>
/// <param name="force">�Ƿ�ǿ��ֹͣԤ�����߳�</param>
void CMessageManager::StopPreprocessIM(bool force)
{
	m_tsMQTTConnected = -1;
	if (!force)
		this->preprocessIMDie = true;
	else
	{
		if (this->threadPreprocessIM)
		{
			DEBUGCODE(Tracer::LogA("[WARNING][CMessageManager::StopPreprocessIM] TerminateThread");)
			::TerminateThread(this->threadPreprocessIM, 1);
			::CloseHandle(this->threadPreprocessIM);
			this->threadPreprocessIM = NULL;
			this->preprocessIMDie = true;
		}
	}
}

/// <summary>
/// ��ʼ����Ԥ����
/// Ԥ�������(�����ݿ�ȡ�����ݲ����ս���������������ڽ������ݸ���)
/// <summary>
void CMessageManager::StartUpdatePreprocess()
{
}

/// <summary>
/// ֹͣ����Ԥ����
/// <summary>
/// <param name="force">�Ƿ�ǿ��ֹͣ����Ԥ�����߳�</param>
void CMessageManager::StopUpdatePreprocess(bool force)
{
}

/// <summary>
/// ����Ҫ��¼�ļ�ʱ��Ϣ����Ԥ����
/// <summary>
DWORD WINAPI CMessageManager::PreporcessIM(void* params)
{
	SetThreadName(-1,"PreporcessIM");
	CMessageManager *messageManager = (CMessageManager*)params;
	if(!messageManager)
		return 0;
	DEBUGCODE(Tracer::LogToConsole(_T("[Thread is running %s"),_T("PreporcessIM"));)
	while (true)
    {
        try
        {
            while (!messageManager->preprocessIMDie)
            {
				//500����򳬹�500����¼�������ݿ�
				messageManager->RecordRIM();

				//modify by ljj �Ż����洦�����
				CSingleLock lockBufferIM(&messageManager->csBufferIM);
				lockBufferIM.Lock();
				bool isQueueEmpty = messageManager->queueIMData.empty();
				lockBufferIM.Unlock();

                //�����ʱ��ϢԤ�����߳�����
				if (isQueueEmpty)
                {				
					Sleep(10);
                    continue;
                }

                IMBasicData *im = NULL;
                //��ȡһ��������ʱ��Ϣ
				lockBufferIM.Lock();
				im = messageManager->queueIMData.front();
				messageManager->queueIMData.pop();
				lockBufferIM.Unlock();
				if (im)
				{
					//�Լ�ʱ��Ϣ����Ԥ����
					//������ϢID					
					//���켴ʱ��Ϣ�洢���ݿ����
					CString sql = messageManager->BuildRecodeMessageSQL2(*im);
					//��Ԥ���������ڼ�ʱ��Ϣ��¼�б���
					IMRecord record;
					record.chatType = im->chatType;
					record.dbMID = im->dbMID;
					record.recordSQL = sql;
					record.targetID = im->targetUID;
					//�������ѵ��߼�����
					record.nTipType = messageManager->GetMessageTipType(im->tss/1e6, im->isSyns, im->isRoaming);
					//����sql��¼
					CSingleLock lockRecordIM(&messageManager->csRecordIM);
					lockRecordIM.Lock();
					messageManager->imRecord.push(record);
					lockRecordIM.Unlock();

					//�ͷ�����					
					delete im;
					im = NULL;
				}
				////500����򳬹�500����¼�������ݿ�
				//messageManager->RecordRIM();
            }
        }
        catch(...)
        {
        }

        if (messageManager->preprocessIMDie)
        {
			DEBUGCODE(Tracer::Log(_T("[CMessageManager::PreporcessIM] die"));)
            break;
        }
    }

	return 0l;
}

/// <summary>
/// ɾ����Ϣ
/// <summary>
/// <param name="dbMID">��Ϣ�����ݿ�ID</param>
void CMessageManager::DeleteMessage(CString dbMID)
{
	CSingleLock lock(&m_csIM);
	lock.Lock();
	//����Ϣ����Ϣ�б���ɾ��
	for(list<CInstantMessage*>::iterator iter = m_listInstantMessage.begin(); iter != m_listInstantMessage.end(); iter++)
	{
		CInstantMessage* instantMessage = (*iter);
		if(instantMessage && instantMessage->dbMID == dbMID)
		{
			delete instantMessage, (*iter) = NULL;
			m_listInstantMessage.erase(iter);
			break;
		}
	}
	lock.Unlock();

	//�������ݿ����
	CString sql;
	sql.Format(_T("delete from %s where id = '%s'"), ICT_IMTABLE, dbMID);
	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//ִ�м�¼��Ϣ����
	DBExecute(sqlline);
	delete sqlline;
}

void CMessageManager::DeleteMessageFrom(CString targetId, __int64 time)
{
	//�������ݿ����
	CString sql;
	sql.Format(_T("delete from %s where target_id = '%s' and time_serv >= '%I64d';"), ICT_IMTABLE,targetId, time);
	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//ִ�м�¼��Ϣ����
	DBExecute(sqlline);
	delete sqlline;
}

/// <summary>
/// �����Ϣ��¼
/// <summary>
void CMessageManager::ClearMessage()
{
	CSingleLock lock(&m_csIM);
	lock.Lock();
	//�����Ϣ�б�
	for(list<CInstantMessage*>::iterator iter = m_listInstantMessage.begin(); iter != m_listInstantMessage.end(); iter++)
	{
		CInstantMessage* instantMessage = (*iter);
		if(instantMessage)
		{
			delete instantMessage, (*iter) = NULL;					
		}
	}
	m_listInstantMessage.clear();
	lock.Unlock();

	//�������ݿ����
	CString sql;
	sql.Format(_T("delete from \"%s\""), ICT_IMTABLE);
	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//ִ�м�¼��Ϣ����
	DBExecute(sqlline);
	delete sqlline;
}

/// <summary>
/// �����Ϣ
/// <summary>
/// <param name="targetUID">�������ID</param>
void CMessageManager::ClearMessage(CString targetUID)
{
	CSingleLock lock(&m_csIM);
	lock.Lock();
	//����Ϣ����Ϣ�б���ɾ��
	list<list<CInstantMessage*>::iterator> iterList;
	for(list<CInstantMessage*>::iterator iter = m_listInstantMessage.begin(); iter != m_listInstantMessage.end(); iter++)
	{
		CInstantMessage* instantMessage = (*iter);
		if(instantMessage && instantMessage->targetUID == targetUID)
		{
			delete instantMessage, (*iter) = NULL;
			iterList.push_back(iter);
		}
	}
	for(list<list<CInstantMessage*>::iterator>::iterator iterI = iterList.begin(); iterI != iterList.end(); iterI++)
	{
		m_listInstantMessage.erase(*iterI);
	}
	lock.Unlock();

	//�������ݿ����
	CString sql;
	sql.Format(_T("delete from %s where %s = '%s'"), ICT_IMTABLE, ICT_IMFIELD_TARGERTID, targetUID);
	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//ִ�м�¼��Ϣ����
	DBExecute(sqlline);
	delete sqlline;
}

/// <summary>
/// ��ȡ�����ϵ��
/// <summary>
/// <param name="targetUID">�������ID</param>
/// <param name="ret">�����ϵ����Ϣ</param>
void CMessageManager::GetRecentContact(CString targetUID, CRecentContact& ret)
{
	//�������ݿ����
	CString sql;	
	sql.Format(_T("select *,sum(hasRead == 0) as unReadNum from %s where %s = '%s' group by %s, %s order by max(time_pro) asc"), ICT_IMTABLE, ICT_IMFIELD_TARGERTID, targetUID, ICT_IMFIELD_CHATTYPE, ICT_IMFIELD_TARGERTID);
	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//ִ�в�ѯ����
	result = DBSelect(sqlline, row, col);
	delete sqlline;

	//�����ѯ���
	if (row > 0)
	{
		int index = col;
		for(int i = 0 ; i < row; i++)
		{
			CInstantMessage* message = new CInstantMessage();
			for(int j = 0; j < col; j++)
			{
				//�ֶ�����
				CString title;
				DBChar2CString(result[j], title);
				//�ֶ�ֵ
				CString value;
				DBChar2CString(result[index], value);

#ifdef MESSAGEM_LOG
				//��־
				DEBUGCODE(Tracer::LogToConsole(_T("[CMessageManager::GetRecentContacts] - result[%d] = %s:%s"), index, title, value);)
#endif
				if (ICT_IMFIELD_ID == title)
				{
					message->dbMID = value;
				}
				else if (ICT_IMFIELD_SID == title)
				{
					message->serverMID = value;
				}
				else if (ICT_IMFIELD_TARGERTID == title)
				{
					ret.id = value;
					message->targetUID = value;
				}
				else if (ICT_IMFIELD_TARGETDES == title)
				{
					message->targetDescription = value;
				}
				else if (ICT_IMFIELD_SENDERID == title)
				{
					message->senderUID = value;
				}
				else if (ICT_IMFIELD_SENDERDES == title)
				{
					ret.description = value;
					message->senderDescription = value;
				}
				else if (ICT_IMFIELD_MESSAGE == title)
				{
					message->message = value;
				}
				else if (ICT_IMFIELD_MESSAGEEXTEN == title)
				{
					message->messageExten = value;
				}
				else if (ICT_IMFIELD_MESSAGETYPE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					message->messageType = (MessageType)n;
				}
				else if (ICT_IMFIELD_CHATTYPE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					message->chatType = (ChatType)n;
				}
				else if (_T("time_pro") == title)
				{
					message->SetTime(value);
				}
				else if (ICT_IMFIELD_PID == title)
				{
					message->messagePID = _atoi64(result[index]);
				}
				else if (ICT_IMFIELD_TSC == title)
				{
					message->tsc = _atoi64(result[index]);
				}
				else if (ICT_IMFIELD_RECEIVE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isReceive = n;
					message->isReceive = isReceive;
				}
				else if (ICT_IMFIELD_SYNS == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isSyns = n;
					message->isSyns = isSyns;
					ret.isSyns = isSyns;
				}
				else if (ICT_IMFIELD_READ == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL hasRead = n;
					message->hasRead = hasRead;
				}
				else if (ICT_IMFIELD_ACTIVE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isActive = n;
					message->isActive = isActive;
				}
				else if (ICT_IMFIELD_UNREADNUM == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					ret.unReadNum = n;
				}
				else if (ICT_IMFIELD_TOPIC == title)
				{
					message->messageTopic = value;
				}
				++index;
			}
			//��ʱ��Ϣ����
			message->MessageToObject();
			//����
			ret.lastMessage = message;
		}
	}

	//ʹ�ù�����Ҫ����ѯ����ͷ�
	sqlite3_free_table(result);
}

/// <summary>
/// ��ȡ�����ϵ��
/// <summary>
/// <param name="ret">�����ϵ����Ϣ�б�</param>
void CMessageManager::GetRecentContacts(list<CRecentContact*>& ret)
{
	//char *sql = "select * from IM group by chat_type, target_id order by max(time_pro) asc";
	char *sql = "select *,sum(hasRead==0) as unReadNum from IM group by chat_type, target_id having target_description is not 2 order by target_description desc, max(time_pro) desc";
	int row = 0;
	int col = 0;
	char **result;
	//ִ�в�ѯ����
	result = DBSelect(sql, row, col);

	//�����ѯ���
	if (row > 0)
	{
		int index = col;
		for(int i = 0 ; i < row; i++)
		{
			CRecentContact *contact = new CRecentContact();
			CInstantMessage *message = new CInstantMessage();
			for(int j = 0; j < col; j++)
			{
				//�ֶ�����
				CString title;
				DBChar2CString(result[j], title);
				//�ֶ�ֵ
				CString value;
				DBChar2CString(result[index], value);

#ifdef MESSAGEM_LOG
				//��־
				DEBUGCODE(Tracer::LogToConsole(_T("[CMessageManager::GetRecentContacts] - result[%d] = %s:%s"), index, title, value);)
#endif
				if (ICT_IMFIELD_ID == title)
				{
					message->dbMID = value;
				}
				else if (ICT_IMFIELD_SID == title)
				{
					message->serverMID = value;
				}
				else if (ICT_IMFIELD_TARGERTID == title)
				{
					contact->id = value;
					message->targetUID = value;
				}
				else if (ICT_IMFIELD_TARGETDES == title)
				{
					message->targetDescription = value;
				}
				else if (ICT_IMFIELD_SENDERID == title)
				{
					message->senderUID = value;
				}
				else if (ICT_IMFIELD_SENDERDES == title)
				{
					contact->description = value;
					message->senderDescription = value;
				}
				else if (ICT_IMFIELD_MESSAGE == title)
				{
					message->message = value;
				}
				else if (ICT_IMFIELD_MESSAGEEXTEN == title)
				{
					message->messageExten = value;
				}
				else if (ICT_IMFIELD_MESSAGETYPE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					message->messageType = (MessageType)n;
				}
				else if (ICT_IMFIELD_CHATTYPE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					message->chatType = (ChatType)n;
				}
				else if (_T("time_pro") == title)
				{
					message->SetTime(value);
				}
				else if (ICT_IMFIELD_PID == title)
				{
					message->messagePID = _atoi64(result[index]);
				}
				else if (ICT_IMFIELD_TSC == title)
				{
					message->tsc = _atoi64(result[index]);
				}
				else if (ICT_IMFIELD_RECEIVE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isReceive = n;
					message->isReceive = isReceive;
				}
				else if (ICT_IMFIELD_SYNS == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isSyns = n;
					message->isSyns = isSyns;
				}
				else if (ICT_IMFIELD_READ == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL hasRead = n;
					message->hasRead = hasRead;
				}
				else if (ICT_IMFIELD_ACTIVE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isActive = n;
					message->isActive = isActive;
				}
				else if (ICT_IMFIELD_UNREADNUM == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					contact->unReadNum = n;
				}
				else if (ICT_IMFIELD_TOPIC == title)
				{
					message->messageTopic = value;
				}
				++index;
			}

			//��ʱ��Ϣ����
			message->MessageToObject();
			//����
			contact->lastMessage = message;
			ret.push_back(contact);
		}
	}

	//ʹ�ù�����Ҫ����ѯ����ͷ�
	sqlite3_free_table(result);
}
//�����ݿ�ɾ�������Ϣ�б��ĳ����¼
void CMessageManager::DeleteRecordFrom(CString uid)
{
	CString sql;
	sql.Format(_T("delete from %s where '%s' = '%s'"), ICT_IMTABLE, ICT_IMFIELD_TARGERTID, uid);
	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//ִ�м�¼��Ϣ����
	DBExecute(sqlline);
	delete sqlline;
}


void CMessageManager::RecordMessageJson(IMBasicData imData)
{	
	CString strSql = BuildRecodeMessageSQL2(imData);
	if(!strSql.IsEmpty())
	{		
		char *sqlline;
		DBwchar2char(strSql.GetBuffer(strSql.GetLength() + 1), sqlline);
		strSql.ReleaseBuffer();

		//ִ�м�¼��Ϣ����
		DBExecute(sqlline);
		delete sqlline;			
	}	
	return;
}

/// <summary>
/// ��������Ϣ����
/// <summary>
/// <param name="dbMID">��Ϣ�����ݿ�ID</param>
/// <param name="subMessageID">����Ϣ��ϢID</param>
/// <param name="type">����Ϣ����</param>
/// <param name="smUpdate">����Ϣ��Ҫ���µĲ���</param>
void CMessageManager::SetSubMsgParam(CString dbMID, CString subMessageID, SubMessageType type, SubMsgUpdate smUpdate)
{
	//������ϢID��ȡ��Ϣ
	CInstantMessage message;
	this->GetMessage(dbMID, message);
	if (message.dbMID == _T(""))
		return;

	//������չ��Ϣ����, ���޸ı��ش洢·��
	CString messageExten = message.messageExten;

	char cMessageExten[10240] = {0};
	if(!messageExten.IsEmpty())
		CString2Char(messageExten, cMessageExten);

	Json::Reader reader;
	Json::Value root;
	string strJson;
	strJson.assign(cMessageExten);
	if(!reader.parse(strJson, root))
	{
		DEBUGCODE(Tracer::LogA("[CMessageManager::SetSubMsgShowStyle] [json]:[%s] parse failed", strJson.c_str());)
		return;
	}
	else
	{
		//���������������������
		CString valueName;
		if (type == SMT_Image)
			valueName = subMessageID + _T("_image");
		else if (type == SMT_File)
			valueName = subMessageID + _T("_file");
		else if (type == SMT_Audio)
			valueName = subMessageID + _T("_audio");
		else if (type == SMT_Map)
			valueName = subMessageID + _T("_map");
		else
			return;

		char cValueName[64];
		CString2Char(valueName, cValueName);
		string sValueName = cValueName;
		if (root.type() == Json::objectValue && root.isMember(sValueName))
		{
			//��ȡ�ļ��ڵ�
			Json::Value *valueSubMessage = &root[sValueName];
			if (valueSubMessage->type() == Json::objectValue)
			{
				//�޸���ʾ��ʽ
				if(valueSubMessage->isMember("showstyle") && smUpdate.dwFlag & SMF_SHOWSTYLE)
					(*valueSubMessage)["showstyle"] = Json::Value(smUpdate.showStyle);
				//�޸�����ͼ��ַ
				if(valueSubMessage->isMember("smalluri") && smUpdate.dwFlag & SMF_SMALLURI)
				{
					char temp[512] = {0};
					CString2Char(smUpdate.smallUri, temp);
					(*valueSubMessage)["smalluri"] = temp;
				}					
				//�޸����ص�ַ
				if(valueSubMessage->isMember("uri") && smUpdate.dwFlag & SMF_URI)
				{
					char temp[512] = {0};
					CString2Char(smUpdate.uri, temp);
					(*valueSubMessage)["uri"] = temp;
				}					
				//�޸ı���·��
				if(valueSubMessage->isMember("localpath") && smUpdate.dwFlag & SMF_LOCALPATH)
				{
					char temp[512] = {0};
					CString2Char(smUpdate.fileFullPath, temp);
					(*valueSubMessage)["localpath"] = temp;
				}					
				//�޸Ĵ���״̬
				if(valueSubMessage->isMember("TransState") && smUpdate.dwFlag & SMF_STATE)
					(*valueSubMessage)["TransState"] = Json::Value(smUpdate.state);
				//�޸����һ�δ���Ĵ�С
				if(valueSubMessage->isMember("lastTranSize") && smUpdate.dwFlag & SMF_LASTTRANSSIZE)
					(*valueSubMessage)["lastTranSize"] = Json::Value(smUpdate.lastTranSize);
				//�޸�У���
				if(valueSubMessage->isMember("lastChecksum") && smUpdate.dwFlag & SMF_LASTCHECKSUM)
				{
					char temp[512] = {0};
					CString2Char(smUpdate.lastChecksum, temp);
					(*valueSubMessage)["lastChecksum"] = temp;
				}	
				//�޸��ļ�����ID
				if(valueSubMessage->isMember("transID") && smUpdate.dwFlag & SMF_TRANSID)
					(*valueSubMessage)["transID"] = Json::Value(smUpdate.transID);
				//�޸��ļ�����sessionID
				if(valueSubMessage->isMember("sessionID") && smUpdate.dwFlag & SMF_SESSIONID)
				{
					char temp[512] = {0};
					CString2Char(smUpdate.sessionID, temp);
					(*valueSubMessage)["sessionID"] = temp;
				}					

				//Json�ṹ�ı���
				const char* pJsonContent = NULL;
				Json::FastWriter fast_writer;
				string temp = fast_writer.write(root);

				//�������ݿ�����
				pJsonContent = temp.c_str();
				CString messageExten;
				Char2CString(pJsonContent, messageExten);
				this->SetMessageExtenByMessageID(dbMID, messageExten);
			}
		}
	}

	CSingleLock lock(&m_csIM);
	lock.Lock();
	//���¼�ʱ��Ϣ�б��е�չ����ʽ
	for(list<CInstantMessage*>::iterator iter = m_listInstantMessage.begin(); iter != m_listInstantMessage.end(); iter++)
	{
		CInstantMessage* instantMessage = (*iter);
		if(instantMessage->dbMID == dbMID)
		{
			CSubInstantMessage* subInstantMessage = instantMessage->GetSubInstantMessage(subMessageID);
			if(subInstantMessage)
			{
				if(smUpdate.dwFlag & SMF_SHOWSTYLE)
					subInstantMessage->showStyle = smUpdate.showStyle;
				if(smUpdate.dwFlag & SMF_SMALLURI)
					subInstantMessage->smallUri = smUpdate.smallUri;
				if(smUpdate.dwFlag & SMF_URI)
					subInstantMessage->uri = smUpdate.uri;
				if(smUpdate.dwFlag & SMF_LOCALPATH)
					subInstantMessage->fileFullPath = smUpdate.fileFullPath;
				if(smUpdate.dwFlag & SMF_STATE)
					subInstantMessage->state = smUpdate.state;
				if(smUpdate.dwFlag & SMF_LASTTRANSSIZE)
					subInstantMessage->lastTranSize = smUpdate.lastTranSize;
				if(smUpdate.dwFlag & SMF_LASTCHECKSUM)
					subInstantMessage->lastChecksum = smUpdate.lastChecksum;
				if(smUpdate.dwFlag & SMF_TRANSID)
					subInstantMessage->transID = smUpdate.transID;
				if(smUpdate.dwFlag & SMF_SESSIONID)
					subInstantMessage->sessionID = smUpdate.sessionID;
			}
		}
	}
	lock.Unlock();
}

/// <summary>
/// ��ȡ��ʱ��Ϣ
/// <summary>
/// <param name="targetUID">�������ID</param>
/// <param name="ret">��ʱ��Ϣ</param>
/// <param name="setRead">�Ƿ���Ҫ�����Ѷ�</param>
void CMessageManager::GetMessages(CString targetUID, list<CInstantMessage*>& ret, bool setRead)
{
	//�������ݿ����
	CString condition;
	condition.Format(_T("target_id = '%s'"), targetUID);
	CString sql;
	sql.Format(_T("select * from %s where %s order by time_pro asc, rowid asc"), ICT_IMTABLE, condition);
	//sql.Format(_T("select * from %s where %s order by rowid asc"), ICT_IMTABLE, condition);
	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//ִ�в�ѯ����
	result = DBSelect(sqlline, row, col);
	delete sqlline;

	//�����ѯ���
	if (row > 0)
	{
		int index = col;
		for(int i = 0 ; i < row; i++)
		{
			CInstantMessage *message = new CInstantMessage();
			for(int j = 0; j < col; j++)
			{
				CString title;
				DBChar2CString(result[j], title);
				CString value;
				DBChar2CString(result[index], value);

#ifdef MESSAGEM_LOG
				DEBUGCODE(Tracer::LogToConsole(_T("[CMessageManager::GetMessages] - result[%d] = %s:%s"), index, title, value);)
#endif
				if (ICT_IMFIELD_ID == title)
				{
					message->dbMID = value;
				}
				else if (ICT_IMFIELD_SID == title)
				{
					message->serverMID = value;
				}
				else if (ICT_IMFIELD_TARGERTID == title)
				{
					message->targetUID = value;
				}
				else if (ICT_IMFIELD_TARGETDES == title)
				{
					message->targetDescription = value;
				}
				else if (ICT_IMFIELD_SENDERID == title)
				{
					message->senderUID = value;
				}
				else if (ICT_IMFIELD_SENDERDES == title)
				{
					message->senderDescription = value;
				}
				else if (ICT_IMFIELD_MESSAGE == title)
				{
					message->message = value;
				}
				else if (ICT_IMFIELD_MESSAGEEXTEN == title)
				{
					message->messageExten = value;
				}
				else if (ICT_IMFIELD_MESSAGETYPE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					message->messageType = (MessageType)n;
				}
				else if (ICT_IMFIELD_CHATTYPE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					message->chatType = (ChatType)n;
				}
				else if (_T("time_pro") == title)
				{
					message->SetTime(value);
				}
				else if (ICT_IMFIELD_PID == title)
				{
					message->messagePID = _atoi64(result[index]);
				}
				else if (ICT_IMFIELD_TSC == title)
				{
					message->tsc = _atoi64(result[index]);
				}
				else if (ICT_IMFIELD_RECEIVE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isReceive = n;
					message->isReceive = isReceive;
				}
				else if (ICT_IMFIELD_SYNS == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isSyns = n;
					message->isSyns = isSyns;
				}
				else if (ICT_IMFIELD_READ == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL hasRead = n;
					message->hasRead = hasRead;
				}
				else if (ICT_IMFIELD_ACTIVE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isActive = n;
					message->isActive = isActive;
				}
				else if (ICT_IMFIELD_TOPIC == title)
				{
					message->messageTopic = value;
				}
				++index;
			}
			//��ʱ��Ϣ����
			message->MessageToObject();
			//����
			ret.push_back(message);
		}
	}

	//ʹ�ù�����Ҫ����ѯ����ͷ�
	sqlite3_free_table(result);

	if (setRead)
	{
		//������ϢΪ�Ѷ�
		CString updateCondition;
		updateCondition.Format(_T("(select id from %s where %s order by time_pro asc, rowid asc)"), ICT_IMTABLE, condition);
		SetMessageRead(updateCondition);
	}
}

/// <summary>
/// ��ȡ��ʱ��Ϣ
/// ���øú�������Ҫ���е���MESSAGE_MANAGER::RealseMessages(messages)�����ͷŻ�ȡ������Ϣ
/// <summary>
/// <param name="ret">��ʱ��Ϣ</param>
/// <param name="count">��ʱ��Ϣ����</param>
/// <param name="setRead">�Ƿ���Ҫ�����Ѷ�</param>
void CMessageManager::GetMessages(list<CInstantMessage*>& ret, int count, bool setRead)
{
	//�������ݿ����
	CString condition;
	condition.Format(_T("limit %d"), count);
	CString sql;
	sql.Format(_T("select * from %s order by time_pro asc, rowid asc %s"), ICT_IMTABLE, condition);
	//sql.Format(_T("select * from %s order by rowid asc %s"), ICT_IMTABLE, condition);
	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//ִ�в�ѯ����
	result = DBSelect(sqlline, row, col);
	delete sqlline;

	//�����ѯ���
	if (row > 0)
	{
		int index = col;
		for(int i = 0 ; i < row; i++)
		{
			CInstantMessage *message = new CInstantMessage();
			for(int j = 0; j < col; j++)
			{
				CString title;
				DBChar2CString(result[j], title);
				CString value;
				DBChar2CString(result[index], value);

#ifdef MESSAGEM_LOG
				DEBUGCODE(Tracer::LogToConsole(_T("[CMessageManager::GetMessages] - result[%d] = %s:%s"), index, title, value);)
#endif
				if (ICT_IMFIELD_ID == title)
				{
					message->dbMID = value;
				}
				else if (ICT_IMFIELD_SID == title)
				{
					message->serverMID = value;
				}
				else if (ICT_IMFIELD_TARGERTID == title)
				{
					message->targetUID = value;
				}
				else if (ICT_IMFIELD_TARGETDES == title)
				{
					message->targetDescription = value;
				}
				else if (ICT_IMFIELD_SENDERID == title)
				{
					message->senderUID = value;
				}
				else if (ICT_IMFIELD_SENDERDES == title)
				{
					message->senderDescription = value;
				}
				else if (ICT_IMFIELD_MESSAGE == title)
				{
					message->message = value;
				}
				else if (ICT_IMFIELD_MESSAGEEXTEN == title)
				{
					message->messageExten = value;
				}
				else if (ICT_IMFIELD_MESSAGETYPE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					message->messageType = (MessageType)n;
				}
				else if (ICT_IMFIELD_CHATTYPE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					message->chatType = (ChatType)n;
				}
				else if (_T("time_pro") == title)
				{
					message->SetTime(value);
				}
				else if (ICT_IMFIELD_PID == title)
				{
					message->messagePID = _atoi64(result[index]);
				}
				else if (ICT_IMFIELD_TSC == title)
				{
					message->tsc = _atoi64(result[index]);
				}
				else if (ICT_IMFIELD_RECEIVE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isReceive = n;
					message->isReceive = isReceive;
				}
				else if (ICT_IMFIELD_SYNS == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isSyns = n;
					message->isSyns = isSyns;
				}
				else if (ICT_IMFIELD_READ == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL hasRead = n;
					message->hasRead = hasRead;
				}
				else if (ICT_IMFIELD_ACTIVE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isActive = n;
					message->isActive = isActive;
				}
				else if (ICT_IMFIELD_TOPIC == title)
				{
					message->messageTopic = value;
				}
				++index;
			}
			//��ʱ��Ϣ����
			message->MessageToObject();
			//����
			ret.push_back(message);
		}
	}

	//ʹ�ù�����Ҫ����ѯ����ͷ�
	sqlite3_free_table(result);

	if (setRead)
	{
		//������ϢΪ�Ѷ�
		CString updateCondition;
		updateCondition.Format(_T("(select id from %s order by time_pro asc, rowid asc %s)"), ICT_IMTABLE, condition);
		SetMessageRead(updateCondition);
	}
}

/// <summary>
/// ������Ϣ���ݿ�ID��ȡ��Ϣ����
/// <summary>
/// <param name="dbMID">��Ϣ�����ݿ�ID</param>
/// <param name="ret">��ʱ��Ϣ</param>
void CMessageManager::GetMessage(CString dbMID, CInstantMessage& message, bool setRead)
{
	if (dbMID == _T(""))
		return;

	CString sql;
	sql.Format(_T("select * from %s where id = '%s'"), ICT_IMTABLE, dbMID);

	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//ִ�в�ѯ����
	result = DBSelect(sqlline, row, col);
	delete sqlline;

	//�����ѯ���
	if (row > 0)
	{
		int index = col;
		for(int i = 0 ; i < row; i++)
		{
			for(int j = 0; j < col; j++)
			{
				CString title;
				DBChar2CString(result[j], title);
				CString value;
				DBChar2CString(result[index], value);

#ifdef MESSAGEM_LOG
				DEBUGCODE(Tracer::LogToConsole(_T("[CMessageManager::GetMessagesC] - result[%d] = %s:%s"), index, title, value);)
#endif
				if (ICT_IMFIELD_ID == title)
				{
					message.dbMID = value;
				}
				else if (ICT_IMFIELD_SID == title)
				{
					message.serverMID = value;
				}
				else if (ICT_IMFIELD_TARGERTID == title)
				{
					message.targetUID = value;
				}
				else if (ICT_IMFIELD_TARGETDES == title)
				{
					message.targetDescription = value;
				}
				else if (ICT_IMFIELD_SENDERID == title)
				{
					message.senderUID = value;
				}
				else if (ICT_IMFIELD_SENDERDES == title)
				{
					message.senderDescription = value;
				}
				else if (ICT_IMFIELD_MESSAGE == title)
				{
					message.message = value;
				}
				else if (ICT_IMFIELD_MESSAGEEXTEN == title)
				{
					message.messageExten = value;
				}
				else if (ICT_IMFIELD_MESSAGETYPE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					message.messageType = (MessageType)n;
				}
				else if (ICT_IMFIELD_CHATTYPE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					message.chatType = (ChatType)n;
				}
				else if (_T("time_pro") == title)
				{
					message.SetTime(value);
				}
				else if (ICT_IMFIELD_PID == title)
				{
					message.messagePID = _atoi64(result[index]);
				}
				else if (ICT_IMFIELD_TSC == title)
				{
					message.tsc = _atoi64(result[index]);
				}
				else if (ICT_IMFIELD_RECEIVE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isReceive = n;
					message.isReceive = isReceive;
				}
				else if (ICT_IMFIELD_SYNS == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isSyns = n;
					message.isSyns = isSyns;
				}
				else if (ICT_IMFIELD_READ == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL hasRead = n;
					message.hasRead = hasRead;
				}
				else if (ICT_IMFIELD_ACTIVE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isActive = n;
					message.isActive = isActive;
				}
				else if (ICT_IMFIELD_TOPIC == title)
				{
					message.messageTopic = value;
				}
				++index;
			}
		}
		//��ʱ��Ϣ����
		message.MessageToObject();
	}

	//ʹ�ù�����Ҫ����ѯ����ͷ�
	sqlite3_free_table(result);

	if (setRead)
	{
		//������ϢΪ�Ѷ�
		CString updateCondition;
		updateCondition.Format(_T("( '%s')"),dbMID);
		SetMessageRead(updateCondition);
	}
}

/// <summary>
/// ��ȡ��Ϣ����(������Ϣ���ݿ�ID��ȡ��Ϣ����)
/// <summary>
/// <param name="dbMID">��Ϣ�����ݿ�ID</param>
/// <param name="ret">��ʱ��Ϣ</param>
void CMessageManager::GetMessage(CString dbMID, CInstantMessage* &ret)
{
	CSingleLock lock(&m_csIM);
	lock.Lock();
	//��ȥ��ʱ��Ϣ�б��в���
	for(list<CInstantMessage*>::iterator iter = m_listInstantMessage.begin(); iter != m_listInstantMessage.end(); iter++)
	{
		CInstantMessage* instantMessage = (*iter);
		if(instantMessage->dbMID == dbMID)
		{
			ret = instantMessage;
			return;
		}
	}	

	//���Ҳ�����ӱ������ݿ��й���
	ret = new CInstantMessage();
	this->GetMessage(dbMID, (*ret));
	m_listInstantMessage.push_back(ret);

	lock.Unlock();
}

/// <summary>
/// ��ȡ��ʱ��Ϣ(����Ŀ��ID��ȡδ����ʱ��Ϣ, ��ҳ������)
/// **���øú�������Ҫ���е���MESSAGE_MANAGER::RealseMessages(messages)�����ͷŻ�ȡ������Ϣ
/// <summary>
/// <param name="targetUID">�������ID</param>
/// <param name="ret">��ʱ��Ϣ</param>
void CMessageManager::GetUnreadMessages(CString targetUID, list<CInstantMessage*>& ret)
{
	CString targetInformation = _T("");
	if (targetUID != _T(""))
		targetInformation.Format(_T("target_id = '%s' and"), targetUID);

	//�������ݿ����
	CString sql;
	sql.Format(_T("select * from %s where %s hasRead = 0"), ICT_IMTABLE, targetInformation);
	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//ִ�в�ѯ����
	result = DBSelect(sqlline, row, col);
	delete sqlline;

	//�����ѯ���
	if (row > 0)
	{
		int index = col;
		for(int i = 0 ; i < row; i++)
		{
			CInstantMessage *message = new CInstantMessage();
			for(int j = 0; j < col; j++)
			{
				CString title;
				DBChar2CString(result[j], title);
				CString value;
				DBChar2CString(result[index], value);

#ifdef MESSAGEM_LOG
				DEBUGCODE(Tracer::LogToConsole(_T("[CMessageManager::GetMessages] - result[%d] = %s:%s"), index, title, value);)
#endif
				if (ICT_IMFIELD_ID == title)
				{
					message->dbMID = value;
				}
				else if (ICT_IMFIELD_SID == title)
				{
					message->serverMID = value;
				}
				else if (ICT_IMFIELD_TARGERTID == title)
				{
					message->targetUID = value;
				}
				else if (ICT_IMFIELD_TARGETDES == title)
				{
					message->targetDescription = value;
				}
				else if (ICT_IMFIELD_SENDERID == title)
				{
					message->senderUID = value;
				}
				else if (ICT_IMFIELD_SENDERDES == title)
				{
					message->senderDescription = value;
				}
				else if (ICT_IMFIELD_MESSAGE == title)
				{
					message->message = value;
				}
				else if (ICT_IMFIELD_MESSAGEEXTEN == title)
				{
					message->messageExten = value;
				}
				else if (ICT_IMFIELD_MESSAGETYPE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					message->messageType = (MessageType)n;
				}
				else if (ICT_IMFIELD_CHATTYPE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					message->chatType = (ChatType)n;
				}
				else if (_T("time_pro") == title)
				{
					message->SetTime(value);
				}
				else if (ICT_IMFIELD_PID == title)
				{
					message->messagePID = _atoi64(result[index]);
				}
				else if (ICT_IMFIELD_TSC == title)
				{
					message->tsc = _atoi64(result[index]);
				}
				else if (ICT_IMFIELD_RECEIVE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isReceive = n;
					message->isReceive = isReceive;
				}
				else if (ICT_IMFIELD_SYNS == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isSyns = n;
					message->isSyns = isSyns;
				}
				else if (ICT_IMFIELD_READ == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL hasRead = n;
					message->hasRead = hasRead;
				}
				else if (ICT_IMFIELD_ACTIVE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isActive = n;
					message->isActive = isActive;
				}
				else if (ICT_IMFIELD_TOPIC == title)
				{
					message->messageTopic = value;
				}
				++index;
			}
			//��ʱ��Ϣ����
			message->MessageToObject();
			//����
			ret.push_back(message);
		}
	}

	//ʹ�ù�����Ҫ����ѯ����ͷ�
	sqlite3_free_table(result);
}

/// <summary>
/// ��ȡ��ʱ��Ϣ
/// <summary>
/// <param name="ret">��ʱ��Ϣ</param>
/// <param name="page">ҳ��</param>
/// <param name="count">ÿҳ��Ϣ����</param>
/// <param name="conditions">��ȡ����</param>
/// <param name="isAsc">�Ƿ�����</param>
/// <param name="setRead">�Ƿ���Ҫ�����Ѷ�</param>
void CMessageManager::GetMessagesC(list<CInstantMessage*>& ret, int page, int count, CString conditions, bool isAsc, bool setRead)
{
	if (page <= 0 || count <= 0)
		return;

	//�����׸���Ϣ���
	int first = (page - 1) * count;
	return GetMessagesCEx(ret, first, count, conditions, isAsc, setRead);
}

/// <summary>
/// ��ȡ��ʱ��Ϣ
/// <summary>
/// <param name="ret">��ʱ��Ϣ</param>
/// <param name="first">������Ϣ���</param>
/// <param name="count">ÿҳ��Ϣ����</param>
/// <param name="conditions">��ȡ����</param>
/// <param name="isAsc">�Ƿ�����</param>
/// <param name="setRead">�Ƿ���Ҫ�����Ѷ�</param>
void CMessageManager::GetMessagesCEx(list<CInstantMessage*>& ret, int first, int count, CString conditions, bool isAsc, bool setRead)
{
	//�������ݿ����
	CString condition;
	CString sql;
	CString dir = isAsc ? _T("asc") : _T("desc");
	if (_T("") != conditions)
	{
		//modified by zhangpeng on 20160531 begin
		//condition.Format(_T("%s order by time_pro %s, rowid %s LIMIT %d,%d"), conditions, dir, dir, first, count);
		condition.Format(_T("%s order by auto_id %s, rowid %s LIMIT %d,%d"), conditions, dir, dir, first, count);
		
	//	condition.Format(_T("%s order by rowid %s LIMIT %d,%d"), conditions, dir, first, count);

		sql.Format(_T("select * from %s where id in (select id from %s where %s) order by auto_id desc"), ICT_IMTABLE, ICT_IMTABLE, condition);
		//sql.Format(_T("select * from %s where id in (select id from %s where %s) order by time_pro desc"), ICT_IMTABLE, ICT_IMTABLE, condition);
	//	sql.Format(_T("select * from %s where id in (select id from %s where %s) order by rowid desc"), ICT_IMTABLE, ICT_IMTABLE, condition);
	}
	else
	{
		condition.Format(_T("order by auto_id %s, rowid %s LIMIT %d,%d"), dir, dir, first, count);
		//condition.Format(_T("order by time_pro %s, rowid %s LIMIT %d,%d"), dir, dir, first, count);
		//modified by zhangpeng on 20160531 end
	//	condition.Format(_T("order by rowid %s LIMIT %d,%d"), dir, first, count);
		sql.Format(_T("select * from %s"), ICT_IMTABLE);
	}

	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//ִ�в�ѯ����
	result = DBSelect(sqlline, row, col);
	delete sqlline;

	//�����ѯ���
	if (row > 0)
	{
		int index = col;
		for(int i = 0 ; i < row; i++)
		{
			CInstantMessage *message = new CInstantMessage();
			for(int j = 0; j < col; j++)
			{
				CString title;
				DBChar2CString(result[j], title);
				CString value;
				DBChar2CString(result[index], value);

#ifdef MESSAGEM_LOG
				DEBUGCODE(Tracer::LogToConsole(_T("[CMessageManager::GetMessagesC] - result[%d] = %s:%s"), index, title, value);)
#endif
				if (ICT_IMFIELD_ID == title)
				{
					message->dbMID = value;
				}
				else if (ICT_IMFIELD_SID == title)
				{
					message->serverMID = value;
				}
				else if (ICT_IMFIELD_TARGERTID == title)
				{
					message->targetUID = value;
				}
				else if (ICT_IMFIELD_TARGETDES == title)
				{
					message->targetDescription = value;
				}
				else if (ICT_IMFIELD_SENDERID == title)
				{
					message->senderUID = value;
				}
				else if (ICT_IMFIELD_SENDERDES == title)
				{
					message->senderDescription = value;
				}
				else if (ICT_IMFIELD_MESSAGE == title)
				{
					message->message = value;
				}
				else if (ICT_IMFIELD_MESSAGEEXTEN == title)
				{
					message->messageExten = value;
				}
				else if (ICT_IMFIELD_MESSAGETYPE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					message->messageType = (MessageType)n;
				}
				else if (ICT_IMFIELD_CHATTYPE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					message->chatType = (ChatType)n;
				}
				//modified by zhangpeng on 20160531 begin
				/*else if (_T("time_pro") == title)
				{
				message->SetTime(value);
				}*/
				else if (ICT_IMFIELD_TSS == title)
				{
					message->SetTime(value);
					message->tss = _atoi64(result[index]);
				}
				//modified by zhangpeng on 20160531 end
				else if (ICT_IMFIELD_PID == title)
				{
					message->messagePID = _atoi64(result[index]);
				}
				else if (ICT_IMFIELD_TSC == title)
				{
					message->tsc = _atoi64(result[index]);
				}
				else if (ICT_IMFIELD_RECEIVE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isReceive = n;
					message->isReceive = isReceive;
				}
				else if (ICT_IMFIELD_SYNS == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isSyns = n;
					message->isSyns = isSyns;
				}
				else if (ICT_IMFIELD_READ == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL hasRead = n;
					message->hasRead = hasRead;
				}
				else if (ICT_IMFIELD_ACTIVE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isActive = n;
					message->isActive = isActive;
				}
				else if (ICT_IMFIELD_TOPIC == title)
				{
					message->messageTopic = value;
				}
				++index;
			}
			//��ʱ��Ϣ����
			message->MessageToObject();
			//��¼
			ret.push_back(message);
		}
	}

	//ʹ�ù�����Ҫ����ѯ����ͷ�
	sqlite3_free_table(result);

	if (setRead)
	{
		//������ϢΪ�Ѷ�
		CString updateCondition;
		if (_T("") != conditions)
			updateCondition.Format(_T("(select id from %s where %s)"), ICT_IMTABLE, condition);
		else
			updateCondition.Format(_T("(select id from %s order by time_pro asc, rowid asc LIMIT %d,%d)"), ICT_IMTABLE, condition);

		SetMessageRead(updateCondition);
	}
}

/// <summary>
/// ��ȡ��ʱ��Ϣ
/// <summary>
/// <param name="page">ҳ��</param>
/// <param name="count">ÿҳ��Ϣ����</param>
/// <param name="ret">��ʱ��Ϣ</param>
/// <param name="setRead">�Ƿ���Ҫ�����Ѷ�</param>
void CMessageManager::GetMessages(int page, int count, list<CInstantMessage*>& ret, bool setRead)
{
	if (page <= 0 || count <= 0)
		return;
	return GetMessagesC(ret, page, count, _T(""), setRead);
}

/// <summary>
/// ��ȡ��ʱ��Ϣ
/// <summary>
/// <param name="targetUID">�������ID</param>
/// <param name="page">ҳ��</param>
/// <param name="count">ÿҳ��Ϣ����</param>
/// <param name="ret">��ʱ��Ϣ</param>
/// <param name="date">����</param>
/// <param name="isUnread">�Ƿ�δ��</param>
/// <param name="isAsc">�Ƿ�����</param>
/// <param name="setRead">�Ƿ���Ҫ�����Ѷ�</param>
void CMessageManager::GetMessages(CString targetUID, int page, int count, list<CInstantMessage*>& ret, CString date, bool isUnread, bool isAsc, bool setRead)
{
	if (page <= 0 || count <= 0 || _T("") == targetUID)
		return;

	//�����׸���Ϣ���
	int first = (page - 1) * count;
	return GetMessagesEx(targetUID, first, count, ret, date, isUnread, isAsc, setRead);
}

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
void CMessageManager::GetMessagesEx(CString targetUID, int first, int count, list<CInstantMessage*>& ret, CString date, bool isUnread, bool isAsc, bool setRead)
{
	//�������ݿ��ѯ����
	//�����ϵ��ID����
	CString conditions;
	conditions.Format(_T("%s = '%s'"), ICT_IMFIELD_TARGERTID, targetUID);
	//�����������
	if (_T("") != date)
	{
		COleDateTime oDTS;  
		oDTS.ParseDateTime(date + _T(" 00:00:00"));  
		SYSTEMTIME stS;
		oDTS.GetAsSystemTime(stS);  
		CTime timeS(stS);

		COleDateTime oDTE;  
		oDTE.ParseDateTime(date + _T(" 23:59:59"));  
		SYSTEMTIME stE;
		oDTE.GetAsSystemTime(stE);
		CTime timeE(stE);

		CString szTimeS = timeS.Format(_T("%Y-%m-%d %H:%M:%S"));
		CString szTimeE = timeE.Format(_T("%Y-%m-%d %H:%M:%S"));
		//�������ݿ��ѯ����
		CString strJTimeS;
		strJTimeS.Format(_T(" julianday('%s')"), szTimeS);
		CString strJTimeE;
		strJTimeE.Format(_T(" julianday('%s')"), szTimeE);
		CString conditionDate;
		conditionDate.Format(_T("time_pro < %s and time_pro > %s"), strJTimeE, strJTimeS);
		conditions += (_T(" and ") + conditionDate);
	}

	if (isUnread)
		conditions += (_T(" and hasRead = 0"));
	return GetMessagesCEx(ret, first, count, conditions, isAsc, setRead);
}

/// <summary>
/// ��ȡ��ʱ��Ϣ
/// <summary>
/// <param name="date">����</param>
/// <param name="page">ҳ��</param>
/// <param name="count">ÿҳ��Ϣ����</param>
/// <param name="ret">��ʱ��Ϣ</param>
/// <param name="setRead">�Ƿ���Ҫ�����Ѷ�</param>
void CMessageManager::GetMessagesD(CString date, int page, int count, list<CInstantMessage*>& ret, bool setRead)
{
	if (page <= 0 || count <= 0 || _T("") == date)
		return;

	COleDateTime oDTS;  
	oDTS.ParseDateTime(date + _T(" 00:00:00"));  
	SYSTEMTIME stS;
	oDTS.GetAsSystemTime(stS);  
	CTime timeS(stS);

	COleDateTime oDTE;  
	oDTE.ParseDateTime(date + _T(" 23:59:59"));  
	SYSTEMTIME stE;
	oDTE.GetAsSystemTime(stE);
	CTime timeE(stE);

	CString szTimeS = timeS.Format(_T("%Y-%m-%d %H:%M:%S"));
	CString szTimeE = timeE.Format(_T("%Y-%m-%d %H:%M:%S"));
	//�������ݿ��ѯ����
	CString strJTimeS;
	strJTimeS.Format(_T(" julianday('%s')"), szTimeS);
	CString strJTimeE;
	strJTimeE.Format(_T(" julianday('%s')"), szTimeE);
	CString conditions;
	conditions.Format(_T("time_pro < %s and time_pro > %s"), strJTimeE, strJTimeS);
	return GetMessagesC(ret, page, count, conditions, setRead);
}

/// <summary>
/// ��ȡ��ʱ��Ϣ
/// <summary>
/// <param name="key">��Ϣ���ݹؼ���</param>
/// <param name="page">ҳ��</param>
/// <param name="count">ÿҳ��Ϣ����</param>
/// <param name="ret">��ʱ��Ϣ</param>
/// <param name="targetUID">Ŀ��ID</param>
/// <param name="setRead">�Ƿ���Ҫ�����Ѷ�</param>
void CMessageManager::GetMessagesK(CString key, int page, int count, list<CInstantMessage*>& ret, CString targetUID, bool setRead)
{
	if (page <= 0 || count <= 0 || _T("") == key)
		return;
	//�������ݿ��ѯ����
	CString conditions;
	conditions.Format(_T("message like '%%%s%%'"), key);
	//�����ϵ��id����
	if (_T("") != targetUID)
	{
		CString conditionID;
		conditionID.Format(_T("%s = '%s'"), ICT_IMFIELD_TARGERTID, targetUID);
		conditions += (_T(" and ") + conditionID);
	}
	return GetMessagesC(ret, page, count, conditions, true, setRead);
}

void CMessageManager::CopyAndUpdateMessage(CMessageManager *pMessageMan)
{
	if(!pMessageMan)
		return;

	int offset = 0, limit = 1000;
	while(1)
	{
		vector<IMBasicData*> vecIMData;
		pMessageMan->GetMessages(offset, limit, vecIMData);
		int nSize = vecIMData.size();
		if(nSize == 0)
		{
			break;
		}

		list<CString> lsSql;
		for(vector<IMBasicData*>::const_iterator iter = vecIMData.begin(); iter != vecIMData.end(); iter++)
		{
			IMBasicData *pIMData = (*iter);
			if(pIMData)
			{
				CString strSql = GetMessageSql(pIMData);
				if(!strSql.IsEmpty())
				{
					lsSql.push_back(strSql);
				}
				delete pIMData, pIMData = NULL;
			}			
		}
		vecIMData.clear();

		if(lsSql.size() > 0)
		{
			DBTransaction(lsSql);
		}

		offset += nSize;
	}
}

void CMessageManager::GetMessages(int offset, int limit, vector<IMBasicData*> &vecIMData)
{
	CString sql;
	sql.Format(_T("select *, datetime(time_pro) from %s limit %d offset %d"), ICT_IMTABLE, limit, offset);

	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//ִ�в�ѯ����
	result = DBSelect(sqlline, row, col);
	delete sqlline;

	//�����ѯ���
	if (row > 0)
	{		
		int index = col;
		for(int i = 0 ; i < row; i++)
		{
			IMBasicData *pIMData = new IMBasicData;
			for(int j = 0; j < col; j++)
			{
				CString title;
				DBChar2CString(result[j], title);
				CString value;
				DBChar2CString(result[index], value);

				if (ICT_IMFIELD_ID == title)
				{
					pIMData->dbMID = value;
				}
				else if (ICT_IMFIELD_SID == title)
				{
					pIMData->serverMID = value;
				}
				else if (ICT_IMFIELD_TARGERTID == title)
				{
					pIMData->targetUID = value;
				}
				else if (ICT_IMFIELD_TARGETDES == title)
				{
					pIMData->targetDescription = value;
				}
				else if (ICT_IMFIELD_SENDERID == title)
				{
					pIMData->senderUID = value;
				}
				else if (ICT_IMFIELD_SENDERDES == title)
				{
					pIMData->senderDescription = value;
				}
				else if (ICT_IMFIELD_MESSAGE == title)
				{
					pIMData->message = value;
				}
				else if (ICT_IMFIELD_MESSAGEEXTEN == title)
				{
					pIMData->messageJson = value;
				}
				else if (ICT_IMFIELD_MESSAGETYPE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					pIMData->messageType = (MessageType)n;
				}
				else if (ICT_IMFIELD_CHATTYPE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					pIMData->chatType = (ChatType)n;
				}
				else if (_T("datetime(time_pro)") == title)
				{
					// ���ݿ�汾<V104.db
					COleDateTime time;
					if(time.ParseDateTime(value))
					{
						SYSTEMTIME st;
						time.GetAsSystemTime(st);
						CTime t(st);
						pIMData->time = t.GetTime()*pow(10.0, 6);
					}
				}
				else if (_T("time_pro") == title)
				{
					// ���ݿ�汾>=V104.db
					if(value.Find(_T(".")) == -1)
					{
						pIMData->time = _atoi64(result[index]);
					}
				}
				else if (ICT_IMFIELD_TSC == title)
				{
					pIMData->tsc = _atoi64(result[index]);
				}
				else if (ICT_IMFIELD_TSS == title)
				{
					pIMData->tss = _atoi64(result[index]);
				}
				else if (ICT_IMFIELD_PID == title)
				{
					pIMData->pid = _atoi64(result[index]);
				}
				else if (ICT_IMFIELD_RECEIVE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isReceive = n;
					pIMData->isReceive = isReceive;
				}
				else if (ICT_IMFIELD_SYNS == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isSyns = n;
					pIMData->isSyns = isSyns;
				}
				else if (ICT_IMFIELD_READ == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL hasRead = n;
					pIMData->hasRead = hasRead;
				}
				else if (ICT_IMFIELD_ACTIVE == title)
				{
					int n = _ttoi(value.GetBuffer());
					value.ReleaseBuffer();
					BOOL isActive = n;
					pIMData->isActive = isActive;
				}
				else if (ICT_IMFIELD_TOPIC == title)
				{
					pIMData->messageTopic = value;
				}
				++index;
			}
			vecIMData.push_back(pIMData);
		}		
	}

	//ʹ�ù�����Ҫ����ѯ����ͷ�
	sqlite3_free_table(result);
}

CString CMessageManager::GetMessageSql(IMBasicData *pIMData)
{
	if(!pIMData)
		return _T("");

	//�������ݿ����
	CString fields;
	fields.Format(
		_T("(%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)"),
		ICT_IMFIELD_ID, ICT_IMFIELD_SID, ICT_IMFIELD_TARGERTID, ICT_IMFIELD_TARGETDES, ICT_IMFIELD_SENDERID, ICT_IMFIELD_SENDERDES, ICT_IMFIELD_MESSAGE, ICT_IMFIELD_MESSAGEEXTEN, ICT_IMFIELD_MESSAGETYPE, ICT_IMFIELD_CHATTYPE, ICT_IMFIELD_TIME, ICT_IMFIELD_TSS, ICT_IMFIELD_TSC, ICT_IMFIELD_PID, ICT_IMFIELD_RECEIVE, ICT_IMFIELD_SYNS, ICT_IMFIELD_READ, ICT_IMFIELD_ACTIVE, ICT_IMFIELD_TOPIC
		);

	CString values;
	values.Format(
		_T("VALUES('%s', '%s', '%s', '%d', '%s', '%s', '%s', '%s', '%d', '%d', '%I64d', '%I64d', '%I64d', '%I64d', '%d', '%d', '%d', '%d', '%d')"), 
		pIMData->dbMID, pIMData->serverMID, pIMData->targetUID, pIMData->targetDescription, pIMData->senderUID, pIMData->senderDescription, pIMData->message, pIMData->messageJson, (int)(pIMData->messageType), (int)(pIMData->chatType), pIMData->time, pIMData->tss, pIMData->tsc, pIMData->pid, pIMData->isReceive, pIMData->isSyns, pIMData->hasRead, pIMData->isActive, pIMData->messageTopic
		);

	CString sql;
	sql.Format(_T("insert into %s %s %s;"), ICT_IMTABLE, fields, values);
	return sql;
}

/// <summary>
/// ��ȡ��ʱ��Ϣ����
/// <summary>
/// <param name="onlyUnread">�Ƿ�ֻ��ȡδ����</param>
int CMessageManager::GetMessageCount(bool onlyUnread)
{
	return GetMessageCountC(_T(""), onlyUnread);
}

/// <summary>
/// ��ȡ��ʱ��Ϣ����
/// <summary>
/// <param name="targetUID">�������ID</param>
/// <param name="date">����</param>
/// <param name="onlyUnread">�Ƿ�ֻ��ȡδ����</param>
int CMessageManager::GetMessageCount(CString targetUID, CString date, bool onlyUnread)
{
	//�������ݿ��ѯ����
	//�����ϵ��ID����
	CString condition;
	condition.Format(_T("%s = '%s'"), ICT_IMFIELD_TARGERTID, targetUID);
	//�����������
	if (_T("") != date)
	{
		COleDateTime oDTS;  
		oDTS.ParseDateTime(date + _T(" 00:00:00"));  
		SYSTEMTIME stS;
		oDTS.GetAsSystemTime(stS);  
		CTime timeS(stS);

		COleDateTime oDTE;  
		oDTE.ParseDateTime(date + _T(" 23:59:59"));  
		SYSTEMTIME stE;
		oDTE.GetAsSystemTime(stE);
		CTime timeE(stE);

		CString szTimeS = timeS.Format(_T("%Y-%m-%d %H:%M:%S"));
		CString szTimeE = timeE.Format(_T("%Y-%m-%d %H:%M:%S"));
		//�������ݿ��ѯ����
		CString strJTimeS;
		strJTimeS.Format(_T(" julianday('%s')"), szTimeS);
		CString strJTimeE;
		strJTimeE.Format(_T(" julianday('%s')"), szTimeE);
		CString conditionDate;
		conditionDate.Format(_T("time_pro < %s and time_pro > %s"), strJTimeE, strJTimeS);
		condition += (_T(" and ") + conditionDate);
	}
	return GetMessageCountC(condition, onlyUnread);
}

/// <summary>
/// ��ȡ��ʱ��Ϣ����
/// <summary>
/// <param name="date">����</param>
/// <param name="onlyUnread">�Ƿ�ֻ��ȡδ����</param>
int CMessageManager::GetMessageCountD(CString date, bool onlyUnread)
{
	COleDateTime oDTS;  
	oDTS.ParseDateTime(date + _T(" 00:00:00"));  
	SYSTEMTIME stS;
	oDTS.GetAsSystemTime(stS);  
	CTime timeS(stS);

	COleDateTime oDTE;  
	oDTE.ParseDateTime(date + _T(" 23:59:59"));  
	SYSTEMTIME stE;
	oDTE.GetAsSystemTime(stE);
	CTime timeE(stE);

	CString szTimeS = timeS.Format(_T("%Y-%m-%d %H:%M:%S"));
	CString szTimeE = timeE.Format(_T("%Y-%m-%d %H:%M:%S"));

	//�������ݿ��ѯ����
	CString strJTimeS;
	strJTimeS.Format(_T(" julianday('%s')"), szTimeS);
	CString strJTimeE;
	strJTimeE.Format(_T(" julianday('%s')"), szTimeE);
	CString conditions;
	conditions.Format(_T("time_pro < %s and time_pro > %s"), strJTimeE, strJTimeS);
	return GetMessageCountC(conditions, onlyUnread);
}

/// <summary>
/// ��ȡ��ʱ��Ϣ����
/// <summary>
/// <param name="key">���ݹؼ���</param>
/// <param name="targetUID">�������ID</param>
/// <param name="onlyUnread">�Ƿ�ֻ��ȡδ����</param>
int CMessageManager::GetMessageCountK(CString key, CString targetUID, bool onlyUnread)
{
	//�������ݿ��ѯ����
	CString conditions;
	conditions.Format(_T("message like '%%%s%%'"), key);

	//�����ϵ��id����
	if (_T("") != targetUID)
	{
		CString conditionID;
		conditionID.Format(_T("%s = '%s'"), ICT_IMFIELD_TARGERTID, targetUID);
		conditions += (_T(" and ") + conditionID);
	}

	return GetMessageCountC(conditions, onlyUnread);
}


/// <summary>
/// ��ȡ��ʱ��Ϣ����
/// <summary>
/// <param name="conditions">��ȡ����</param>
int CMessageManager::GetMessageCountC(CString conditions, bool onlyUnread)
{
	//�������ݿ����
	if (onlyUnread)
		conditions += (_T(" and hasRead = 0"));
	CString sql;
	if (_T("") != conditions)
		sql.Format(_T("select count(*) from %s where %s"), ICT_IMTABLE, conditions);
	else
		sql.Format(_T("select count(*) from %s"), ICT_IMTABLE);

	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//ִ�в�ѯ����
	result = DBSelect(sqlline, row, col);
	delete sqlline;

	//�����ѯ���
	if (row > 0)
	{
		int index = col;
		for(int i = 0 ; i < row; i++)
		{
			for(int j = 0; j < col; j++)
			{
				CString title;
				DBChar2CString(result[j], title);
				CString value;
				DBChar2CString(result[index], value);

#ifdef MESSAGEM_LOG
				DEBUGCODE(Tracer::LogToConsole(_T("[CMessageManager::GetMessageCountC] - result[%d] = %s:%s"), index, title, value);)
#endif
				int n = _ttoi(value.GetBuffer());
				return n;
			}
		}
	}

	return 0;
}


/// <summary>
/// ������Ϣtsc
/// <summary>
/// <param name="targetUID">�������ID</param>
void CMessageManager::SetMessageTscByMID(CString mid, __int64 tsc)
{
	//�������ݿ����
	CString sql;
	sql.Format(_T("update %s set %s = '%I64d', %s = '%I64d' where %s = '%s';"), ICT_IMTABLE, ICT_IMFIELD_TSC, tsc, ICT_IMFIELD_TIME, tsc, ICT_IMFIELD_ID, mid);
	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//ִ�м�¼��Ϣ����
	DBExecute(sqlline);
	delete sqlline;
}

//modified by zhangpeng on 20160419	begin
/// <summary>
/// ������Ϣ�Ѷ�
/// <summary>
/// <param name="targetUID">�������ID</param>
void CMessageManager::SetMessageReadByTargetID(CString targetUID,char* tss)
{
	//�������ݿ����
	CString sql;
	CString cstrtss;
	Char2CString(tss,cstrtss);
	sql.Format(_T("update %s set hasRead = 1 where hasRead != 1 and %s = '%s' and %s < '%s';"), ICT_IMTABLE, ICT_IMFIELD_TARGERTID, targetUID,ICT_IMFIELD_TSS,cstrtss);
	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//ִ�м�¼��Ϣ����
	DBExecute(sqlline);
	delete sqlline;
}
//modified by zhangpeng on 20160419	end
/// <summary>
/// ������Ϣ�Ѷ�
/// <summary>
/// <param name="targetUID">�������ID</param>
void CMessageManager::SetMessageReadByTargetID(CString targetUID)
{
	//�������ݿ����
	CString sql;
	sql.Format(_T("update %s set hasRead = 1 where hasRead != 1 and %s = '%s';"), ICT_IMTABLE, ICT_IMFIELD_TARGERTID, targetUID);
	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//ִ�м�¼��Ϣ����
	DBExecute(sqlline);
	delete sqlline;
}
void CMessageManager::SetTargetDescriptionDeleteByTargetID(CString targetUID)
{
	//�������ݿ����
	CString sql;
	sql.Format(_T("update IM set target_description = 2 where target_id = '%s' and time_pro=(select max(time_pro) from IM where target_id = '%s');;"),targetUID,targetUID);
	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//ִ�м�¼��Ϣ����
	DBExecute(sqlline);
	delete sqlline;
}
void CMessageManager::SetTargetDescriptionTopByTargetID(CString targetUID)
{
	//�������ݿ����
	CString sql;
	sql.Format(_T("update %s set target_description = 1 where %s = '%s';"), ICT_IMTABLE, ICT_IMFIELD_TARGERTID, targetUID);
	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//ִ�м�¼��Ϣ����
	DBExecute(sqlline);
	delete sqlline;
}


/// <summary>
/// ������Ϣ�Ѷ�(����Ϣ���ݿ�ID������Ϣ�Ѷ�)
/// <summary>
/// <param name="dbMID">��Ϣ�����ݿ�ID</param>
void CMessageManager::SetMessageReadByMessageID(CString dbMID)
{
	//�������ݿ����
	CString sql;
	sql.Format(_T("update %s set hasRead = 1 where id = '%s';"), ICT_IMTABLE, dbMID);
	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//ִ�м�¼��Ϣ����
	DBExecute(sqlline);
	delete sqlline;
}

/// <summary>
/// ������Ϣ��չ����
/// <summary>
/// <param name="dbMID">��Ϣ�����ݿ�ID</param>
/// <param name="messageExten">��Ϣ��չ����</param>
void CMessageManager::SetMessageExtenByMessageID(CString dbMID, CString messageExten)
{
	//�������ݿ����
	CString sql;
	sql.Format(_T("update %s set message_exten = '%s' where id = '%s';"), ICT_IMTABLE, messageExten, dbMID);
	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//ִ�м�¼��Ϣ����
	DBExecute(sqlline);
	delete sqlline;
}


/// <summary>
/// ������Ϣ�Ƿ����
/// <summary>
/// <param name="dbMID">��Ϣ�����ݿ�ID</param>
/// <param name="isActive">�Ƿ񼤻�</param>
void CMessageManager::SetMessageActive(CString dbMID, bool isActive)
{
	int active = 0;
	if (isActive)
		active = 1;
	//�������ݿ����
	CString sql;
	sql.Format(_T("update %s set isActive = %d where id = '%s';"), ICT_IMTABLE, active, dbMID);
	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//ִ�м�¼��Ϣ����
	DBExecute(sqlline);
	delete sqlline;
}

/// <summary>
/// ������Ϣ�Ѷ�
/// �Ӳ�ѯ�ֶ���id
/// <summary>
/// <param name="condition">��������</param>
void CMessageManager::SetMessageRead(CString condition)
{
	//�������ݿ����
	CString sql;
	sql.Format(_T("update %s set hasRead = 1 where id in %s;"), ICT_IMTABLE, condition);
	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//ִ�м�¼��Ϣ����
	DBExecute(sqlline);
	delete sqlline;
}

/// <summary>
/// ����������Ϣ�Ѷ�
/// <summary>
void CMessageManager::SetAllMessageRead()
{
	//�������ݿ����
	CString sql;
	sql.Format(_T("update %s set hasRead = 1;"), ICT_IMTABLE);
	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//ִ�м�¼��Ϣ����
	DBExecute(sqlline);
	delete sqlline;
}

/// <summary>
/// ɾ��ת���ַ�
/// <summary>
/// <param name="text">�ı�(��������ת���ַ�)</param>
CString CMessageManager::RemoveFace(CString text)
{
	if (text.Find(_T("[")) != -1 && text.Find(_T("]")) != -1)
	{
		//�п��ܰ�������, ��鲢�ԶԱ�����Ϣ�������⴦��(֧�ֶ���������)
		int pos = 0;
		CUserAgentCore* ua = CUserAgentCore::GetUserAgentCore();
		while (pos != -1)
		{
			CString temp = text;
			//����ҵ��Ĺؼ���ǰ���Ƿ��б���ת���ַ��Ŀ�ʼ���պϱ�ʶ
			int start = temp.Find(_T("["), pos);
			int end = -1;
			if (start != -1)
			{
				//�ҵ�ת���ַ���ʼ����"["
				//����ת���ַ���������"]"
				end = temp.Find(_T("]"), start + 1);
			}
			if (end == -1)
				pos = -1;

			if (pos == -1)
				//δ�ҵ�, �˳�
				continue;

			CString faceTag = temp.Mid(start, (end - start + 1));
			CString faceName = ua->GetFaceManager()->GetNameByTag(faceTag);
			if (faceName != _T(""))
			{
				//����ת���ַ��޳�
				temp.Replace(faceTag, _T(""));
				text = temp;
				pos = start;
			}
			else
			{
				//���Ǳ���ת���ַ�, ������β��ҵĿ�ʼλ��+1���������
				pos = start + 1;
				continue;
			}
		}
	}
	return text;
}

int CMessageManager::GetMessageTipType(time_t nMsgTime, bool isSync, bool isRoaming)
{
	if(isRoaming)
		return TIP_ROAMING;


	//ͬ����Ϣ�����κ�����
	if(isSync)
		return 0;

	int nTipType = TIP_FLICKER | TIP_REDPOINT;

	//�ж��Ƿ������������
	bool bSndTip = false;
	static bool bHadTip = false;
	if(nMsgTime < m_tsMQTTConnected)
	{
		//������Ϣ������һ������		
		if(!bHadTip)
		{			
			bSndTip = true;
			bHadTip = true;
		}
	}
	else
	{
		//��������Ϣ������������
		bSndTip = true;
		bHadTip = false;
	}
	if(bSndTip)
	{
		nTipType |= TIP_SOUND;
	}
	return nTipType;
}

int CMessageManager::GetIdByTsc(__int64 tsc,CString &dbID)
{
	CString sql, condition;
	condition.Format(_T("%s = '%I64d'"),ICT_IMFIELD_TSC,  tsc);
	sql.Format(_T("select %s from %s where %s"),ICT_IMFIELD_ID, ICT_IMTABLE, condition);

	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//ִ�в�ѯ����
	result = DBSelect(sqlline, row, col);
	delete sqlline;

	//�����ѯ���
	if (row > 0)
	{
		dbID = CString(result[1]);
	}
	return 0;
}
//modified by zhangpeng on 20160417 begin
int CMessageManager::GetRecvMaxTimePro(char* targetUID,CString& outMaxProTime)
{
	//�������ݿ����
	CString sql, condition,cstruid ;
	Char2CString(targetUID,cstruid);
	condition.Format(_T("%s = '%s'and %s = 1"),ICT_IMFIELD_TARGERTID,cstruid,ICT_IMFIELD_RECEIVE);
	sql.Format(_T("select max(%s) from %s where %s order by %s asc"),ICT_IMFIELD_TSS, ICT_IMTABLE, condition, ICT_IMFIELD_TSS);//��ѯ���ProTime

	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//ִ�в�ѯ����
	result = DBSelect(sqlline, row, col);
	delete sqlline;
	int64 i64 = 0;
	//�����ѯ���
	if (row > 0)
	{
		for(int i = 1 ; i <= row; i++)
		{
			outMaxProTime = result[i];
		}
		
	}

	return 0;
}

//modified by zhangpeng on 20160417 end

int CMessageManager::GetTscTimes(CString targetUID, __int64 tscStart, __int64 tscEnd, vector<__int64> &times)
{
	//�������ݿ����
	CString sql, condition;
	condition.Format(_T("%s = '%s' and %s >= '%I64d' and %s <= '%I64d'"),ICT_IMFIELD_TARGERTID, targetUID, ICT_IMFIELD_TSC, tscStart, ICT_IMFIELD_TSC, tscEnd);

	sql.Format(_T("select %s from %s where %s order by %s asc"),ICT_IMFIELD_TSC, ICT_IMTABLE, condition, ICT_IMFIELD_TSC);


	//���ݿ�����ʽת��
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//ִ�в�ѯ����
	result = DBSelect(sqlline, row, col);
	delete sqlline;

	//�����ѯ���
	if (row > 0)
	{
		for(int i = 1 ; i <= row; i++)
		{
			times.push_back(_atoi64(result[i]));
		}
	}

	return 0;
}

bool CMessageManager::QueueIMDataIsEmpty()
{
	return queueIMData.empty() && imRecord.empty() && imUpdate.empty();
	
}

/// <summary>
/// �ͷż�ʱ��Ϣ
/// <summary>
/// <param name="messages">��ʱ��Ϣ�б�</param>
void RealseMessages(list<CInstantMessage*> &messages)
{
	list<CInstantMessage*>::iterator itor = messages.begin();
	for (; itor != messages.end(); itor++)
	{
		if (*itor)
		{
			delete (*itor);
			*itor = NULL;
		}
	}
	messages.clear();
}

/// <summary>
/// �ͷ������ϵ��
/// <summary>
/// <param name="contacts">�����ϵ���б�</param>
void RealseContacts(list<CRecentContact*> &contacts)
{
	list<CRecentContact*>::iterator itor = contacts.begin();
	for (; itor != contacts.end(); itor++)
	{
		if (*itor)
		{
			delete (*itor);
			*itor = NULL;
		}
	}
	contacts.clear();
}

}
/////////////////////////////////////////////////////////////////////////////////////////////////////
