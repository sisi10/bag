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
#define ICT_IMFIELD_SID				_T("sid")//空值
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
#define ICT_IMFIELD_TOPIC		    _T("message_topic")//空值

pfunIMUpdateCallBack CMessageManager::m_pfunIMUpdateCallBack = NULL;

/// <summary>
/// 消息管理器
/// <summary>
CMessageManager::CMessageManager()
{		
	//初始化MQTT连接成功的时间
	m_tsMQTTConnected = -1;
	//即时消息预处理线程
	this->threadPreprocessIM = NULL;
	//即时消息预处理线程是否停止
	this->preprocessIMDie = false;
	//初始化时间
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
	//初始化在线状态更新时间间隔
	this->imRecordInterval = 500;

	//设置消息数据库名称
	this->nameMessageDB.Format(_T("MessageV%d.db"), MSG_DB_VERSION);
	//设置参数
	SetParams(_T(""), this->nameMessageDB, _T(""));
	//设置数据表格
	SetTable();
}

/// <summary>
/// 消息管理器
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
/// 消息管理器
/// <summary>
/// <param name="path">数据库路径</param>
CMessageManager::CMessageManager(CString path)
{
	//初始化MQTT连接成功的时间
	m_tsMQTTConnected = -1;
	//设置消息数据库名称
	this->nameMessageDB.Format(_T("MessageV%d.db"), MSG_DB_VERSION);
	//设置参数
	SetParams(path, this->nameMessageDB, _T(""));
	//设置数据表格
	SetTable();
}

/// <summary>
/// 记录需要记录的即时消息
/// <summary>
/// <param name="dbMID">即时消息数据库ID</param>
/// <param name="im">需要记录的即时消息信息</param>
bool CMessageManager::RecordRIM()
{
	//存储延时策略
	bool toRecord = false;
	CSingleLock lockRecordIM(&this->csRecordIM);
	lockRecordIM.Lock();
	//更新状态更新记录时间
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
		//(时间满足存储)或者(数量满足存储), 即将需要更新, 重置时间
		GetLocalTime(&this->timeLastRecord);
		if (this->imRecord.size() > 0)
		{
			//如果触发的时候遍历预处理数据记录数据库操作语句
			while (!this->imRecord.empty())
	        {
				//构造数据库操作语句
				IMRecord record = this->imRecord.front();
				sqls.push_back(record.recordSQL);
				//记录更新操作
				if(record.nTipType != TIP_ROAMING)
					RecordUIM(record.dbMID, record.targetID, record.chatType, record.nTipType);
				this->imRecord.pop();
	        }
	
			toRecord = true;
		}
	}
	lockRecordIM.Unlock();
	
	//记录
	if (toRecord && sqls.size() > 0)
	{
		//满足条件进行存储
		//DEBUGCODE(Tracer::LogToConsole(_T("[CMessageManager::RecordRIM] 1.1 [sql size]:%d"), sqls.size());)
		this->RecordMessageTransation(sqls);
		DEBUGCODE(Tracer::LogToConsole(_T("[CMessageManager::RecordRIM] 1"));)
	
		if (this->m_pfunIMUpdateCallBack)
		{
			//满足条件进行更新
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
/// 记录需要记录的即时消息
/// <summary>
/// <param name="dbMID">即时消息数据库ID</param>
/// <param name="im">需要记录的即时消息信息</param>
bool CMessageManager::RecordRIM(CString dbMID, IMRecord im)
{
	if (dbMID != _T(""))
	{
		//存储延时策略
		bool toRecord = false;
		CSingleLock lockRecordIM(&this->csRecordIM);
		lockRecordIM.Lock();
		//存在好用在线状态变化状态则更新, 不存在则添加
		this->imRecord.push(im);
		//更新状态更新记录时间
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
			//(时间满足存储)或者(数量满足存储), 即将需要更新, 重置时间
			GetLocalTime(&this->timeLastRecord);
			if (this->imRecord.size() > 0)
			{
				//如果触发的时候遍历预处理数据记录数据库操作语句
				while (!this->imRecord.empty())
                {
					//构造数据库操作语句
					IMRecord record = this->imRecord.front();
					sqls.push_back(record.recordSQL);
					//记录更新操作
					RecordUIM(record.dbMID, record.targetID, record.chatType, record.nTipType);
					this->imRecord.pop();
                }

				toRecord = true;
			}
		}
		lockRecordIM.Unlock();

		//记录
		if (toRecord && sqls.size() > 0)
		{
			//满足条件进行存储
			//DEBUGCODE(Tracer::LogToConsole(_T("[CMessageManager::RecordRIM] 1.1 [sql size]:%d"), sqls.size());)
			this->RecordMessageTransation(sqls);
			DEBUGCODE(Tracer::LogToConsole(_T("[CMessageManager::RecordRIM] 1"));)

			if (this->m_pfunIMUpdateCallBack)
			{
				//满足条件进行更新
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
		//存储强制触发
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
				//遍历预处理数据记录数据库操作语句
				while (!this->imRecord.empty())
                {
					//构造数据库操作语句
					IMRecord record = this->imRecord.front();
					sqls.push_back(record.recordSQL);
					//记录更新操作
					RecordUIM(record.dbMID, record.targetID, record.chatType, record.nTipType);
					this->imRecord.pop();
                }

				toRecord = true;
			}
		}
		lockRecordIM.Unlock();

		//记录
		if (toRecord && sqls.size() > 0)
		{
			//满足条件进行存储
			//DEBUGCODE(Tracer::LogToConsole(_T("[MessageManager::RecordRIM] 2.1 [sql size]:%d"), sqls.size());)
			this->RecordMessageTransation(sqls);
			DEBUGCODE(Tracer::LogToConsole(_T("[MessageManager::RecordRIM] 2"));)

			if (this->m_pfunIMUpdateCallBack)
			{
				//满足条件进行更新
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
/// 记录需要更新的即时消息
/// <summary>
/// <param name="dbMID">即时消息数据库ID</param>
/// <param name="targetUID">交互对象UID</param>
/// <param name="chatType">聊天类型</param>
bool CMessageManager::RecordUIM(CString dbMID, CString targetUID, ChatType chatType, int nTipType)
{
	if (dbMID == _T("") || targetUID == _T(""))
		return false;

	CSingleLock lockUpdateIM(&this->csUpdateIM);
	lockUpdateIM.Lock();
	//添加需要更新的即时消息
	//按照交互UID进行区分
	IMUpdateMap::iterator iter = this->imUpdate.find(targetUID);
	if(iter != this->imUpdate.end())
	{
		//找到需要记录的信息对应的交互对象, 记录消息数据库ID到交互对象中
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
/// 设置数据库表(在这里定义数据库消息存储表结构)
/// <summary>
void CMessageManager::SetTable()
{
	//数据库表格名称
	DBTable table(ICT_IMTABLE);

	//添加数据库表格字段
	//add by zhangpeng on 20160531 begin
	DBTableValue valueAutoID;
	valueAutoID.name = ICT_IMFIELD_AUTO_ADD;
	valueAutoID.type = "INTEGER";
	valueAutoID.Fun = " PRIMARY KEY AUTOINCREMENT";//自增排序
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

	//添加表格
	AddDBTable(table);
}

void CMessageManager::SetOldTable()
{
	//数据库表格名称
	DBTable table(ICT_IMTABLE);

	//添加数据库表格字段
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

	//添加表格
	AddDBTable(table);
}

/// <summary>
/// 设置路径
/// <summary>
/// <param name="">数据库路径</param>
void CMessageManager::SetPath(CString path)
{
	//断开当前数据库连接
	DisconnectDB();
	this->pathDB = path;
}

void CMessageManager::RecordMessageJson2(IMBasicData imData)
{
	IMBasicData *im = new IMBasicData;
	*im = imData;

	//记录消息内容
	CSingleLock lock(&this->csBufferIM);
	lock.Lock();
	this->queueIMData.push(im);
	lock.Unlock();	
}

/// <summary>
/// 记录消息(SQL语句事务操作)
/// <summary>
/// <param name="sqls">SQL语句列表</param>
void CMessageManager::RecordMessageTransation(list<CString> sqls)
{
	this->DBTransaction(sqls);
}

CString CMessageManager::BuildRecodeMessageSQL2(IMBasicData imData)
{	
	//转换内容：serv->db
	bool bRet = ConvertIMFromServToDB(&imData);

	//构造数据库语句
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
			//纯文本消息
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
					//提取co中的内容作为文本内容
					sMessage  = root["co"].asString();		
					Char2CString(sMessage.c_str(), message);
				}

				//构造消息扩展内容				
				Json::Value rootBuild;
				Json::FastWriter fast_writer;
				//纯文本消息内容
				//构造纯文本数据名称
				string sValueName;
				char cValueName[64] = {0};
				sprintf(cValueName, "%d_text", idSubMessage);
				sValueName.assign(cValueName);				
				//构造纯文本消息内容节点
				rootBuild[sValueName] = sMessage;
				string temp = fast_writer.write(rootBuild);							
				Char2CString(temp.c_str(), pIMData->messageJson);

				//提前处理一下消息, 消息中的表情转义字符不保存在消息内容之中, 只保存在消息扩展信息中
				pIMData->message = RemoveFace(message);				
				bRet = true;
			}
		}
		else if(pIMData->messageType == MT_Map)
		{
			//地图消息
			pIMData->message = _T("[位置]");		
			bRet = true;
		}
		else if (pIMData->messageType == MT_Image || pIMData->messageType == MT_Audio || pIMData->messageType == MT_Video || pIMData->messageType == MT_Document || pIMData->messageType == MT_Unknown)
		{
			//文件
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
					//文件服务器存储地址
					sUri = root["co"].asString();
					if (pIMData->isReceive || pIMData->isSyns)
						//接收情况下记录消息的时候本地存储路径先置为空, 带文件接收完毕再设置保存路径
						sFilePath = "";
					else
						//发送情况下记录消息的时候本地存储路径与uri相同
						sFilePath = sUri;
				}

				if (root.type() == Json::objectValue && root.isMember("si"))
				{
					//文件大小
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
					//文件显示名称
					sFileName = root["fn"].asString();
					Char2CString(sFileName.c_str(), fileName);
				}

				//构造消息扩展内容				
				Json::Value rootBuild;
				Json::Value rootFile;
				Json::FastWriter fast_writer;
				//显示样式
				rootFile["showstyle"] = Json::Value(SUBMSG_ME_SHOW_CH_SHOW);
				//缩略图地址
				rootFile["smalluri"] = "";
				//下载地址
				rootFile["uri"] = sUri;
				//本地存储路径
				rootFile["localpath"] = sFilePath;				
				//传输状态
				rootFile["TransState"] = Json::Value(Trans_Unknown);
				//最近一次传输的大小
				rootFile["lastTranSize"] = Json::Value(0);				
				//校验和
				rootFile["lastChecksum"] = "";
				//传输id
				rootFile["transID"] = Json::Value(-1);
				//传输sessionID
				rootFile["sessionID"] = "";
				if (pIMData->messageType != MT_Image)
				{
					//非图片文件设置文件名和文件大小
					//文件名
					rootFile["name"] = sFileName;
					//文件大小
					rootFile["size"] = sFileSize;
					//构造单文件数据名称
					string sValueName;
					char cValueName[64];
					if (pIMData->messageType == MT_Audio)
						sprintf(cValueName, "%d_audio", idSubMessage);
					else
						sprintf(cValueName, "%d_file", idSubMessage);
					sValueName.assign(cValueName);
					//构造纯文本消息内容节点
					rootBuild[sValueName] = rootFile;
				}
				else
				{
					//构造单文件数据名称
					string sValueName;
					char cValueName[64];
					sprintf(cValueName, "%d_image", idSubMessage);
					sValueName.assign(cValueName);
					//构造纯文本消息内容节点
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
			//富媒体(混合内容)
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
				//构造JSON使用的根节点
				Json::Value rootBuild;
				Json::Value co;
				if (root.type() == Json::objectValue && root.isMember("co"))
				{
					//富媒体消息内容节点
					co = root["co"];
				}

				string sMaTi, sMaAb, sMaPl, sMaWl;
				if (co.type() == Json::objectValue && co.isMember("ma"))
				{
					//富媒体主段落节点
					Json::Value ma = co["ma"];

					//解析主段落信息
					if(ma.type() == Json::objectValue && ma.isMember("ti"))
						//主标题
						sMaTi = ma["ti"].asString();

					if(ma.type() == Json::objectValue && ma.isMember("ab"))
						//主摘要
						sMaAb = ma["ab"].asString();

					if(ma.type() == Json::objectValue && ma.isMember("pl"))
						//主标题图片Uri
						sMaPl = ma["pl"].asString();

					if(ma.type() == Json::objectValue && ma.isMember("wl"))
						//主段落链接
						sMaWl = ma["wl"].asString();

					//构造JSON使用的临时节点
					Json::Value temp;
					string sValueName;
					char cValueName[64];
					////////////////////////////////////////////////////
					temp["text"] = sMaTi;
					temp["uri"] = sMaWl;
					//构造主段落标题数据名称
					sprintf(cValueName, "%d_title", idSubMessage);
					sValueName.assign(cValueName);
					rootBuild[sValueName] = temp;
					///////////////////////////////////////////////////
					//构造主段落摘要数据名称
					sprintf(cValueName, "%d_text", ++idSubMessage);
					sValueName.assign(cValueName);
					rootBuild[sValueName] = sMaAb;
					///////////////////////////////////////////////////
					temp.clear();
					temp["uri"] = sMaPl;
					temp["localpath"] = "";
					//构造主段落图片数据名称
					sprintf(cValueName, "%d_image", ++idSubMessage);
					sValueName.assign(cValueName);
					rootBuild[sValueName] = temp;
				}

				if (co.type() == Json::objectValue && co.isMember("op"))
				{
					//解析副段落信息, 遍历所有副段落信息
					string sOpTe, sOpPl, sOpWl;
					Json::Value ops = co["op"];
					for (int i = 0; i < (int)ops.size(); i++)
					{
						Json::Value op = ops[i];
						if(op.type() == Json::objectValue && op.isMember("te"))
							//副标题
							sOpTe = op["te"].asString();

						if(op.type() == Json::objectValue && op.isMember("pl"))
							//副标题图片Uri
							sOpPl = op["pl"].asString();

						if(op.type() == Json::objectValue && op.isMember("wl"))
							//副段落链接
							sOpWl = op["wl"].asString();

						//构造JSON使用的临时节点
						Json::Value temp;
						string sValueName;
						char cValueName[64];
						////////////////////////////////////////////////////
						temp["text"] = sOpTe;
						temp["uri"] = sOpWl;
						//构造主段落标题数据名称
						sprintf(cValueName, "%d_title", ++idSubMessage);
						sValueName.assign(cValueName);
						rootBuild[sValueName] = temp;
						///////////////////////////////////////////////////
						temp.clear();
						temp["uri"] = sOpPl;
						temp["localpath"] = "";
						//构造主段落图片数据名称
						sprintf(cValueName, "%d_image", ++idSubMessage);
						sValueName.assign(cValueName);
						rootBuild[sValueName] = temp;
					}
				}

				//构造消息扩展内容				
				Json::FastWriter fast_writer;
				string temp = fast_writer.write(rootBuild);				
				Char2CString(temp.c_str(), pIMData->messageJson);
				pIMData->message = fileName;

				//记录消息内容
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
					//提取co中的内容作为文本内容
					string sMessage  = root["title"].asString();		
					Char2CString(sMessage.c_str(), pIMData->message);
				}				
				bRet = true;				
			}
		}		
		else if(pIMData->messageType == MT_Reply)
		{
			//回复类消息
			/*
				{
					"co": "已收到",
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
					//提取co中的内容作为文本内容
					string sMessage  = root["co"].asString();		
					Char2CString(sMessage.c_str(), pIMData->message);
				}

				//构造消息扩展内容				
				Json::Value rootBuild;
				Json::FastWriter fast_writer;

				string sValueName;
				char cValueName[64] = {0};
				sprintf(cValueName, "%d_reply", idSubMessage);
				sValueName.assign(cValueName);				

				//构造纯文本消息内容节点
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
/// 开始处理即时消息
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
/// 停止预处理即时消息
/// <summary>
/// <param name="force">是否强制停止预处理线程</param>
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
/// 开始更新预处理
/// 预处理包括(从数据库取出数据并按照交互对象分类以用于界面数据更新)
/// <summary>
void CMessageManager::StartUpdatePreprocess()
{
}

/// <summary>
/// 停止更新预处理
/// <summary>
/// <param name="force">是否强制停止更新预处理线程</param>
void CMessageManager::StopUpdatePreprocess(bool force)
{
}

/// <summary>
/// 对需要记录的即时消息进行预处理
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
				//500毫秒或超过500条记录保存数据库
				messageManager->RecordRIM();

				//modify by ljj 优化保存处理机制
				CSingleLock lockBufferIM(&messageManager->csBufferIM);
				lockBufferIM.Lock();
				bool isQueueEmpty = messageManager->queueIMData.empty();
				lockBufferIM.Unlock();

                //如果即时消息预处理线程运行
				if (isQueueEmpty)
                {				
					Sleep(10);
                    continue;
                }

                IMBasicData *im = NULL;
                //获取一个待处理即时消息
				lockBufferIM.Lock();
				im = messageManager->queueIMData.front();
				messageManager->queueIMData.pop();
				lockBufferIM.Unlock();
				if (im)
				{
					//对即时消息进行预处理
					//构造消息ID					
					//构造即时消息存储数据库语句
					CString sql = messageManager->BuildRecodeMessageSQL2(*im);
					//将预处理结果放在即时消息记录列表中
					IMRecord record;
					record.chatType = im->chatType;
					record.dbMID = im->dbMID;
					record.recordSQL = sql;
					record.targetID = im->targetUID;
					//声音提醒的逻辑处理
					record.nTipType = messageManager->GetMessageTipType(im->tss/1e6, im->isSyns, im->isRoaming);
					//缓存sql记录
					CSingleLock lockRecordIM(&messageManager->csRecordIM);
					lockRecordIM.Lock();
					messageManager->imRecord.push(record);
					lockRecordIM.Unlock();

					//释放数据					
					delete im;
					im = NULL;
				}
				////500毫秒或超过500条记录保存数据库
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
/// 删除消息
/// <summary>
/// <param name="dbMID">消息的数据库ID</param>
void CMessageManager::DeleteMessage(CString dbMID)
{
	CSingleLock lock(&m_csIM);
	lock.Lock();
	//把消息从消息列表中删除
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

	//构造数据库语句
	CString sql;
	sql.Format(_T("delete from %s where id = '%s'"), ICT_IMTABLE, dbMID);
	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//执行记录消息操作
	DBExecute(sqlline);
	delete sqlline;
}

void CMessageManager::DeleteMessageFrom(CString targetId, __int64 time)
{
	//构造数据库语句
	CString sql;
	sql.Format(_T("delete from %s where target_id = '%s' and time_serv >= '%I64d';"), ICT_IMTABLE,targetId, time);
	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//执行记录消息操作
	DBExecute(sqlline);
	delete sqlline;
}

/// <summary>
/// 清空消息记录
/// <summary>
void CMessageManager::ClearMessage()
{
	CSingleLock lock(&m_csIM);
	lock.Lock();
	//清空消息列表
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

	//构造数据库语句
	CString sql;
	sql.Format(_T("delete from \"%s\""), ICT_IMTABLE);
	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//执行记录消息操作
	DBExecute(sqlline);
	delete sqlline;
}

/// <summary>
/// 清空消息
/// <summary>
/// <param name="targetUID">聊天对象ID</param>
void CMessageManager::ClearMessage(CString targetUID)
{
	CSingleLock lock(&m_csIM);
	lock.Lock();
	//把消息从消息列表中删除
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

	//构造数据库语句
	CString sql;
	sql.Format(_T("delete from %s where %s = '%s'"), ICT_IMTABLE, ICT_IMFIELD_TARGERTID, targetUID);
	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//执行记录消息操作
	DBExecute(sqlline);
	delete sqlline;
}

/// <summary>
/// 获取最近联系人
/// <summary>
/// <param name="targetUID">聊天对象ID</param>
/// <param name="ret">最近联系人信息</param>
void CMessageManager::GetRecentContact(CString targetUID, CRecentContact& ret)
{
	//构造数据库语句
	CString sql;	
	sql.Format(_T("select *,sum(hasRead == 0) as unReadNum from %s where %s = '%s' group by %s, %s order by max(time_pro) asc"), ICT_IMTABLE, ICT_IMFIELD_TARGERTID, targetUID, ICT_IMFIELD_CHATTYPE, ICT_IMFIELD_TARGERTID);
	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//执行查询操作
	result = DBSelect(sqlline, row, col);
	delete sqlline;

	//处理查询结果
	if (row > 0)
	{
		int index = col;
		for(int i = 0 ; i < row; i++)
		{
			CInstantMessage* message = new CInstantMessage();
			for(int j = 0; j < col; j++)
			{
				//字段名称
				CString title;
				DBChar2CString(result[j], title);
				//字段值
				CString value;
				DBChar2CString(result[index], value);

#ifdef MESSAGEM_LOG
				//日志
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
			//即时消息对象化
			message->MessageToObject();
			//返回
			ret.lastMessage = message;
		}
	}

	//使用过后需要将查询结果释放
	sqlite3_free_table(result);
}

/// <summary>
/// 获取最近联系人
/// <summary>
/// <param name="ret">最近联系人信息列表</param>
void CMessageManager::GetRecentContacts(list<CRecentContact*>& ret)
{
	//char *sql = "select * from IM group by chat_type, target_id order by max(time_pro) asc";
	char *sql = "select *,sum(hasRead==0) as unReadNum from IM group by chat_type, target_id having target_description is not 2 order by target_description desc, max(time_pro) desc";
	int row = 0;
	int col = 0;
	char **result;
	//执行查询操作
	result = DBSelect(sql, row, col);

	//处理查询结果
	if (row > 0)
	{
		int index = col;
		for(int i = 0 ; i < row; i++)
		{
			CRecentContact *contact = new CRecentContact();
			CInstantMessage *message = new CInstantMessage();
			for(int j = 0; j < col; j++)
			{
				//字段名称
				CString title;
				DBChar2CString(result[j], title);
				//字段值
				CString value;
				DBChar2CString(result[index], value);

#ifdef MESSAGEM_LOG
				//日志
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

			//即时消息对象化
			message->MessageToObject();
			//返回
			contact->lastMessage = message;
			ret.push_back(contact);
		}
	}

	//使用过后需要将查询结果释放
	sqlite3_free_table(result);
}
//从数据库删除最近消息列表的某条记录
void CMessageManager::DeleteRecordFrom(CString uid)
{
	CString sql;
	sql.Format(_T("delete from %s where '%s' = '%s'"), ICT_IMTABLE, ICT_IMFIELD_TARGERTID, uid);
	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//执行记录消息操作
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

		//执行记录消息操作
		DBExecute(sqlline);
		delete sqlline;			
	}	
	return;
}

/// <summary>
/// 设置子消息参数
/// <summary>
/// <param name="dbMID">消息的数据库ID</param>
/// <param name="subMessageID">子消息消息ID</param>
/// <param name="type">子消息类型</param>
/// <param name="smUpdate">子消息需要更新的参数</param>
void CMessageManager::SetSubMsgParam(CString dbMID, CString subMessageID, SubMessageType type, SubMsgUpdate smUpdate)
{
	//根据消息ID获取消息
	CInstantMessage message;
	this->GetMessage(dbMID, message);
	if (message.dbMID == _T(""))
		return;

	//解析扩展消息内容, 并修改本地存储路径
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
		//构造主段落标题数据名称
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
			//获取文件节点
			Json::Value *valueSubMessage = &root[sValueName];
			if (valueSubMessage->type() == Json::objectValue)
			{
				//修改显示样式
				if(valueSubMessage->isMember("showstyle") && smUpdate.dwFlag & SMF_SHOWSTYLE)
					(*valueSubMessage)["showstyle"] = Json::Value(smUpdate.showStyle);
				//修改缩略图地址
				if(valueSubMessage->isMember("smalluri") && smUpdate.dwFlag & SMF_SMALLURI)
				{
					char temp[512] = {0};
					CString2Char(smUpdate.smallUri, temp);
					(*valueSubMessage)["smalluri"] = temp;
				}					
				//修改下载地址
				if(valueSubMessage->isMember("uri") && smUpdate.dwFlag & SMF_URI)
				{
					char temp[512] = {0};
					CString2Char(smUpdate.uri, temp);
					(*valueSubMessage)["uri"] = temp;
				}					
				//修改本地路径
				if(valueSubMessage->isMember("localpath") && smUpdate.dwFlag & SMF_LOCALPATH)
				{
					char temp[512] = {0};
					CString2Char(smUpdate.fileFullPath, temp);
					(*valueSubMessage)["localpath"] = temp;
				}					
				//修改传输状态
				if(valueSubMessage->isMember("TransState") && smUpdate.dwFlag & SMF_STATE)
					(*valueSubMessage)["TransState"] = Json::Value(smUpdate.state);
				//修改最近一次传输的大小
				if(valueSubMessage->isMember("lastTranSize") && smUpdate.dwFlag & SMF_LASTTRANSSIZE)
					(*valueSubMessage)["lastTranSize"] = Json::Value(smUpdate.lastTranSize);
				//修改校验和
				if(valueSubMessage->isMember("lastChecksum") && smUpdate.dwFlag & SMF_LASTCHECKSUM)
				{
					char temp[512] = {0};
					CString2Char(smUpdate.lastChecksum, temp);
					(*valueSubMessage)["lastChecksum"] = temp;
				}	
				//修改文件传输ID
				if(valueSubMessage->isMember("transID") && smUpdate.dwFlag & SMF_TRANSID)
					(*valueSubMessage)["transID"] = Json::Value(smUpdate.transID);
				//修改文件传输sessionID
				if(valueSubMessage->isMember("sessionID") && smUpdate.dwFlag & SMF_SESSIONID)
				{
					char temp[512] = {0};
					CString2Char(smUpdate.sessionID, temp);
					(*valueSubMessage)["sessionID"] = temp;
				}					

				//Json结构文本化
				const char* pJsonContent = NULL;
				Json::FastWriter fast_writer;
				string temp = fast_writer.write(root);

				//更新数据库数据
				pJsonContent = temp.c_str();
				CString messageExten;
				Char2CString(pJsonContent, messageExten);
				this->SetMessageExtenByMessageID(dbMID, messageExten);
			}
		}
	}

	CSingleLock lock(&m_csIM);
	lock.Lock();
	//更新即时消息列表中的展现样式
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
/// 获取即时消息
/// <summary>
/// <param name="targetUID">聊天对象ID</param>
/// <param name="ret">即时消息</param>
/// <param name="setRead">是否需要设置已读</param>
void CMessageManager::GetMessages(CString targetUID, list<CInstantMessage*>& ret, bool setRead)
{
	//构造数据库语句
	CString condition;
	condition.Format(_T("target_id = '%s'"), targetUID);
	CString sql;
	sql.Format(_T("select * from %s where %s order by time_pro asc, rowid asc"), ICT_IMTABLE, condition);
	//sql.Format(_T("select * from %s where %s order by rowid asc"), ICT_IMTABLE, condition);
	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//执行查询操作
	result = DBSelect(sqlline, row, col);
	delete sqlline;

	//处理查询结果
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
			//即时消息对象化
			message->MessageToObject();
			//返回
			ret.push_back(message);
		}
	}

	//使用过后需要将查询结果释放
	sqlite3_free_table(result);

	if (setRead)
	{
		//设置消息为已读
		CString updateCondition;
		updateCondition.Format(_T("(select id from %s where %s order by time_pro asc, rowid asc)"), ICT_IMTABLE, condition);
		SetMessageRead(updateCondition);
	}
}

/// <summary>
/// 获取即时消息
/// 调用该函数后需要自行调用MESSAGE_MANAGER::RealseMessages(messages)方法释放获取到的消息
/// <summary>
/// <param name="ret">即时消息</param>
/// <param name="count">即时消息个数</param>
/// <param name="setRead">是否需要设置已读</param>
void CMessageManager::GetMessages(list<CInstantMessage*>& ret, int count, bool setRead)
{
	//构造数据库语句
	CString condition;
	condition.Format(_T("limit %d"), count);
	CString sql;
	sql.Format(_T("select * from %s order by time_pro asc, rowid asc %s"), ICT_IMTABLE, condition);
	//sql.Format(_T("select * from %s order by rowid asc %s"), ICT_IMTABLE, condition);
	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//执行查询操作
	result = DBSelect(sqlline, row, col);
	delete sqlline;

	//处理查询结果
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
			//即时消息对象化
			message->MessageToObject();
			//返回
			ret.push_back(message);
		}
	}

	//使用过后需要将查询结果释放
	sqlite3_free_table(result);

	if (setRead)
	{
		//设置消息为已读
		CString updateCondition;
		updateCondition.Format(_T("(select id from %s order by time_pro asc, rowid asc %s)"), ICT_IMTABLE, condition);
		SetMessageRead(updateCondition);
	}
}

/// <summary>
/// 根据消息数据库ID获取消息内容
/// <summary>
/// <param name="dbMID">消息的数据库ID</param>
/// <param name="ret">即时消息</param>
void CMessageManager::GetMessage(CString dbMID, CInstantMessage& message, bool setRead)
{
	if (dbMID == _T(""))
		return;

	CString sql;
	sql.Format(_T("select * from %s where id = '%s'"), ICT_IMTABLE, dbMID);

	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//执行查询操作
	result = DBSelect(sqlline, row, col);
	delete sqlline;

	//处理查询结果
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
		//即时消息对象化
		message.MessageToObject();
	}

	//使用过后需要将查询结果释放
	sqlite3_free_table(result);

	if (setRead)
	{
		//设置消息为已读
		CString updateCondition;
		updateCondition.Format(_T("( '%s')"),dbMID);
		SetMessageRead(updateCondition);
	}
}

/// <summary>
/// 获取消息内容(根据消息数据库ID获取消息内容)
/// <summary>
/// <param name="dbMID">消息的数据库ID</param>
/// <param name="ret">即时消息</param>
void CMessageManager::GetMessage(CString dbMID, CInstantMessage* &ret)
{
	CSingleLock lock(&m_csIM);
	lock.Lock();
	//先去即时消息列表中查找
	for(list<CInstantMessage*>::iterator iter = m_listInstantMessage.begin(); iter != m_listInstantMessage.end(); iter++)
	{
		CInstantMessage* instantMessage = (*iter);
		if(instantMessage->dbMID == dbMID)
		{
			ret = instantMessage;
			return;
		}
	}	

	//若找不到则从本地数据库中构造
	ret = new CInstantMessage();
	this->GetMessage(dbMID, (*ret));
	m_listInstantMessage.push_back(ret);

	lock.Unlock();
}

/// <summary>
/// 获取即时消息(根据目标ID获取未读即时消息, 无页码条件)
/// **调用该函数后需要自行调用MESSAGE_MANAGER::RealseMessages(messages)方法释放获取到的消息
/// <summary>
/// <param name="targetUID">聊天对象ID</param>
/// <param name="ret">即时消息</param>
void CMessageManager::GetUnreadMessages(CString targetUID, list<CInstantMessage*>& ret)
{
	CString targetInformation = _T("");
	if (targetUID != _T(""))
		targetInformation.Format(_T("target_id = '%s' and"), targetUID);

	//构造数据库语句
	CString sql;
	sql.Format(_T("select * from %s where %s hasRead = 0"), ICT_IMTABLE, targetInformation);
	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//执行查询操作
	result = DBSelect(sqlline, row, col);
	delete sqlline;

	//处理查询结果
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
			//即时消息对象化
			message->MessageToObject();
			//返回
			ret.push_back(message);
		}
	}

	//使用过后需要将查询结果释放
	sqlite3_free_table(result);
}

/// <summary>
/// 获取即时消息
/// <summary>
/// <param name="ret">即时消息</param>
/// <param name="page">页码</param>
/// <param name="count">每页消息条数</param>
/// <param name="conditions">获取条件</param>
/// <param name="isAsc">是否正序</param>
/// <param name="setRead">是否需要设置已读</param>
void CMessageManager::GetMessagesC(list<CInstantMessage*>& ret, int page, int count, CString conditions, bool isAsc, bool setRead)
{
	if (page <= 0 || count <= 0)
		return;

	//计算首个消息编号
	int first = (page - 1) * count;
	return GetMessagesCEx(ret, first, count, conditions, isAsc, setRead);
}

/// <summary>
/// 获取即时消息
/// <summary>
/// <param name="ret">即时消息</param>
/// <param name="first">首条消息编号</param>
/// <param name="count">每页消息条数</param>
/// <param name="conditions">获取条件</param>
/// <param name="isAsc">是否正序</param>
/// <param name="setRead">是否需要设置已读</param>
void CMessageManager::GetMessagesCEx(list<CInstantMessage*>& ret, int first, int count, CString conditions, bool isAsc, bool setRead)
{
	//构造数据库语句
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

	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//执行查询操作
	result = DBSelect(sqlline, row, col);
	delete sqlline;

	//处理查询结果
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
			//即时消息对象化
			message->MessageToObject();
			//记录
			ret.push_back(message);
		}
	}

	//使用过后需要将查询结果释放
	sqlite3_free_table(result);

	if (setRead)
	{
		//设置消息为已读
		CString updateCondition;
		if (_T("") != conditions)
			updateCondition.Format(_T("(select id from %s where %s)"), ICT_IMTABLE, condition);
		else
			updateCondition.Format(_T("(select id from %s order by time_pro asc, rowid asc LIMIT %d,%d)"), ICT_IMTABLE, condition);

		SetMessageRead(updateCondition);
	}
}

/// <summary>
/// 获取即时消息
/// <summary>
/// <param name="page">页码</param>
/// <param name="count">每页消息条数</param>
/// <param name="ret">即时消息</param>
/// <param name="setRead">是否需要设置已读</param>
void CMessageManager::GetMessages(int page, int count, list<CInstantMessage*>& ret, bool setRead)
{
	if (page <= 0 || count <= 0)
		return;
	return GetMessagesC(ret, page, count, _T(""), setRead);
}

/// <summary>
/// 获取即时消息
/// <summary>
/// <param name="targetUID">聊天对象ID</param>
/// <param name="page">页码</param>
/// <param name="count">每页消息条数</param>
/// <param name="ret">即时消息</param>
/// <param name="date">日期</param>
/// <param name="isUnread">是否未读</param>
/// <param name="isAsc">是否正序</param>
/// <param name="setRead">是否需要设置已读</param>
void CMessageManager::GetMessages(CString targetUID, int page, int count, list<CInstantMessage*>& ret, CString date, bool isUnread, bool isAsc, bool setRead)
{
	if (page <= 0 || count <= 0 || _T("") == targetUID)
		return;

	//计算首个消息编号
	int first = (page - 1) * count;
	return GetMessagesEx(targetUID, first, count, ret, date, isUnread, isAsc, setRead);
}

/// <summary>
/// 获取即时消息
/// <summary>
/// <param name="targetUID">聊天对象ID</param>
/// <param name="first">首条消息编号</param>
/// <param name="count">每页消息条数</param>
/// <param name="ret">即时消息</param>
/// <param name="date">日期</param>
/// <param name="isUnread">是否未读</param>
/// <param name="isAsc">是否正序</param>
/// <param name="setRead">是否需要设置已读</param>
void CMessageManager::GetMessagesEx(CString targetUID, int first, int count, list<CInstantMessage*>& ret, CString date, bool isUnread, bool isAsc, bool setRead)
{
	//构造数据库查询条件
	//添加联系人ID条件
	CString conditions;
	conditions.Format(_T("%s = '%s'"), ICT_IMFIELD_TARGERTID, targetUID);
	//添加日期条件
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
		//构造数据库查询条件
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
/// 获取即时消息
/// <summary>
/// <param name="date">日期</param>
/// <param name="page">页码</param>
/// <param name="count">每页消息条数</param>
/// <param name="ret">即时消息</param>
/// <param name="setRead">是否需要设置已读</param>
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
	//构造数据库查询条件
	CString strJTimeS;
	strJTimeS.Format(_T(" julianday('%s')"), szTimeS);
	CString strJTimeE;
	strJTimeE.Format(_T(" julianday('%s')"), szTimeE);
	CString conditions;
	conditions.Format(_T("time_pro < %s and time_pro > %s"), strJTimeE, strJTimeS);
	return GetMessagesC(ret, page, count, conditions, setRead);
}

/// <summary>
/// 获取即时消息
/// <summary>
/// <param name="key">消息内容关键字</param>
/// <param name="page">页码</param>
/// <param name="count">每页消息条数</param>
/// <param name="ret">即时消息</param>
/// <param name="targetUID">目标ID</param>
/// <param name="setRead">是否需要设置已读</param>
void CMessageManager::GetMessagesK(CString key, int page, int count, list<CInstantMessage*>& ret, CString targetUID, bool setRead)
{
	if (page <= 0 || count <= 0 || _T("") == key)
		return;
	//构造数据库查询条件
	CString conditions;
	conditions.Format(_T("message like '%%%s%%'"), key);
	//添加联系人id条件
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

	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//执行查询操作
	result = DBSelect(sqlline, row, col);
	delete sqlline;

	//处理查询结果
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
					// 数据库版本<V104.db
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
					// 数据库版本>=V104.db
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

	//使用过后需要将查询结果释放
	sqlite3_free_table(result);
}

CString CMessageManager::GetMessageSql(IMBasicData *pIMData)
{
	if(!pIMData)
		return _T("");

	//构造数据库语句
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
/// 获取即时消息数量
/// <summary>
/// <param name="onlyUnread">是否只获取未读的</param>
int CMessageManager::GetMessageCount(bool onlyUnread)
{
	return GetMessageCountC(_T(""), onlyUnread);
}

/// <summary>
/// 获取即时消息数量
/// <summary>
/// <param name="targetUID">聊天对象ID</param>
/// <param name="date">日期</param>
/// <param name="onlyUnread">是否只获取未读的</param>
int CMessageManager::GetMessageCount(CString targetUID, CString date, bool onlyUnread)
{
	//构造数据库查询条件
	//添加联系人ID条件
	CString condition;
	condition.Format(_T("%s = '%s'"), ICT_IMFIELD_TARGERTID, targetUID);
	//添加日期条件
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
		//构造数据库查询条件
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
/// 获取即时消息数量
/// <summary>
/// <param name="date">日期</param>
/// <param name="onlyUnread">是否只获取未读的</param>
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

	//构造数据库查询条件
	CString strJTimeS;
	strJTimeS.Format(_T(" julianday('%s')"), szTimeS);
	CString strJTimeE;
	strJTimeE.Format(_T(" julianday('%s')"), szTimeE);
	CString conditions;
	conditions.Format(_T("time_pro < %s and time_pro > %s"), strJTimeE, strJTimeS);
	return GetMessageCountC(conditions, onlyUnread);
}

/// <summary>
/// 获取即时消息数量
/// <summary>
/// <param name="key">内容关键字</param>
/// <param name="targetUID">聊天对象ID</param>
/// <param name="onlyUnread">是否只获取未读的</param>
int CMessageManager::GetMessageCountK(CString key, CString targetUID, bool onlyUnread)
{
	//构造数据库查询条件
	CString conditions;
	conditions.Format(_T("message like '%%%s%%'"), key);

	//添加联系人id条件
	if (_T("") != targetUID)
	{
		CString conditionID;
		conditionID.Format(_T("%s = '%s'"), ICT_IMFIELD_TARGERTID, targetUID);
		conditions += (_T(" and ") + conditionID);
	}

	return GetMessageCountC(conditions, onlyUnread);
}


/// <summary>
/// 获取即时消息数量
/// <summary>
/// <param name="conditions">获取条件</param>
int CMessageManager::GetMessageCountC(CString conditions, bool onlyUnread)
{
	//构造数据库语句
	if (onlyUnread)
		conditions += (_T(" and hasRead = 0"));
	CString sql;
	if (_T("") != conditions)
		sql.Format(_T("select count(*) from %s where %s"), ICT_IMTABLE, conditions);
	else
		sql.Format(_T("select count(*) from %s"), ICT_IMTABLE);

	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//执行查询操作
	result = DBSelect(sqlline, row, col);
	delete sqlline;

	//处理查询结果
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
/// 设置消息tsc
/// <summary>
/// <param name="targetUID">聊天对象ID</param>
void CMessageManager::SetMessageTscByMID(CString mid, __int64 tsc)
{
	//构造数据库语句
	CString sql;
	sql.Format(_T("update %s set %s = '%I64d', %s = '%I64d' where %s = '%s';"), ICT_IMTABLE, ICT_IMFIELD_TSC, tsc, ICT_IMFIELD_TIME, tsc, ICT_IMFIELD_ID, mid);
	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//执行记录消息操作
	DBExecute(sqlline);
	delete sqlline;
}

//modified by zhangpeng on 20160419	begin
/// <summary>
/// 设置消息已读
/// <summary>
/// <param name="targetUID">聊天对象ID</param>
void CMessageManager::SetMessageReadByTargetID(CString targetUID,char* tss)
{
	//构造数据库语句
	CString sql;
	CString cstrtss;
	Char2CString(tss,cstrtss);
	sql.Format(_T("update %s set hasRead = 1 where hasRead != 1 and %s = '%s' and %s < '%s';"), ICT_IMTABLE, ICT_IMFIELD_TARGERTID, targetUID,ICT_IMFIELD_TSS,cstrtss);
	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//执行记录消息操作
	DBExecute(sqlline);
	delete sqlline;
}
//modified by zhangpeng on 20160419	end
/// <summary>
/// 设置消息已读
/// <summary>
/// <param name="targetUID">聊天对象ID</param>
void CMessageManager::SetMessageReadByTargetID(CString targetUID)
{
	//构造数据库语句
	CString sql;
	sql.Format(_T("update %s set hasRead = 1 where hasRead != 1 and %s = '%s';"), ICT_IMTABLE, ICT_IMFIELD_TARGERTID, targetUID);
	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//执行记录消息操作
	DBExecute(sqlline);
	delete sqlline;
}
void CMessageManager::SetTargetDescriptionDeleteByTargetID(CString targetUID)
{
	//构造数据库语句
	CString sql;
	sql.Format(_T("update IM set target_description = 2 where target_id = '%s' and time_pro=(select max(time_pro) from IM where target_id = '%s');;"),targetUID,targetUID);
	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//执行记录消息操作
	DBExecute(sqlline);
	delete sqlline;
}
void CMessageManager::SetTargetDescriptionTopByTargetID(CString targetUID)
{
	//构造数据库语句
	CString sql;
	sql.Format(_T("update %s set target_description = 1 where %s = '%s';"), ICT_IMTABLE, ICT_IMFIELD_TARGERTID, targetUID);
	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//执行记录消息操作
	DBExecute(sqlline);
	delete sqlline;
}


/// <summary>
/// 设置消息已读(根据息数据库ID设置消息已读)
/// <summary>
/// <param name="dbMID">消息的数据库ID</param>
void CMessageManager::SetMessageReadByMessageID(CString dbMID)
{
	//构造数据库语句
	CString sql;
	sql.Format(_T("update %s set hasRead = 1 where id = '%s';"), ICT_IMTABLE, dbMID);
	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//执行记录消息操作
	DBExecute(sqlline);
	delete sqlline;
}

/// <summary>
/// 设置消息扩展内容
/// <summary>
/// <param name="dbMID">消息的数据库ID</param>
/// <param name="messageExten">消息扩展内容</param>
void CMessageManager::SetMessageExtenByMessageID(CString dbMID, CString messageExten)
{
	//构造数据库语句
	CString sql;
	sql.Format(_T("update %s set message_exten = '%s' where id = '%s';"), ICT_IMTABLE, messageExten, dbMID);
	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//执行记录消息操作
	DBExecute(sqlline);
	delete sqlline;
}


/// <summary>
/// 设置消息是否可用
/// <summary>
/// <param name="dbMID">消息的数据库ID</param>
/// <param name="isActive">是否激活</param>
void CMessageManager::SetMessageActive(CString dbMID, bool isActive)
{
	int active = 0;
	if (isActive)
		active = 1;
	//构造数据库语句
	CString sql;
	sql.Format(_T("update %s set isActive = %d where id = '%s';"), ICT_IMTABLE, active, dbMID);
	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//执行记录消息操作
	DBExecute(sqlline);
	delete sqlline;
}

/// <summary>
/// 设置消息已读
/// 子查询字段是id
/// <summary>
/// <param name="condition">设置条件</param>
void CMessageManager::SetMessageRead(CString condition)
{
	//构造数据库语句
	CString sql;
	sql.Format(_T("update %s set hasRead = 1 where id in %s;"), ICT_IMTABLE, condition);
	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//执行记录消息操作
	DBExecute(sqlline);
	delete sqlline;
}

/// <summary>
/// 设置所有消息已读
/// <summary>
void CMessageManager::SetAllMessageRead()
{
	//构造数据库语句
	CString sql;
	sql.Format(_T("update %s set hasRead = 1;"), ICT_IMTABLE);
	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();
	//执行记录消息操作
	DBExecute(sqlline);
	delete sqlline;
}

/// <summary>
/// 删除转义字符
/// <summary>
/// <param name="text">文本(包含表情转义字符)</param>
CString CMessageManager::RemoveFace(CString text)
{
	if (text.Find(_T("[")) != -1 && text.Find(_T("]")) != -1)
	{
		//有可能包含表情, 检查并对对表情消息进行特殊处理(支持多个表情混排)
		int pos = 0;
		CUserAgentCore* ua = CUserAgentCore::GetUserAgentCore();
		while (pos != -1)
		{
			CString temp = text;
			//检查找到的关键字前后是否有表情转义字符的开始及闭合标识
			int start = temp.Find(_T("["), pos);
			int end = -1;
			if (start != -1)
			{
				//找到转义字符开始符号"["
				//查找转义字符结束符号"]"
				end = temp.Find(_T("]"), start + 1);
			}
			if (end == -1)
				pos = -1;

			if (pos == -1)
				//未找到, 退出
				continue;

			CString faceTag = temp.Mid(start, (end - start + 1));
			CString faceName = ua->GetFaceManager()->GetNameByTag(faceTag);
			if (faceName != _T(""))
			{
				//表情转义字符剔除
				temp.Replace(faceTag, _T(""));
				text = temp;
				pos = start;
			}
			else
			{
				//不是表情转义字符, 跳过这次查找的开始位置+1后继续查找
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


	//同步消息不做任何提醒
	if(isSync)
		return 0;

	int nTipType = TIP_FLICKER | TIP_REDPOINT;

	//判断是否进行声音提醒
	bool bSndTip = false;
	static bool bHadTip = false;
	if(nMsgTime < m_tsMQTTConnected)
	{
		//离线消息，仅做一次提醒		
		if(!bHadTip)
		{			
			bSndTip = true;
			bHadTip = true;
		}
	}
	else
	{
		//非离线消息正常声音提醒
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

	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//执行查询操作
	result = DBSelect(sqlline, row, col);
	delete sqlline;

	//处理查询结果
	if (row > 0)
	{
		dbID = CString(result[1]);
	}
	return 0;
}
//modified by zhangpeng on 20160417 begin
int CMessageManager::GetRecvMaxTimePro(char* targetUID,CString& outMaxProTime)
{
	//构造数据库语句
	CString sql, condition,cstruid ;
	Char2CString(targetUID,cstruid);
	condition.Format(_T("%s = '%s'and %s = 1"),ICT_IMFIELD_TARGERTID,cstruid,ICT_IMFIELD_RECEIVE);
	sql.Format(_T("select max(%s) from %s where %s order by %s asc"),ICT_IMFIELD_TSS, ICT_IMTABLE, condition, ICT_IMFIELD_TSS);//查询最大ProTime

	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//执行查询操作
	result = DBSelect(sqlline, row, col);
	delete sqlline;
	int64 i64 = 0;
	//处理查询结果
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
	//构造数据库语句
	CString sql, condition;
	condition.Format(_T("%s = '%s' and %s >= '%I64d' and %s <= '%I64d'"),ICT_IMFIELD_TARGERTID, targetUID, ICT_IMFIELD_TSC, tscStart, ICT_IMFIELD_TSC, tscEnd);

	sql.Format(_T("select %s from %s where %s order by %s asc"),ICT_IMFIELD_TSC, ICT_IMTABLE, condition, ICT_IMFIELD_TSC);


	//数据库语句格式转换
	char *sqlline;
	DBwchar2char(sql.GetBuffer(sql.GetLength() + 1), sqlline);
	sql.ReleaseBuffer();

	int row = 0;
	int col = 0;
	char **result;
	//执行查询操作
	result = DBSelect(sqlline, row, col);
	delete sqlline;

	//处理查询结果
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
/// 释放即时消息
/// <summary>
/// <param name="messages">即时消息列表</param>
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
/// 释放最近联系人
/// <summary>
/// <param name="contacts">最近联系人列表</param>
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
