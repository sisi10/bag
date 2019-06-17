#ifndef  ACCOUNT_MAN_H_
#define  ACCOUNT_MAN_H_
#include "ictsdktypes.h"
#include "generallist.h"
#include "CoreInterface/UA/ConfigDataManage.h"//add by zhangpeng
#include "../CoreInterface/MessageManage/InstantMessage.h"
#include "UA\UserAgentCore.h"
#include <queue>
using namespace std;
typedef enum{
	OKDT_PC = 0,
	OKDT_IPTEL = 1
}  OneKeyDialType;


typedef enum{
	WST_UpdatePwd = 1,
}  WSReqType;
//
typedef pWSCallback  pLoginCallback;

typedef struct
{
	//唯一编号
	char8 uid[255];
	//账号, 通常是手机号
	char8 acc[32];
	//密码, 不大于16字节
	char8 pwd[32];
	//域名
	char8 realm[255];
	//保存账号
	BOOL saveAccInfo;
	//自动登录选项
	BOOL autologin;
	//自动运行
	BOOL autoRun;
   //其他选项根据需要添加
} ICTACCINFO, *PICTACCINFO;

typedef enum{
	INVALID_DATA = 0, //无效的输入数据
	LOGIN_BEGIN, //登录开始	
	LOGIN_NETWORK_AUTH_SUCC, //网络认证成功
	LOGIN_NETWORK_AUTH_FAIL, //网络认证失败
	LOGIN_OFFLINE_AUTH_SUCC, //离线认证成功
	LOGIN_OFFLINE_AUTH_FAIL, //离线认证失败	
	LOGIN_NETWORK_DISCONNECT, //网络未连接	
	LOGIN_UPDATE_USERSELF_UI, //更新用户自己的UI（姓名、签名、头像）
	LOGIN_UPDATE_ENTPB_UI, //更新企业通讯录的界面
	LOGIN_UPDATE_PERSONALPB_UI, //更新个人通讯录的界面
	LOGIN_UPDATE_GROUP_UI, //更新群组的界面
	LOGIN_UPDATE_REMSG_UI, //更新最近消息的界面
	LOGIN_UPDATE_CAREC_UI, //更新通话记录的界面
	LOGIN_UPDATE_LAPP_UI, //更新轻应用的界面
	LOGIN_SWITCH_MAIN_UI, //切换到主界面
	LOGIN_OFFLINE_TIPS, //离线登录提示	
	LOGIN_UNREAD_MSG_P2P_TIPS, //一对一的未读消息的提醒
	LOGIN_UNREAD_MSG_GROUP_TIPS, //群组的未读消息的提醒
	LOGIN_UPGRADE_TIPS,//升级提醒（只返回升级参数结构体PUPGRADE_UI_DATA，有UI决定是否升级）
}LOGIN_STATE_CODE;

//登录类型
typedef enum _login_type
{
	INVALID_TYPE = -1, //无效登录类型
	OFFLINE_LOGIN = 0, //离线登录
	NETWORK_LOGIN = 1, //联网登录

}LOGINTYPE;

//五种类型的登录状态
#define LOGIN_STA_LOADED_LOCAL_DATA 0
#define LOGIN_STA_LOADED_SERVER_DATA 1
#define LOGIN_STA_UPDATE_UI 2
#define LOGIN_STA_OFFLINE 3
#define LOGIN_STA_NETWORK 4

//数据加载状态
#define LOADED_DATA_ENTERPRISE_PHONEBOOK   0x01
#define LOADED_DATA_PERSONAL_PHONEBOOK   0x02
#define LOADED_DATA_GROUPS   0x04
#define LOADED_DATA_PUB_ACCOUNT   0x08
#define LOADED_DATA_RECENT_CONTACT   0x10
#define LOADED_DATA_CALL_RECORD   0x20
#define LOADED_DATA_CUR_LOGIN_USER   0x40

//界面更新状态
#define UPDATE_UI_ENTERPRISE_PHONEBOOK   0x01
#define UPDATE_UI_PERSONAL_PHONEBOOK   0x02
#define UPDATE_UI_GROUPS   0x04
#define UPDATE_UI_PUB_ACCOUNT   0x08
#define UPDATE_UI_RECENT_CONTACT   0x10
#define UPDATE_UI_CALL_RECORD   0x20
#define UPDATE_UI_CUR_LOGIN_USER   0x40

//服务器连接状态
#define LOGIN_SUCCESS 0x01
#define MQTT_CONNECT_SUCCESS 0x02
#define SIP_CONNECT_SUCCESS 0x04

//登录状态结构
typedef struct _login_state 
{
public:		
	LOGINTYPE enLoginType; //登录类型
	long lOfflineLoginSta; //离线登录状态，第一位代表登录成功与否，0：登录失败，1：登录成功，其它位保留
	long lNetworkLoginSta; //在线登录状态，第一位代表登录成功与否，0：登录失败，1：登录成功，第二位代表MQTT登录状态，第三位代表SIP登录状态，其它位保留	
	long lLoadDataStaFromLocal; //本地数据的加载状态，前七位依次代表企业通讯录、个人通讯录、群组、公共账号、最近联系人、通话记录、当前登录用户数据
	long lLoadDataStaFromServer; //服务器数据的加载状态，前四位依次代表企业通讯录、个人通讯录、群组、公共账号
	long lUIUpdateSta; //界面更新状态，前六位依次代表企业通讯录、个人通讯录、群组、公共账号、最近联系人、通话记录	
public:	
	_login_state()
	{
		InitState();
	}

	void InitState()
	{		
		enLoginType = OFFLINE_LOGIN;
		lOfflineLoginSta = 0;
		lNetworkLoginSta = 0;
		lLoadDataStaFromLocal = 0;
		lLoadDataStaFromServer = 0;
		lUIUpdateSta = 0;		
	}

}LOGIN_STATE, *PLOGIN_STATE;

//当前登录用户数据
typedef struct _cur_user_ui_data
{
	char8 uid[256]; //用户id
	char8 name[256]; //姓名
	char8 sig[256]; //签名	

}CUR_USER_UI_DATA, *PCUR_USER_UI_DATA;

//最近联系人UI数据，主要进行填充树视图
typedef struct _recent_contact_ui_data
{
	char8 uid[256]; //用户唯一UId
	char8 sipUri[256]; //用户sip账号
	int32 chatType; //聊天类型
	char8 name[256]; //聊天对象的名字
	//char8 headPath[256]; //聊天对象的头像路径
	int32 status; //聊天对象的状态
	char8 msgContent[1024*2]; //消息内容
	int64 msgTime; //消息时间
	int32  unreadNum; //未读消息数量
} RECENT_CONTACT_UI_DATA, *PRECENT_CONTACT_UI_DATA;

//呼叫记录UI数据，主要进行填充树视图
typedef struct _call_record_ui_data
{
	char8 uid[256]; //用户唯一UId
	char8 sipUri[256]; //用户sip账号
	int32 callType; //通话类型
	char8 name[256]; //聊天对象的名字
	char8 headPath[256]; //聊天对象的头像路径
	int32 status; //聊天对象的状态	
	char8 numCallback[256]; //回呼号码
	char8 displayNum[256]; //显示号码
	char8 callTime[256]; //呼叫时间

} CALL_RECORD_UI_DATA, *PCALL_RECORD_UI_DATA;

//轻应用数据结构
typedef struct _lapp_data
{
	char8 uid[256]; //用户id
	char8 name[256]; //姓名
	char8 desc[256]; //描述
	int32 show_way; //展现方式
}LAPP_UI_DATA, *PLAPP_UI_DATA;

//升级结构体
typedef struct _upgrade_ui_data
{
	char8 updateLink[256]; //升级地址
	BOOL  bCompeletedUpdate; //增量升级或全量升级 TRUE:全量 FALSE：增量
	char8 version[32]; //升级版本	
	char8 forceVersion[32]; //强制升级版本	
}UPGRADE_UI_DATA, *PUPGRADE_UI_DATA;
// interface of account info

//add by zhangpeng on 20160530 begin
//确定唯一用户的结构
typedef struct _puid
{
	CString strParentId;
	CString strUid;
	bool bUpdatePhoto;
	bool operator < (const _puid &m) const
	{
		return strParentId + strUid > m.strParentId + m.strUid;
	}

} PUID, *PPUID;

typedef struct _update_phbk_ui
{
	CString strParentId;
	CString strUid;
	int nMethod;

}UPDATE_PHBK_UI, *PUPDATE_PHBK_UI;

//更新节点结构
typedef struct _update_node
{
	int nType; //节点类型，0：部门，1：用户
	DWORD dwLastUpdateTime; //上次更新时间
	CString strVersion; //当前本地的版本号

}UPDATE_NODE, *PUPDATE_NODE;

//用户节点更新结构
typedef struct _puid_update
{
	PUID puid;
	UPDATE_NODE un;

}PUID_UPDATE, *PPUID_UPDATE;

//登录数据结构
typedef struct _login_data
{
public:
	bool bLogin[3]; //登录时机控制
	bool bTokenExpire; //登录是否过期 //没用到，仅保留
	bool bEntPBVerChange; //企业通讯录的版本号是否发生变化
	bool bPersonalPBVerChange; //个人通讯录的版本号是否发生变化
	bool bUpdateLinkChange; //更新地址是否发生变化
	volatile bool bNeedUpdate; //是否需要更新的标记

	bool bCheckedIP;
	CCriticalSection  csIPCheckcs;

	volatile __time64_t tokenExpireTime; // token过期时间

	void* hLoadLAPPThread;  //轻应用获取线程
	void* hLoadPBThread; //通讯录（企业和个人）数据加载线程
	void* hLoadGroupThread; //群组数据加载线程
	void* hLoadRecentContactThread; //最近联系人数据加载线程
	void* hLoadCallRecordThread; //通话记录数据加载线程
	//20170603 gavin add
	void* hLoadPersonalPBThread; //个人通讯录数据加载线程
	//20170603 gavin end
	void* hUpdateCheckThread; //升级检测线程
	void* hGetUserInfoThread; //获取用户信息线程

	CCriticalSection csLoginState; //登录状态临界区
	LOGIN_STATE loginState; //登录状态

	PHONEBOOKITEM pbItemSelf; //当前登录用户的通讯录数据

	PGList listCallRecord; //通话记录数据

	//最新版本号结构
	LAST_DATA_VER stLDV;

	map<CString, UPDATE_NODE> mapUid2UpdateNode1; //结构1用于组节点的更新
	map<PUID, UPDATE_NODE> mapUid2UpdateNode2; //结构2用于用户节点的更新
	queue<PUID_UPDATE> queUid2UpdateNode3; //结构3用于用户更加关系的节点更新
	queue<PUID_UPDATE> queUid2UpdateNode3s; //结构3用于用户更加关系的节点更新
	queue<PUID> queUid2UpdateNode4; //结构4用于信息不完整的用户的更新
	//20170612 gavin add
	queue<PUID> queUid2UpdateNode5; //结构5用于信息不完整的个人通讯录的用户数据的更新
	//20170612 gavin end
	queue<UPDATE_PHBK_UI> queUpdatePhBkUI; //通讯录界面更新结构
	CCriticalSection  csUN1;
	CCriticalSection  csUN2;
	CCriticalSection  csUN3;
	CCriticalSection  csUN4;
	//20170612 gavin add
	CCriticalSection  csUN5;
	//20170612 gavin end
	CCriticalSection  csUPUI;

	CCriticalSection csEGI;
	vector<CString> vecExpandGroupIds;

	void* hUpdateThread1; //执行组节点的更新
	void* hUpdateThread2; //执行用户节点的更新
	void* hUpdateThread3; //对当前用户更加关心的节点执行更新
	void* hUpdateThread4; //对信息不完整的用户进行后台完善
	void* hUpdateThread5; //对信息不完整的个人通信录用户进行后台完善
	void* hUpdateThread3s; //对当前用户更加关心的节点执行更新

	UI_PARA2 uiPara[2]; //界面参数结构, 0：登录，1：登出

	CUserAgentCore *pUserAgentCore; //ua对象

public:
	_login_data()
	{
		pUserAgentCore = NULL;

		InitData();
	}

	void InitData()
	{
		bLogin[0] = bLogin[1] = bLogin[2] = false;
		bTokenExpire = bEntPBVerChange = bUpdateLinkChange = bPersonalPBVerChange = bCheckedIP =false;

		bPersonalPBVerChange = true;

		hLoadLAPPThread = hLoadPBThread = hLoadGroupThread = hLoadRecentContactThread = hLoadCallRecordThread = hGetUserInfoThread = NULL;
		hUpdateThread1 = hUpdateThread2 = hUpdateThread3 = hUpdateThread4 = hUpdateThread5 = NULL;
		tokenExpireTime = 0;

		memset(&pbItemSelf, 0, sizeof(PHONEBOOKITEM));
		listCallRecord = NULL;
		memset(&uiPara[0], 0, sizeof(UI_PARA2));
		memset(&uiPara[1], 0, sizeof(UI_PARA2));
	}

}LOGIN_DATA, *PLOGIN_DATA;

//登录数据
extern LOGIN_DATA g_loginData;
//登录数据
extern APP_CONFIG Man_appConfig;
void getconfig(APP_CONFIG& config);
extern BOOL boIsAutoLogin;//是否elgg、mqtt、sip一起登陆/TRUE =  直接登录/FALSE = 登录elgg、mqtt、qsip单步登录 for sdk
//add by zhangpeng on 20160530 end

//升级检测线程启动
void StartUpdateCheckThread(pWSCallback cb);
void StartUpdateCheckThreadManually(pWSCallback cb);

/*******************************************************************************
*  @fun         AM_UpdateLoginState
*  @brief       update login state
*  @param[in]   type: main type
*  @param[in]   subType: sub type
*  @param[in]   bSet: set or unset
*  @return      
*  @pre          
*  @remarks     更新登陆状态（界面开发者只需要更新界面状态）
usage example：
				AM_UpdateLoginState(LOGIN_STA_UPDATE_UI, UPDATE_UI_ENTERPRISE_PHONEBOOK, true);

*  @see          
*******************************************************************************/
void AM_UpdateLoginState(int type, long subType, bool bSet);

/*******************************************************************************
*  @fun         GetRecentAccs
*  @brief       get recent accinfo list 
*  @param[in/out]   pacclist: to save accinfos 
*  @return       suc return zero, otherwise none zero  
*  @pre          
*  @remarks     调用者负责释放这个list
usage example：
               PGList pacclist = NULL;
				GetRecentAccs(&pacclist);
				//do something
				ReleaseGList(pacclist);
				pacclist = NULL;
*  @see          
*******************************************************************************/
int32 GetRecentAccs(PGList*  pacclist);

/*******************************************************************************
*  @fun         GetCurAccount
*  @brief       get cur account 
*  @param[in/out]  accinfo:save cur accinfo 
*  @return      suc return zero, otherwise none zero  
*  @pre          
*  @remarks      

*  @see          
*******************************************************************************/
int32 AM_GetCurAccount(PICTACCINFO accountInfo);


/*******************************************************************************
*  @fun         ConfigCurAccount
*  @brief       config cur account 
*  @param[in]  accinfo: cur account info   
*  @param[out]   
*  @return     suc return zero, otherwise none zero  
*  @pre          
*  @remarks     只有在未登录的情况下该接口才会正确返回
*  @see          
*******************************************************************************/
int32 AM_SetCurAccount2UA(PICTACCINFO accountInfo);


/*******************************************************************************
*  @fun         AM_SearchAccounts
*  @brief       search account info by fileter  
*  @param[in/out]   pacclist: save result
*  @param[in]          filter: var to compare
*  @return       suc return zero, otherwise none zero  
*  @pre           
*  @remarks      
					usage example：
					PGList pacclist = NULL;
					AM_SearchAccounts(&pacclist,filter);
					//do something
					ReleaseGList(pacclist);
					pacclist = NULL;
*  @see          
*******************************************************************************/
int32 AM_SearchAccounts(PICTACCINFO accinfo,char8* filter);



/*******************************************************************************
*  @fun         GetLoginState
*  @brief       get login state any time
*  @param[in/out]    pLoginState 
*  @return      suc return zero, otherwise none zero  
*  @pre          
*  @remarks      
*  @see          
*******************************************************************************/
int32 GetLoginState(PLOGIN_STATE pLoginState);


/*******************************************************************************
*  @fun         UpdatePassword
*  @brief       update password 
*  @param[in]    newpwd: new password
*  @param[in]    userdata: user data
*  @param[in]    cb: callback function,
*  @return       suc return zero, otherwise none zero  
*  @pre          
*  @remarks     只能对已经登录的账号进行密码修改，如果不关注状态返回，cb和userdata可以为空
*  @see          
*******************************************************************************/
int32 UpdatePassword(char8* newpwd,void* userdata = NULL,pWSCallback cb = NULL);

//20170617 gavin 
//采用上次登录用户配置用户账号信息
int32 AM_ConfigUser();
int32 AM_ConfigUser(PGList* pacclist);
//配置用户账号信息
int32 AM_ConfigUser(CString name, CString pwd, CString realm, bool savePwd, bool autoLogin, bool autoRun, CTime time);


/*******************************************************************************
*  @fun         Login
*  @brief       login  
*  @param[in]    bOnline:on line or offline
*  @param[in]    userdata: user data
*  @param[in]    cb: callback function,
*  @return       suc return zero, otherwise none zero  
*  @pre          
*  @remarks      
*  @see          
*******************************************************************************/
int32 AM_Login(int32 bOnline, void* userdata = NULL, pLoginCallback cb = NULL);


/*******************************************************************************
*  @fun         Logoff
*  @brief       Logoff  
*  @param[in]    userdata: user data
*  @param[in]    cb: callback function,
*  @return       suc return zero, otherwise none zero  
*  @pre          
*  @remarks      
*  @see          
*******************************************************************************/
int32 Logoff(void* userdata = NULL, pLoginCallback cb = NULL);

/*******************************************************************************
*  @fun         		GetLAPPList
*  @brief       		获取公告地址
*  @param[in/out]		clist: 轻应用列表 PGList的data结构体为 PLAPP_UI_DATA
*  @return       		suc return zero, otherwise none zero  	
*  @remarks			    在更多里列表显示，头像在LAPP目录下找uid，使用后注意释放
*******************************************************************************/
int32 GetLAPPList(PGList* clist);

/*******************************************************************************
*  @fun         		GetLAPPUrl
*  @brief       		获取公告地址
*  @param[in]		    uid: 轻应用id 来自GetLAPPList
*  @param[in/out]		url: 公告url //缓冲区大小至少8192
*  @return       		suc return zero, otherwise none zero  	
*  @remarks			    url直接在WEB控件中展示
		PGList clist = NULL;
		GetLAPPList(&clist);
		PGList pNode = clist;
		while(pNode)
		{
			char8* uid = (char8*)pNode->data;
			char8 url[1024];
			GetLAPPUrl(uid,url);
			pNode = pNode->next;
		}
		ReleaseGList(clist);
		clist = NULL;
*******************************************************************************/
int32 GetLAPPUrl(char8* uid,char8* url,BOOL bRecentContact = 0);

/*******************************************************************************
*  @fun         		DownloadLAPP
*  @brief       		下载轻应用地址和图片
*  @param[in/out]		cb: 回调 真正数据通过GetLAPPUrl获取
*  @return       		suc return zero, otherwise none zero  	
*  @remarks			    url直接在WEB控件中展示
*******************************************************************************/
int32 DownloadLAPP(pWSCallback cb);

//************************************
// Method:    GetPrefix
// FullName:  获取出局码
// Access:    public 
// Returns:   int32 suc return zero, otherwise none zero  	
// Qualifier: 
// Parameter[in/out]: char8 * prefix char prex[256]
//************************************
int32 GetPrefix(char8* prefix);

//************************************
// Method:    GetCurPsersonalDir
// FullName:  获取当前用户目录
// Access:    public 
// Returns:   int32 suc return zero, otherwise none zero  	
// Qualifier: 
// Parameter[in/out]: char8 * dir 目录路径
//************************************
int32 GetCurPsersonalDir(char8* dir);

/*******************************************************************************
*  @fun         		UpdateVisibleUserInfo
*  @brief       		更新可见联系人信息
*  @param[in]			groupIdLists: 所有展开的部门的ID列表，由被调用者负责释放，GList中的data字段类型为char*
*  @return       		suc return zero, otherwise none zero  	
*  @remarks			    当需要更新可见联系人信息时，调用此接口，传递已展开的部门的ID列表即可
*******************************************************************************/
int32 UpdateVisibleUserInfo(PGList groupIdLists);
/*******************************************************************************
*  @fun         		FastUpdateUserInfo
*  @brief       		立即更新某个人的信息
*  @param[in]			uid: 用户uid
*  @return       		suc return zero, otherwise none zero  	
*  @remarks			    当需要立即更新某个人的信息时，可以调用此接口。例如打开聊天窗口，可以调用此接口，传递用户的uid即可
*******************************************************************************/
int32 FastUpdateUserInfo(char8* uid);

/*******************************************************************************
*  @fun         		DoFullPhBkUpdate
*  @brief       		通讯录全量更新方法
*  @remarks			    当需要手动进行通讯录的全量更新时，调用此接口即可
*******************************************************************************/
void DoFullPhBkUpdate();

//更新Token
void UpdateAuthToken(BOOL bForceUpdate = FALSE);
//获取最近消息  chattype = CT_Group时只取群组消息
void GetLastIM(ChatType chattype, PGList* plst);


void SetGroupHeadPortraitLoadingCallback(pGroupHeadPortraitLoading  cb);//外部调用
#endif

//删除消息列表的某条记录
int32 DelRecord(char8* uid);
int32 SetTargetDeccriptionDeleteFlag(char8* uid);//设置从列表删除的对话target_description标志为1
//设置target_id的值为2说明该消息已经在消息列表中删除
int32 SetTargetDescriptionDelete(char8* uid);
//根据target_id设置target_description的值  1：置顶 0:普通
int32 SetTargetDescriptionTop(char8* uid);