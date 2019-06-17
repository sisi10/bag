#include "stdafx.h"
#include "AccountMan.h"
#include "server.h"
#include "contacts.h"
#include "string.h"
#include "stdlib.h"
//#include "UA\UserAgentCore.h"
#include  "Common\UALicence.h"
#include "Common\Tracer.h"
#include "Common\vmd5.h"
#include "ELGGWebService.h"
#include "pgm\XCAPInternetSession.h"
#include "CoreInterface/threads/WSThread.h"
#include <atlutil.h>  //SafeStringCopy
#include <sstream>
#include <xstring>//add by zhangpeng
using namespace std;
extern void PhBkItem2ContactItem(CPhBkItem* pbitem, CONTACTITEM* pContact);
using namespace UA_UTIL;

//登录数据
LOGIN_DATA g_loginData;
//add by zhangpeng on 20160530 begin
APP_CONFIG Man_appConfig;
BOOL boIsAutoLogin = TRUE;
//add by zhangpeng on 20160530 end
extern void  InitUA();//来自server
extern void StopAllServerThread();//来自server
extern void SetMQTTStop(bool bStop);//来自server
extern int32 StartWSWorkerThread(void* pOwner, LPARAM lparam,bool bSharedParam = false);

//创建用户账号目录
void AM_CreateUserPaths(CString name, CString realm);

//获取用户头像文件路径
CString GetUserPortraitPath(CString strUid);

CString GetConfigUserPortraitPath(CString strName, CString strRealm);

//加载ICT数据
void LoadICTData();

//升级检测线程处理
DWORD WINAPI UpdateCheckThread(LPVOID lpParameter);

//升级检测INI文件字符流中获取相应字段
CString GetIniDate(CString strContent, const CString& strDesLine);

//轻应用数据加载线程
DWORD WINAPI LoadLAPPThread(LPVOID lpParameter);

//通讯录（企业和个人）数据加载线程
DWORD WINAPI LoadPBThread(LPVOID lpParameter);

//个人通讯录数据加载线程
DWORD WINAPI LoadPersonalPBThread(LPVOID lpParameter);

//获取最近消息  chattype = CT_Group时只取群组消息
void GetLastIM(ChatType chattype, PGList* plst);

//删除消息列表的某条记录
int32 DelRecord(char8* uid);

void ValidateUrl(CString& strUrl, bool bLocal = false);

//群组数据加载线程
DWORD WINAPI LoadGroupThread(LPVOID lpParameter);

//最近消息数据加载线程
DWORD WINAPI LoadRecentContactThread(LPVOID lpParameter);

//呼叫记录数据加载线程
DWORD WINAPI LoadCallRecordThread(LPVOID lpParameter);

//获取当前登录用户的通讯录信息线程
DWORD WINAPI GetCurUserPBInfoThread(LPVOID lpParameter);

//加载剩余本地数据
void LoadRestLocalData();

//登录结果线程处理
DWORD WINAPI ProcessLoginResThread(LPVOID lpParameter);

//获取部门的nVersion
CString GetGroupVersion(vector<CString> &vecUids, vector<CString> &vecVers);

//处理部门的更新
void UpdateNodeForGroup();

//处理用户的更新
void UpdateNodeForUser();

int32 FastUpdateUserInfo(CString strUserId);

//通讯录节点更新线程1
DWORD WINAPI UpdateNodeThread1(LPVOID lpParameter);

//通讯录节点更新线程2
DWORD WINAPI UpdateNodeThread2(LPVOID lpParameter);

//通讯录节点更新线程3
DWORD WINAPI UpdateNodeThread3(LPVOID lpParameter);

//通讯录节点更新线程4
DWORD WINAPI UpdateNodeThread4(LPVOID lpParameter);

//个人通讯录节点更新线程5
DWORD WINAPI UpdateNodeThread5(LPVOID lpParameter);



//在线登录入口, 网页服务接口认证
void AM_WSAuth(ACCINFO *accountInfo);

//在线登录回调
int LoginWSAuthProc( int nStatus, char* szResonse, int dwSize, char* contenttype, char* szEtag, void* pActData );

//离线登录
void AM_OfflineLogin(ACCINFO *accountInfo);

//正常联网登录
void AM_ProcessNormalLogin(ACCINFO *pAccInfo);

//Token是否过期
BOOL AM_IsAuthTokenExpired();

//更新Token回调
int UpdateAuthTokenProc( int nStatus, char* szResonse, int dwSize, char* contenttype, char* szEtag, void* pActData );

//格式化最近消息时间

int64 DateTimeToUnixepoch(CString strTime);

//加载未读消息
void LoadUnreadMessages();

//终止所有线程
void TerminateAllThread();

//丰富联系人结构
void PerfectPhbkItem(CPhBkGroupManager *pPhbkGroupManager, MAP_UID2PHBKITEM &mapUid2PhBkItems);

// 更新当前登录用户的数据版本号至本地配置文件
void UpdateDataVerToPersonalCfg(DATA_VER_TYPE dvt);

//企业通讯录更新界面回调函数
int __stdcall UpdatePhBkCallBack(CString strParentId, CString strUserId, int nMethod);

//批量更新用户签名
void UpdateMultiUserSig(const map<CString, CString> &mapUId2Sig);

//升级消息数据库
void DoUpdateMsgDatabase(const char *pAcc, const char *pServ);

int32 InitAccountMan()
{
	if(!g_loginData.pUserAgentCore)
		g_loginData.pUserAgentCore = new CUserAgentCore;

	//初始化应用配置数据库路径 - 在主窗口建立的时候已经设置完毕
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	ua->SetAppDBPath();
	//初始化应用配置数据库
	CConfigDBManager* config = ua->GetConfigManager();
	config->InitConfig();

	//初始化UA
	InitUA();

	return 0;
}

bool IsLoginTimeReached(uint8 nIndex)
{
	if (nIndex > 2)
	{
		ASSERT(false);
		return false;
	}
	return g_loginData.bLogin[nIndex];
}

int32 GetRecentAccs(PGList*  pacclist)
{
	if (pacclist == NULL) return S_INVALIDPARAM;
	*pacclist = NULL;

	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	CConfigDBManager* config = ua->GetConfigManager();
	list<CAPPUser> & accountList = config->GetUsers() ;
	PGList pList = NULL;
	for(list<CAPPUser>::iterator iter=accountList.begin(); iter!=accountList.end(); iter++)
	{
		CAccountInfo curAccount(iter->GetName(), iter->GetRealm(), _T(""), false, true);
		curAccount.SetUserID(iter->GetUserID());

		// 这里逻辑上不太对
		/*CAccountDBManager *accountManager = ua->GetAccountDBManager();
		accountManager->GetAccount(curAccount);*/

		//ua->GetAccountById(iter->GetUserID(), curAccount);

		PICTACCINFO pICTInfo = (PICTACCINFO)malloc(sizeof(ICTACCINFO));
		memset(pICTInfo, 0, sizeof(ICTACCINFO));
		strcpy_s(pICTInfo->uid,curAccount.GetAccInfo()->chMqttAccount);
		CString2Char(curAccount.GetName(), pICTInfo->acc);
		CString2Char(curAccount.GetPwd(), pICTInfo->pwd);
		CString2Char(curAccount.GetRealm(), pICTInfo->realm);
		pICTInfo->saveAccInfo = curAccount.GetRemPwd();
		pICTInfo->autologin = curAccount.GetAutoLogin();
		pICTInfo->autoRun = curAccount.GetAutoRun();

		if(!(*pacclist))
		{
			(*pacclist) = pList = (PGList)malloc(sizeof(GList));
			(*pacclist)->data = pICTInfo;
		}
		else
		{
			PGList pTemp = pList;
			pList = (PGList)malloc(sizeof(GList));
			pTemp->next = pList;
			pList->data = pICTInfo;
		}
	}
	if(pList)
		pList->next = NULL;

	return S_SUC;
}

//
int32 AM_GetCurAccount(ICTACCINFO* accountInfo)
{
	if(!accountInfo)
		return S_INVALIDPARAM;

	CUserAgentCore* ua = CUserAgentCore::GetUserAgentCore();
	CConfigDBManager* config = ua->GetConfigManager();
	CAPPUser user;
	bool succ = config->GetRecentUser(user);
	if (!succ)
		return -1;

	CAccountInfo account(user.GetName(), user.GetRealm(), _T(""), false, true);
	account.SetUserID(user.GetUserID());

	//真正的账号的所有数据
	CAccountDBManager *accountManager = ua->GetAccountDBManager();
	accountManager->GetAccount(account);
	//赋值 - Mqtt账号
	strcpy_s(accountInfo->uid, account.GetAccInfo()->chMqttAccount);
	//赋值 - 统一账号
	CString2Char(account.GetName(), accountInfo->acc);
	//赋值 - 统一账号的密码
	CString2Char(account.GetPwd(), accountInfo->pwd);
	//赋值 - 统一账号的域名
	CString2Char(account.GetRealm(), accountInfo->realm);
	accountInfo->saveAccInfo = account.GetRemPwd();
	accountInfo->autologin = account.GetAutoLogin();
	accountInfo->autoRun = account.GetAutoRun();
	return 0;
}

int32 AM_SetCurAccount2UA(PICTACCINFO accountInfo)
{
	if(!accountInfo) 
		return S_INVALIDPARAM;

	CUserAgentCore* ua = CUserAgentCore::GetUserAgentCore();
	CString userName, pwd, realm;
	Char2CString(accountInfo->acc, userName);
	Char2CString(accountInfo->pwd, pwd);
	Char2CString(accountInfo->realm, realm);
	ua->m_curAccount.SetName(userName);
	ua->m_curAccount.SetPwd(pwd);
	ua->m_curAccount.SetRealm(realm);
	ua->m_curAccount.SetRemPwd(accountInfo->saveAccInfo);
	ua->m_curAccount.SetAutoLogin(accountInfo->autologin);
	ua->m_curAccount.SetAutoRun(accountInfo->autoRun);
	return 0;
}

int32 AM_SearchAccounts(PICTACCINFO accinfo,char8* name)
{
	int32 ret = S_FAILED;
	PGList clist = NULL;
	PGList pNode;
	CString strNameFilter,strName;

	//取数据库
	GetRecentAccs(&clist);
	pNode = clist;
	Char2CString(name,strNameFilter);

	//遍历获取搜索数据
	while(pNode)
	{
		PICTACCINFO pAccount = (PICTACCINFO)pNode->data;
		Char2CString(pAccount->acc,strName);
		if(strName == strNameFilter)
		{
			memcpy_s(accinfo,sizeof(ICTACCINFO),pAccount,sizeof(ICTACCINFO));
			ret = S_SUC;
			break;
		}
		pNode = pNode->next;
	}

	ReleaseGList(clist);
	return ret;
}

int32 GetLoginState(PLOGIN_STATE pLoginState)
{
	memcpy(pLoginState, &g_loginData.loginState, sizeof(LOGIN_STATE));
	return 0;
}

int32 UpdatePassword(char8* newpwd,void* userdata,pWSCallback cb)
{
	CString strNewPwd;
	Char2CString(newpwd, strNewPwd);
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	int32 bRet = S_FAILED;

	if (ua && ua->GetPGMManager()->ResetUserPassword(ua->m_curAccount.GetPwd(), strNewPwd))
	{
		CAccountDBManager* config = ua->GetAccountDBManager();
		if(config->UpdatePasswd(strNewPwd))
		{
			ua->m_curAccount.SetPwd(strNewPwd);
			bRet = S_SUC;
		}
	}

	return bRet;
}

void LoadRestLocalData()
{
	//加载最近联系人
	g_loginData.hLoadRecentContactThread = CreateThread(NULL, 0, LoadRecentContactThread, NULL, 0, NULL);

	//加载通话记录
	g_loginData.hLoadCallRecordThread = CreateThread(NULL, 0, LoadCallRecordThread, NULL, 0, NULL);
}

//更新登录状态
void AM_UpdateLoginState(int type, long subType, bool bSet)
{
	CSingleLock lock(&g_loginData.csLoginState);
	lock.Lock();

	if(type == LOGIN_STA_LOADED_LOCAL_DATA)
	{
		//本地数据加载状态
		if(bSet)
			g_loginData.loginState.lLoadDataStaFromLocal |= subType;
		else
			g_loginData.loginState.lLoadDataStaFromLocal &= ~subType;
	}
	else if(type == LOGIN_STA_LOADED_SERVER_DATA)
	{
		//服务器数据加载状态
		if(bSet)
			g_loginData.loginState.lLoadDataStaFromServer |= subType;
		else
			g_loginData.loginState.lLoadDataStaFromServer &= ~subType;
	}
	else if(type == LOGIN_STA_UPDATE_UI)
	{
		//数据对应的界面更新状态
		if(bSet)
			g_loginData.loginState.lUIUpdateSta |= subType;
		else
			g_loginData.loginState.lUIUpdateSta &= ~subType;
	}
	else if(type == LOGIN_STA_OFFLINE)
	{
		//离线登录连接状态
		if(bSet)
			g_loginData.loginState.lOfflineLoginSta |= subType;
		else
			g_loginData.loginState.lOfflineLoginSta &= ~subType;
	}
	else if(type == LOGIN_STA_NETWORK)
	{
		//联网登录连接状态
		if(bSet)
			g_loginData.loginState.lNetworkLoginSta |= subType;
		else
			g_loginData.loginState.lNetworkLoginSta &= ~subType;
	}

	//联网登录在当前登录用户数据获取完就可以切换到主界面
	if(g_loginData.loginState.lLoadDataStaFromServer & LOADED_DATA_CUR_LOGIN_USER && g_loginData.loginState.lUIUpdateSta & UPDATE_UI_CUR_LOGIN_USER)
	{
		if(!g_loginData.bLogin[0])
		{
			//联网登录可以早点切换到主界面
			if(g_loginData.loginState.enLoginType == NETWORK_LOGIN)
			{
				DEBUGCODE(Tracer::Log(_T("[AM_UpdateLoginState] satify condition of switching to main ui"));)

				//获取所有在线状态 reqtype = ICTWSR_GetAllPresence
				//GetPresenceAll(NULL);

				//切换到主界面
				if(g_loginData.uiPara[0].cb)
					g_loginData.uiPara[0].cb(LOGIN_SWITCH_MAIN_UI, NULL, g_loginData.uiPara[0].user_data);

				//modified by zhengpeng for sdk不自动连接sip和mqtt服务器 on20160806 begin
				if (boIsAutoLogin)
				{
					//modified by zhengpeng for sdk不自动连接sip和mqtt服务器 on20160806 end
					//连接服务器（MQTT）
					ConnectServer(SERV_TYPE_MQTT);
					g_loginData.bLogin[0] = true;
				}
			}
		}
	}

	//企业通讯录和群组数据\个人通讯录数据加载完成之后加载剩余本地数据（最近联系人和通话记录...）
	if((g_loginData.loginState.lLoadDataStaFromLocal & LOADED_DATA_ENTERPRISE_PHONEBOOK || g_loginData.loginState.lLoadDataStaFromServer & LOADED_DATA_ENTERPRISE_PHONEBOOK)
		&& (g_loginData.loginState.lLoadDataStaFromLocal & LOADED_DATA_GROUPS || g_loginData.loginState.lLoadDataStaFromServer & LOADED_DATA_GROUPS)
		&& (g_loginData.loginState.lLoadDataStaFromLocal & LOADED_DATA_PERSONAL_PHONEBOOK || g_loginData.loginState.lLoadDataStaFromServer & LOADED_DATA_PERSONAL_PHONEBOOK)
		)
	{
		if(!g_loginData.bLogin[1])
		{
			DEBUGCODE(Tracer::Log(_T("[AM_UpdateLoginState] satify condition of loading rest local data"));)
			//加载剩余的本地数据（最近联系人、通话记录）
			LoadRestLocalData();
			g_loginData.bLogin[1] = true;
		}
	}

	//企业通讯录、群组、个人通讯录、最近联系人、通话记录的数据和界面都完成之后切换到主界面
	if( (g_loginData.loginState.lLoadDataStaFromLocal & LOADED_DATA_ENTERPRISE_PHONEBOOK || g_loginData.loginState.lLoadDataStaFromServer & LOADED_DATA_ENTERPRISE_PHONEBOOK)
		&& (g_loginData.loginState.lLoadDataStaFromLocal & LOADED_DATA_GROUPS || g_loginData.loginState.lLoadDataStaFromServer & LOADED_DATA_GROUPS)
		&& (g_loginData.loginState.lLoadDataStaFromLocal & LOADED_DATA_PERSONAL_PHONEBOOK || g_loginData.loginState.lLoadDataStaFromServer & LOADED_DATA_PERSONAL_PHONEBOOK)
		&& (g_loginData.loginState.lLoadDataStaFromLocal & LOADED_DATA_RECENT_CONTACT || g_loginData.loginState.lLoadDataStaFromServer & LOADED_DATA_RECENT_CONTACT)
		&& (g_loginData.loginState.lLoadDataStaFromLocal & LOADED_DATA_CALL_RECORD || g_loginData.loginState.lLoadDataStaFromServer & LOADED_DATA_CALL_RECORD)
		)
	{
		if(!g_loginData.bLogin[2])
		{
			//离线登录可以晚点切换到主界面
			if(g_loginData.loginState.enLoginType == OFFLINE_LOGIN)
			{
				DEBUGCODE(Tracer::Log(_T("[AM_UpdateLoginState] satify condition of switching to main ui"));)

				//离线登录提示
				if(g_loginData.uiPara[0].cb)
					g_loginData.uiPara[0].cb(LOGIN_OFFLINE_TIPS, NULL, g_loginData.uiPara[0].user_data);

				//切换到主界面
				if(g_loginData.uiPara[0].cb)
					g_loginData.uiPara[0].cb(LOGIN_SWITCH_MAIN_UI, NULL, g_loginData.uiPara[0].user_data);
			}

			if(g_loginData.loginState.enLoginType == NETWORK_LOGIN)
			{
				//加载未读消息
				LoadUnreadMessages();
				//modified by zhengpeng for sdk不自动连接sip和mqtt服务器 on20160806 begin
				if (boIsAutoLogin)
				{
					//modified by zhengpeng for sdk不自动连接sip和mqtt服务器 on20160806 end
					//连接服务器（SIP）
					DEBUGCODE(Tracer::Log(_T("[AM_UpdateLoginState] ConnectServer sip"));)
					ConnectServer(SERV_TYPE_SIP);
				}

				//更新地址发生变化则再次检查更新
				if(g_loginData.bUpdateLinkChange)
					StartUpdateCheckThread(g_loginData.uiPara[0].cb);
			}
			//modified by zhengpeng for sdk不自动连接sip和mqtt服务器 on20160806 begin
			if (boIsAutoLogin)
			{
				//modified by zhengpeng for sdk不自动连接sip和mqtt服务器 on20160806 end			
				g_loginData.bLogin[2] = true;
			}
		}
	}

	DEBUGCODE(Tracer::Log(_T("[AM_UpdateLoginState] [type]:[%d] [subType]:[%d] [bSet]:[%d]"), type, subType, bSet);)
	lock.Unlock();
}

void GetCurAccountContactItem(PCONTACTITEM pItem)
{
	CUserAgentCore* ua = CUserAgentCore::GetUserAgentCore();
	pItem->type = CT_Contacts;
	strcpy_s(pItem->name,g_loginData.pbItemSelf.name);
	strcpy_s(pItem->uid,g_loginData.pbItemSelf.uid);
	strcpy_s(pItem->sipUri,g_loginData.pbItemSelf.sipUri);
	strcpy_s(pItem->cornet,g_loginData.pbItemSelf.cornet);
	strcpy_s(pItem->mobile,g_loginData.pbItemSelf.mobile);
	strcpy_s(pItem->email,g_loginData.pbItemSelf.email);
	strcpy_s(pItem->workDepartment,g_loginData.pbItemSelf.workDepartment);
	strcpy_s(pItem->employeeType,g_loginData.pbItemSelf.employeeType);
	strcpy_s(pItem->sig,g_loginData.pbItemSelf.sig);
	strcpy_s(pItem->sortLevel,g_loginData.pbItemSelf.sortLevel);
	pItem->nBindWeixin  = g_loginData.pbItemSelf.nBindWeixin ;
	pItem->nEntPhBkVer = g_loginData.pbItemSelf.nEntPhBkVer;
}

DWORD WINAPI LoadLAPPThread(LPVOID lpParameter)
{
	DEBUGCODE(Tracer::LogA("[LoadLAPPThread] Running...");)
	UA_UTIL::SetThreadName(-1,"LoadLAPPThread");
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();

	bool bFromLocal = true;

	CLAPPManager* pLAPPMan = ua->GetPGMManager()->GetLAPPManager(ua->GetUserID());
	if(!pLAPPMan)
	{
		DEBUGCODE(Tracer::LogA("[LoadLAPPThread] pLAPPMan is null return");)
		return -1;
	}

	CString strRealm = ua->m_curAccount.GetRealm();
	int nPos = strRealm.ReverseFind(':');
	if (nPos > 0)
	{
		strRealm.GetBuffer()[nPos] = '\0';
		strRealm.ReleaseBuffer();
	}

	pLAPPMan->m_szFileName = ua->GetUserPath(ua->GetUserID()) + ua->m_curAccount.GetName() + _T("@") + strRealm + _T(".lapp.txt");

	CString strEguid;
	Char2CString(ua->m_curAccount.GetAccInfo()->chEguid, strEguid);


	int nLocalVer = ua->m_curAccount.GetAccInfo()->nLAPPVer;
	pLAPPMan->m_bVerChanged = nLocalVer != g_loginData.stLDV.nLAPPVer;
	if(g_loginData.loginState.enLoginType == NETWORK_LOGIN && (pLAPPMan->m_bVerChanged || !UA_UTIL::IsFileExist(pLAPPMan->m_szFileName)))
	{
		if(ua->GetPGMManager()->GetLAPPListFromServer(ua->GetUserID(), strEguid) == 0)
		{
			bFromLocal = false;

//			g_loginData.stLDV.nLAPPVer = pLAPPMan->m_nVersion;
			pLAPPMan->m_nVersion = g_loginData.stLDV.nLAPPVer;

			//下载轻应用ICON
			if(g_loginData.uiPara[0].cb)
			{
				g_loginData.uiPara[0].cb(LOGIN_UPDATE_LAPP_UI, S_SUC, NULL);
			}

			//更新本地轻应用的版本号
			UpdateDataVerToPersonalCfg(DVT_LAPP);

			CSingleLock lock(&pLAPPMan->m_cs);
			lock.Lock();

			pLAPPMan->SaveToFile();

			lock.Unlock();

		}
	}

	if(bFromLocal)
	{
		//从本地加载轻应用数据
		if(pLAPPMan->LoadFromFile())
		{
			if(g_loginData.uiPara[0].cb)
			{
				g_loginData.uiPara[0].cb(LOGIN_UPDATE_LAPP_UI, S_SUC, NULL);
			}
		}
	}
	DEBUGCODE(Tracer::LogA("[LoadLAPPThread] Ending...");)
	return 0;
}

DWORD WINAPI LoadPBThread(LPVOID lpParameter)
{
	DEBUGCODE(Tracer::LogA("[LoadPBThread] running...");)
	bool bLoadFromLocal = false, bLoadFromServer = false;
	CUserAgentCore* ua = CUserAgentCore::GetUserAgentCore();
	CPhBkGroupManager *managerEPB = ua->GetPGMManager()->GetPhBkManager(ua->GetUserID(), PB_ENTERPRISE_ADDRESS_BOOK);
	if(!managerEPB)
	{
		DEBUGCODE(Tracer::LogA("[LoadPBThread] enterprise phonebook is null");)
		return 0;
	}

	CPhBkGroupManager *managerPPB = ua->GetPGMManager()->GetPhBkManager(ua->GetUserID(), PB_BUDDIES);
	if(!managerPPB)
	{
		DEBUGCODE(Tracer::LogA("[LoadPBThread] personal phonebook is null");)
		return 0;
	}

	UA_UTIL::SetThreadName(-1, "LoadPBThread");
	{
		////////////////////////////////////////////////////////////////////////
		// 企业通讯录
		////////////////////////////////////////////////////////////////////////
		//设置本地企业通讯录的文件名用于读取和更新
		CString fileEPB = ua->GetPGMManager()->GetPhBkLocalFileName(ua->GetUserID(), PB_ENTERPRISE_ADDRESS_BOOK);
		managerEPB->SetFileName((fileEPB));
		DEBUGCODE(Tracer::Log(_T("[LoadPBThread] local enterprice phonebook filename: %s"), fileEPB);)

		if(g_loginData.loginState.enLoginType == NETWORK_LOGIN)
		{
			//在线登录
			if(!UA_UTIL::IsFileExist(fileEPB) || UA_UTIL::GetFileBytes(fileEPB) == 0)
			{
				//文件不存在，则认为是首次登陆
				bLoadFromServer = true;
			}
			else if(g_loginData.bEntPBVerChange)
			{
				//文件存在, 版本号发生变化
				//优先从本地加载数据
				managerEPB->Clear();
				if(managerEPB->LoadFromFile())
				{
					//更新自己的数据
					vector<GenericItem*> itemList;
					managerEPB->GetItemListByUID(ua->selfGuid, itemList);
					if(itemList.size()>0)
					{
						for(vector<GenericItem*>::iterator iter = itemList.begin(); iter != itemList.end(); iter++)
						{
							CPhBkItem* pPhBkItem = (CPhBkItem*)(*iter);
							if(pPhBkItem && strlen(g_loginData.pbItemSelf.uid) > 0)
							{
								CString strTempDep = pPhBkItem->m_strWorkDepartment;
								CString strSortLevel = pPhBkItem->m_strSortLevel;
								CString strVer = pPhBkItem->m_strVer;
								pPhBkItem->CopyFromStructToItem(&g_loginData.pbItemSelf);
								pPhBkItem->m_strWorkDepartment = strTempDep;
								pPhBkItem->m_strSortLevel = strSortLevel;
								pPhBkItem->m_strVer = strVer;
							}
						}
					}

					//全量更新通讯录
					bool bSuccess = false;
					CELGGWebService webService;
					CString strMethod = _T("ldap.client.search");
					MAPCString mapContent;
					mapContent[_T("user_id")] = ua->selfGuid;
					mapContent[_T("id_list[0]")] = _T("null");
					mapContent[_T("attr_list[0]")] = _T("uid");
					mapContent[_T("attr_list[1]")] = _T("sortlevel");
					mapContent[_T("attr_list[2]")] = _T("nodeversion");
					DWORD status = webService.Post(strMethod, mapContent);

					//对WS响应的处理
					char* resBuf = webService.m_pResBuf;
					string strJson;
					if(resBuf)
					{
						strJson.assign(resBuf);
						delete resBuf, resBuf = NULL;
					}
					ICTDBLOGA("[LAPPHEAD] - POST response = %s", strJson.c_str());
					if (status == HTTP_STATUS_OK)
					{
						Json::Value root;
						Json::Reader reader;
						if (reader.parse(strJson, root))
						{
							if (!root.isMember("status"))
							{
								//企业通讯录数据获取失败的处理
								CELGGWebService::ProcessWebServiceError(root);

								//更新企业通讯录从服务器的加载状态
								AM_UpdateLoginState(LOGIN_STA_LOADED_SERVER_DATA, LOADED_DATA_ENTERPRISE_PHONEBOOK, false);
							}
							else
							{
								CSingleLock lock(&managerEPB->m_cs);
								lock.Lock();

								//企业通讯录数据获取成功的处理
								managerEPB->Clear(false); //清除组织架构，不清除成员
								MAP_UID2PHBKITEM mapUid2PhBkItems = managerEPB->m_mapUid2PhBkItems; //备份map结构
								managerEPB->ClearAllItemList(); //清除之前的map结构
								if (managerEPB->FillDataWithJson(root, PB_ENTERPRISE_ADDRESS_BOOK))
								{
									//丰富联系人结构
									PerfectPhbkItem(managerEPB, mapUid2PhBkItems);

									//更新配置文件中当前登录用户的企业通讯录版本号
									UpdateDataVerToPersonalCfg(DVT_ENT);
									UpdateDataVerToPersonalCfg(DVT_ROLE);

									//更新本地企业通讯录文件
									managerEPB->SaveToFile();

									//更新企业通讯录从服务器的加载状态
									AM_UpdateLoginState(LOGIN_STA_LOADED_SERVER_DATA, LOADED_DATA_ENTERPRISE_PHONEBOOK, true);

									//更新企业通讯录界面
									g_loginData.uiPara[0].cb(LOGIN_UPDATE_ENTPB_UI, NULL, g_loginData.uiPara[0].user_data);

									bSuccess = true;
								}

								lock.Unlock();
							}
						}
					}

					if(!bSuccess)
					{
						//更新企业通讯录从本地的加载状态
						AM_UpdateLoginState(LOGIN_STA_LOADED_LOCAL_DATA, LOADED_DATA_ENTERPRISE_PHONEBOOK, true);

						//更新企业通讯录界面
						g_loginData.uiPara[0].cb(LOGIN_UPDATE_ENTPB_UI, NULL, g_loginData.uiPara[0].user_data);
					}
				}
				else
				{
					bLoadFromServer = true;
				}
			}
			else
			{
				//直接从本地获取即可
				bLoadFromLocal = true;
			}
		}
		else
		{
			//离线登录
			bLoadFromLocal = true;
		}


		//从本地加载企业通讯录数据
		if(bLoadFromLocal)
		{
			DEBUGCODE(Tracer::Log(_T("[LoadPBThread] get pb from local"));)

			CSingleLock lock(&managerEPB->m_cs);
			lock.Lock();

			managerEPB->Clear();
			if(managerEPB->LoadFromFile())
			{
				CONTACTITEM* ppbitem = new CONTACTITEM;
				vector<GenericItem*> itemList;
				managerEPB->GetItemListByUID(ua->selfGuid, itemList);
				if(itemList.size()>0)
				{
					for(vector<GenericItem*>::iterator iter = itemList.begin(); iter != itemList.end(); iter++)
					{
						CPhBkItem* pPhBkItem = (CPhBkItem*)(*iter);
						//离线登录更新自己头像和个性签名
						if(pPhBkItem)
						{
							PhBkItem2ContactItem(pPhBkItem, ppbitem);
							break;
						}
					}
				}

				//更新企业通讯录从本地加载状态
				AM_UpdateLoginState(LOGIN_STA_LOADED_LOCAL_DATA, LOADED_DATA_ENTERPRISE_PHONEBOOK, true);

				lock.Unlock();

				if (g_loginData.uiPara[0].cb)
				{
					if (g_loginData.loginState.enLoginType == OFFLINE_LOGIN) //避免正常登录时收到两次LOGIN_UPDATE_USERSELF_UI
					{
						//更新自己头像和个性签名
						g_loginData.uiPara[0].cb(LOGIN_UPDATE_USERSELF_UI, ppbitem, g_loginData.uiPara[0].user_data);
						ppbitem = NULL;
					}

					//更新企业通讯录界面
					g_loginData.uiPara[0].cb(LOGIN_UPDATE_ENTPB_UI, NULL, g_loginData.uiPara[0].user_data);
				}
				if (ppbitem) delete ppbitem;
			}
			else
			{
				//更新企业通讯录从本地加载状态
				AM_UpdateLoginState(LOGIN_STA_LOADED_LOCAL_DATA, LOADED_DATA_ENTERPRISE_PHONEBOOK, false);
				lock.Unlock();
			}
		}

		//从服务器加载企业通讯录
		if(bLoadFromServer)
		{
			DEBUGCODE(Tracer::Log(_T("[LoadPBThread] get pb from server"));)

			//全量获取企业通讯录的基本信息
			CELGGWebService webService;
			CString strMethod = _T("ldap.client.search");
			MAPCString mapContent;
			mapContent[_T("user_id")] = ua->selfGuid;
			mapContent[_T("id_list[0]")] = _T("null");
			mapContent[_T("attr_list[0]")] = _T("uid");
			mapContent[_T("attr_list[1]")] = _T("sortlevel");
			mapContent[_T("attr_list[2]")] = _T("nodeversion");
			mapContent[_T("attr_list[3]")] = _T("membername");
			DWORD status = webService.Post(strMethod, mapContent);

			//对WS响应的处理
			char* resBuf = webService.m_pResBuf;
			string strJson;
			if(resBuf)
			{
				strJson.assign(resBuf);
				delete resBuf, resBuf = NULL;
			}
			ICTDBLOGA("[LAPPHEAD] - POST response = %s", strJson.c_str());
			if (status == HTTP_STATUS_OK)
			{
				Json::Value root;
				Json::Reader reader;
				if (reader.parse(strJson, root))
				{
					if (!root.isMember("status"))
					{
						//企业通讯录数据获取失败的处理
						CELGGWebService::ProcessWebServiceError(root);

						//更新企业通讯录从服务器的加载状态
						AM_UpdateLoginState(LOGIN_STA_LOADED_SERVER_DATA, LOADED_DATA_ENTERPRISE_PHONEBOOK, false);
					}
					else
					{
						CSingleLock lock(&managerEPB->m_cs);
						lock.Lock();

						//企业通讯录数据获取成功的处理
						managerEPB->Clear();
						if (managerEPB->FillDataWithJson(root, PB_ENTERPRISE_ADDRESS_BOOK))
						{
							//更新自己的数据
							vector<GenericItem*> itemList;
							managerEPB->GetItemListByUID(ua->selfGuid, itemList);
							if(itemList.size()>0)
							{
								for(vector<GenericItem*>::iterator iter = itemList.begin(); iter != itemList.end(); iter++)
								{
									CPhBkItem* pPhBkItem = (CPhBkItem*)(*iter);
									if(pPhBkItem && strlen(g_loginData.pbItemSelf.uid) > 0)
									{
										CString strTempDep = pPhBkItem->m_strWorkDepartment;
										CString strSortLevel = pPhBkItem->m_strSortLevel;
										CString strVer = pPhBkItem->m_strVer;
										pPhBkItem->CopyFromStructToItem(&g_loginData.pbItemSelf);
										pPhBkItem->m_strWorkDepartment = strTempDep;
										pPhBkItem->m_strSortLevel = strSortLevel;
										pPhBkItem->m_strVer = strVer;
										pPhBkItem->m_nWhole = 1;
									}
								}
							}

							//更新企业通讯录从服务器的加载状态
							AM_UpdateLoginState(LOGIN_STA_LOADED_SERVER_DATA, LOADED_DATA_ENTERPRISE_PHONEBOOK, true);

							//更新配置文件中当前登录用户的企业通讯录版本号
							UpdateDataVerToPersonalCfg(DVT_ENT);
							UpdateDataVerToPersonalCfg(DVT_ROLE);

							//更新本地企业通讯录文件
							managerEPB->SaveToFile();

							lock.Unlock();

							//更新企业通讯录界面
							g_loginData.uiPara[0].cb(LOGIN_UPDATE_ENTPB_UI, NULL, g_loginData.uiPara[0].user_data);
						}
					}
				}
			}
		}
	}

	{
		////////////////////////////////////////////////////////////////////////
		// 个人通讯录
		////////////////////////////////////////////////////////////////////////
		CString filePPB = ua->GetPGMManager()->GetPhBkLocalFileName(ua->GetUserID(), PB_BUDDIES);
		managerPPB->SetFileName((filePPB));
		DEBUGCODE(Tracer::Log(_T("[LoadPBThread] local personal phonebook filename: %s"), filePPB);)

		bLoadFromLocal = false;
		bLoadFromServer = false;
		if (g_loginData.loginState.enLoginType == NETWORK_LOGIN)
			//在线登录, 从服务器加载数据（个人通讯录没有版本号）
			bLoadFromServer = true;
		else
			//离线登录, 从本地文件加载数据
			bLoadFromLocal = true;
	
		if (bLoadFromLocal)
		{
			//从本地加载个人通讯录数据
			//DEBUGCODE(Tracer::Log(_T("[LoadPBThread] get personal phonebook from local"));)

			CSingleLock lock(&managerPPB->m_cs);
			lock.Lock();

			managerPPB->Clear();

			// 允许加载个人通讯录
			if (Man_appConfig.bPersonalContact && managerPPB->LoadFromFile())
			{
				ICTDBLOG(_T("[LoadPBThread] get personal phonebook from local"));
				//从本地文件加载个人通讯录数据
				CONTACTITEM* ppbitem = new CONTACTITEM;
				vector<GenericItem*> itemList;
				managerPPB->GetItemListByUID(ua->selfGuid, itemList);
				if(itemList.size() > 0)
				{
					for(vector<GenericItem*>::iterator iter = itemList.begin(); iter != itemList.end(); iter++)
					{
						CPhBkItem* pPhBkItem = (CPhBkItem*)(*iter);
						//离线登录更新自己头像和个性签名
						if(pPhBkItem)
						{
							PhBkItem2ContactItem(pPhBkItem, ppbitem);
							break;
						}
					}
				}

				//更新个人通讯录从本地加载状态
				AM_UpdateLoginState(LOGIN_STA_LOADED_LOCAL_DATA, LOADED_DATA_PERSONAL_PHONEBOOK, true);

				lock.Unlock();

				if (g_loginData.uiPara[0].cb)
				{
					if (g_loginData.loginState.enLoginType == OFFLINE_LOGIN) //避免正常登录时收到两次LOGIN_UPDATE_USERSELF_UI
					{
						//更新自己头像和个性签名
						g_loginData.uiPara[0].cb(LOGIN_UPDATE_USERSELF_UI, ppbitem, g_loginData.uiPara[0].user_data);
						ppbitem = NULL;
					}

					//更新个人通讯录界面
					g_loginData.uiPara[0].cb(LOGIN_UPDATE_PERSONALPB_UI, NULL, g_loginData.uiPara[0].user_data);
				}
				if (ppbitem) delete ppbitem;
			}
			else
			{
				ICTDBLOG(_T("[LoadPBThread] get personal phonebook from local failed or personal contact disabled"));
				//更新个人通讯录从本地加载状态
				AM_UpdateLoginState(LOGIN_STA_LOADED_LOCAL_DATA, LOADED_DATA_PERSONAL_PHONEBOOK, false);
				lock.Unlock();
			}
		}

		if (bLoadFromServer)
		{
			//从服务器加载个人通讯录
			//DEBUGCODE(Tracer::Log(_T("[LoadPBThread] get personal phonebook from server"));)

			// 允许加载个人通讯录
			if (Man_appConfig.bPersonalContact)
			{
				ICTDBLOG(_T("[LoadPBThread] get personal phonebook from server"));
				CString strEid;
				Char2CString(ua->m_curAccount.GetAccInfo()->chEid, strEid);
				//全量获取个人通讯录的基本信息
				CELGGWebService webService;
				CString strMethod = _T("ldap.contacts.list");
				MAPCString mapContent;
				mapContent[_T("eid")] = strEid;
				mapContent[_T("uid")] = ua->selfGuid;
				DWORD status = webService.Post(strMethod, mapContent);

				//对WS响应的处理
				char* resBuf = webService.m_pResBuf;
				string strJson;
				if(resBuf)
				{
					strJson.assign(resBuf);
					delete resBuf, resBuf = NULL;
				}
				ICTDBLOGA("[LAPPHEAD] - POST response = %s", strJson.c_str());
				if (status == HTTP_STATUS_OK)
				{
					Json::Value root;
					Json::Reader reader;
					if (reader.parse(strJson, root))
					{
						if (!root.isMember("status"))
						{
							//个人通讯录数据获取失败的处理
							CELGGWebService::ProcessWebServiceError(root);

							//更新个人通讯录从服务器的加载状态
							AM_UpdateLoginState(LOGIN_STA_LOADED_SERVER_DATA, LOADED_DATA_PERSONAL_PHONEBOOK, false);
						}
						else
						{
							//个人通讯录数据获取成功的处理
							CSingleLock lock(&managerPPB->m_cs);
							lock.Lock();

							//清除组织架构
							managerPPB->Clear();
							//使用json数据填充个人通讯录
							if (managerPPB->FillDataWithJson(root, PB_BUDDIES))
							{
								//更新个人通讯录从服务器的加载状态
								AM_UpdateLoginState(LOGIN_STA_LOADED_SERVER_DATA, LOADED_DATA_PERSONAL_PHONEBOOK, true);

								//更新本地个人通讯录文件
								managerPPB->SaveToFile();

								lock.Unlock();

								//更新个人通讯录界面, 需要修改
								g_loginData.uiPara[0].cb(LOGIN_UPDATE_PERSONALPB_UI, NULL, g_loginData.uiPara[0].user_data);
							}
						}
					}
				}
			}
			else
			{
				ICTDBLOG(_T("[LoadPBThread] get personal phonebook from server disabled"));
				CSingleLock lock(&managerPPB->m_cs);
				lock.Lock();

				//清除组织架构
				managerPPB->Clear();
				managerPPB->SaveToFile();
				lock.Unlock();
				
				//更新个人通讯录从服务器的加载状态
				AM_UpdateLoginState(LOGIN_STA_LOADED_SERVER_DATA, LOADED_DATA_PERSONAL_PHONEBOOK, true);
			}
		}
	}

	if (g_loginData.loginState.enLoginType == NETWORK_LOGIN)
	{
		FastUpdateUserInfo(ua->selfGuid); //登录时保证自己的头像能及时更新 add by guanfm 20160303
		g_loginData.bNeedUpdate = true; //该标志用于忽略时间上次更新时间限制，重新登录后应保证展开部门时能及时更新 add by guanfm 20160304

		//后台获取未完成的个人信息
		if(g_loginData.hUpdateThread4)
		{
			if (WaitForSingleObject(g_loginData.hUpdateThread4, 3000) == WAIT_TIMEOUT)
			{
				DEBUGCODE(Tracer::LogA("[WARNING][LoadPBThread] TerminateThread: UpdateNodeThread4");)
				TerminateThread(g_loginData.hUpdateThread4, 1);
			}
			CloseHandle(g_loginData.hUpdateThread4);
			g_loginData.hUpdateThread4 = NULL;
		}

		CSingleLock lock(&managerEPB->m_cs);
		CSingleLock lock4(&g_loginData.csUN4);
		lock.Lock();
		lock4.Lock();

		//清空队列
		while(!g_loginData.queUid2UpdateNode4.empty())
		{
			g_loginData.queUid2UpdateNode4.pop();
		}

		for(MAP_UID2PHBKITEM::const_iterator iterM = managerEPB->m_mapUid2PhBkItems.begin(); iterM != managerEPB->m_mapUid2PhBkItems.end(); iterM++)
		{
			for(vector<GenericItem*>::const_iterator iter = iterM->second.begin(); iter != iterM->second.end(); iter++)
			{
				CPhBkItem* pPhBkItem = (CPhBkItem*)(*iter);
				if(pPhBkItem && pPhBkItem->m_nWhole == 0)
				{
					if(pPhBkItem->m_pParent)
					{
						CPhBkGroup* pGroup = (CPhBkGroup*)pPhBkItem->m_pParent;
						PUID puid;
						puid.strParentId = 	pGroup->m_stCommonGroup.GetGroupId();
						puid.strUid = pPhBkItem->m_strUID;
						puid.bUpdatePhoto = false;

						g_loginData.queUid2UpdateNode4.push(puid);
					}
				}
			}
		}

		for(MAP_UID2PHBKITEM::const_iterator iterM = managerPPB->m_mapUid2PhBkItems.begin(); iterM != managerPPB->m_mapUid2PhBkItems.end(); iterM++)
		{
			for(vector<GenericItem*>::const_iterator iter = iterM->second.begin(); iter != iterM->second.end(); iter++)
			{
				CPhBkItem* pPhBkItem = (CPhBkItem*)(*iter);
				if(pPhBkItem && pPhBkItem->m_nWhole == 0)
				{
					if(pPhBkItem->m_pParent)
					{
						CPhBkGroup* pGroup = (CPhBkGroup*)pPhBkItem->m_pParent;
						PUID puid;
						puid.strParentId = 	pGroup->m_stCommonGroup.GetGroupId();
						puid.strUid = pPhBkItem->m_strUID;
						puid.bUpdatePhoto = false;

						g_loginData.queUid2UpdateNode4.push(puid);
					}
				}
			}
		}

		if(!g_loginData.queUid2UpdateNode4.empty())
		{
			g_loginData.hUpdateThread4 = CreateThread(NULL, 0, UpdateNodeThread4, NULL, 0, NULL);
		}

		lock4.Unlock();
		lock.Unlock();
	}
	DEBUGCODE(Tracer::LogA("[LoadPBThread] ending...");)
	return 0;
}

DWORD WINAPI FullUpdatePhBkThread(LPVOID lpParameter)
{
	DWORD dwRet = -1;
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	CPhBkGroupManager *managerPB = ua->GetPGMManager()->GetPhBkManager(ua->GetUserID(), PB_ENTERPRISE_ADDRESS_BOOK);
	if(!managerPB)
		return 1;
	UA_UTIL::SetThreadName(-1,"FullUpdatePhBkThread");

	g_loginData.bNeedUpdate = true; //该标志用于忽略时间上次更新时间限制，手动刷新通讯录后应保证展开部门时能及时更新 add by guanfm 20160304

	//设置本地企业通讯录的文件名用于读取和更新
	CString filename = ua->GetPGMManager()->GetPhBkLocalFileName(ua->GetUserID(), PB_ENTERPRISE_ADDRESS_BOOK);
	managerPB->SetFileName((filename));

	DEBUGCODE(Tracer::Log(_T("[UPDATE][CBCSClientDlg::FullUpdatePhBkThread] [local phonebook filename]:[%s]"), filename);)

	//发起企业通讯录数据获取的同步WS
	CELGGWebService webService;
	CString strMethod = _T("ldap.client.search");
	MAPCString mapContent;
	mapContent[_T("user_id")] = ua->selfGuid;
	mapContent[_T("id_list[0]")] = _T("null");
	mapContent[_T("attr_list[0]")] = _T("uid");
	mapContent[_T("attr_list[1]")] = _T("sortlevel");
	mapContent[_T("attr_list[2]")] = _T("nodeversion");
	DWORD status = webService.Post(strMethod, mapContent);

	//对WS响应的处理
	char* resBuf = webService.m_pResBuf;
	string strJson;
	if(resBuf)
	{
		strJson.assign(resBuf);
		delete resBuf, resBuf = NULL;
	}
	ICTDBLOGA("[LAPPHEAD] - POST response = %s", strJson.c_str());
	if (status == HTTP_STATUS_OK)
	{
		Json::Value root;
		Json::Reader reader;
		if (reader.parse(strJson, root))
		{
			if (!root.isMember("status"))
			{
				//企业通讯录数据获取失败的处理
				CELGGWebService::ProcessWebServiceError(root);
				dwRet = 2;
			}
			else
			{
				CSingleLock lock(&managerPB->m_cs);
				lock.Lock();

				//企业通讯录数据获取成功的处理
				managerPB->Clear(false); //清除组织架构，不清除成员
				MAP_UID2PHBKITEM mapUid2PhBkItems = managerPB->m_mapUid2PhBkItems; //备份map结构
				managerPB->ClearAllItemList(); //清除之前的map结构
				if (managerPB->FillDataWithJson(root, PB_ENTERPRISE_ADDRESS_BOOK))
				{
					//丰富联系人结构
					PerfectPhbkItem(managerPB, mapUid2PhBkItems);

					//更新配置文件中当前登录用户的企业通讯录版本号
					UpdateDataVerToPersonalCfg(DVT_ENT);
					UpdateDataVerToPersonalCfg(DVT_ROLE);

					//更新本地企业通讯录文件
					managerPB->SaveToFile();

					lock.Unlock();

					//更新企业通讯录界面
					g_loginData.uiPara[0].cb(LOGIN_UPDATE_ENTPB_UI, NULL, g_loginData.uiPara[0].user_data);

					//更新消息提醒
					//SetTimer(WM_TIMER_UPDATE_P2P_MSG_NOTIFY, 1000, NULL);

					dwRet = 0;
				}
			}
		}
	}
	return dwRet;
}

DWORD WINAPI LoadGroupThread(LPVOID lpParameter)
{
	DEBUGCODE(Tracer::LogA("[LoadGroupThread] Running...");)

	int local_group_version, last_group_version;
	bool bLoadFromLocal = false;
	CUserAgentCore*ua = CUserAgentCore::GetUserAgentCore();
	GroupInfosMan* pGroupInfosMan = ua->GetPGMManager()->GetGroupManager(ua->GetUserID());
	if(!pGroupInfosMan)
	{
		DEBUGCODE(Tracer::LogA("[LoadGroupThread] pGroupInfosMan is null return");)
		return 0;
	}
	UA_UTIL::SetThreadName(-1,"LoadGroupThread");

	//设置本地群组数据文件用于读取和更新
	CString filename = ua->GetPGMManager()->GetGroupLocalFileName(ua->GetUserID());
	pGroupInfosMan->SetFileName(filename);

	last_group_version = g_loginData.stLDV.nGroupVer;
	local_group_version = ua->m_curAccount.GetDataVer(DVT_GROUP);
	pGroupInfosMan->m_bVerChanged = last_group_version != local_group_version;
	DEBUGCODE(Tracer::LogA("[LoadGroupThread] last_group_version: %d, local_group_version: %d",last_group_version, local_group_version);)

	if(g_loginData.loginState.enLoginType == NETWORK_LOGIN &&
		(!UA_UTIL::IsFileExist(filename) || pGroupInfosMan->m_bVerChanged))
	{
		//发起获取群组的WS请求
		const int nLimit = 100; //获取群组数量的上限
		CELGGWebService webService;
		CString strMethod = _T("group.get.groups");
		CString strLimit;
		strLimit.Format(_T("%d"), nLimit);
		MAPCString mapContent;
		mapContent[_T("context")] = _T("mine");
		mapContent[_T("limit")] = strLimit;
		DWORD status = webService.Post(strMethod, mapContent);

		//对WS响应的处理
		bool bSuccess = false;
		char* resBuf = webService.m_pResBuf;
		string strJson;
		if(resBuf)
		{
			strJson.assign(resBuf);
			delete resBuf, resBuf = NULL;
		}
		ICTDBLOGA("[LAPPHEAD] - POST response = %s", strJson.c_str());
		if (status == HTTP_STATUS_OK)
		{
			Json::Reader reader;
			Json::Value  root;
			if(reader.parse(strJson, root))
			{
				bool bGetSuc = false;
				if (!root.isMember("status"))
				{
					bSuccess = false;
					int nErrorCode = CELGGWebService::ProcessWebServiceError(root);
					if(nErrorCode == ELGG_ERROR_NO_GROUPS)
					{
						bGetSuc = true;
					}
				}
				else
					bGetSuc = true;
				if(bGetSuc)
				{
					bSuccess = true;
					pGroupInfosMan->Clear();
					pGroupInfosMan->FromJson(root);

					//更新群组从服务器的加载状态
					AM_UpdateLoginState(LOGIN_STA_LOADED_SERVER_DATA, LOADED_DATA_GROUPS, true);

					//载入群组信息之后获取群组成员
					int nCfgId = ua->GetUserID();
					CStringArray guids;
					pGroupInfosMan->GetGroupUidList(guids);
					int size = guids.GetSize();
					for (int i = 0; i < size; i++)
					{
						CString guid;
						guid = guids.GetAt(i);
						if (guid.IsEmpty() == FALSE)
						{
							int nRetryT = 0;
							while(!pGroupInfosMan->LoadGroupMembers(nCfgId,guid))
							{
								//因网络状况差而获取失败则重复5次获取
								if(++nRetryT >= 5)
								{
									break;
								}
							}
						}
					}

					//更新到本地文件
					pGroupInfosMan->SaveToFile();

					//更新本地版本号
					UpdateDataVerToPersonalCfg(DVT_GROUP);

					//更新群组界面
					if(g_loginData.uiPara[0].cb)
						g_loginData.uiPara[0].cb(LOGIN_UPDATE_GROUP_UI, NULL, g_loginData.uiPara[0].user_data);
				}
			}
		}
		else
		{
			bSuccess = false;
		}

		//如果服务器数据获取失败，则加载本地群组
		if(!bSuccess)
		{
			//更新群组从服务器的加载状态
			AM_UpdateLoginState(LOGIN_STA_LOADED_SERVER_DATA, LOADED_DATA_GROUPS, false);

			bLoadFromLocal = true;
		}
	}
	else
	{
		//非联网登录则从本地获取群组
		bLoadFromLocal = true;
	}

	//从本地加载群组数据
	if(bLoadFromLocal)
	{
		//更新群组从服务器的加载状态
		AM_UpdateLoginState(LOGIN_STA_LOADED_SERVER_DATA, LOADED_DATA_GROUPS, false);

		//清除群组数据
		pGroupInfosMan->Clear();

		//加载本地群组数据
		if(pGroupInfosMan->LoadFromFile())
		{
			//更新群组从本地的加载状态
			AM_UpdateLoginState(LOGIN_STA_LOADED_LOCAL_DATA, LOADED_DATA_GROUPS, true);

			//更新群组界面
			if(g_loginData.uiPara[0].cb)
				g_loginData.uiPara[0].cb(LOGIN_UPDATE_GROUP_UI, NULL, g_loginData.uiPara[0].user_data);
		}
		else
		{
			//如果服务器和本地都加载失败，为了登录成功，应认为本地数据和界面都更新成功

			//更新群组从本地的加载状态
			AM_UpdateLoginState(LOGIN_STA_LOADED_LOCAL_DATA, LOADED_DATA_GROUPS, true);

			//更新群组的界面更新状态
			AM_UpdateLoginState(LOGIN_STA_UPDATE_UI, UPDATE_UI_GROUPS, true);
		}
	}
	DEBUGCODE(Tracer::LogA("[LoadGroupThread] Ending...");)
	return 0;
}

//全量更新群组列表线程
DWORD WINAPI FullUpdateGroupListThread(LPVOID lpParameter)
{
	CUserAgentCore*ua = CUserAgentCore::GetUserAgentCore();
	GroupInfosMan* pGroupInfosMan = ua->GetPGMManager()->GetGroupManager(ua->GetUserID());
	if(!pGroupInfosMan)
		return 0;
	UA_UTIL::SetThreadName(-1,"FullUpdateGroupListThread");

	//设置本地群组数据文件用于读取和更新
	CString filename = ua->GetPGMManager()->GetGroupLocalFileName(ua->GetUserID());
	pGroupInfosMan->SetFileName(filename);

	const int nLimit = 100; //获取群组数量的上限
	CELGGWebService webService;
	CString strMethod = _T("group.get.groups");
	CString strLimit;
	strLimit.Format(_T("%d"), nLimit);
	MAPCString mapContent;
	mapContent[_T("context")] = _T("mine");
	mapContent[_T("limit")] = strLimit;
	DWORD status = webService.Post(strMethod, mapContent);

	//对WS响应的处理
	bool bSuccess = false;
	char* resBuf = webService.m_pResBuf;
	string strJson;
	if(resBuf)
	{
		strJson.assign(resBuf);
		delete resBuf, resBuf = NULL;
	}
	ICTDBLOGA("[LAPPHEAD] - POST response = %s", strJson.c_str());
	if (status == HTTP_STATUS_OK)
	{
		Json::Reader reader;
		Json::Value  root;
		if(reader.parse(strJson, root))
		{
			if (!root.isMember("status"))
			{
				bSuccess = false;
				int nErrorCode = CELGGWebService::ProcessWebServiceError(root);
				if(nErrorCode == ELGG_ERROR_NO_GROUPS)
				{
					//更新本地版本号
					AM_UpdateLoginState(LOGIN_STA_LOADED_SERVER_DATA, LOADED_DATA_GROUPS, true);
					AM_UpdateLoginState(LOGIN_STA_UPDATE_UI, UPDATE_UI_GROUPS, true);
					return 0;
				}
			}
			else
			{
				bSuccess = true;
				pGroupInfosMan->Clear();
				if(pGroupInfosMan->FromJson(root))
				{
					//更新群组从服务器的加载状态
					AM_UpdateLoginState(LOGIN_STA_LOADED_SERVER_DATA, LOADED_DATA_GROUPS, true);

					//载入群组信息之后获取群组成员
					int nCfgId = ua->GetUserID();
					CStringArray guids;
					pGroupInfosMan->GetGroupUidList(guids);
					int size = guids.GetSize();
					for (int i = 0; i < size; i++)
					{
						CString guid;
						guid = guids.GetAt(i);
						if (guid.IsEmpty() == FALSE)
						{
							int nRetryT = 0;
							while(!pGroupInfosMan->LoadGroupMembers(nCfgId,guid))
							{
								//因网络状况差而获取失败则重复5次获取
								if(++nRetryT >= 5)
								{
									break;
								}
							}
						}
					}

					//更新到本地文件
					pGroupInfosMan->SaveToFile();

					//更新本地版本号
					int nGroupVer = -1;
					int ret = ua->GetPGMManager()->GetGroupListVersion(nGroupVer, false);
					if(ret != -1)
					{
						g_loginData.stLDV.nGroupVer = nGroupVer;
						UpdateDataVerToPersonalCfg(DVT_GROUP);
					}

					//更新群组界面
					if(g_loginData.uiPara[0].cb)
						g_loginData.uiPara[0].cb(LOGIN_UPDATE_GROUP_UI, NULL, g_loginData.uiPara[0].user_data);
				}
			}
		}
	}
	return 0;
}

void GetLastIM(ChatType chattype, PGList* plstRecentMsg)
{
	ASSERT(plstRecentMsg);
	if (!plstRecentMsg)
		return;
	*plstRecentMsg = NULL;

	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();

	//获取最近联系人信息
	list<CRecentContact*> ret;
	ua->GetMessageManager()->GetRecentContacts(ret);
	DEBUGCODE(Tracer::Log(_T("[AccountMan.cpp::GetLastIM] -ret.size():%d"),ret.size());)


	//遍历最近联系人结果
	PGList pList = NULL;
	for (list<CRecentContact*>::iterator it = ret.begin(); it != ret.end(); it++)
	{
		CRecentContact* contact = *it;

		if (!contact || contact->id == _T(""))
			continue;

		//过滤只取群组信息
		if(chattype == CT_Group && contact->MChatType() != CT_Group)
		{
			continue;
		}

		CString uid = _T(""), uri, mbName, dep, sipUri;
		CString description = _T("");
		//CString filenameHead = _T("");
		//设置最近联系人信息
		TELPO_PRES status = TELPO_PRE_PC_ONLINE;
		char strUid[512];
		CString2Char(contact->id, strUid);
		if (contact->MChatType() == CT_Group)
		{
			//群组类型的最近联系人
			JsonSaveDM::GroupInfosMan* manager = ua->GetGroupsManaTelpo(ua->GetUserID());
			if (manager != NULL)
			{
				CSingleLock lock(&(manager->m_cs));
				lock.Lock();
				JsonSaveDM::GroupInfo* group = manager->FindGroup(contact->id);
				lock.Unlock();
				if (group != NULL)
				{
					//群组唯一标识
					uid = group->guid;
					//群组名称
					description = group->name;
					//群组头像
					//filenameHead = group->GetImagePath();
				}
				else
				{
					uid = _T("");
				}

			}
		}
		else if (contact->MChatType() == CT_P2P)
		{
			//一对一类型的最近联系人
			PHONEBOOKITEM item;
			if (ua->GetPhoneBookItemByUid(contact->id,&item) == 0)
			{
				Char2CString(item.uid, uid);
				Char2CString(item.sipUri, uri);
				Char2CString(item.name, description);

				//status = (TELPO_PRES)ua->GetPGMManager()->GetPresenceManager(ua->GetUserID())->GetPresence(uid, NULL);
				//filenameHead = ua->GetCurWorkingDir() + _T("res\\HeadPortrait\\user_portrait\\") + uid + _T(".png");		//modify by hech 20140729 支持在线头像 hech TODO 建议通讯录获取头像文件
			}
			else
			{
				uid = _T("");
			}
		}
		else if (contact->MChatType() == CT_PubAccount)
		{
			//公共账号类型的最近联系人
			JsonSaveDM::PubAccMan* manager = ua->GetPubAccManTelpo(ua->GetUserID());
			if (manager != NULL)
			{
				JsonSaveDM::PubAccItem* item = manager->FindPubAcc(contact->id);
				if (item != NULL)
				{
					//公共账号唯一标识
					uid = item->uid;
					//群组名称
					description = item->description;
					//公共帐号头像
					//filenameHead = item->GetImagePath();
				}
			}
		}
		else if (contact->MChatType() == CT_LAPP)
		{
			//轻应用
			CLAPPManager* pLAPPMan = ua->GetPGMManager()->GetLAPPManager(ua->GetUserID());
			if(pLAPPMan)
			{
				CSingleLock lock(&pLAPPMan->m_cs);
				lock.Lock();
				CLAPPItem* pItem = pLAPPMan->GetLAPPItemById(contact->id);
				if(pItem)
				{
					if(!(pItem->m_noticeWay&2))
					{
						lock.Unlock();
						continue;
					}
					uid = pItem->m_szId;
					description = pItem->m_szName;
				}
				lock.Unlock();
			}
		}

		if (!uid.IsEmpty())
		{
			//临时存储在最近联系人UI结构中
			RECENT_CONTACT_UI_DATA* pRcud = (RECENT_CONTACT_UI_DATA*)malloc(sizeof(RECENT_CONTACT_UI_DATA));
			CString2Char(uid, pRcud->uid);
			CString2Char(uri, pRcud->sipUri);
			CString2Char(description, pRcud->name);
			CString2Char(contact->lastMessage->FormatContent(), pRcud->msgContent);
			pRcud->msgTime = DateTimeToUnixepoch(contact->lastMessage->GetTime());
			pRcud->chatType = (int32)contact->MChatType();
			pRcud->status = status;
			pRcud->unreadNum = contact->unReadNum;
			if(!*plstRecentMsg)
			{
				*plstRecentMsg = pList = (PGList)malloc(sizeof(GList));
				(*plstRecentMsg)->data = pRcud;
			}
			else
			{
				PGList pTemp = pList;
				pList = (PGList)malloc(sizeof(GList));
				pTemp->next = pList;
				pList->data = pRcud;
			}
		}
		uid.Empty();
		uri.Empty();
		mbName.Empty();
		dep.Empty();
		sipUri.Empty();
		description.Empty();
	}
	if(pList)
		pList->next = NULL;

	MESSAGE_MANAGER::RealseContacts(ret);
}
int32 DelRecord(char8* uid)
{
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	//list<CRecentContact*> ret;
	CString strUid;
	Char2CString(uid,strUid);
	ua->GetMessageManager()->DeleteRecordFrom(strUid);
	return S_SUC;
	
}

int32 SetTargetDescriptionDelete(char8* uid)
{
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	//list<CRecentContact*> ret;
	CString strUid;
	Char2CString(uid,strUid);
	ua->GetMessageManager()->SetTargetDescriptionDeleteByTargetID(strUid);
	return S_SUC;

}
int32 SetTargetDescriptionTop(char8* uid)
{
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	//list<CRecentContact*> ret;
	CString strUid;
	Char2CString(uid,strUid);
	ua->GetMessageManager()->SetTargetDescriptionTopByTargetID(strUid);
	return S_SUC;

}

DWORD WINAPI LoadRecentContactThread(LPVOID lpParameter)
{
	DEBUGCODE(Tracer::LogA("[LoadRecentContactThread] Running...");)

	//更新登陆状态
	AM_UpdateLoginState(LOGIN_STA_LOADED_LOCAL_DATA, LOADED_DATA_RECENT_CONTACT, true);

	//更新到界面
	if(g_loginData.uiPara[0].cb)
	{
		PGList plstRecentMsg;
		GetLastIM(CT_Other, &plstRecentMsg);
		g_loginData.uiPara[0].cb(LOGIN_UPDATE_REMSG_UI, plstRecentMsg, g_loginData.uiPara[0].user_data);
	}

	DEBUGCODE(Tracer::LogA("[LoadRecentContactThread] Ending...");)
	return 0;
}

DWORD WINAPI LoadCallRecordThread(LPVOID lpParameter)
{
	DEBUGCODE(Tracer::LogA("[LoadCallRecordThread] Running...");)
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();

	//释放最近联系人结构
	ReleaseGList(g_loginData.listCallRecord);
	g_loginData.listCallRecord = NULL;

	//获取最近通话信息
	list<CRecentCall*> ret;
	ua->GetCDRManager()->GetRecentCalls(ret);

	//遍历最近通话结果
	PGList pList = NULL;
	for (list<CRecentCall*>::iterator it = ret.begin(); it != ret.end(); it++)
	{
		CRecentCall* call = *it;

		if (call->id == _T(""))
			continue;

		DEBUGCODE(Tracer::LogToConsole(_T("[LoadCallRecordThread] - %s:%s:%s"), call->id, call->lastCDR->GetStartTime(), call->lastCDR->numberCallback);)

		CString name(call->lastCDR->description);
		name.AppendFormat(_T(" ( %d )"),call->totalCount);
		CString uid;
		PHONEBOOKITEM item;
		if(ua->GetPhoneBookItemByNumber(call->lastCDR->sipNumber, &item) == 0)
			//在通信录中
			Char2CString(item.uid, uid);
		else
			//不在通讯录中
			uid = call->lastCDR->sipNumber;

		TELPO_PRES status = TELPO_PRE_PC_ONLINE;
		CString filenameHead = ua->GetAppDataRootPath() + _T("res\\HeadPortrait\\user_portrait\\") + uid + _T(".png");

		//临时存储在通话记录UI结构中
		CALL_RECORD_UI_DATA* pCrud = (CALL_RECORD_UI_DATA*)malloc(sizeof(CALL_RECORD_UI_DATA));
		CString2Char(uid, pCrud->uid);
		CString2Char(call->lastCDR->sipNumber, pCrud->sipUri);
		CString2Char(name, pCrud->name);
		CString2Char(filenameHead, pCrud->headPath);
		pCrud->callType = (int32)call->lastCDR->cdrType;//CDR_MANAGER::CDRType2Int(call->lastCDR->cdrType);
		CString2Char(call->lastCDR->addPrefix? UA_UTIL::GetAddPrexSipNum(call->lastCDR->numberCallback):call->lastCDR->numberCallback, pCrud->numCallback);
		CString2Char(UA_UTIL::GetCutPrexSipNum(call->lastCDR->numberCallback), pCrud->displayNum);
		CString2Char(call->lastCDR->GetFormatStartTime(), pCrud->callTime);

		if(!g_loginData.listCallRecord)
		{
			g_loginData.listCallRecord = pList = (PGList)malloc(sizeof(GList));
			g_loginData.listCallRecord->data = pCrud;
		}
		else
		{
			PGList pTemp = pList;
			pList = (PGList)malloc(sizeof(GList));
			pTemp->next = pList;
			pList->data = pCrud;
		}
	}
	if(pList)
		pList->next = NULL;

	CDR_MANAGER::RealseRecentCalls(ret);

	//更新登陆状态
	AM_UpdateLoginState(LOGIN_STA_LOADED_LOCAL_DATA, LOADED_DATA_CALL_RECORD, true);

	//更新到界面
	if(g_loginData.uiPara[0].cb)
		g_loginData.uiPara[0].cb(LOGIN_UPDATE_CAREC_UI, g_loginData.listCallRecord, g_loginData.uiPara[0].user_data);

	DEBUGCODE(Tracer::LogA("[LoadCallRecordThread] Ending...");)
	return 0;
}

DWORD WINAPI GetCurUserPBInfoThread(LPVOID lpParameter)
{
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();

	//获取当前用户的通讯录数据
	memset(&g_loginData.pbItemSelf, 0, sizeof(PHONEBOOKITEM));
	ua->GetPGMManager()->GetPhBkItemFromServByUid(ua->selfGuid, &g_loginData.pbItemSelf);

	//更新当前登录状态
	AM_UpdateLoginState(LOGIN_STA_LOADED_SERVER_DATA, LOADED_DATA_CUR_LOGIN_USER, true);

	//更新当前用户的界面
	CONTACTITEM& pbitem = *(new CONTACTITEM);
	GetCurAccountContactItem(&pbitem);

	//设置当前用户个人信息
	CString szname,szsip;
	Char2CString(pbitem.name,szname);
	Char2CString(pbitem.sipUri,szsip);
	ua->SetSelfInfomations(szname,szsip);

	//ua->m_curAccount.SetName();

	//界面回调
	if(g_loginData.uiPara[0].cb)
		g_loginData.uiPara[0].cb(LOGIN_UPDATE_USERSELF_UI, &pbitem, g_loginData.uiPara[0].user_data);
	else delete &pbitem;

	//更新当前登录状态
	AM_UpdateLoginState(LOGIN_STA_UPDATE_UI, LOADED_DATA_CUR_LOGIN_USER, true);
	return 0;
}


void LoadICTData()
{
	//获取轻应用
	g_loginData.hLoadLAPPThread = CreateThread(NULL, 0, LoadLAPPThread, NULL, 0, NULL);

	//执行企业通讯录数据加载流程
	g_loginData.hLoadPBThread = CreateThread(NULL, 0, LoadPBThread, NULL, 0, NULL);

	//执行群组数据加载流程
	g_loginData.hLoadGroupThread = CreateThread(NULL, 0, LoadGroupThread, NULL, 0, NULL);
}

//创建用户账号目录
void AM_CreateUserPaths(CString name, CString realm)
{
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();

	CString m_strImageSubDir = _T("Image");
	CString m_strVideoSubDir = _T("Video");
	CString m_strAudioSubDir = _T("Audio");
	CString m_strTempSubDir = _T("Temp");
	CString m_strRevFileSubDir = _T("RecvFile");
	CString m_strLAPPSubDir = _T("LAPP");

	//获取用户路径
	//先使用UserID获取, 前提是
	CString pathUser = ua->GetUserPath(ua->GetUserID());
	if (pathUser.IsEmpty())
	{
		//如果获取不到则使用用户名和账号获取
		char nameT[128];
		char realmT[128];
		CString2Char(name, nameT);
		CString2Char(realm, realmT);
		pathUser = ua->GetUserPath(nameT, realmT);
	}

	DEBUGCODE(Tracer::Log(_T("[AM_CreateUserPaths] user path - %s"), pathUser);)
	//创建用户根目录
	if (!pathUser.IsEmpty())
		::CreateDirectory(pathUser, NULL);

	//设置用户数据库路径
	ua->SetUserDBsPath(pathUser);

	//创建用户子目录
	list<CString> paths;
	CString af = _T(".\\");
	if (!pathUser.IsEmpty())
	{
		af = pathUser;
	}
	paths.push_back(af + m_strImageSubDir);
	paths.push_back(af + m_strVideoSubDir);
	paths.push_back(af + m_strAudioSubDir);
	paths.push_back(af + m_strTempSubDir);
	paths.push_back(af + m_strRevFileSubDir);
	paths.push_back(af + m_strLAPPSubDir);

	for(list<CString>::iterator iter = paths.begin(); iter != paths.end(); iter++)
	{
		CString path = (*iter);

		WIN32_FIND_DATA fd;
		void* hFind = ::FindFirstFile(path, &fd);
		if ((hFind != INVALID_HANDLE_VALUE) && (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			::FindClose(hFind);
		else
		{
			::CreateDirectory(path, NULL);
			::FindClose(hFind);
		}
	}
}

//获取头像资源路径
CString GetUserPortraitPath(CString strUid)
{
	return UA_UTIL::GetAppDataDir() + _T("res\\HeadPortrait\\user_portrait\\") + strUid + _T(".png");
}

//获取用户配置头像资源路径
CString GetConfigUserPortraitPath(CString strName, CString strRealm)
{
	return UA_UTIL::GetAppDataDir() + _T("res\\HeadPortrait\\user_portrait\\") + strName + _T("@") + strRealm + _T(".png");
}


BOOL AM_WSQuerySipAccount(CString server, CString uid, const char* authtoken, const char* api_key, ACCINFO *pAccInfo)
{
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	CString szAuthToken = _T(""), szApiKey = _T("");
	Char2CString(authtoken, szAuthToken), Char2CString(api_key, szApiKey);

	CELGGWebService webService;
	webService.m_szServer = server;
	webService.m_bAddToken = webService.m_bAddAPIKey = false;

	CString szMethod = _T("ldap.sipaccount.get"); //webservice方法
	MAPCString mapContent;
	mapContent[_T("uid")] = uid;
	mapContent[_T("auth_token")] = szAuthToken;
	mapContent[_T("api_key")] = szApiKey;

	DWORD status = webService.Post(szMethod, mapContent); //发起Post请求
	char* resBuf = webService.m_pResBuf; //给出的响应缓冲区，使用完需用delete释放掉

	using namespace std;
	string strJson;
	if(resBuf)
	{
		strJson.assign(resBuf);
		delete resBuf;
		resBuf = NULL;
	}
	CConfigDBManager* configManager = ua->GetConfigManager();

	if (status == HTTP_STATUS_OK)
	{
		Json::Reader reader;
		Json::Value root;

		//DEBUGCODE(Tracer::LogA("[AM_WSQuerySipAccount] Get - Json: %s", strJson.c_str());)
		ICTDBLOGA("[POST::AM_WSQuerySipAccount] : response = %s", strJson.c_str());

		if(!reader.parse(strJson, root))
		{
			DEBUGCODE(Tracer::LogA("[AM_WSQuerySipAccount] [json]:[%s] parse failed", strJson.c_str());)
		}
		else
		{
			if (!root.isMember("status"))
			{
				CELGGWebService::ProcessWebServiceError(root);
				return FALSE;
			}
			if (root.isMember("result"))
			{
				Json::Value result = root["result"];
				if(result.empty())
					return FALSE;

				//获取sip账号
				if(result.isMember("account"))
				{
					string temp = result["account"].asString();
					strncpy(pAccInfo->chSipAccount, temp.c_str(), strlen(temp.c_str()) + 1);
				}

				//获取sip密码
				if(result.isMember("password"))
				{
					string temp = result["password"].asString();
					UA_UTIL::DecodeBase64RC4(temp.c_str(), pAccInfo->chSipPwd, api_key);
					DEBUGCODE(Tracer::LogA("[AM_WSQuerySipAccount] [src]:[%s] [dst]:[%s], [key]:[%s]", temp.c_str(), pAccInfo->chSipPwd, api_key);)
				}

				//获取sip服务器地址
				if(result.isMember("address"))
				{
					string temp = result["address"].asString();
					strncpy(pAccInfo->chSipServer, temp.c_str(), strlen(temp.c_str()) + 1);
				}

				//获取sip服务器端口
				string strport;
				if(result.isMember("port"))
				{
					strport = result["port"].asString();
				}

				//获取加密信令端口和秘钥
				pAccInfo->nSipEncriptMethod = 0;
				string strsport;
				if(result.isMember("sport"))
				{

					string strsport = result["sport"].asString();

					if (strsport.empty() == false  && result.isMember("skey"))
					{
						string temp = result["skey"].asString();

						//文档修改，secretsipkey不做加密
						// 2015-06-16
						strcpy(pAccInfo->chSecretSipKey,temp.c_str());
						pAccInfo->nSecretSipKeyLen = strlen(pAccInfo->chSecretSipKey);
						DEBUGCODE(Tracer::LogA("[AM_WSQuerySipAccount] sipkey [src]:[%s] [dst]:[%s], [key]:[%s]", temp.c_str(), pAccInfo->chSecretSipKey, api_key);)
						pAccInfo->nSipEncriptMethod = 3;
						strcpy(pAccInfo->chSSipServer, pAccInfo->chSipServer);
						strcat(pAccInfo->chSSipServer, ":");
						strcat(pAccInfo->chSSipServer, strsport.c_str());
					}

				}

				//媒体参数暂时copy信令参数
				pAccInfo->nMediaEncriptMethod = pAccInfo->nSipEncriptMethod;
				strcpy(pAccInfo->chSecretMediaKey, pAccInfo->chSecretSipKey);
				pAccInfo->nSecretMediaKeyLen = pAccInfo->nSecretSipKeyLen;
				pAccInfo->nMediaEncriptMethod = 0;
				if (pAccInfo->nSipEncriptMethod > 0)
				{
					if (configManager)
					{
						configManager->SetNetConfValue(SIPEncrip, _T("1"));
					}

				}
				else
				{
					if (configManager)
					{
						configManager->SetNetConfValue(SIPEncrip, _T("0"));
					}
				}


				if (pAccInfo->nMediaEncriptMethod > 0)
				{
					if (configManager)
					{
						configManager->SetNetConfValue(MediaEncrip, _T("1"));
					}
				}
				else
				{
					if (configManager)
					{
						configManager->SetNetConfValue(MediaEncrip, _T("1"));
					}
				}

				if (strlen(strport.c_str()) > 0)
				{
					strcat(pAccInfo->chSipServer, ":");
					strcat(pAccInfo->chSipServer, strport.c_str());
				}
				strcpy(pAccInfo->chSipProxy, pAccInfo->chSipServer);
				strcpy(pAccInfo->chSSipProxy, pAccInfo->chSSipServer);

				//获取自己的群内短号
				if(result.isMember("cornet"))
				{
					string temp = result["cornet"].asString();
					strncpy(pAccInfo->chCornet, temp.c_str(), strlen(temp.c_str()) + 1);
				}

				//获取自己的出群前缀
				if(result.isMember("prefix"))
				{
					string temp = result["prefix"].asString();
					strncpy(pAccInfo->chPrex, temp.c_str(), strlen(temp.c_str()) + 1);
				}
				return TRUE;
			}
		}
	}
	return FALSE;
}

//处理正常登录过程
void AM_ProcessNormalLogin(ACCINFO *loginInfo)
{
	DEBUGCODE(Tracer::Log(_T("[AM_ProcessNormalLogin] enter"));)
	CUserAgentCore* ua = CUserAgentCore::GetUserAgentCore();
	////////////////////////////////////////////////////////////////////////////////////
	// 获取账号信息 - 之前通过WS已经获取到所有登录所需信息
	CString name, realm, passwd;
	//用户名
	Char2CString(loginInfo->chAccount, name);
	//WS服务器
	Char2CString(loginInfo->chRealm, realm);
	//密码
	Char2CString(loginInfo->chPassWord, passwd);
	//Mqtt用户名
	Char2CString(loginInfo->chMqttAccount, ua->selfGuid);
	//登录时间
	CTime logintime = CTime::GetCurrentTime();

	//查询SIP账号信息, 将相关信息补充至loginInfo中, 顺序不能乱解决出局码问题
	bool getSip = AM_WSQuerySipAccount(realm, ua->selfGuid, loginInfo->chAuthToken, loginInfo->chAPIKey, loginInfo);
	//与SIP相关的数据已经获取到, 向Account数据库更新
	ua->UpdateAccount(loginInfo);
	
	//更新当前用户状态为在线状态
	USER_STATUS status;
	status.code=TELPO_PRE_PC_ONLINE;
	status.des=_T("");
	ua->m_curAccount.SetStatus(status);

	//获取当前登录用户的通讯录信息
	if(g_loginData.hGetUserInfoThread)
	{
		if (WaitForSingleObject(g_loginData.hGetUserInfoThread, 3000) == WAIT_TIMEOUT)
		{
			DEBUGCODE(Tracer::LogA("[WARNING][AM_ProcessNormalLogin] TerminateThread: GetCurUserPBInfoThread");)
			TerminateThread(g_loginData.hGetUserInfoThread, 1);
		}
		CloseHandle(g_loginData.hGetUserInfoThread);
	}
	g_loginData.hGetUserInfoThread = CreateThread(NULL, 0, GetCurUserPBInfoThread, NULL, 0, NULL);

	//SIP服务器互通检测
	if (getSip)
	{
		int count = 0;
		BOOL checkedIP = FALSE;
		CSingleLock lock(&g_loginData.csIPCheckcs);
		do
		{
			lock.Lock();
			checkedIP = g_loginData.bCheckedIP;
			lock.Unlock();
			if (checkedIP)
			{
				break;
			}
			count++;
			Sleep(200);
		} while (checkedIP == FALSE && count < 3);
	}

	//初始化UA
	if(ua->IsInitialized() == 0)
		ua->InitUA();

	//设置用户SIP账号信息
	if(getSip)
	{
		int profId = 0;
		if (loginInfo->nSipEncriptMethod > 0)
		{
			profId = ua->SetUserProfile(loginInfo->chSipAccount
				,loginInfo->chSipPwd
				,loginInfo->chSSipServer
				,"FNNYN-LNLH-67RR-SCS4-FJF3"
				,loginInfo->chSipAccount
				,loginInfo->chNickname);
		}
		else
		{
			profId = ua->SetUserProfile(loginInfo->chSipAccount
				,loginInfo->chSipPwd
				,loginInfo->chSipServer
				,"FNNYN-LNLH-67RR-SCS4-FJF3"
				,loginInfo->chSipAccount
				,loginInfo->chNickname);
		}

		ua->GetLinphoneManager()->SetCurProfileId(profId);
	}

	::WaitForSingleObject(g_loginData.hGetUserInfoThread, 5000);

	//执行数据加载流程
	LoadICTData();
}

//离线登录
void AM_OfflineLogin(ACCINFO *accountInfo)
{
	CUserAgentCore* ua = CUserAgentCore::GetUserAgentCore();

	//判断个人配置文件中的用户信息和用户输入的信息是否一致
	CString name, pwd, realm;
	Char2CString(accountInfo->chAccount, name);
	Char2CString(accountInfo->chPassWord, pwd);
	Char2CString(accountInfo->chRealm, realm);

	//获取用户信息
	CAccountInfo account;
	CAccountDBManager *accountManager = ua->GetAccountDBManager();
	bool findAccount = accountManager->GetAccount(account);
	if(findAccount)
	{
		if(!pwd.IsEmpty()  && pwd == account.GetPwd())
		{
			//离线登录回调
			if(g_loginData.uiPara[0].cb)
				g_loginData.uiPara[0].cb(LOGIN_OFFLINE_AUTH_SUCC, NULL, g_loginData.uiPara[0].user_data);

			//离线登录成功标志
			AM_UpdateLoginState(LOGIN_STA_OFFLINE, LOGIN_SUCCESS, true);

			//更新UA中的当前用户的uid
			Char2CString(account.GetAccInfo()->chMqttAccount, ua->selfGuid);

			//执行数据加载流程
			LoadICTData();
		}
		else
		{
			//离线登录失败标志
			AM_UpdateLoginState(LOGIN_STA_OFFLINE, LOGIN_SUCCESS, false);

			if(g_loginData.uiPara[0].cb)
				g_loginData.uiPara[0].cb(LOGIN_OFFLINE_AUTH_FAIL, NULL, g_loginData.uiPara[0].user_data);
		}
	}
	else
	{
		//离线登录失败标志
		AM_UpdateLoginState(LOGIN_STA_OFFLINE, LOGIN_SUCCESS, false);

		if(g_loginData.uiPara[0].cb)
			g_loginData.uiPara[0].cb(LOGIN_OFFLINE_AUTH_FAIL, NULL, g_loginData.uiPara[0].user_data);
	}
}

//账号认证结果处理
DWORD WINAPI ProcessLoginResThread(LPVOID lpParameter)
{
	struct LOGINST
	{
		ACCINFO *pAccInfo;
		int result;
	};

	LOGINST *pLoginSt = (LOGINST *)lpParameter;
	if(pLoginSt)
	{
		//获取账号信息
		ACCINFO *accountInfo = pLoginSt->pAccInfo;
		int result = pLoginSt->result;
		delete pLoginSt;
		pLoginSt = NULL;

		if(accountInfo)
		{
			//设置token未过期的标志
			g_loginData.bTokenExpire = false;

			//设置一次、二次、三次登录未进行标志
			g_loginData.bLogin[0] = g_loginData.bLogin[1] = g_loginData.bLogin[2] = false;

			if(result == 0)
			{
				//网络连接正常、认证正常、获取数据正常（进行正常登录）
				//设置登录状态
				g_loginData.loginState.enLoginType = NETWORK_LOGIN;
				AM_UpdateLoginState(LOGIN_STA_NETWORK, LOGIN_SUCCESS, true);
				//进行正常登录流程 - 账号信息继续传递
				AM_ProcessNormalLogin(accountInfo);
			}
			else if(result == 2)
			{
				//网络连接异常（进行离线登录）
				//设置登录状态
				g_loginData.loginState.enLoginType = OFFLINE_LOGIN;
				//进行离线登录
				if(g_loginData.uiPara[0].cb)
					//之后考虑检查登录失败状态
					g_loginData.uiPara[0].cb(LOGIN_NETWORK_DISCONNECT, NULL, g_loginData.uiPara[0].user_data);
			}
			else
			{
				//网络连接正常、认证失败、获取数据异常（返回登录界面）
				//设置登录状态
				g_loginData.loginState.enLoginType = NETWORK_LOGIN;
				AM_UpdateLoginState(LOGIN_STA_NETWORK, LOGIN_SUCCESS, false);
				//返回登录界面
				if(g_loginData.uiPara[0].cb)
					//之后考虑检查登录失败状态
					g_loginData.uiPara[0].cb(LOGIN_NETWORK_AUTH_FAIL, NULL, g_loginData.uiPara[0].user_data);
			}

			//更改日志文件的名称
			if(result == 0 || result == 2)
			{
				char filename[MAX_PATH] = {0};
				Tracer::GetCurFileName(filename);
				if(!strchr(filename, '@'))
				{
					char *p1 = strrchr(filename, '.');
					if(p1)
					{
						char title[MAX_PATH] = {0}, ext[10] = {0}, add[128] = {0};
						memcpy(title, filename, p1-filename), strcpy(ext, p1);
						sprintf(add, "_%s@%s", accountInfo->chAccount, accountInfo->chRealm);
						char *p2 = strchr(add, ':');
						if(p2)
							*p2 = '-';
						strcpy(filename, title), strcat(filename, add), strcat(filename, ext);
						Tracer::RenameCurFile(filename);
					}
				}
			}

			//释放账号信息
			if(accountInfo)
			{
				delete accountInfo;
				accountInfo = NULL;
			}
		}
	}
	return 0;
}

//网页服务接口认证结果处理
int LoginWSAuthProc(int nStatus, char* szResonse, int dwSize, char* contenttype, char* szEtag, void* pActData)
{
	//获取token完毕
	int result = 0;

	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	BOOL bSecure = ConfigDataManage::m_appConfig.bSecure;

	//获取账号信息
	CXcapTool::LPXCAPTOOLDATA pHttpData = (CXcapTool::LPXCAPTOOLDATA)pActData;
	ACCINFO* accountInfo = (ACCINFO*)pHttpData->dwUserData;

	if (nStatus == HTTP_STATUS_OK)
	{
		if(!szResonse || strlen(szResonse)==0)
		{
			DEBUGCODE(Tracer::Log(_T("[LoginWSAuthProc] response is empty"));)
			result = -1;
		}
		else
		{
			DEBUGCODE(Tracer::LogToConsoleA(("[LoginWSAuthProc] response: %s"), szResonse);)
			string strJson;
			strJson.assign(szResonse);
			ICTDBLOGA("[POST::LoginWSAuthProc] : response = %s", strJson.c_str());
			Json::Value root;
			Json::Reader reader;
			if (reader.parse(strJson, root))
			{
				if (!root.isMember("status"))
				{
					DEBUGCODE(Tracer::LogA("[LoginWSAuthProc] ERROR json string is invalid, no [status]\n%s", strJson.c_str());)
					CELGGWebService::ProcessWebServiceError(root);
					result = -1;
				}
				else
				{
					if (root.isMember("result"))
					{
						Json::Value result = root["result"];

						//获取token
						if(result.isMember("auth_token"))
						{
							string auth_token = result["auth_token"].asString();
							SafeStringCopy(accountInfo->chAuthToken, auth_token.c_str());
							SafeStringCopy(accountInfo->chMqttAuthToken, auth_token.c_str());
							DEBUGCODE(Tracer::LogA("[LoginWSAuthProc] auth_token: %s", accountInfo->chAuthToken);)
						}

						if (result.isMember("expires"))
						{
							LONGLONG expires = 0;
							stringstream ss(result["expires"].asString());
							ss >> expires;
							g_loginData.tokenExpireTime = expires;
						}

						//获取uid
						if(result.isMember("uid"))
						{
							//要是网络登录, 这时候才拿到正确的Mqtt账号
							string uid = result["uid"].asString();
							SafeStringCopy(accountInfo->chMqttAccount, uid.c_str());
						}

						//获取eid
						if (result.isMember("eid"))
						{
							//要是网络登录, 这时候才拿到正确的企业用户账号
							string eid = result["eid"].asString();
							SafeStringCopy(accountInfo->chEid, eid.c_str());
						}

						//获取eguid
						if(result.isMember("eguid"))
						{
							string eguid = result["eguid"].asString();
							SafeStringCopy(accountInfo->chEguid, eguid.c_str());
						}

						//获取mqtt_ip
						if (result.isMember("mqtt_ip"))
						{
							string mqttSvr = result["mqtt_ip"].asString();
							SafeStringCopy(accountInfo->chMqttServer, mqttSvr.c_str());
						}

						//获取meeting_ip
						if (result.isMember("meeting_ip"))
						{
							string confSvr = result["meeting_ip"].asString();
							string wsUrl = bSecure? ("https://" + confSvr + "/webservice/") : ("http://" + confSvr + "/ippbx/webservice/");
							SafeStringCopy(accountInfo->chGSoapServer, wsUrl.c_str());
						}

						//获取sip_ip
						if (result.isMember("sip_ip"))
						{
							string sipSvr = result["sip_ip"].asString();
							SafeStringCopy(accountInfo->chSipServer, sipSvr.c_str());
							SafeStringCopy(accountInfo->chSipProxy, accountInfo->chSipServer);
						}

						//获取offline_ip
						if(result.isMember("offline_ip"))
						{
							string offline_ip = result["offline_ip"].asString();
							SafeStringCopy(accountInfo->chOfflineMediaServer, offline_ip.c_str());
						}

						//获取update_ip
						if(result.isMember("update_ip"))
						{
							string update_ip = result["update_ip"].asString();
							if(bSecure)
								strcpy(accountInfo->chUpdateServer, "http://"), strcat(accountInfo->chUpdateServer, update_ip.c_str());
							else
								strcpy(accountInfo->chUpdateServer, "http://"), strcat(accountInfo->chUpdateServer, update_ip.c_str());
						}

						//获取book_version
						if (result.isMember("book_version"))
						{
							if(result["book_version"].isInt())
								g_loginData.stLDV.nEntVer = result["book_version"].asInt();
						}

						//获取role_version
						if(result.isMember("role_version"))
						{
							if(result["role_version"].isInt())
								g_loginData.stLDV.nRoleVer = result["role_version"].asInt();
						}

						//获取group_version
						if (result.isMember("group_version"))
						{
							if(result["group_version"].isInt())
								g_loginData.stLDV.nGroupVer = result["group_version"].asInt();
						}

						//获取lapp_version
						if(result.isMember("lapp_version"))
						{
							if(result["lapp_version"].isInt())
								g_loginData.stLDV.nLAPPVer = result["lapp_version"].asInt();
						}

						//成功返回
						result = 0;
					}
				}
			}
			else
			{
				DEBUGCODE(Tracer::LogA("[Rst_LoginElggProc] ERROR json string syntex error,\n%s",strJson.c_str());)
				result = -1;
			}
		}
	}
	else
	{
		//连接异常
		result = 2;
	}

	//部分账号信息（除了SIP相关的详细信息都已经获取到）, 向Account数据库更新
	//由于获取到了新book, role, group, ldap的版本号, 这里需要在向数据库更新之前, 对版本进行判断并设置更新参数
	CAccountDBManager* accountDBManager = ua->GetAccountDBManager();
	CAccountInfo account;
	//真正的账号的所有数据
	accountDBManager->GetAccount(account);
	//判断程序更新信息
	//1. 企业通讯录更新
	if(account.GetDataVer(DVT_ENT) != g_loginData.stLDV.nEntVer)
	{
		//企业通讯录版本不一致, 设置企业通讯录更新标识
		g_loginData.bEntPBVerChange = true;
	}
	//2. 应用更新
	//应用更新链接不一致, 设置应用更新标识
	g_loginData.bUpdateLinkChange = (strcmp(account.GetAccInfo()->chUpdateServer, accountInfo->chUpdateServer) != 0);
	if (!g_loginData.bUpdateLinkChange)
	{
		CConfigDBManager* configDBManager = ua->GetConfigManager();
		CAPPUser recentAccount;
		configDBManager->GetRecentUser(recentAccount);
		CString realm;
		Char2CString(accountInfo->chRealm, realm);
		if (recentAccount.GetRealm() != realm)
			//切换服务器，需重新检查升级
			g_loginData.bUpdateLinkChange = true;
	}

	//记录账号的新数据 - 在这之后就没有上一次该账号的基础数据了, 所以新旧数据检查要在这之间做
	ua->UpdateAccount(accountInfo);

	//认证通过, 账号信息继续向下一步传
	struct LOGINST
	{
		ACCINFO *pAccInfo;
		int result;
	};

	LOGINST *pLoginSt = new LOGINST;
	pLoginSt->pAccInfo = accountInfo;
	pLoginSt->result = result;
	//开始后续处理
	CreateThread(NULL, 0, ProcessLoginResThread, (void *)pLoginSt, 0, NULL);
	return result;
}

//在线登录入口, 网页服务接口认证
void AM_WSAuth(ACCINFO *accountInfo)
{
	if(!accountInfo)
		return;

	CString account = _T(""), realm = _T(""), pwd = _T(""), apiKey = _T("");
	Char2CString(accountInfo->chAccount, account);
	Char2CString(accountInfo->chRealm, realm);
	Char2CString(accountInfo->chPassWord, pwd);
	Char2CString(accountInfo->chAPIKey, apiKey);

	//通过异步WS获取token
	CELGGWebService webService;
	webService.m_szServer = realm;
	webService.m_bAddToken = webService.m_bAddAPIKey = false;
	//沈师版本为auth.gettoken1
	CString method = _T("auth.gettoken");
	CString authstr = webService.GenAuthStr(apiKey, account, pwd);
	MAPCString mapContent;
	mapContent[_T("name")] = account;
	//终端类型，1：企业门户；2：PC终端；3：移动终端
	mapContent[_T("flag")] = _T("2");
	mapContent[_T("password")] = pwd;
	mapContent[_T("api_key")] = apiKey;
	mapContent[_T("auth")] = authstr;

	//发出WS请求
	webService.Post(method, mapContent, LoginWSAuthProc, (DWORD)accountInfo);
}

inline BOOL AM_IsAuthTokenExpired()
{
	return (::_time64( NULL ) >= g_loginData.tokenExpireTime);
}

void UpdateAuthToken(BOOL bForceUpdate)
{
	if (!bForceUpdate && !AM_IsAuthTokenExpired())
		return;

	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	if (ua && ua->m_curAccount.GetName()!=_T(""))
	{
		CString szAccount = _T(""), szRealm = _T(""), szPwd = _T(""), szAPIKey = _T("");

		szAccount = ua->m_curAccount.GetName();
		szRealm = ua->m_curAccount.GetRealm();
		szPwd = ua->m_curAccount.GetPwd();

		Char2CString(ua->m_curAccount.GetAccInfo()->chAPIKey, szAPIKey);

		CELGGWebService webService;
		webService.m_szServer = szRealm;
		webService.m_bAddToken = webService.m_bAddAPIKey = false;
		CString strMethod = _T("auth.gettoken");
		MAPCString mapContent;
		mapContent[_T("name")] = szAccount;
		mapContent[_T("flag")] = _T("2"); //终端类型，1：企业门户；2：PC终端；3：移动终端
		mapContent[_T("password")] = szPwd;
		mapContent[_T("api_key")] = szAPIKey;
		webService.Post(strMethod, mapContent, UpdateAuthTokenProc, NULL, NULL);
	}
}

//获取Token回调函数
int UpdateAuthTokenProc(int nStatus, char* szResonse, int dwSize, char* contenttype, char* szEtag, void* pActData)
{
	//int result = 0;
	DEBUGCODE(Tracer::LogA("[UpdateAuthTokenProc] nStatus: %d", nStatus);)
	if (nStatus == HTTP_STATUS_OK)
	{
		if(!szResonse || strlen(szResonse) == 0)
		{
			DEBUGCODE(Tracer::Log(_T("[UpdateAuthTokenProc] no response."));)
			return -1;
		}
		else
		{
			DEBUGCODE(Tracer::LogToConsoleA(("[UpdateAuthTokenProc] [response]:%s"), szResonse);)
			string strJson;
			strJson.assign(szResonse);
			Json::Value root;
			Json::Reader reader;
			if (reader.parse(strJson, root))
			{
				if (!root.isMember("status"))
				{
					DEBUGCODE(Tracer::LogA("[UpdateAuthTokenProc] ERROR json string is invalid, no [status]\n%s", strJson.c_str());)
					CELGGWebService::ProcessWebServiceError(root);
					return -1;
				}
				else
				{
					if (root.isMember("result"))
					{
						Json::Value result = root["result"];

						//获取token
						if(result.isMember("auth_token"))
						{
							string auth_token = result["auth_token"].asString();
							CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();

							CAccountInfo account=ua->m_curAccount;
							if (account.GetName()!=_T(""))
							{
								SafeStringCopy(account.GetAccInfo()->chAuthToken, auth_token.c_str());
								char strUID[256]; //strPwd[256];
								CString2Char(ua->selfGuid, strUID);
								ua->GetMQTTManager()->SetAccount(strUID, account.GetAccInfo()->chAuthToken);
								ua->GetMQTTManager()->Login();
							}
						}

						if (result.isMember("expires"))
						{
							LONGLONG expires = 0;
							stringstream ss(result["expires"].asString());
							ss >> expires;
							g_loginData.tokenExpireTime = expires;
						}
					}	
				}				
			}
			else
			{
				DEBUGCODE(Tracer::LogA("[UpdateAuthTokenProc] ERROR json string syntex error,\n%s",strJson.c_str());)
				return -1;
			}
		}
		return 0;
	}
	return 2; //连接异常
}


int64 DateTimeToUnixepoch(CString strTime)
{
	try
	{
		int year, month, day, hour = 0, minute = 0, second = 0;
		_stscanf_s(strTime, _T("%d-%d-%d %d:%d:%d"), &year, &month, &day, &hour, &minute, &second);
		CTime time(year, month, day, hour, minute, second);
		return time.GetTime();
	}
	catch (...)
	{
		return 0;
	}
}


void LoadUnreadMessages()
{
	//IM最新消息结构体，用于提示窗口
	typedef struct _msg_receive_new
	{
		char8 uid[256]; //用户唯一UId
		int32 chatType; //聊天类型
		char8 msgContent[1024*2]; //消息内容
		int64 msgTime;//消息时间
		int32  unreadNum; //未读消息数量
	} MSG_RECEIVE_NEW, *PMSG_RECEIVE_NEW;

	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();

	bool bSoundTip = false; //是否进行声音提示

	list<CInstantMessage*> messagesAllUnread;
	ua->GetMessageManager()->GetUnreadMessages(_T(""), messagesAllUnread);

	PGList pHead = NULL;
	PGList pTail = NULL;
	PGList pNode = NULL;

	for (list<CInstantMessage*>::iterator itor = messagesAllUnread.begin(); itor != messagesAllUnread.end(); itor++)
	{
		CInstantMessage *msg = *itor;
		if (!msg || msg->dbMID == _T(""))
			continue;

		if (msg->chatType == CT_P2P)
		{
			if (!msg->isSyns || msg->targetUID == ua->selfGuid) //非同步消息或者接受者是自己进行提示
			{
				pNode = (PGList)malloc(sizeof(GList));
				pNode->data = malloc(sizeof(MSG_RECEIVE_NEW));
				PMSG_RECEIVE_NEW pmsgReceiveNew = (PMSG_RECEIVE_NEW)pNode->data;
				CString2Char(msg->targetUID, pmsgReceiveNew->uid);
				CString2Char(msg->FormatContent(), pmsgReceiveNew->msgContent);
				pmsgReceiveNew->msgTime = DateTimeToUnixepoch(msg->GetTime());
				pmsgReceiveNew->chatType = (int32)msg->chatType;
				pmsgReceiveNew->unreadNum = 1;

				////P2P消息提示
				//if(g_loginData.uiPara[0].cb)
				//	g_loginData.uiPara[0].cb(LOGIN_UNREAD_MSG_P2P_TIPS, pHead, g_loginData.uiPara[0].user_data);

				bSoundTip = true;
			}
		}
		else if (msg->chatType == CT_Group)
		{
			//群组消息提示
			GroupInfosMan* grpsMana = ua->GetGroupsManaTelpo();
			if (grpsMana)
			{
				CSingleLock lock(&(grpsMana->m_cs));
				lock.Lock();
				GroupInfo* pGroup = grpsMana->FindGroup(msg->targetUID);
				if (pGroup)
				{
					GroupType groupType = ConvetTelpoGT2GroupType(pGroup->GetGroupType());
					lock.Unlock();
					if(!msg->isSyns) //非同步消息进行提示
					{
						pNode = (PGList)malloc(sizeof(GList));
						pNode->data = malloc(sizeof(MSG_RECEIVE_NEW));
						PMSG_RECEIVE_NEW pmsgReceiveNew = (PMSG_RECEIVE_NEW)pNode->data;
						CString2Char(msg->targetUID, pmsgReceiveNew->uid);
						CString2Char(msg->FormatContent(), pmsgReceiveNew->msgContent);
						pmsgReceiveNew->msgTime = DateTimeToUnixepoch(msg->GetTime());
						pmsgReceiveNew->chatType = (int32)msg->chatType;
						pmsgReceiveNew->unreadNum = 1;
						if (pHead == NULL)
						{
							pHead = pNode;
							pTail = pNode;
						}
						else
						{
							pTail->next = pNode;
							pTail = pNode;
						}

	/*					if(g_loginData.uiPara[0].cb)
							g_loginData.uiPara[0].cb(LOGIN_UNREAD_MSG_GROUP_TIPS, NULL, g_loginData.uiPara[0].user_data);*/

						bSoundTip = true;
					}
				}
				else
				{
					lock.Unlock();
				}
			}
		}
		else if (msg->chatType == CT_PubAccount)
		{
			if(!msg->isSyns) //非同步消息进行提示
			{
				//公共账号消息提示

				bSoundTip = true;
			}
		}
	}

	//声音提醒
	if(bSoundTip)
	{
		//P2P消息提示
		if(g_loginData.uiPara[0].cb && pHead)
			g_loginData.uiPara[0].cb(LOGIN_UNREAD_MSG_P2P_TIPS, pHead, g_loginData.uiPara[0].user_data);
	}

	//销毁消息
	MESSAGE_MANAGER::RealseMessages(messagesAllUnread);
}

int32 AM_ConfigUser()
{
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	//配置用户数据库
	//获取应用配置管理器
	CConfigDBManager* configManager = ua->GetConfigManager();
	CAPPUser user;
	bool find = configManager->GetRecentUser(user);
	if (find)
	{
		//找到上次使用的用户, 进行用户账号配置
		//设置当前用户ID, 考虑到不支持多账号同时登录, 所以一次登录过程全局只调用一次
		//此接口还会创建PGM对象
		ua->SetUserID(user.GetUserID());

		//登录用户信息存在, 进行后续操作
		//创建用户账号目录 - 此操作使用了cf中存储的用户id, 所以此方法必须要在cf数据库操作成功之后调用
		AM_CreateUserPaths(user.GetName(), user.GetRealm());

		//进行账号数据存储 - 向Account数据库中存储账号相关初始数据
		CAccountDBManager *accountDBManager = ua->GetAccountDBManager();
		CAccountInfo account = CAccountInfo(user.GetName(), user.GetRealm(), _T(""), false, false);
		account.SetUserID(user.GetUserID());
		bool getAccount = accountDBManager->GetAccount(account);
		if (getAccount)
		{
			//已经存在该账号, 更新当前账号数据
			ua->m_curAccount = account;

			//更新当前用户状态为在线状态
			USER_STATUS status;
			status.code = TELPO_PRE_PC_ONLINE;
			status.des = _T("");
			ua->m_curAccount.SetStatus(status);
		}

		return user.GetUserID();
	}

	return -1;
}


int32 AM_ConfigUser(PGList* pacclist)
{
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	//配置用户数据库
	//获取应用配置管理器
	CConfigDBManager* configManager = ua->GetConfigManager();
	
	BOOL bExist = FALSE;
	CAPPUser user;
	bool find = configManager->GetRecentUser(user);
	if (find)
	{
		//找到上次使用的用户, 进行用户账号配置
		//设置当前用户ID, 考虑到不支持多账号同时登录, 所以一次登录过程全局只调用一次
		//此接口还会创建PGM对象
		ua->SetUserID(user.GetUserID());

		//登录用户信息存在, 进行后续操作
		//创建用户账号目录 - 此操作使用了cf中存储的用户id, 所以此方法必须要在cf数据库操作成功之后调用
		AM_CreateUserPaths(user.GetName(), user.GetRealm());

		//进行账号数据存储 - 向Account数据库中存储账号相关初始数据
		CAccountDBManager *accountDBManager = ua->GetAccountDBManager();
		CAccountInfo account = CAccountInfo(user.GetName(), user.GetRealm(), _T(""), false, false);
		account.SetUserID(user.GetUserID());
		bool getAccount = accountDBManager->GetAccount(account);
		if (getAccount)
		{
			bExist = TRUE;
			//已经存在该账号, 更新当前账号数据
			ua->m_curAccount = account;

			//更新当前用户状态为在线状态
			USER_STATUS status;
			status.code = TELPO_PRE_PC_ONLINE;
			status.des = _T("");
			ua->m_curAccount.SetStatus(status);
		}
	}

	list<CAPPUser> userList = configManager->GetUsers();
	PGList pList = NULL;
	for (list<CAPPUser>::iterator iter=userList.begin(); iter!=userList.end(); iter++)
	{
		CAccountInfo curAccount(iter->GetName(), iter->GetRealm(), _T(""), false, true);
		if (bExist && iter->GetName() == ua->m_curAccount.GetName())
		{
			curAccount.SetAutoLogin(ua->m_curAccount.GetAutoLogin());
			curAccount.SetPwd(ua->m_curAccount.GetPwd());
		}

		curAccount.SetUserID(iter->GetUserID());
		//ua->GetAccountById(iter->GetUserID(), curAccount);
		/*CAccountDBManager *accountManager = ua->GetAccountDBManager();
		accountManager->GetAccount(curAccount);*/
		ICTDBLOGA("[Login::AM_ConfigUser]:setaccount");
		PICTACCINFO pICTInfo = (PICTACCINFO)malloc(sizeof(ICTACCINFO));
		memset(pICTInfo, 0, sizeof(ICTACCINFO));
		strcpy_s(pICTInfo->uid,curAccount.GetAccInfo()->chMqttAccount);
		CString2Char(curAccount.GetName(), pICTInfo->acc);
		CString2Char(curAccount.GetPwd(), pICTInfo->pwd);
		CString2Char(curAccount.GetRealm(), pICTInfo->realm);
		pICTInfo->saveAccInfo = curAccount.GetRemPwd();
		pICTInfo->autologin = curAccount.GetAutoLogin();
		pICTInfo->autoRun = curAccount.GetAutoRun();

		

		if(!(*pacclist))
		{
			(*pacclist) = pList = (PGList)malloc(sizeof(GList));
			(*pacclist)->data = pICTInfo;
		}
		else
		{
			PGList pTemp = pList;
			pList = (PGList)malloc(sizeof(GList));
			pTemp->next = pList;
			pList->data = pICTInfo;
		}
	}
	if (pList)
		pList->next = NULL;

	if (find)
	{
		return user.GetUserID();
	}

	return -1;
}

//账号配置
//只用于登录是创建用户配置信息（创建用户文件目录及相关数据库）
int32 AM_ConfigUser(CString name, CString pwd, CString realm, bool savePwd, bool autoLogin, bool autoRun, CTime time)
{
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	//配置用户数据库
	//获取应用配置管理器
	CConfigDBManager* configManager = ua->GetConfigManager();
	//在cf数据库中创建或者更新用户登录数据
	int userId = configManager->CreateUser(name, realm, time);
	if (userId != -1)
	{
		//设置当前用户ID, 考虑到不支持多账号同时登录, 所以一次登录过程全局只调用一次
		//此接口还会创建PGM对象
		ua->SetUserID(userId);

		//登录用户信息存在, 进行后续操作
		//创建用户账号目录 - 此操作使用了cf中存储的用户id, 所以此方法必须要在cf数据库操作成功之后调用
		AM_CreateUserPaths(name, realm);

		//进行账号数据存储 - 向Account数据库中存储账号相关初始数据
		CAccountDBManager *accountDBManager = ua->GetAccountDBManager();
		CAccountInfo account = CAccountInfo(name, realm, pwd, autoLogin, savePwd);
		account.SetUserID(userId);
		bool getAccount = accountDBManager->GetAccount(account);
		if (!getAccount)
		{
			//Account数据库中没有获取到该用户信息, 应该是新用户或者数据记录错误
			//数据记录错误这种情况应该不会出现, 除非之前此账号登录时出现了异常, 导致Account数据库中数据插入失败
			//针对以上情况, 添加该用户信息
			// - 此时没有更多账号信息了, 因此记录基本信息
			ua->RecordAccount(&account);
		}
		else
		{
			ua->UpdateAccount(account);
			//已经存在该账号, 更新当前账号数据
			ua->m_curAccount = account;
		}

		//更新当前用户状态为在线状态
		USER_STATUS status;
		status.code = TELPO_PRE_PC_ONLINE;
		status.des = _T("");
		ua->m_curAccount.SetStatus(status);
	}
	
	return 1;
}

//用户登录
int32 AM_Login(int32 bOnline, void* userdata, pLoginCallback cb)
{
	ICTACCINFO* acInfo = (ICTACCINFO*)userdata;

	if(strlen(acInfo->acc) <= 0 || strlen(acInfo->pwd) <= 0 || strlen(acInfo->realm) <= 0)
	{
		cb(INVALID_DATA, NULL, userdata);
		return -1;
	}

	DEBUGCODE(Tracer::LogA("[Login] [account]: %s, [server]: %s", acInfo->acc, acInfo->realm);)

	//填充账号信息
	ACCINFO* accountInfo = new ACCINFO;
	memset(accountInfo, 0, sizeof(ACCINFO));
	//如果首次登录, Mqtt账号信息没这么早能拿到. 要是历史账号, 完全可以在使用的时候再获取
	//strcpy(accountInfo->chMqttAccount, acInfo->uid);
	strcpy(accountInfo->chAccount, acInfo->acc);
	strcpy(accountInfo->chPassWord, acInfo->pwd);
	strcpy(accountInfo->chRealm, acInfo->realm);
	accountInfo->bSaveAccountInfo = (acInfo->saveAccInfo == TRUE);
	accountInfo->bAutoLogin = (acInfo->autologin == TRUE);
	//设置API Key
	strcpy(accountInfo->chAPIKey, "36116967d1ab95321b89df8223929b14207b72b1");
	
	//记录登录信息
	g_loginData.uiPara[0].user_data = userdata;
	g_loginData.uiPara[0].cb = cb;

	if(bOnline)
		//在线登录入口, 网页服务接口认证
		AM_WSAuth(accountInfo);
	else
		//离线登录入口
		AM_OfflineLogin(accountInfo);

	return 1;
}

DWORD WINAPI MQTTLogoffThread(LPVOID lpParameter)
{
	DEBUGCODE(Tracer::LogA(__FILE__"(%d): MQTT Logoff", __LINE__);)
		CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	ua->GetMQTTManager()->Logoff();
	return 0;
}

int32 Logoff(void* userdata, pLoginCallback cb)
{
	g_loginData.uiPara[1].user_data = userdata;
	g_loginData.uiPara[1].cb = cb;

	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	if(g_loginData.loginState.enLoginType == NETWORK_LOGIN)
	{
		//联网登录
		if(g_loginData.loginState.lNetworkLoginSta & SIP_CONNECT_SUCCESS)
		{
			//断开SIP的连接
			int nProfID = ua->GetLinphoneManager()->GetCurProfileId();
			if (ua->IsProfileRegistered(nProfID))
			{
				DEBUGCODE(Tracer::LogA(__FILE__"(%d): SIP Logoff", __LINE__);)
					ua->UnregisterProfile(nProfID);
				//add by zhangpeng on 20160808 begin
				//断开sip，则需将sip连接标志断开，否则检查不出是否连接
				g_loginData.loginState.lNetworkLoginSta = g_loginData.loginState.lNetworkLoginSta &0xfffb;//将第三位sip连接标志位清零
				//add by zhangpeng on 20160808 end
			}
		}
		if(g_loginData.loginState.lNetworkLoginSta & MQTT_CONNECT_SUCCESS)
		{
			//断开MQTT连接

			/* Mark
				*  @author     guanfm       2016/05/18 09:25
				*  @brief      退出无响应临时解决办法，因注销时有时会在MQTT的Logoff一直卡住
			*/
			HANDLE hThread = CreateThread(NULL, 0, MQTTLogoffThread, NULL, 0, NULL);
			if (WaitForSingleObject(hThread, 3000) == WAIT_TIMEOUT)
			{
				DEBUGCODE(Tracer::LogA("[WARNING][Logoff] TerminateThread: MQTTLogoffThread");)
				::TerminateThread(hThread, 1);
			}
			CloseHandle(hThread);
		}
	}
	else if(g_loginData.loginState.enLoginType == OFFLINE_LOGIN)
	{
		//离线登录
	}

	//终止当前正在进行的线程
	DEBUGCODE(Tracer::LogA(__FILE__"(%d): TerminateAllThread", __LINE__);)
		TerminateAllThread();
	DEBUGCODE(Tracer::LogA(__FILE__"(%d): StopAllServerThread", __LINE__);)
		StopAllServerThread();
	DEBUGCODE(Tracer::LogA(__FILE__"(%d): InitData", __LINE__);)
		//把登陆状态置为初始状态
		g_loginData.InitData();
	g_loginData.loginState.InitState();

	//保存通讯录文件
	CPhBkGroupManager *managerEPB = ua->GetPGMManager()->GetPhBkManager(ua->GetUserID(), PB_ENTERPRISE_ADDRESS_BOOK);
	if(managerEPB)
		managerEPB->SaveToFile();

	CPhBkGroupManager *managerPPB = ua->GetPGMManager()->GetPhBkManager(ua->GetUserID(), PB_BUDDIES);
	if(managerPPB)
		managerPPB->SaveToFile();

	//释放核心类
	// 	if(g_loginData.pUserAgentCore)
	// 		delete g_loginData.pUserAgentCore;
	DEBUGCODE(Tracer::LogA(__FILE__"(%d): Logoff return", __LINE__);)
		return 0;
}

void TerminateAllThread()
{
	//终止轻应用获取线程
	if(g_loginData.hLoadLAPPThread)
	{
		if (WaitForSingleObject(g_loginData.hLoadLAPPThread, 400) == WAIT_TIMEOUT)
		{
			DEBUGCODE(Tracer::LogA("[WARNING][TerminateAllThread] TerminateThread: LoadLAPPThread");)
			::TerminateThread(g_loginData.hLoadLAPPThread, 1); //可能会引起死锁
		}
		CloseHandle(g_loginData.hLoadLAPPThread);
		g_loginData.hLoadLAPPThread = NULL;
	}

	//终止通讯录获取线程
	if(g_loginData.hLoadPBThread)
	{
		if (WaitForSingleObject(g_loginData.hLoadPBThread, 400) == WAIT_TIMEOUT)
		{
			DEBUGCODE(Tracer::LogA("[WARNING][TerminateAllThread] TerminateThread: LoadPBThread");)
			::TerminateThread(g_loginData.hLoadPBThread, 1); //可能会引起死锁
		}
		CloseHandle(g_loginData.hLoadPBThread);
		g_loginData.hLoadPBThread = NULL;
	}

	//终止群组获取线程
	if(g_loginData.hLoadGroupThread)
	{
		if (WaitForSingleObject(g_loginData.hLoadGroupThread, 400) == WAIT_TIMEOUT)
		{
			DEBUGCODE(Tracer::LogA("[WARNING][TerminateAllThread] TerminateThread: LoadGroupThread");)
			::TerminateThread(g_loginData.hLoadGroupThread, 1); //可能会引起死锁
		}
		CloseHandle(g_loginData.hLoadGroupThread);
		g_loginData.hLoadGroupThread = NULL;
	}

	//终止最近联系人获取线程
	if(g_loginData.hLoadRecentContactThread)
	{
		if (WaitForSingleObject(g_loginData.hLoadRecentContactThread, 400) == WAIT_TIMEOUT)
		{
			DEBUGCODE(Tracer::LogA("[WARNING][TerminateAllThread] TerminateThread: LoadRecentContactThread");)
			::TerminateThread(g_loginData.hLoadRecentContactThread, 1); //可能会引起死锁
		}
		CloseHandle(g_loginData.hLoadRecentContactThread);
		g_loginData.hLoadRecentContactThread = NULL;
	}

	//终止通话记录获取线程
	if(g_loginData.hLoadCallRecordThread)
	{
		if (WaitForSingleObject(g_loginData.hLoadCallRecordThread, 400) == WAIT_TIMEOUT)
		{
			DEBUGCODE(Tracer::LogA("[WARNING][TerminateAllThread] TerminateThread: LoadCallRecordThread");)
			::TerminateThread(g_loginData.hLoadCallRecordThread, 1); //可能会引起死锁
		}

		CloseHandle(g_loginData.hLoadCallRecordThread);
		g_loginData.hLoadCallRecordThread = NULL;
	}
}

//取轻应用列表
int32 GetLAPPList(PGList* clist)
{
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	if(ua == NULL) return S_INVALIDPARAM;
	if(ua->GetPGMManager() == NULL) return S_INVALIDPARAM;
	//双击轻应用节点
	CLAPPManager* pLAPPMan = ua->GetPGMManager()->GetLAPPManager(ua->GetUserID());
	if(pLAPPMan == NULL)return S_INVALIDPARAM;

	GLSetHead
	for(list<CLAPPItem*>::const_iterator iter = pLAPPMan->m_listItem.begin(); iter != pLAPPMan->m_listItem.end(); iter++)
	{
		CLAPPItem* pItem = (CLAPPItem*)(*iter);
		pNode = (PGList)malloc(sizeof(GList));
		pNode->data = malloc(sizeof(LAPP_UI_DATA));
		PLAPP_UI_DATA pLapp = (PLAPP_UI_DATA)pNode->data;
		CString2Char(pItem->m_szId,pLapp->uid);
		CString2Char(pItem->m_szName,pLapp->name);
		CString2Char(pItem->m_szDesc,pLapp->desc);
		pLapp->show_way = pItem->m_showWay;
		GLSetTail
	}
	return S_SUC;
}


//取轻应用 url TODO需去列表
int32 GetLAPPUrl(char8* uid, char8* url, BOOL bRecentContact)
{
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	bool bLocal = false;
	if(ua == NULL) return S_INVALIDPARAM;
	if(ua->GetPGMManager() == NULL) return S_INVALIDPARAM;

	UpdateAuthToken();

	CString strUid, strUrl;
	Char2CString(uid, strUid);

	if (bRecentContact)
	{
		CRecentContact contact;
		//获取最近联系人信息
		ua->GetMessageManager()->GetRecentContact(strUid, contact);

		string strJson;
		ConvertStringToUTF8(contact.lastMessage->messageExten, strJson);

		Json::Reader reader;
		Json::Value root;
		if (reader.parse(strJson, root))
		{
			CString message;
			if (root.type() == Json::objectValue && root.isMember("url"))
			{
				string sMessage = root["url"].asString();
				Char2CString(sMessage.c_str(), strUrl);
			}
		}
	}
	else
	{
		CLAPPManager* pLAPPMan = ua->GetPGMManager()->GetLAPPManager(ua->GetUserID());
		if(pLAPPMan)
		{
			CLAPPItem* pItem = pLAPPMan->GetLAPPItemById(strUid);
		
			if(pItem->m_WebType == 0)//web应用
			{
				strUrl = pItem->m_szUrl;
			}
			else if(pItem->m_WebType == 1)//本地应用
			{
				//构造本地路径
				CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
				strUrl = pItem->getRePoPath() + pItem->m_szUrl;
				bLocal = true;
			}
			else if(pItem->m_WebType == 2) //本地h5 + web应用 
			{
				//构造本地路径
				CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
				strUrl = pItem->getRePoPath() + pItem->m_szUrl;
				bLocal = true;
			}
		}
	}

	if (!strUrl.IsEmpty())
	{
		ValidateUrl(strUrl, bLocal);
		CString2CharSafe(strUrl, url, 8192);
		return S_SUC;
	}

	return S_FAILED;
}

void ValidateUrl(CString& strUrl, bool bLocal)
{
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();

	if (strUrl.Find(_T("http://")) == -1 && strUrl.Find(_T("https://")) == -1 && !bLocal)
	{
		CString strProtocal = ConfigDataManage::m_appConfig.bSecure ? _T("https://") : _T("http://");
		strUrl = strProtocal + ua->m_curAccount.GetRealm() + strUrl;
	}

	int nPos;
	CString strEguid, strToken;
	CString strKeyUid = _T("uid="), strKeyEguid = _T("eguid="), strKeyToken = _T("auth_token=");
	Char2CString(ua->m_curAccount.GetAccInfo()->chEguid, strEguid);
	Char2CString(ua->m_curAccount.GetAccInfo()->chAuthToken, strToken);
	nPos = strUrl.Find(strKeyUid);
	if (nPos > 0) {
		strUrl.Insert(nPos + strKeyUid.GetLength(), ua->selfGuid);
	}

	nPos = strUrl.Find(strKeyEguid);
	if (nPos > 0) {
		strUrl.Insert(nPos + strKeyEguid.GetLength(), strEguid);
	}

	nPos = strUrl.Find(strKeyToken);
	if (nPos > 0) {
		strUrl.Insert(nPos + strKeyToken.GetLength(), strToken);
	}
}


//下载轻应用到内存 通过GetLAPPUrl获取，同时回调UI更新消息更新轻应用
int32 DownloadLAPP(pWSCallback cb)
{
	WSDownloadLAPP* param = (WSDownloadLAPP*)malloc(sizeof(WSDownloadLAPP));
	memset(param,0,sizeof(WSDownloadLAPP));
	param->type = ICTWS_DownloadLAPP;
	param->userdata = NULL;
	param->cb = cb;
	param->at = AT_Ent;
	return StartWSWorkerThread(NULL,(LPARAM)param);
}

//获取出局码
int32 GetPrefix(char8* prefix)
{
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	if(ua == NULL) return S_INVALIDPARAM;

	if(ua->m_curAccount.GetName()!=_T(""))
	{
		int nsize = sizeof(ua->m_curAccount.GetAccInfo()->chPrex);
		//memcpy_s(prefix,nsize,ua->m_curAccount.GetAccInfo()->chPrex,nsize);
		strncpy(prefix,ua->m_curAccount.GetAccInfo()->chPrex,nsize+1);
		return S_SUC;
	}
	else
	{
		return S_FAILED;
	}
}

//获取用户目录
int32 GetCurPsersonalDir(char8* dir)
{
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	if(ua == NULL) return S_INVALIDPARAM;
	CString strPersonalDir = ua->GetUserPath(ua->GetUserID());
	CString2CharSafe(strPersonalDir,dir, MAX_PATH);
	return S_SUC;
}

//------ 升级检测处理 start
//升级检测线程启动
void StartUpdateCheckThread(pWSCallback cb)
{
	//暂时停止升级检测
	//return;
	if (g_loginData.hUpdateCheckThread)
	{
		if (WaitForSingleObject(g_loginData.hUpdateCheckThread, 1000) == WAIT_TIMEOUT)
		{
			DEBUGCODE(Tracer::LogA("[WARNING][StartUpdateCheckThread] TerminateThread: UpdateCheckThread");)
			TerminateThread(g_loginData.hUpdateCheckThread, -1);
		}
		CloseHandle(g_loginData.hUpdateCheckThread);
	}
	g_loginData.hUpdateCheckThread = CreateThread(NULL, 0, UpdateCheckThread,cb, 0, NULL);
}

//------ 升级检测处理 start
//升级检测线程启动
void StartUpdateCheckThreadManually(pWSCallback cb)
{
	if (g_loginData.hUpdateCheckThread)
	{
		if (WaitForSingleObject(g_loginData.hUpdateCheckThread, 1000) == WAIT_TIMEOUT)
		{
			DEBUGCODE(Tracer::LogA("[WARNING][StartUpdateCheckThread] TerminateThread: UpdateCheckThread");)
				TerminateThread(g_loginData.hUpdateCheckThread, -1);
		}
		CloseHandle(g_loginData.hUpdateCheckThread);
	}
	g_loginData.hUpdateCheckThread = CreateThread(NULL, 0, UpdateCheckThread,cb, 0, NULL);
}

//升级检测线程处理
DWORD WINAPI UpdateCheckThread(LPVOID lpParameter)
{
	pWSCallback cb = (pWSCallback)lpParameter;
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	if(ua == NULL) return S_INVALIDPARAM;

	PUPGRADE_UI_DATA pUIData = new UPGRADE_UI_DATA;

	UA_UTIL::SetThreadName(-1,("UpdateCheckThread"));
	//添加一项ip检测的工作
	CSingleLock lock(&g_loginData.csIPCheckcs);
	lock.Lock();
	BOOL bCheckIP = g_loginData.bCheckedIP;
	lock.Unlock();
	if (bCheckIP == FALSE)
	{
		int ret = 0;
	//	ret = ua->GetLinphoneManager()->GetConnectIP("120.24.175.212:5060","120.24.175.212:5060",3000);
		if ( ret == 0)
		{
	//		ret = ua->GetSIPManager()->GetConnectIP("iptel.org","iptel.org",3000);
		}
		bCheckIP = TRUE;
	}
	lock.Lock();
	g_loginData.bCheckedIP = bCheckIP;
	lock.Unlock();

	CXCAPInternetSession session;
	CString server;
	CString sztag;
	char* pbuf = NULL;
	int dwSize = 0;
	DWORD Status =0;
	CString contentType;
	CString strFileName;
	DWORD dwType;

	INTERNET_PORT m_dwPort;
	CString strUpdateLink  = _T("");

	CAccountInfo account=ua->m_curAccount;
	if(account.GetName()!=_T(""))
	{
		Char2CString(account.GetAccInfo()->chUpdateServer,strUpdateLink);
	}
	if(strUpdateLink.IsEmpty())
	{
		CConfigDBManager* config = ua->GetConfigManager();
		if(config)
		{
			list<CAPPUser> accountList = config->GetUsers();
			if(accountList.size()>0)
			{
				CAPPUser account = accountList.front();
				CAccountInfo accInfo(account.GetName(),account.GetRealm(),_T(""),false,true);
				accInfo.SetUserID(account.GetUserID());

				CAccountDBManager *accountManager = ua->GetAccountDBManager();
				bool bGetAccInfo = accountManager->GetAccount(accInfo);

				if(bGetAccInfo)
				{
					Char2CString(accInfo.GetAccInfo()->chUpdateServer, strUpdateLink);
				}
			}
		}
	}

	CString2Char(strUpdateLink,pUIData->updateLink);//ui的升级地址/qifahao/pc
	//modified by zhangpeng on 20160530 begin 
	//系统升级区分系统名称
	wstring wstrNameEn = Man_appConfig.strNameEN;
	//大写转小写
	_wcslwr_s(const_cast<wchar_t*>(wstrNameEn.data()),wstrNameEn.length() + 1);
	strUpdateLink += _T("/");
	strUpdateLink += wstrNameEn.data();
	//modified by zhangpeng on 20160530 end
	strUpdateLink += _T("/pc/telpo_update.ini");

	DEBUGCODE(Tracer::Log(_T("[CBCSClientDlg::UpdateCheckThread] [m_strUpdateLink]:%s"),strUpdateLink);)

	AfxParseURL(strUpdateLink,dwType,server,strFileName,m_dwPort);
	if (!server.IsEmpty())
	{
		Status = session.Get(server,m_dwPort,strFileName,&pbuf,sztag,dwSize,contentType);
	}

	if (Status >=200 && Status <= 299)
	{
		CString strContent, strVersion, strForceVersion, strForced, strCompleted;
		Char2CString(pbuf,strContent);
		delete pbuf;
		strForced = GetIniDate(strContent, _T("FORCED="));
		strForced = strForced.Left(1);
		strCompleted = GetIniDate(strContent, _T("COMPLETED="));
		strCompleted = strCompleted.Left(1);
		strVersion = GetIniDate(strContent, _T("VERSION="));
		strForceVersion= GetIniDate(strContent, _T("FORCED_VERSION="));

		CString2Char(strVersion,pUIData->version);//ui的升级版本
		CString2Char(strForceVersion,pUIData->forceVersion);//ui的强制升级版本

		//增量升级或全量升级
		if (_T("1") == strCompleted)
			pUIData->bCompeletedUpdate = TRUE;//ui的全量升级
		else
			pUIData->bCompeletedUpdate = FALSE;//ui的增量升级
		if(cb)
		{
			cb(LOGIN_UPGRADE_TIPS,pUIData,NULL);
			pUIData = NULL;
		}
	}

	g_loginData.hUpdateCheckThread = NULL;

	if (pUIData)
	{
		delete pUIData;
	}
	//if(g_loginData.uiPara[0].cb)
	//	g_loginData.uiPara[0].cb(LOGIN_UPGRADE_TIPS, pUIData, g_loginData.uiPara[0].user_data);

	return 0;
}

//升级检测INI文件字符流中获取相应字段
CString GetIniDate(CString strContent, const CString& strDesLine)
{
	CString strData;
	int nPos =-1;
	nPos = strContent.Find(strDesLine);
	int end =-1;
	if (nPos > -1)
	{
		end = strContent.Find(_T("\r"),nPos+1);
		if (end < 0)
		{
			end = strContent.Find(_T("\n"),nPos+1);
		}
		if (end > -1)
		{
			strContent = strContent.Mid(nPos,end-nPos);
		}
	}
	nPos = strContent.Find(strDesLine);
	if (nPos >-1)
	{
		int nDesLength = strDesLine.GetLength();
		strData = strContent.Mid(nDesLength, strContent.GetLength() - nDesLength);
		//清楚空格
		strData.TrimLeft();
	}
	return strData;
}

//------ 升级检测处理 end

//完善联系人的信息
void PerfectPhbkItem(CPhBkGroupManager *pPhbkGroupManager, MAP_UID2PHBKITEM &mapUid2PhBkItems)
{
	if(!pPhbkGroupManager)
		return;

	CUserAgentCore* ua = CUserAgentCore::GetUserAgentCore();
	DEBUGCODE(Tracer::Log(_T("[:PerfectPhbkItem] enter"));)

	//先获取需要从服务器添加的节点
	vector<CString> vecAddUids;
	CSingleLock lock(&pPhbkGroupManager->m_cs);
	lock.Lock();
	for(MAP_UID2PHBKITEM::const_iterator iterM = pPhbkGroupManager->m_mapUid2PhBkItems.begin(); iterM != pPhbkGroupManager->m_mapUid2PhBkItems.end(); iterM++)
	{
		CString strUid = iterM->first; //成员uid
		MAP_UID2PHBKITEM::const_iterator iterMF = mapUid2PhBkItems.find(strUid);
		if(iterMF == mapUid2PhBkItems.end())
		{
			vecAddUids.push_back(strUid);

			DEBUGCODE(Tracer::Log(_T("[CBCSClientDlg::PerfectPhbkItem] Uid:%s"), strUid);)
		}
	}
	lock.Unlock();

	//批量从服务器获取用户数据
	vector<CString> vecTempAddUids = vecAddUids;
	while(1)
	{
		int index = 0;
		CELGGWebService webService;
		CString strMethod = _T("ldap.client.get.node.info");
		MAPCString mapContent;
		mapContent[_T("user_id")] = ua->selfGuid;
		mapContent[_T("attr_list[0]")] = _T("true");
		for(vector<CString>::const_iterator iterAdd = vecTempAddUids.begin(); iterAdd != vecTempAddUids.end(); index++)
		{
			CString strUid = *iterAdd;
			CString strIdlist;
			strIdlist.Format(_T("id_list[%d]"), index);
			mapContent[strIdlist] = strUid;

			iterAdd = vecTempAddUids.erase(iterAdd);

			if(index >= 300)
				break;
		}

		if(index <= 0)
		{
			break;
		}
		else
		{
			ICTDBLOG(_T("[POST::PerfectPhbkItem], method = %s, user_id = %s, attr_list[0] = %s"), strMethod, ua->selfGuid, _T("true"));
			DWORD status = webService.Post(strMethod, mapContent); //发起Post请求
			char* resBuf = webService.m_pResBuf; //给出的响应缓冲区，使用完需用delete释放掉

			using namespace std;
			string strJson;
			if(resBuf)
			{
				strJson.assign(resBuf);
				delete resBuf;
				resBuf = NULL;
			}
			ICTDBLOGA("[LAPPHEAD] - POST response = %s", strJson.c_str());
			if(status == HTTP_STATUS_OK)
			{
				Json::Value root;
				Json::Reader reader;
				if (reader.parse(strJson, root))
				{
					if (!root.isMember("status"))
					{
						CELGGWebService::ProcessWebServiceError(root);
					}

					if (root.isMember("result"))
					{
						Json::Value result = root["result"];
						for (int i = 0 ; i < result.size() ; i++)
						{
							if (result[i].isMember("data"))
							{
								Json::Value data = result[i]["data"];
								if(data.empty())
									continue;

								CPhBkItem *pPhBkItem = new CPhBkItem(NULL);
								pPhBkItem->FillDataWithJson(data, PB_ENTERPRISE_ADDRESS_BOOK);
								pPhBkItem->m_nWhole = 1;
								MAP_UID2PHBKITEM::iterator iter = mapUid2PhBkItems.find(pPhBkItem->m_strUID);
								if(iter == mapUid2PhBkItems.end())
								{
									vector<GenericItem*> itemList;
									itemList.push_back(pPhBkItem);
									mapUid2PhBkItems.insert(pair<CString, vector<GenericItem*> >(pPhBkItem->m_strUID, itemList));
								}
								else
								{
									for(vector<GenericItem*>::iterator iterV = iter->second.begin(); iterV!= iter->second.end(); iterV++)
									{
										if((*iterV)==pPhBkItem)
										{
											iterV = iter->second.erase(iterV);
										}
										else
											iterV++;
									}
									iter->second.push_back(pPhBkItem);
								}
							}
						}

						DEBUGCODE(Tracer::Log(_T("[CBCSClientDlg::PerfectPhbkItem] get done [size]:[%d]"), result.size());)
					}
				}
			}
		}
	}

	//填充组织架构中的联系人数据
	lock.Lock();
	for(MAP_UID2PHBKITEM::const_iterator iterM = pPhbkGroupManager->m_mapUid2PhBkItems.begin(); iterM != pPhbkGroupManager->m_mapUid2PhBkItems.end(); iterM++)
	{
		CString strUid = iterM->first; //成员uid
		vector<GenericItem*> vecItems = iterM->second; //成员uid对应的各部门成员结构
		for(vector<GenericItem*>::const_iterator iterV = vecItems.begin(); iterV != vecItems.end(); iterV++)
		{
			CPhBkItem *pPhBkItem = (CPhBkItem*)(*iterV);
			if(!pPhBkItem) continue;

			CString strParUid = _T("");
			CPhBkGroup *pParentGroup = (CPhBkGroup*)pPhBkItem->m_pParent;
			if(pParentGroup)
			{
				strParUid = pParentGroup->m_stCommonGroup.GetGroupId();
			}
			MAP_UID2PHBKITEM::const_iterator iterMF = mapUid2PhBkItems.find(strUid);
			if(iterMF != mapUid2PhBkItems.end())
			{
				//之前的联系人结构中查找到就直接替换
				vector<GenericItem*> vecItemsF = iterMF->second;
				vector<GenericItem*>::const_iterator iterVF = vecItemsF.begin();
				if(iterVF != vecItemsF.end())
				{
					CPhBkItem *pPhBkItemF = (CPhBkItem*)(*iterVF);
					if(pPhBkItemF)
					{
						//复制之前备份sortlevel,部门名称
						CString strSortLevel = pPhBkItem->m_strSortLevel;
						CString strDepName = pPhBkItem->m_strWorkDepartment;
						pPhBkItem->CopyFrom(pPhBkItemF);
						pPhBkItem->m_strSortLevel = strSortLevel;
						pPhBkItem->m_strWorkDepartment = strDepName;
					}
				}
			}
			else
			{
				//如果依然没有则不再获取了
			}
		}
	}
	lock.Unlock();

	DEBUGCODE(Tracer::Log(_T("[CBCSClientDlg::PerfectPhbkItem] perfect done"));)

	//获取新增联系人的头像
	/*for(vector<CString>::const_iterator iterAdd = vecAddUids.begin(); iterAdd != vecAddUids.end(); iterAdd++)
	{
		ua->GetPGMManager()->PutUserPhotoProc(*iterAdd, 0);
	}*/

	DEBUGCODE(Tracer::Log(_T("[CBCSClientDlg::PerfectPhbkItem] get new photo done"));)

	//清除之前的map结构
	for(MAP_UID2PHBKITEM::iterator iterMO = mapUid2PhBkItems.begin(); iterMO != mapUid2PhBkItems.end(); iterMO++)
	{
		vector<GenericItem*> vecItems = iterMO->second; //成员uid对应的各部门成员结构
		for(vector<GenericItem*>::const_iterator iterV = vecItems.begin(); iterV != vecItems.end(); iterV++)
		{
			CPhBkItem *pPhBkItem = (CPhBkItem*)(*iterV);
			if(!pPhBkItem) continue;

			delete pPhBkItem, pPhBkItem = NULL;
		}
		iterMO->second.clear();
	}
	mapUid2PhBkItems.clear();

	DEBUGCODE(Tracer::Log(_T("[CBCSClientDlg::PerfectPhbkItem] clear old user data done"));)
}

void UpdateDataVerToPersonalCfg(DATA_VER_TYPE dvt)
{
	CUserAgentCore* ua = CUserAgentCore::GetUserAgentCore();
	CAccountDBManager* config = ua->GetAccountDBManager();
	CAccountInfo account = ua->m_curAccount;
	if(config && account.GetName()!=_T(""))
	{
		int nLastVer = 0;
		switch(dvt)
		{
		case DVT_ENT:
			nLastVer = g_loginData.stLDV.nEntVer;
			break;
		case DVT_ROLE:
			nLastVer = g_loginData.stLDV.nRoleVer;
			break;
		case DVT_GROUP:
			nLastVer = g_loginData.stLDV.nGroupVer;
			break;
		case DVT_LAPP:
			nLastVer = g_loginData.stLDV.nLAPPVer;
			break;
		}
		DEBUGCODE(Tracer::Log(_T("[CBCSClientDlg::UpdateDataVerToPersonalCfg] [dvt]:[%d] [nPreVer]:[%d] [nNextVer]:[%d]"),dvt, account.GetDataVer(dvt), nLastVer);)

		ua->m_curAccount.SetDataVer(dvt, nLastVer);
		config->UpdateVer(dvt, nLastVer);

	}
}

DWORD WINAPI UpdateNodeThread1(LPVOID lpParameter)
{
	UA_UTIL::SetThreadName(-1,"UpdateNodeThread1");

	while (true)
	{
		UpdateNodeForGroup();
		UpdateNodeForUser();

		Sleep(100);
		if (g_loginData.vecExpandGroupIds.empty())
			break;
	}

	CloseHandle(g_loginData.hUpdateThread1);
	g_loginData.hUpdateThread1 = NULL;
	g_loginData.bNeedUpdate = false;

	return 0;
}

DWORD WINAPI UpdateNodeThread2(LPVOID lpParameter)
{
	UA_UTIL::SetThreadName(-1,"UpdateNodeThread2");
	return 0;
}

DWORD WINAPI UpdateNodeThread3(LPVOID lpParameter)
{
	UA_UTIL::SetThreadName(-1,"UpdateNodeThread3");

	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
ReUpdate3:

	CPhBkGroupManager *managerPB = ua->GetPGMManager()->GetPhBkManager(ua->GetUserID(), PB_ENTERPRISE_ADDRESS_BOOK);
	if(!managerPB)
	{
		CloseHandle(g_loginData.hUpdateThread3);
		g_loginData.hUpdateThread3 = NULL;
		return 0;
	}

	CSingleLock lock(&managerPB->m_cs);

	vector<PUID_UPDATE> vecUids;

	//WS可能会有改动
	CELGGWebService webService;
	CString strMethod = _T("ldap.client.get.node.info");
	MAPCString mapContent;
	mapContent[_T("user_id")] = ua->selfGuid;
	mapContent[_T("attr_list[0]")] = _T("nodeversion");
	mapContent[_T("attr_list[1]")] = _T("imgurl");
	ICTDBLOG(_T("[POST::UpdateNodeThread3], method = %s, user_id = %s, attr_list[0] = %s, attr_list[1] = %s"), strMethod, ua->selfGuid, _T("nodeversion"), _T("imgurl"));
	CSingleLock lock2(&g_loginData.csUN2);
	CSingleLock lock3(&g_loginData.csUN3);
	lock3.Lock();

	int index = 0;
	while (g_loginData.queUid2UpdateNode3.size() > 0)
	{
		PUID_UPDATE pu = g_loginData.queUid2UpdateNode3.front();
		g_loginData.queUid2UpdateNode3.pop();

		CString strParentId = pu.puid.strParentId;
		CString strUserId = pu.puid.strUid;

		CString strId, strPId;
		strId.Format(_T("id_list[%d]"), index);
		strPId.Format(_T("pid_list[%d]"), index);
		mapContent[strId] = strUserId;
		mapContent[strPId] = strParentId;

		index++;
	}

	lock3.Unlock();

	if(index > 0)
	{
		DWORD status = webService.Post(strMethod, mapContent); //发起Post请求
		char* resBuf = webService.m_pResBuf; //给出的响应缓冲区，使用完需用delete释放掉

		using namespace std;
		string strJson;
		if(resBuf)
		{
			strJson.assign(resBuf);
			delete resBuf;
			resBuf = NULL;
		}
		ICTDBLOGA("[LAPPHEAD] - POST response = %s", strJson.c_str());
		if(status == HTTP_STATUS_OK)
		{
			Json::Value root;
			Json::Reader reader;
			if (reader.parse(strJson, root))
			{
				if (!root.isMember("status"))
				{
					CELGGWebService::ProcessWebServiceError(root);
				}

				if (root.isMember("result"))
				{
					Json::Value result = root["result"];
					for (int i = 0 ; i < result.size() ; i++)
					{
						CString strUserId, strParentId, strVersion, strImageUrl;
						if(result[i].isMember("id"))
						{
							string temp = result[i]["id"].asString();
							Char2CString(temp.c_str(), strUserId);
						}
						if(result[i].isMember("pid"))
						{
							string temp = result[i]["pid"].asString();
							Char2CString(temp.c_str(), strParentId);
						}
						if (result[i].isMember("data"))
						{
							Json::Value data = result[i]["data"];
							if(data.empty())
								continue;

							if(data.isMember("nodeversion"))
							{
								Json::Value arryObj = data["nodeversion"];
								for (Json::ValueIterator index = arryObj.begin(); index != arryObj.end() ; ++index)
								{
									string temp = (*index).asString();
									Char2CString(temp.c_str(), strVersion);
								}
							}

							if(data.isMember("imgurl"))
							{
								Json::Value arryObj = data["imgurl"];
								for (Json::ValueIterator index = arryObj.begin(); index != arryObj.end() ; ++index)
								{
									string temp = (*index).asString();
									Char2CString(temp.c_str(), strImageUrl);
								}
							}
						}

						PUID_UPDATE pu;
						pu.puid.strParentId = strParentId;
						pu.puid.strUid = strUserId;
						pu.puid.bUpdatePhoto = false;
						lock.Lock();
						CPhBkGroup *pParentGroup = managerPB->GetPhBkGroupByGroupId(strParentId);
						if(!strParentId.IsEmpty() && pParentGroup)
						{
							CPhBkItem *pPhBkItem = pParentGroup->GetPhBkItemByUid(strUserId);
							if(pPhBkItem)
							{
								//用户有个性化头像但头像文件不存在时需要下载头像
								bool bNeedUpdatePhoto = false;
								if (pPhBkItem->m_strVer != strVersion ||
									(bNeedUpdatePhoto = (!strImageUrl.IsEmpty() && !UA_UTIL::IsFileExist(GetUserPortraitPath(strUserId)))))
								{
									if (bNeedUpdatePhoto || pPhBkItem->m_strPhoto != strImageUrl)
									{
										pu.puid.bUpdatePhoto = true;
									}
									pu.un.strVersion = strVersion;
									vecUids.push_back(pu);
								}
								else
								{
									pPhBkItem->m_dwLastUpdateTime = GetTickCount();
								}
							}
						}
						else
						{
							CPhBkItem *pPhBkItem = ua->GetPhBkItemByUid(strUserId);
							if(pPhBkItem)
							{
								if(pPhBkItem->m_strVer != strVersion)
								{
									if(pPhBkItem->m_strPhoto != strImageUrl)
									{
										pu.puid.bUpdatePhoto = true;
									}
									pu.un.strVersion = strVersion;
									pu.puid.strParentId = ((CPhBkGroup*)(pPhBkItem->m_pParent))->m_stCommonGroup.GetGroupId();
									vecUids.push_back(pu);
								}
								else
								{
									pPhBkItem->m_dwLastUpdateTime = GetTickCount();
								}
							}
						}
						lock.Unlock();

						DEBUGCODE(Tracer::Log(_T("[CBCSClientDlg::UpdateNodeThread3] [strParentId]:[%s] [strUserId]:[%s] [bUpdatePhoto]:[%d] [strVersion]:[%s]"), pu.puid.strParentId, pu.puid.strUid, pu.puid.bUpdatePhoto, strVersion);)

					}
				}
			}
		}

		for(vector<PUID_UPDATE>::iterator iterV = vecUids.begin(); iterV != vecUids.end(); iterV++)
		{
			PUID_UPDATE pu = (*iterV);
			CString strParentId = pu.puid.strParentId;
			CString strUserId = pu.puid.strUid;
			bool bUpdatePhoto = pu.puid.bUpdatePhoto;

			ua->GetPGMManager()->UpdatePhBkInfo(2, strParentId, strUserId, bUpdatePhoto, UpdatePhBkCallBack);

			lock.Lock();
			CPhBkGroup *pParentGroup = managerPB->GetPhBkGroupByGroupId(strParentId);
			if(pParentGroup)
			{
				CPhBkItem *pPhBkItem = pParentGroup->GetPhBkItemByUid(strUserId);
				if(pPhBkItem)
				{
					pPhBkItem->m_dwLastUpdateTime = GetTickCount();
					pPhBkItem->m_strVer = pu.un.strVersion;
				}
			}
			lock.Unlock();
		}
	}

	::Sleep(5 * 1000);

	goto ReUpdate3;

	CloseHandle(g_loginData.hUpdateThread3);
	g_loginData.hUpdateThread3 = NULL;

	return 0;
}

//DWORD WINAPI UpdateNodeThread3s(LPVOID lpParameter)
//{
//	UA_UTIL::SetThreadName(-1,"UpdateNodeThread3s");
//
//	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
//ReUpdate3s:
//
//	CPhBkGroupManager *managerPB = ua->GetPGMManager()->GetPhBkManager(ua->GetUserID(), PB_ENTERPRISE_ADDRESS_BOOK);
//	if(!managerPB)
//	{
//		CloseHandle(g_loginData.hUpdateThread3s);
//		g_loginData.hUpdateThread3s = NULL;
//		return 0;
//	}
//
//	CSingleLock lock(&managerPB->m_cs);
//
//	vector<PUID_UPDATE> vecUids;
//
//	//WS可能会有改动
//	CELGGWebService webService;
//	CString strMethod = _T("ldap.client.get.node.info");
//	MAPCString mapContent;
//	mapContent[_T("user_id")] = ua->selfGuid;
//	mapContent[_T("attr_list[0]")] = _T("nodeversion");
//	mapContent[_T("attr_list[1]")] = _T("imgurl");
//	ICTDBLOG(_T("[POST::UpdateNodeThread3], method = %s, user_id = %s, attr_list[0] = %s, attr_list[1] = %s"), strMethod, ua->selfGuid, _T("nodeversion"), _T("imgurl"));
//	CSingleLock lock2(&g_loginData.csUN2);
//	CSingleLock lock3(&g_loginData.csUN3);
//	lock3.Lock();
//
//	int index = 0;
//	while (g_loginData.queUid2UpdateNode3s.size() > 0)
//	{
//		PUID_UPDATE pu = g_loginData.queUid2UpdateNode3s.front();
//		g_loginData.queUid2UpdateNode3.pop();
//
//		CString strParentId = pu.puid.strParentId;
//		CString strUserId = pu.puid.strUid;
//
//		CString strId, strPId;
//		strId.Format(_T("id_list[%d]"), index);
//		strPId.Format(_T("pid_list[%d]"), index);
//		mapContent[strId] = strUserId;
//		mapContent[strPId] = strParentId;
//
//		index++;
//	}
//
//	lock3.Unlock();
//
//	if(index > 0)
//	{
//		DWORD status = webService.Post(strMethod, mapContent); //发起Post请求
//		char* resBuf = webService.m_pResBuf; //给出的响应缓冲区，使用完需用delete释放掉
//
//		using namespace std;
//		string strJson;
//		if(resBuf)
//		{
//			strJson.assign(resBuf);
//			delete resBuf;
//			resBuf = NULL;
//		}
//		ICTDBLOGA("[LAPPHEAD] - POST response = %s", strJson.c_str());
//		if(status == HTTP_STATUS_OK)
//		{
//			Json::Value root;
//			Json::Reader reader;
//			if (reader.parse(strJson, root))
//			{
//				if (!root.isMember("status"))
//				{
//					CELGGWebService::ProcessWebServiceError(root);
//				}
//
//				if (root.isMember("result"))
//				{
//					Json::Value result = root["result"];
//					for (int i = 0 ; i < result.size() ; i++)
//					{
//						CString strUserId, strParentId, strVersion, strImageUrl;
//						if(result[i].isMember("id"))
//						{
//							string temp = result[i]["id"].asString();
//							Char2CString(temp.c_str(), strUserId);
//						}
//						if(result[i].isMember("pid"))
//						{
//							string temp = result[i]["pid"].asString();
//							Char2CString(temp.c_str(), strParentId);
//						}
//						if (result[i].isMember("data"))
//						{
//							Json::Value data = result[i]["data"];
//							if(data.empty())
//								continue;
//
//							if(data.isMember("nodeversion"))
//							{
//								Json::Value arryObj = data["nodeversion"];
//								for (Json::ValueIterator index = arryObj.begin(); index != arryObj.end() ; ++index)
//								{
//									string temp = (*index).asString();
//									Char2CString(temp.c_str(), strVersion);
//								}
//							}
//
//							if(data.isMember("imgurl"))
//							{
//								Json::Value arryObj = data["imgurl"];
//								for (Json::ValueIterator index = arryObj.begin(); index != arryObj.end() ; ++index)
//								{
//									string temp = (*index).asString();
//									Char2CString(temp.c_str(), strImageUrl);
//								}
//							}
//						}
//
//						PUID_UPDATE pu;
//						pu.puid.strParentId = strParentId;
//						pu.puid.strUid = strUserId;
//						pu.puid.bUpdatePhoto = false;
//						lock.Lock();
//						CPhBkGroup *pParentGroup = managerPB->GetPhBkGroupByGroupId(strParentId);
//						if(!strParentId.IsEmpty() && pParentGroup)
//						{
//							CPhBkItem *pPhBkItem = pParentGroup->GetPhBkItemByUid(strUserId);
//							if(pPhBkItem)
//							{
//								//用户有个性化头像但头像文件不存在时需要下载头像
//								bool bNeedUpdatePhoto = false;
//								if (pPhBkItem->m_strVer != strVersion ||
//									(bNeedUpdatePhoto = (!strImageUrl.IsEmpty() && !UA_UTIL::IsFileExist(GetUserPortraitPath(strUserId)))))
//								{
//									if (bNeedUpdatePhoto || pPhBkItem->m_strPhoto != strImageUrl)
//									{
//										pu.puid.bUpdatePhoto = true;
//									}
//									pu.un.strVersion = strVersion;
//									vecUids.push_back(pu);
//								}
//								else
//								{
//									pPhBkItem->m_dwLastUpdateTime = GetTickCount();
//								}
//							}
//						}
//						else
//						{
//							CPhBkItem *pPhBkItem = ua->GetPhBkItemByUid(strUserId);
//							if(pPhBkItem)
//							{
//								if(pPhBkItem->m_strVer != strVersion)
//								{
//									if(pPhBkItem->m_strPhoto != strImageUrl)
//									{
//										pu.puid.bUpdatePhoto = true;
//									}
//									pu.un.strVersion = strVersion;
//									pu.puid.strParentId = ((CPhBkGroup*)(pPhBkItem->m_pParent))->m_stCommonGroup.GetGroupId();
//									vecUids.push_back(pu);
//								}
//								else
//								{
//									pPhBkItem->m_dwLastUpdateTime = GetTickCount();
//								}
//							}
//						}
//						lock.Unlock();
//
//						DEBUGCODE(Tracer::Log(_T("[CBCSClientDlg::UpdateNodeThread3] [strParentId]:[%s] [strUserId]:[%s] [bUpdatePhoto]:[%d] [strVersion]:[%s]"), pu.puid.strParentId, pu.puid.strUid, pu.puid.bUpdatePhoto, strVersion);)
//
//					}
//				}
//			}
//		}
//
//		for(vector<PUID_UPDATE>::iterator iterV = vecUids.begin(); iterV != vecUids.end(); iterV++)
//		{
//			PUID_UPDATE pu = (*iterV);
//			CString strParentId = pu.puid.strParentId;
//			CString strUserId = pu.puid.strUid;
//			bool bUpdatePhoto = pu.puid.bUpdatePhoto;
//
//			ua->GetPGMManager()->UpdatePhBkInfo(2, strParentId, strUserId, bUpdatePhoto, UpdatePhBkCallBack);
//
//			lock.Lock();
//			CPhBkGroup *pParentGroup = managerPB->GetPhBkGroupByGroupId(strParentId);
//			if(pParentGroup)
//			{
//				CPhBkItem *pPhBkItem = pParentGroup->GetPhBkItemByUid(strUserId);
//				if(pPhBkItem)
//				{
//					pPhBkItem->m_dwLastUpdateTime = GetTickCount();
//					pPhBkItem->m_strVer = pu.un.strVersion;
//				}
//			}
//			lock.Unlock();
//		}
//	}
//
//	CloseHandle(g_loginData.hUpdateThread3s);
//	g_loginData.hUpdateThread3s = NULL;
//
//	return 0;
//}

//更新通讯录（企业与个人通讯录）
DWORD WINAPI UpdateNodeThread4(LPVOID lpParameter)
{
	UA_UTIL::SetThreadName(-1, "UpdateNodeThread4");

	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	int nLoopTimes = 0;
ReUpdate4:
	CPhBkGroupManager *managerEPB = ua->GetPGMManager()->GetPhBkManager(ua->GetUserID(), PB_ENTERPRISE_ADDRESS_BOOK);
	CPhBkGroupManager *managerPPB = ua->GetPGMManager()->GetPhBkManager(ua->GetUserID(), PB_BUDDIES);
	if(!managerEPB && !managerPPB)
		return 0;

	CSingleLock lockEPB(&managerEPB->m_cs);
	CSingleLock lockPPB(&managerPPB->m_cs);
	CSingleLock lockUN4(&g_loginData.csUN4);

	//WS可能会有改动
	CELGGWebService webService;
	CString strMethod = _T("ldap.client.get.node.info");
	MAPCString mapContent;
	mapContent[_T("user_id")] = ua->selfGuid;
	mapContent[_T("attr_list[0]")] = _T("true");

	lockUN4.Lock();
	int itemCount = 0;
	map<PUID, bool> mapPUID;
	map<CString, CString> mapUserPhoto;

	while(!g_loginData.queUid2UpdateNode4.empty())
	{
		PUID puid = g_loginData.queUid2UpdateNode4.front();
		g_loginData.queUid2UpdateNode4.pop();

		CString strId, strPId;
		strId.Format(_T("id_list[%d]"), itemCount);
		strPId.Format(_T("pid_list[%d]"), itemCount);
		mapContent[strId] = puid.strUid;
		mapContent[strPId] = puid.strParentId;
		if (puid.strParentId.IsEmpty())
		{
			puid.strParentId = _T("p_pb");
			mapPUID[puid] = false;
		}
		else
			mapPUID[puid] = false;

		itemCount++;
		if(itemCount >= 300)
			break;
	}
	lockUN4.Unlock();

	if(itemCount > 0)
	{
		DWORD status = webService.Post(strMethod, mapContent); //发起Post请求
		char* resBuf = webService.m_pResBuf; //给出的响应缓冲区，使用完需用delete释放掉

		using namespace std;
		string strJson;
		if(resBuf)
		{
			strJson.assign(resBuf);
			delete resBuf;
			resBuf = NULL;
		}
		ICTDBLOGA("[LAPPHEAD] - POST response = %s", strJson.c_str());
		if (status == HTTP_STATUS_OK)
		{
			Json::Value root;
			Json::Reader reader;
			if (reader.parse(strJson, root))
			{
				if (!root.isMember("status"))
					CELGGWebService::ProcessWebServiceError(root);

				if (root.isMember("result"))
				{
					map<CString, CString> mapUser2Sig;
					lockEPB.Lock();
					Json::Value result = root["result"];
					//// queue会清空并全部赋值给map，同步节点如果缺少联系人理应删掉
					//if (result > 0)
					//{
				//		mapPUID.clear();
				//	}
					for (int i = 0 ; i < result.size() ; i++)
					{
						CString strUserId, strParentId;
						if(result[i].isMember("id"))
						{
							string temp = result[i]["id"].asString();
							Char2CString(temp.c_str(), strUserId);
						}
						if(result[i].isMember("pid"))
						{
							string temp = result[i]["pid"].asString();
							Char2CString(temp.c_str(), strParentId);
						}
						if (result[i].isMember("data"))
						{
							Json::Value data = result[i]["data"];
							if(data.empty())
								continue;

							//可以根据ParentID判断该成员属于企业通讯录还是个人通讯录, 个人通讯录的ParentID是空
							CPhBkGroup *groupParent = NULL;
							if (!strParentId.IsEmpty())
								groupParent = managerEPB->GetPhBkGroupByGroupId(strParentId);
							else
							{
								//由于个人通讯录分组没有ID, 所以此处使用特殊分组ID处理
								strParentId = PERSONALPB_UID;
								groupParent = managerPPB->GetPhBkGroupByGroupId(strParentId);
							}

							if(groupParent)
							{
								CPhBkItem *item = groupParent->GetPhBkItemByUid(strUserId);
								if(item)
								{
									CString strSortLevel = item->m_strSortLevel;

									item->FillDataWithJson(data, PB_BUDDIES);
									item->m_nWhole = 1;
									if(strSortLevel != item->m_strSortLevel)
										groupParent->SortPhBkItemBySortLevel(strUserId);

									PUID puid;
									puid.strParentId = strParentId;
									puid.strUid = strUserId;
									puid.bUpdatePhoto = false;
									mapPUID[puid] = true;

									//添加头像map
									mapUserPhoto[puid.strUid] = item->m_strPhoto;

									//添加签名map
									mapUser2Sig.insert(make_pair(puid.strUid, item->m_strSig));
								}
							}
						}
					}

					if(++nLoopTimes %10 == 0)
					{
						managerEPB->SaveToFile();
						managerPPB->SaveToFile();
					}
					lockEPB.Unlock();

					//集体更新签名
					UpdateMultiUserSig(mapUser2Sig);
				}
			}
		}
	}

	//对于没有成功的节点重复加入到队列
	lockUN4.Lock();
	for(map<PUID, bool>::const_iterator iter = mapPUID.begin(); iter != mapPUID.end(); iter++)
	{
		if(!iter->second)
			g_loginData.queUid2UpdateNode4.push(iter->first);
	}
	lockUN4.Unlock();

	//下载用户头像
	for(map<CString, CString>::const_iterator iterM = mapUserPhoto.begin(); iterM != mapUserPhoto.end(); iterM++)
	{
		CString strUid = iterM->first;
		CString strImgUrl = iterM->second;
		if(!strUid.IsEmpty() && strUid != ua->selfGuid) //非当前登录用户的头像
		{
			CString strPhotoPath = ua->GetAppDataRootPath() + _T("res\\HeadPortrait\\user_portrait\\") + strUid + _T(".png");
			if(!UA_UTIL::IsFileExist(strPhotoPath) && !strImgUrl.IsEmpty())
			{
				ua->GetPGMManager()->PutUserPhotoProc(strUid, 0);
			}
		}
	}

	::Sleep(1*1000);

	goto ReUpdate4;

	return 0;
}

DWORD WINAPI UpdateNodeThread5(LPVOID lpParameter)
{
	UA_UTIL::SetThreadName(-1,"UpdateNodeThread5");

	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	int nLoopTimes = 0;
ReUpdate5:
	CPhBkGroupManager *managerPB = ua->GetPGMManager()->GetPhBkManager(ua->GetUserID(), PB_BUDDIES);
	if(!managerPB)
		return 0;

	CSingleLock lock(&managerPB->m_cs);
	CSingleLock lock5(&g_loginData.csUN5);

	//WS可能会有改动
	CELGGWebService webService;
	CString strMethod = _T("ldap.client.get.node.info");
	MAPCString mapContent;
	mapContent[_T("user_id")] = ua->selfGuid;
	mapContent[_T("attr_list[0]")] = _T("true");

	lock5.Lock();
	int index = 0;
	map<PUID, bool> mapPUID;
	map<CString, CString> mapUserPhoto;
	while(!g_loginData.queUid2UpdateNode5.empty())
	{
		PUID puid = g_loginData.queUid2UpdateNode5.front();
		g_loginData.queUid2UpdateNode5.pop();

		CString strId, strPId;
		strId.Format(_T("id_list[%d]"), index);
		strPId.Format(_T("pid_list[%d]"), index);
		mapContent[strId] = puid.strUid;
		mapContent[strPId] = puid.strParentId;

		mapPUID[puid] = false;

		index++;
		if(index >= 300)
			break;
	}
	lock5.Unlock();

	if(index > 0)
	{
		DWORD status = webService.Post(strMethod, mapContent); //发起Post请求
		char* resBuf = webService.m_pResBuf; //给出的响应缓冲区，使用完需用delete释放掉

		using namespace std;
		string strJson;
		if(resBuf)
		{
			strJson.assign(resBuf);
			delete resBuf;
			resBuf = NULL;
		}
		ICTDBLOGA("[LAPPHEAD] - POST response = %s", strJson.c_str());
		if(status == HTTP_STATUS_OK)
		{
			Json::Value root;
			Json::Reader reader;
			if (reader.parse(strJson, root))
			{
				if (!root.isMember("status"))
				{
					CELGGWebService::ProcessWebServiceError(root);
				}

				if (root.isMember("result"))
				{
					map<CString, CString> mapUser2Sig;
					lock.Lock();
					Json::Value result = root["result"];
					for (int i = 0 ; i < result.size() ; i++)
					{
						CString strUserId, strParentId;
						if(result[i].isMember("id"))
						{
							string temp = result[i]["id"].asString();
							Char2CString(temp.c_str(), strUserId);
						}
						if(result[i].isMember("pid"))
						{
							string temp = result[i]["pid"].asString();
							Char2CString(temp.c_str(), strParentId);
						}
						if (result[i].isMember("data"))
						{
							Json::Value data = result[i]["data"];
							if(data.empty())
								continue;

							CPhBkGroup *pParentGroup = managerPB->GetPhBkGroupByGroupId(strParentId);
							if(pParentGroup)
							{
								CPhBkItem *pPhBkItem = pParentGroup->GetPhBkItemByUid(strUserId);
								if(pPhBkItem)
								{
									CString strSortLevel = pPhBkItem->m_strSortLevel;
									pPhBkItem->FillDataWithJson(data, PB_BUDDIES);
									pPhBkItem->m_nWhole = 1;
									if(strSortLevel != pPhBkItem->m_strSortLevel)
									{
										pParentGroup->SortPhBkItemBySortLevel(strUserId);
									}

									PUID puid;
									puid.strParentId = strParentId;
									puid.strUid = strUserId;
									puid.bUpdatePhoto = false;
									mapPUID[puid] = true;

									//添加头像map
									mapUserPhoto[puid.strUid] = pPhBkItem->m_strPhoto;

									//添加签名map
									mapUser2Sig.insert(make_pair(puid.strUid, pPhBkItem->m_strSig));
								}
							}
						}
					}
					if(++nLoopTimes %10 == 0)
						managerPB->SaveToFile();
					lock.Unlock();

					//集体更新签名
					UpdateMultiUserSig(mapUser2Sig);
				}
			}
		}
	}

	//对于没有成功的节点重复加入到队列
	lock5.Lock();
	for(map<PUID, bool>::const_iterator iter = mapPUID.begin(); iter != mapPUID.end(); iter++)
	{
		if(!iter->second)
		{
			g_loginData.queUid2UpdateNode5.push(iter->first);
		}
	}
	lock5.Unlock();

	//下载用户头像
	for(map<CString, CString>::const_iterator iterM = mapUserPhoto.begin(); iterM != mapUserPhoto.end(); iterM++)
	{
		CString strUid = iterM->first;
		CString strImgUrl = iterM->second;
		if(!strUid.IsEmpty() && strUid != ua->selfGuid) //非当前登录用户的头像
		{
			CString strPhotoPath = ua->GetAppDataRootPath() + _T("res\\HeadPortrait\\user_portrait\\") + strUid + _T(".png");
			if(!UA_UTIL::IsFileExist(strPhotoPath) && !strImgUrl.IsEmpty())
			{
				ua->GetPGMManager()->PutUserPhotoProc(strUid, 0);
			}
		}
	}

	::Sleep(1*1000);

	goto ReUpdate5;

	return 0;
}

CString GetGroupVersion(vector<CString> &vecUids, vector<CString> &vecVers)
{
	CString ret = _T("");
	CString buffer = _T("");

	if(vecUids.size() != vecVers.size())
		return ret;

	for(int i = 0; i< vecUids.size(); i++)
	{
		buffer += vecVers[i] + vecUids[i];
	}

	char secretbuf[1024];
	unsigned char result[16];
	char md5str[33];
	CString2Char(buffer,secretbuf);
	struct MD5Context md5ctx;
	MD5Init(&md5ctx);
	MD5Update(&md5ctx, reinterpret_cast<const unsigned char *>(secretbuf), (unsigned)strlen(secretbuf));
	MD5Final(result, &md5ctx);
	MD52Str(result,16,md5str);
	md5str[32] = 0X0;
	CString strMd5;
	Char2CString(md5str,strMd5);
	ret = strMd5;
	return ret;
}

void UpdateNodeForGroup()
{
	//获取可见组节点的uid
	vector<CString> vecGroupIds;
	CSingleLock lockEGI(&g_loginData.csEGI);
	lockEGI.Lock();
	g_loginData.vecExpandGroupIds.swap(vecGroupIds);
	lockEGI.Unlock();

	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	CPhBkGroupManager *managerPB = ua->GetPGMManager()->GetPhBkManager(ua->GetUserID(), PB_ENTERPRISE_ADDRESS_BOOK);
	if(!managerPB)
	{
		return;
	}

	CSingleLock lock(&managerPB->m_cs);
	CSingleLock lock1(&g_loginData.csUN1);
	CSingleLock lock2(&g_loginData.csUN2);

	lock1.Lock();
	g_loginData.mapUid2UpdateNode1.clear();
	lock1.Unlock();

	lock.Lock();
	DWORD dwCurTime = GetTickCount();
	int nUpdateGroupSize = 0;
	//获取组的版本号
	for(vector<CString>::const_iterator iterV = vecGroupIds.begin(); iterV != vecGroupIds.end(); iterV++)
	{
		CString strGroupId = *iterV;
		CPhBkGroup *pPhBkGroup = ua->GetPGMManager()->GetPhBkGroupByGroupId(ua->GetUserID(), PB_ENTERPRISE_ADDRESS_BOOK, strGroupId);
		if(pPhBkGroup)
		{
			if(!g_loginData.bNeedUpdate && dwCurTime - pPhBkGroup->m_dwLastUpdateTime < 60*1000)
				continue;

			//获取组下面的所有直属用户的uid和version
			vector<CString> vecUids, vecVers;
			CPhBkItem *pItem = NULL;
			POSITION posItem(NULL);
			posItem = pPhBkGroup->mItems.GetHeadPosition();
			while(posItem)
			{
				pItem = dynamic_cast<CPhBkItem *>(pPhBkGroup->mItems.GetNext(posItem));
				if(pItem)
				{
					vecUids.push_back(pItem->m_strUID);
					vecVers.push_back(pItem->m_strVer);
				}
			}

			UPDATE_NODE un;
			un.nType = 0;
			un.dwLastUpdateTime = pPhBkGroup->m_dwLastUpdateTime;
			un.strVersion = GetGroupVersion(vecUids, vecVers);

			lock1.Lock();
			g_loginData.mapUid2UpdateNode1[strGroupId] = un;
			nUpdateGroupSize++;
			lock1.Unlock();

			DEBUGCODE(Tracer::Log(_T("[CBCSClientDlg::UpdateNodeForGroup] [updateTime]:[%d] [strVersion]:[%s]"), un.dwLastUpdateTime, un.strVersion);)
		}
	}
	lock.Unlock();

	lock2.Lock();
	g_loginData.mapUid2UpdateNode2.clear();
	lock2.Unlock();

	vector<CString> vecUpdateGroupIds; //需要更新成员的组Id

	//WS可能会有改动
	CELGGWebService webService;
	CString strMethod = _T("ldap.client.get.node.info");
	MAPCString mapContent;
	mapContent[_T("user_id")] = ua->selfGuid;
	mapContent[_T("attr_list[0]")] = _T("nodeversion");

	lock1.Lock();
	int index = 0;
	int nUpdateUserSize = 0;
	for(map<CString, UPDATE_NODE>::const_iterator iter = g_loginData.mapUid2UpdateNode1.begin(); iter != g_loginData.mapUid2UpdateNode1.end(); iter++, index++)
	{
		CString strGroupId = iter->first;

		CString strId;
		strId.Format(_T("id_list[%d]"), index);
		mapContent[strId] = strGroupId;
	}
	lock1.Unlock();

	if(index > 0)
	{
		DWORD status = webService.Post(strMethod, mapContent); //发起Post请求
		char* resBuf = webService.m_pResBuf; //给出的响应缓冲区，使用完需用delete释放掉

		using namespace std;
		string strJson;
		if(resBuf)
		{
			strJson.assign(resBuf);
			delete resBuf;
			resBuf = NULL;
		}
		ICTDBLOGA("[LAPPHEAD] - POST response = %s", strJson.c_str());
		if(status == HTTP_STATUS_OK)
		{
			Json::Value root;
			Json::Reader reader;
			if (reader.parse(strJson, root))
			{
				if (!root.isMember("status"))
				{
					CELGGWebService::ProcessWebServiceError(root);
				}

				if (root.isMember("result"))
				{
					Json::Value result = root["result"];
					for (int i = 0 ; i < result.size() ; i++)
					{
						CString strGroupId, strVersion;
						if(result[i].isMember("id"))
						{
							string temp = result[i]["id"].asString();
							Char2CString(temp.c_str(), strGroupId);
						}
						if (result[i].isMember("data"))
						{
							Json::Value data = result[i]["data"];
							if(data.empty())
								continue;

							if(data.isMember("nodeversion"))
							{
								Json::Value arryObj = data["nodeversion"];
								for (Json::ValueIterator index = arryObj.begin(); index != arryObj.end() ; ++index)
								{
									string temp = (*index).asString();
									Char2CString(temp.c_str(), strVersion);
								}
							}
						}

						lock.Lock();
						lock1.Lock();
						map<CString, UPDATE_NODE>::iterator iter = g_loginData.mapUid2UpdateNode1.find(strGroupId);
						if(iter != g_loginData.mapUid2UpdateNode1.end())
						{
							if(iter->second.strVersion != strVersion)
							{
								iter->second.strVersion = strVersion;
								vecUpdateGroupIds.push_back(strGroupId);
							}
							iter->second.dwLastUpdateTime = GetTickCount();

							CPhBkGroup *pPhBkGroup = ua->GetPGMManager()->GetPhBkGroupByGroupId(ua->GetUserID(), PB_ENTERPRISE_ADDRESS_BOOK, strGroupId);
							if(pPhBkGroup)
							{
								pPhBkGroup->m_dwLastUpdateTime = iter->second.dwLastUpdateTime;
								pPhBkGroup->m_stCommonGroup.SetVersion(strVersion);
							}
						}
						lock1.Unlock();
						lock.Unlock();

						DEBUGCODE(Tracer::Log(_T("[CBCSClientDlg::UpdateNodeForGroup] [strGroupId]:[%s] [strNewVersion]:[%s]"), strGroupId, strVersion);)
					}
				}
			}
		}

		//遍历需要更新的组Id
		lock.Lock();

		for(vector<CString>::const_iterator iter = vecUpdateGroupIds.begin(); iter != vecUpdateGroupIds.end(); iter++)
		{
			CString strGroupId = *iter;
			CPhBkGroup *pPhBkGroup = ua->GetPGMManager()->GetPhBkGroupByGroupId(ua->GetUserID(), PB_ENTERPRISE_ADDRESS_BOOK, strGroupId);
			if(pPhBkGroup)
			{
				CPhBkItem *pItem = NULL;
				POSITION posItem(NULL);
				posItem = pPhBkGroup->mItems.GetHeadPosition();
				while(posItem)
				{
					pItem = dynamic_cast<CPhBkItem *>(pPhBkGroup->mItems.GetNext(posItem));
					if(pItem)
					{
						UPDATE_NODE un;
						un.nType = 1;
						un.dwLastUpdateTime = pItem->m_dwLastUpdateTime;
						un.strVersion = pItem->m_strVer;

						PUID puid;
						puid.strParentId = pPhBkGroup->m_stCommonGroup.GetGroupId();
						puid.strUid = pItem->m_strUID;
						lock2.Lock();
						g_loginData.mapUid2UpdateNode2[puid] = un;
						nUpdateUserSize++;
						lock2.Unlock();
					}
				}

				DEBUGCODE(Tracer::Log(_T("[CBCSClientDlg::UpdateNodeForGroup] [strUpdateGroupId]:[%s] [nUpdateUserSize]:[%d]"), strGroupId, nUpdateUserSize);)
			}
		}

		lock.Unlock();
	}
}

void UpdateNodeForUser()
{
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	CPhBkGroupManager *managerPB = ua->GetPGMManager()->GetPhBkManager(ua->GetUserID(), PB_ENTERPRISE_ADDRESS_BOOK);
	if(!managerPB)
		return;

	CSingleLock lock(&managerPB->m_cs);
	CSingleLock lock2(&g_loginData.csUN2);

	vector<PUID_UPDATE> vecUids;

	//WS可能会有改动
	CELGGWebService webService;
	CString strMethod = _T("ldap.client.get.node.info");
	MAPCString mapContent;
	mapContent[_T("user_id")] = ua->selfGuid;
	mapContent[_T("attr_list[0]")] = _T("nodeversion");
	mapContent[_T("attr_list[1]")] = _T("imgurl");

	lock2.Lock();
	int index = 0;
	for(map<PUID, UPDATE_NODE>::iterator iter = g_loginData.mapUid2UpdateNode2.begin(); iter != g_loginData.mapUid2UpdateNode2.end(); iter++, index++)
	{
		CString strParentId = iter->first.strParentId;
		CString strUserId = iter->first.strUid;

		CString strId, strPId;
		strId.Format(_T("id_list[%d]"), index);
		strPId.Format(_T("pid_list[%d]"), index);
		mapContent[strId] = strUserId;
		mapContent[strPId] = strParentId;
	}
	lock2.Unlock();

	if(index > 0)
	{
		DWORD status = webService.Post(strMethod, mapContent); //发起Post请求
		char* resBuf = webService.m_pResBuf; //给出的响应缓冲区，使用完需用delete释放掉

		using namespace std;
		string strJson;
		if(resBuf)
		{
			strJson.assign(resBuf);
			delete resBuf;
			resBuf = NULL;
		}
		ICTDBLOGA("[LAPPHEAD] - POST response = %s", strJson.c_str());
		if(status == HTTP_STATUS_OK)
		{
			Json::Value root;
			Json::Reader reader;
			if (reader.parse(strJson, root))
			{
				if (!root.isMember("status"))
				{
					CELGGWebService::ProcessWebServiceError(root);
				}

				if (root.isMember("result"))
				{
					Json::Value result = root["result"];
					for (int i = 0 ; i < result.size() ; i++)
					{
						CString strUserId, strParentId, strVersion, strImageUrl;
						if(result[i].isMember("id"))
						{
							string temp = result[i]["id"].asString();
							Char2CString(temp.c_str(), strUserId);
						}
						if(result[i].isMember("pid"))
						{
							string temp = result[i]["pid"].asString();
							Char2CString(temp.c_str(), strParentId);
						}
						if (result[i].isMember("data"))
						{
							Json::Value data = result[i]["data"];
							if(data.empty())
								continue;

							if(data.isMember("nodeversion"))
							{
								Json::Value arryObj = data["nodeversion"];
								for (Json::ValueIterator index = arryObj.begin(); index != arryObj.end() ; ++index)
								{
									string temp = (*index).asString();
									Char2CString(temp.c_str(), strVersion);
								}
							}

							if(data.isMember("imgurl"))
							{
								Json::Value arryObj = data["imgurl"];
								for (Json::ValueIterator index = arryObj.begin(); index != arryObj.end() ; ++index)
								{
									string temp = (*index).asString();
									Char2CString(temp.c_str(), strImageUrl);
								}
							}
						}

						PUID_UPDATE pu;
						pu.puid.strParentId = strParentId;
						pu.puid.strUid = strUserId;
						pu.puid.bUpdatePhoto = false;
						lock.Lock();
						lock2.Lock();
						map<PUID, UPDATE_NODE>::iterator iter = g_loginData.mapUid2UpdateNode2.find(pu.puid);
						if(iter != g_loginData.mapUid2UpdateNode2.end())
						{
							iter->second.dwLastUpdateTime = GetTickCount();

							CPhBkGroup *pParentGroup = managerPB->GetPhBkGroupByGroupId(strParentId);
							if(!strParentId.IsEmpty() && pParentGroup)
							{
								CPhBkItem *pPhBkItem = pParentGroup->GetPhBkItemByUid(strUserId);
								if(pPhBkItem)
								{
									//用户有个性化头像但头像文件不存在时需要下载头像
									bool bNeedUpdatePhoto = false;
									if (iter->second.strVersion != strVersion ||
										(bNeedUpdatePhoto = (!strImageUrl.IsEmpty() && !UA_UTIL::IsFileExist(GetUserPortraitPath(strUserId)))))
									{
										if (bNeedUpdatePhoto || pPhBkItem->m_strPhoto != strImageUrl)
										{
											pu.puid.bUpdatePhoto = true;
										}
										pu.un.strVersion = strVersion;
										vecUids.push_back(pu);
									}
									else
									{
										pPhBkItem->m_dwLastUpdateTime = iter->second.dwLastUpdateTime;
									}
								}
							}
							else
							{
								CPhBkItem *pPhBkItem = ua->GetPhBkItemByUid(strUserId);
								if(pPhBkItem)
								{
									if(iter->second.strVersion != strVersion)
									{
										if(pPhBkItem->m_strPhoto != strImageUrl)
										{
											pu.puid.bUpdatePhoto = true;
										}
										pu.un.strVersion = strVersion;
										pu.puid.strParentId = ((CPhBkGroup*)(pPhBkItem->m_pParent))->m_stCommonGroup.GetGroupId();
										vecUids.push_back(pu);
									}
									else
									{
										pPhBkItem->m_dwLastUpdateTime = iter->second.dwLastUpdateTime;
									}
								}
							}
						}
						lock2.Unlock();
						lock.Unlock();

						DEBUGCODE(Tracer::Log(_T("[CBCSClientDlg::UpdateNodeForUser] [strParentId]:[%s] [strUserId]:[%s] [bUpdatePhoto]:[%d] [strVersion]:[%s]"), pu.puid.strParentId, pu.puid.strUid, pu.puid.bUpdatePhoto, strVersion);)
					}
				}
			}
		}

		for(vector<PUID_UPDATE>::iterator iterV = vecUids.begin(); iterV != vecUids.end(); iterV++)
		{
			PUID_UPDATE pu = (*iterV);
			CString strParentId = pu.puid.strParentId;
			CString strUserId = pu.puid.strUid;
			bool bUpdatePhoto = pu.puid.bUpdatePhoto;

			ua->GetPGMManager()->UpdatePhBkInfo(2, strParentId, strUserId, bUpdatePhoto, UpdatePhBkCallBack);

			lock.Lock();
			CPhBkGroup *pParentGroup = managerPB->GetPhBkGroupByGroupId(strParentId);
			if(pParentGroup)
			{
				CPhBkItem *pPhBkItem = pParentGroup->GetPhBkItemByUid(strUserId);
				if(pPhBkItem)
				{
					pPhBkItem->m_dwLastUpdateTime = GetTickCount();
					pPhBkItem->m_strVer = pu.un.strVersion;
				}
			}
			lock.Unlock();
		}
	}

}

int32 UpdateVisibleUserInfo(PGList groupIdLists)
{
	if(!groupIdLists)
		return -1;

	CSingleLock lock(&g_loginData.csEGI);
	lock.Lock();
	g_loginData.vecExpandGroupIds.clear();
	PGList pList = groupIdLists;
	while(pList)
	{
		char *pGroupId = (char*)(pList->data);
		if(pGroupId)
		{
			CString strGroupId;
			Char2CString(pGroupId, strGroupId);
			g_loginData.vecExpandGroupIds.push_back(strGroupId);
		}
		pList = pList->next;
	}
	lock.Unlock();
	ReleaseGList(groupIdLists);

	if (!g_loginData.hUpdateThread1)
		g_loginData.hUpdateThread1 = CreateThread(NULL, 0, UpdateNodeThread1, NULL, 0, NULL);

	return S_SUC;
}

int32 FastUpdateUserInfo(CString strUserId)
{
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	if(!ua) return -1;

	CPhBkGroupManager *managerPB = ua->GetPGMManager()->GetPhBkManager(ua->GetUserID(), PB_ENTERPRISE_ADDRESS_BOOK);
	if(!managerPB)
		return -2;

	CSingleLock lock(&managerPB->m_cs);
	lock.Lock();

	CPhBkItem *item = ua->GetPhBkItem(strUserId, PBI_UID);
	if (!item)
		return -3;

	//做更新检查
	PUID_UPDATE pu;
	pu.puid.strParentId = item->m_pParent? ((CPhBkGroup*)(item->m_pParent))->m_stCommonGroup.GetGroupId() : _T("");
	pu.puid.strUid = item->m_strUID;
	pu.un.nType = 1;
	pu.un.dwLastUpdateTime = item->m_dwLastUpdateTime;
	pu.un.strVersion = item->m_strVer;

	lock.Unlock();

	CSingleLock lock3(&g_loginData.csUN3);
	lock3.Lock();
	g_loginData.queUid2UpdateNode3.push(pu);

	if (!g_loginData.hUpdateThread3)
	{
		g_loginData.hUpdateThread3 = CreateThread(NULL, 0, UpdateNodeThread3, NULL, 0, NULL);
	}
	lock3.Unlock();

	return 0;
}

int32 FastUpdateUserInfo(char8* uid)
{
	CString strUserId;
	Char2CString(uid, strUserId);
	return FastUpdateUserInfo(strUserId);
}

// 更新临时版本号
void UpdateDataVerToTemp(DATA_VER_TYPE dvt, int nVer)
{
	switch(dvt)
	{
	case DVT_ENT:
		g_loginData.stLDV.nEntVer = nVer;
		break;
	case DVT_ROLE:
		g_loginData.stLDV.nRoleVer = nVer;
		break;
	case DVT_GROUP:
		g_loginData.stLDV.nGroupVer = nVer;
		break;
	case DVT_LAPP:
		g_loginData.stLDV.nLAPPVer = nVer;
		break;
	}}

void DoUpdateMsgDatabase(const char *pAcc, const char *pServ)
{
	CUserAgentCore *ua = CUserAgentCore::GetUserAgentCore();
	if(!ua || !pAcc || !pServ)
		return;	
	CString strPersonalDir = ua->GetUserPath(pAcc, pServ);
	if(strPersonalDir.IsEmpty())
		return;

	//提取当前个人目录下的消息数据库的最大版本号
	int nMaxVer = 0;
	vector<CString> ret;
	UA_UTIL::FindFile(strPersonalDir, _T("MessageV*.db"), false, ret);
	for(vector<CString>::const_iterator iter = ret.begin(); iter != ret.end(); iter++)
	{
		CString strPath = (*iter);
		int nIndex = strPath.Find(_T("MessageV"));
		if(nIndex != -1)
		{
			CString strVer = strPath.Mid(nIndex+CString(_T("MessageV")).GetLength(), 3);
			int nVer = _wtoi(strVer);
			if(nVer > nMaxVer)
			{
				nMaxVer = nVer;
			}
		}
	}

	if(nMaxVer >= MSG_DB_VERSION)
	{
		//本地已存在的数据库的版本号大于当前数据库的版本号，则不更新
		return;
	}

	//从消息数据库1转到消息数据库2
	CMessageManager *pMessageMan1 = new CMessageManager();
	CMessageManager *pMessageMan2 = new CMessageManager();
	CString strName1, strName2;
	strName1.Format(_T("MessageV%d.db"), nMaxVer);
	strName2.Format(_T("MessageV%d.db"), MSG_DB_VERSION);
	pMessageMan1->SetParams(strPersonalDir, strName1, _T(""));
	pMessageMan2->SetParams(strPersonalDir, strName2, _T(""));


	//执行复制和升级操作
	pMessageMan2->CopyAndUpdateMessage(pMessageMan1);

	delete pMessageMan1, delete pMessageMan2;

	//删除旧数据库文件（为避免升级失败导致数据丢失，暂且不删除）
}
void SetGroupHeadPortraitLoadingCallback(pGroupHeadPortraitLoading  cb)
{
		CUserAgentCore::GetUserAgentCore()->SetHandlerGroupImageLoading(cb);
}
void getconfig(APP_CONFIG& config)
{
	Man_appConfig = config;
}