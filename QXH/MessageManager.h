#pragma once
#include "CoreInterface\DB\DBManager.h"
#include "CoreInterface\JSON\json.h"
#include "CoreInterface\MessageManage\InstantMessage.h"
#include "CoreInterface\MessageManage\RecentContact.h"
#include "CoreInterface\UA\UserAgentCoreDef.h"
#include "afxmt.h"
#include <queue>
/// <summary>
/// 数据库版本
/// <summary>
/// v102 - 添加sid(服务器消息ID)
///        修改描述信息字段description为sender_description
///		   添加聊天目标描述信息字段target_description
///		   添加发送者id(P2P消息 - 与目标ID相同; 群组消息和公共账号消息 - 发送者ID)
/// v103 - 添加isSyns(是否是同步消息)
/// <summary>
#define MSG_DB_VERSION 105

namespace MESSAGE_MANAGER
{
	//消息提醒方式
	enum MSG_TIP_TYPE
	{
		//声音提醒
		TIP_SOUND = 1,
		//闪动提醒
		TIP_FLICKER = 2,
		//红点提醒
		TIP_REDPOINT = 4,
		//漫游消息
		TIP_ROAMING = 5,
	};

/// <summary>
/// 即时消息基础数据
/// 为预处理提供数据
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
/// 即时消息记录信息
/// 用于即时消息的延迟记录
/// <summary>
struct IMRecord
{
	/// <summary>
	/// 聊天类型
	/// <summary>
	ChatType chatType;
	/// <summary>
	/// 消息的数据库ID
	/// <summary>
	CString dbMID;
	/// <summary>
	/// 交互对象ID
	/// <summary>
	CString targetID;
	/// <summary>
	/// 消息插入数据库语句
	/// <summary>
	CString recordSQL;
	/// <summary>
	///提醒类型, 0: 不做提醒，MSG_TIP_TYPE中的值及结合
	/// <summary>	
	int nTipType;
};

/// <summary>
/// 即时消息记录信息
/// 用于即时消息的延迟刷新
/// <summary>
struct IMUpdate
{
	/// <summary>
	/// 聊天类型
	/// <summary>
	ChatType chatType;
	/// <summary>
	/// 交互对象ID
	/// <summary>
	CString targetUID;
	/// <summary>
	/// 需要更新的消息ID
	/// <summary>
	list<CString> dbMIDs;
	/// <summary>
	///提醒类型, 0: 不做提醒，MSG_TIP_TYPE中的值及结合
	/// <summary>	
	int nTipType;
};

/// <summary>
/// 子消息更新结构
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
	/// 是否更新标志参数
	/// <summary>
	DWORD dwFlag;
	/// <summary>
	/// 显示样式
	/// <summary>
	int showStyle;
	/// <summary>
	/// 缩略图下载链接
	/// <summary>
	CString smallUri;
	/// <summary>
	/// 文件下载链接
	/// <summary>
	CString uri;
	/// <summary>
	/// 文件存储路径
	/// <summary>
	CString fileFullPath;
	/// <summary>
	/// 传输状态
	/// <summary>
	FileTransState state;	
	/// <summary>
	/// 最近一次传输的大小
	/// <summary>
	int lastTranSize;
	/// <summary>
	/// 上传文件的校验和
	/// <summary>
	CString lastChecksum;
	/// <summary>
	/// 文件传输ID
	/// <summary>
	int transID;
	/// <summary>
	/// 文件传输sessionID
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
/// 即时消息记录信息Map
/// <summary>
typedef map<CString, struct IMRecord> IMRecordMap;

/// <summary>
/// 即时消息记录信息队列
/// <summary>
typedef queue<struct IMRecord> IMRecordQueue;

/// <summary>
/// 即时消息更新Map
/// <summary>
typedef map<CString, struct IMUpdate> IMUpdateMap;
/********************************************************************
*
*   Copyright (C) 2014
*
*   摘要: 消息管理器
*
*******************************************************************/
class CMessageManager : public CDBManager
{
public:
	/// <summary>
	/// 消息管理器
	/// <summary>
	CMessageManager();
	/// <summary>
	/// 消息管理器
	/// <summary>
	/// <param name="pathDB">数据库路径</param>
	CMessageManager(CString pathDB);
	/// <summary>
	/// 消息管理器
	/// <summary>
	~CMessageManager();
private:
	/// <summary>
	/// 消息数据库名称
	/// <summary>
	CString nameMessageDB;
	/// <summary>
	/// 即时消息锁
	/// <summary>
	CCriticalSection m_csIM;
	/// <summary>
	/// 即时消息列表
	/// <summary>
	list<CInstantMessage*> m_listInstantMessage;
private:
	/// <summary>
	/// 即时消息基础数据列表, 为预处理提供数据
	/// <summary>
	queue<IMBasicData*> queueIMData;
	/// <summary>
	/// 即时消息基础数据列表访问临界区
	/// <summary>
	CCriticalSection csBufferIM;
	/// <summary>
	/// 即时消息预处理线程
	/// <summary>
	HANDLE threadPreprocessIM;
	/// <summary>
	/// 即时消息预处理线程是否执行
	/// <summary>
	bool preprocessIMDie;

	/// <summary>
	/// 需要记录的即时消息
	/// <summary>
	IMRecordQueue imRecord;
	/// <summary>
	/// 即时消息记录信息访问临界区
	/// <summary>
	CCriticalSection csRecordIM;
	/// <summary>
	/// 上一个即时消息时间戳
	/// <summary>
	SYSTEMTIME timeLastRecord;
	/// <summary>
	/// 即时消息更新时间间隔(毫秒)
	/// <summary>
	int imRecordInterval;
public:
	/// <summary>
	/// 需要更新的即时消息
	/// <summary>
	IMUpdateMap imUpdate;
	/// <summary>
	/// 即时消息更新访问临界区
	/// <summary>
	CCriticalSection csUpdateIM;
	//MQTT连接成功的时间
	time_t m_tsMQTTConnected;
private:
	/// <summary>
	/// 记录需要记录的即时消息
	/// <summary>
	bool RecordRIM(CString dbMID, IMRecord im);
	bool RecordRIM();

	/// <summary>
	/// 记录需要更新的即时消息
	/// <summary>
	/// <param name="dbMID">即时消息数据库ID</param>
	/// <param name="targetUID">交互对象UID</param>
	/// <param name="chatType">聊天类型</param>
	bool RecordUIM(CString dbMID, CString targetUID, ChatType chatType, int nTipType = TIP_SOUND|TIP_FLICKER|TIP_REDPOINT);
private:
	/// <summary>
	/// 设置数据库表(在这里定义数据库消息存储表结构)
	/// <summary>
	void SetTable();
public:
	/// <summary>
	/// 设置数据库表(旧版本)
	/// <summary>
	void SetOldTable();
	/// <summary>
	/// 设置数据库路径
	/// <summary>
	/// <param name="pathDB">数据库路径</param>
	void SetPath(CString pathDB);
public:
	//////////////////////////////////////////////////////////////////////////////////////////////
    //
	//  消息记录接口
	//  (此接口以服务器发送的或者向服务器发送的JSON串作为消息内容输入, 所有即时消息内容数据通过解析JSON串获取)
	//
	//////////////////////////////////////////////////////////////////////////////////////////////
	/// <summary>
	/// 记录消息（处理消息中的json）
	/// <summary>	
	void RecordMessageJson(IMBasicData imData);
	/// <summary>
	/// 记录消息
	/// <summary>	
	void RecordMessageJson2(IMBasicData imData);

	/// <summary>
	/// 开始预处理即时消息
	/// 预处理包括(消息内容转换为消息记录数据库语句)
	/// <summary>
	void StartPreprocessIM();
	/// <summary>
	/// 停止预处理即时消息
	/// <summary>
	/// <param name="force">是否强制停止消息预处理线程</param>
	void StopPreprocessIM(bool force = false);

	/// <summary>
	/// 开始更新预处理
	/// 预处理包括(从数据库取出数据并按照交互对象分类以用于界面数据更新)
	/// <summary>
	void StartUpdatePreprocess();
	/// <summary>
	/// 停止更新预处理
	/// <summary>
	/// <param name="force">是否强制停止更新预处理线程</param>
	void StopUpdatePreprocess(bool force = false);
private:
	/// <summary>
	/// 记录消息(SQL语句事务操作)
	/// <summary>
	/// <param name="sqls">SQL语句列表</param>
	void RecordMessageTransation(list<CString> sqls);

	/// <summary>
	/// 创建记录即时消息SQL语句
	/// 由于会比较耗时需要使用线程处理
	/// <summary>	
	CString BuildRecodeMessageSQL2(IMBasicData imData);

	/// <summary>
	/// 把IM数据从服务器数据转为本地数据库的数据
	/// <summary>	
	bool ConvertIMFromServToDB(IMBasicData *pIMData);
	
	/// <summary>
	/// 对需要记录的即时消息进行预处理
	/// <summary>
	static DWORD WINAPI PreporcessIM(LPVOID param);
public:
	//////////////////////////////////////////////////////////////////////////////////////////////
    //
	//  消息删除接口
	//
	//////////////////////////////////////////////////////////////////////////////////////////////
	/// <summary>
	/// 删除消息(根据消息数据库ID删除消息)
	/// <summary>
	/// <param name="dbMID">消息的数据库ID</param>
	void DeleteMessage(CString dbMID);
	/// <summary>
	/// 清空消息(删除所有消息)
	/// <summary>
	void ClearMessage();
	/// <summary>
	/// 清空消息(清除某个目标ID对应的所有消息)
	/// <summary>
	/// <param name="targetUID">聊天对象ID</param>
	void ClearMessage(CString targetUID);
	/// <summary>
	/// 删除某个会话从一个时间点开始之后的全部消息
	/// <summary>
	/// <param name="targetId">目标ID</param>
	/// <param name="time">起始时间</param>
	void DeleteMessageFrom(CString targetId, __int64 time);
public:
	//////////////////////////////////////////////////////////////////////////////////////////////
    //
	//  获取最近联系人接口
	//
	//////////////////////////////////////////////////////////////////////////////////////////////
	/// <summary>
	/// 获取最近联系人(根据目标ID获取最近联系人信息)
	/// <summary>
	/// <param name="targetUID">聊天对象ID</param>
	/// <param name="ret">最近联系人信息</param>
	void GetRecentContact(CString targetUID, CRecentContact& ret);
	/// <summary>
	/// 获取最近联系人列表
	/// **调用该函数后需要自行调用MESSAGE_MANAGER::RealseContacts(contacts)方法释放获取到的最近联系人信息
	/// <summary>
	/// <param name="ret">最近联系人信息</param>
	void GetRecentContacts(list<CRecentContact*>& ret);
	//从数据库删除最近消息列表的某条记录
	void DeleteRecordFrom(CString uid);

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
    //
	//  获取消息接口
	//
	//////////////////////////////////////////////////////////////////////////////////////////////
	/// <summary>
	/// 获取消息内容(根据消息数据库ID获取消息内容)
	/// <summary>
	/// <param name="dbMID">消息的数据库ID</param>
	/// <param name="ret">即时消息</param>
	void GetMessage(CString dbMID, CInstantMessage& ret, bool setRead=false);
	/// <summary>
	/// 获取消息内容(根据消息数据库ID获取消息内容)
	/// <summary>
	/// <param name="dbMID">消息的数据库ID</param>
	/// <param name="ret">即时消息</param>
	void GetMessage(CString dbMID, CInstantMessage* &ret);
	/// <summary>
	/// 获取即时消息(根据聊天对象ID获取未读即时消息, 无页码条件)
	/// **调用该函数后需要自行调用MESSAGE_MANAGER::RealseMessages(messages)方法释放获取到的消息
	/// <summary>
	/// <param name="targetUID">聊天对象ID</param>
	/// <param name="ret">即时消息</param>
	void GetUnreadMessages(CString targetUID, list<CInstantMessage*>& ret);
	/// <summary>
	/// 获取即时消息(根据聊天对象ID获取即时消息, 无页码条件)
	/// **调用该函数后需要自行调用MESSAGE_MANAGER::RealseMessages(messages)方法释放获取到的消息
	/// <summary>
	/// <param name="targetUID">聊天对象ID</param>
	/// <param name="ret">即时消息</param>
	/// <param name="setRead">是否需要设置已读</param>
	void GetMessages(CString targetUID, list<CInstantMessage*>& ret, bool setRead = false);
	/// <summary>
	/// 获取指定个数即时消息(无页码条件)
	/// **调用该函数后需要自行调用MESSAGE_MANAGER::RealseMessages(messages)方法释放获取到的消息
	/// <summary>
	/// <param name="ret">即时消息</param>
	/// <param name="count">即时消息个数</param>
	/// <param name="setRead">是否需要设置已读</param>
	void GetMessages(list<CInstantMessage*>& ret, int count, bool setRead = false);
	/// <summary>
	/// 获取即时消息(根据页码条件获取即时消息)
	/// **调用该函数后需要自行调用MESSAGE_MANAGER::RealseMessages(messages)方法释放获取到的消息
	/// <summary>
	/// <param name="page">页码</param>
	/// <param name="count">每页消息条数</param>
	/// <param name="ret">即时消息</param>
	/// <param name="setRead">是否需要设置已读</param>
	void GetMessages(int page, int count, list<CInstantMessage*>& ret, bool setRead = false);
	/// <summary>
	/// 获取即时消息(根据聊天对象ID、页码条件获取某天的即时消息)
	/// **调用该函数后需要自行调用MESSAGE_MANAGER::RealseMessages(messages)方法释放获取到的消息
	/// <summary>
	/// <param name="targetUID">聊天对象ID</param>
	/// <param name="page">页码</param>
	/// <param name="count">每页消息条数</param>
	/// <param name="ret">即时消息</param>
	/// <param name="date">日期</param>
	/// <param name="isUnread">是否未读</param>
	/// <param name="isAsc">是否正序</param>
	/// <param name="setRead">是否需要设置已读</param>
	void GetMessages(CString targetUID, int page, int count, list<CInstantMessage*>& ret, CString date = _T(""), bool isUnread = false, bool isAsc = true, bool setRead = false);
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
	void GetMessagesEx(CString targetUID, int first, int count, list<CInstantMessage*>& ret, CString date = _T(""), bool isUnread = false, bool isAsc = true, bool setRead = false);
	/// <summary>
	/// 获取即时消息(根据页码条件获取某天的即时消息)
	/// **调用该函数后需要自行调用MESSAGE_MANAGER::RealseMessages(messages)方法释放获取到的消息
	/// <summary>
	/// <param name="date">日期</param>
	/// <param name="page">页码</param>
	/// <param name="count">每页消息条数</param>
	/// <param name="ret">即时消息</param>
	/// <param name="setRead">是否需要设置已读</param>
	void GetMessagesD(CString date, int page, int count, list<CInstantMessage*>& ret, bool setRead = false);
	/// <summary>
	/// 获取即时消息(根据页码条件获取包含某个关键字的即时消息)
	/// **调用该函数后需要自行调用MESSAGE_MANAGER::RealseMessages(messages)方法释放获取到的消息
	/// <summary>
	/// <param name="key">消息内容关键字</param>
	/// <param name="page">页码</param>
	/// <param name="count">每页消息条数</param>
	/// <param name="ret">即时消息</param>
	/// <param name="targetUID">聊天对象ID</param>
	/// <param name="setRead">是否需要设置已读</param>
	void GetMessagesK(CString key, int page, int count, list<CInstantMessage*>& ret, CString targetUID = _T(""), bool setRead = false);

	//消息数据库复制和升级
	void CopyAndUpdateMessage(CMessageManager *pMessageMan);

	//获取指定行数指定数量的消息
	void GetMessages(int offset, int limit, vector<IMBasicData*> &vecIMData);

	//获取消息对应的Sql语句
	CString GetMessageSql(IMBasicData *pIMData = NULL);
public:
	//////////////////////////////////////////////////////////////////////////////////////////////
    //
	//  获取消息数量接口
	//
	//////////////////////////////////////////////////////////////////////////////////////////////
	/// <summary>
	/// 获取即时消息数量
	/// <summary>
	/// <param name="onlyUnread">是否只获取未读的</param>
	int GetMessageCount(bool onlyUnread = false);
	/// <summary>
	/// 获取即时消息数量(根据聊天对象ID获取某天的即时消息数量)
	/// <summary>
	/// <param name="targetUID">聊天对象ID</param>
	/// <param name="date">日期</param>
	/// <param name="onlyUnread">是否只获取未读的</param>
	int GetMessageCount(CString targetUID, CString date = _T(""), bool onlyUnread = false);
	/// <summary>
	/// 获取即时消息数量(获取某天的即时消息数量)
	/// <summary>
	/// <param name="date">日期</param>
	/// <param name="onlyUnread">是否只获取未读的</param>
	int GetMessageCountD(CString date, bool onlyUnread = false);
	/// <summary>
	/// 获取即时消息数量(获取某天包含某个关键字的即时消息)
	/// <summary>
	/// <param name="key">内容关键字</param>
	/// <param name="targetUID">聊天对象ID</param>
	/// <param name="onlyUnread">是否只获取未读的</param>
	int GetMessageCountK(CString key, CString targetUID = _T(""), bool onlyUnread = false);
public:
	//////////////////////////////////////////////////////////////////////////////////////////////
    //
	//  修改消息内容接口
	//
	//////////////////////////////////////////////////////////////////////////////////////////////
	/// <summary>
	/// 设置消息已读(根据聊天对象ID设置消息已读)
	/// <summary>
	/// <param name="targetUID">聊天对象ID</param>
	void SetMessageReadByTargetID(CString targetUID);
	void SetMessageReadByTargetID(CString targetUID,char* tss);
	//通过targetID设置target_description的值为2 说明该消息已经从消息列表删除
	void SetTargetDescriptionDeleteByTargetID(CString targetUID);
	//通过targetID设置target_description的值为1 说明与该联系人的对话已经被设置为置顶
	void SetTargetDescriptionTopByTargetID(CString targetUID);
	/// <summary>
	/// 设置消息已读(根据消息数据库ID设置消息已读)
	/// <summary>
	/// <param name="dbMID">消息的数据库ID</param>
	void SetMessageReadByMessageID(CString dbMID);
	/// <summary>
	/// 设置消息扩展内容(根据消息数据库ID修改消息扩展内容)
	/// <summary>
	/// <param name="dbMID">消息的数据库ID</param>
	/// <param name="messageExten">消息扩展内容</param>
	void SetMessageExtenByMessageID(CString dbMID, CString messageExten);
	/// <summary>
	/// 设置消息是否可用
	/// <summary>
	/// <param name="dbMID">消息的数据库ID</param>
	/// <param name="isActive">是否激活</param>
	void SetMessageActive(CString dbMID, bool isActive);	
	/// <summary>
	/// 设置子消息参数
	/// <summary>
	/// <param name="dbMID">消息的数据库ID</param>
	/// <param name="subMessageID">子消息消息ID</param>
	/// <param name="type">子消息类型</param>
	/// <param name="smUpdate">子消息需要更新的参数</param>
	void SetSubMsgParam(CString dbMID, CString subMessageID, SubMessageType type, SubMsgUpdate smUpdate);

	/// <summary>
	/// 设置消息tsc
	/// <summary>
	/// <param name="targetUID">聊天对象ID</param>
	void CMessageManager::SetMessageTscByMID(CString mid, __int64 tsc);
public:
	/// <summary>
	/// 获取即时消息
	/// <summary>
	/// <param name="ret">即时消息</param>
	/// <param name="page">页码</param>
	/// <param name="count">每页消息条数</param>
	/// <param name="conditions">获取条件</param>
	/// <param name="isAsc">是否正序</param>
	/// <param name="setRead">是否需要设置已读</param>
	void GetMessagesC(list<CInstantMessage*>& ret, int page, int count, CString conditions = _T(""), bool isAsc = true, bool setRead = false);

	/// <summary>
	/// 获取即时消息
	/// <summary>
	/// <param name="ret">即时消息</param>
	/// <param name="first">首条消息编号</param>
	/// <param name="count">每页消息条数</param>
	/// <param name="conditions">获取条件</param>
	/// <param name="isAsc">是否正序</param>
	/// <param name="setRead">是否需要设置已读</param>
	void GetMessagesCEx(list<CInstantMessage*>& ret, int first, int count, CString conditions = _T(""), bool isAsc = true, bool setRead = false);

	/// <summary>
	/// 获取即时消息数量
	/// <summary>
	/// <param name="conditions">获取条件</param>
	/// <param name="onlyUnread">是否只获取未读的</param>
	int GetMessageCountC(CString conditions = _T(""), bool onlyUnread = false);
private:
	/// <summary>
	/// 设置消息已读
	/// 子查询字段是id
	/// <summary>
	/// <param name="condition">设置条件</param>
	void SetMessageRead(CString condition);
	/// <summary>
	/// 设置所有消息已读
	/// <summary>
	void SetAllMessageRead();
	/// <summary>
	/// 删除转义字符
	/// <summary>
	/// <param name="text">文本(包含表情转义字符)</param>
	CString RemoveFace(CString text);

public:
	/// <summary>
	/// 即时消息更新的回调函数
	/// <summary>
	static pfunIMUpdateCallBack m_pfunIMUpdateCallBack;

	/// <summary>
	/// 获取消息提醒类型
	/// <summary>
	int GetMessageTipType(time_t nMsgTime, bool isSync = false, bool isRoaming = false);

public:
	/// <summary>
	/// 获取tscStart 和tscEnd时间段内全部的tsc
	/// <summary>
	int GetTscTimes(CString targetUID, __int64 tscStart, __int64 tscEnd, vector<__int64> &times);
	/// <summary>
	/// 获取最大ID
	/// <summary>
	int GetRecvMaxTimePro(char* targetUID,CString& outMaxProTime);
	/// <summary>
	/// 获取被回复的消息内容
	/// <summary>
	int GetIdByTsc(__int64 tsc,CString &dbID);
	/// <summary>
	/// 消息队列是否为空
	/// <summary>
	bool QueueIMDataIsEmpty();
};

/// <summary>
/// 释放即时消息
/// <summary>
/// <param name="messages">即时消息列表</param>
void RealseMessages(list<CInstantMessage*> &messages);
/// <summary>
/// 释放最近联系人
/// <summary>
/// <param name="contacts">最近联系人列表</param>
void RealseContacts(list<CRecentContact*> &contacts);

}
/////////////////////////////////////////////////////////////////////////////////////////////////////