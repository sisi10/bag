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
	//Ψһ���
	char8 uid[255];
	//�˺�, ͨ�����ֻ���
	char8 acc[32];
	//����, ������16�ֽ�
	char8 pwd[32];
	//����
	char8 realm[255];
	//�����˺�
	BOOL saveAccInfo;
	//�Զ���¼ѡ��
	BOOL autologin;
	//�Զ�����
	BOOL autoRun;
   //����ѡ�������Ҫ���
} ICTACCINFO, *PICTACCINFO;

typedef enum{
	INVALID_DATA = 0, //��Ч����������
	LOGIN_BEGIN, //��¼��ʼ	
	LOGIN_NETWORK_AUTH_SUCC, //������֤�ɹ�
	LOGIN_NETWORK_AUTH_FAIL, //������֤ʧ��
	LOGIN_OFFLINE_AUTH_SUCC, //������֤�ɹ�
	LOGIN_OFFLINE_AUTH_FAIL, //������֤ʧ��	
	LOGIN_NETWORK_DISCONNECT, //����δ����	
	LOGIN_UPDATE_USERSELF_UI, //�����û��Լ���UI��������ǩ����ͷ��
	LOGIN_UPDATE_ENTPB_UI, //������ҵͨѶ¼�Ľ���
	LOGIN_UPDATE_PERSONALPB_UI, //���¸���ͨѶ¼�Ľ���
	LOGIN_UPDATE_GROUP_UI, //����Ⱥ��Ľ���
	LOGIN_UPDATE_REMSG_UI, //���������Ϣ�Ľ���
	LOGIN_UPDATE_CAREC_UI, //����ͨ����¼�Ľ���
	LOGIN_UPDATE_LAPP_UI, //������Ӧ�õĽ���
	LOGIN_SWITCH_MAIN_UI, //�л���������
	LOGIN_OFFLINE_TIPS, //���ߵ�¼��ʾ	
	LOGIN_UNREAD_MSG_P2P_TIPS, //һ��һ��δ����Ϣ������
	LOGIN_UNREAD_MSG_GROUP_TIPS, //Ⱥ���δ����Ϣ������
	LOGIN_UPGRADE_TIPS,//�������ѣ�ֻ�������������ṹ��PUPGRADE_UI_DATA����UI�����Ƿ�������
}LOGIN_STATE_CODE;

//��¼����
typedef enum _login_type
{
	INVALID_TYPE = -1, //��Ч��¼����
	OFFLINE_LOGIN = 0, //���ߵ�¼
	NETWORK_LOGIN = 1, //������¼

}LOGINTYPE;

//�������͵ĵ�¼״̬
#define LOGIN_STA_LOADED_LOCAL_DATA 0
#define LOGIN_STA_LOADED_SERVER_DATA 1
#define LOGIN_STA_UPDATE_UI 2
#define LOGIN_STA_OFFLINE 3
#define LOGIN_STA_NETWORK 4

//���ݼ���״̬
#define LOADED_DATA_ENTERPRISE_PHONEBOOK   0x01
#define LOADED_DATA_PERSONAL_PHONEBOOK   0x02
#define LOADED_DATA_GROUPS   0x04
#define LOADED_DATA_PUB_ACCOUNT   0x08
#define LOADED_DATA_RECENT_CONTACT   0x10
#define LOADED_DATA_CALL_RECORD   0x20
#define LOADED_DATA_CUR_LOGIN_USER   0x40

//�������״̬
#define UPDATE_UI_ENTERPRISE_PHONEBOOK   0x01
#define UPDATE_UI_PERSONAL_PHONEBOOK   0x02
#define UPDATE_UI_GROUPS   0x04
#define UPDATE_UI_PUB_ACCOUNT   0x08
#define UPDATE_UI_RECENT_CONTACT   0x10
#define UPDATE_UI_CALL_RECORD   0x20
#define UPDATE_UI_CUR_LOGIN_USER   0x40

//����������״̬
#define LOGIN_SUCCESS 0x01
#define MQTT_CONNECT_SUCCESS 0x02
#define SIP_CONNECT_SUCCESS 0x04

//��¼״̬�ṹ
typedef struct _login_state 
{
public:		
	LOGINTYPE enLoginType; //��¼����
	long lOfflineLoginSta; //���ߵ�¼״̬����һλ�����¼�ɹ����0����¼ʧ�ܣ�1����¼�ɹ�������λ����
	long lNetworkLoginSta; //���ߵ�¼״̬����һλ�����¼�ɹ����0����¼ʧ�ܣ�1����¼�ɹ����ڶ�λ����MQTT��¼״̬������λ����SIP��¼״̬������λ����	
	long lLoadDataStaFromLocal; //�������ݵļ���״̬��ǰ��λ���δ�����ҵͨѶ¼������ͨѶ¼��Ⱥ�顢�����˺š������ϵ�ˡ�ͨ����¼����ǰ��¼�û�����
	long lLoadDataStaFromServer; //���������ݵļ���״̬��ǰ��λ���δ�����ҵͨѶ¼������ͨѶ¼��Ⱥ�顢�����˺�
	long lUIUpdateSta; //�������״̬��ǰ��λ���δ�����ҵͨѶ¼������ͨѶ¼��Ⱥ�顢�����˺š������ϵ�ˡ�ͨ����¼	
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

//��ǰ��¼�û�����
typedef struct _cur_user_ui_data
{
	char8 uid[256]; //�û�id
	char8 name[256]; //����
	char8 sig[256]; //ǩ��	

}CUR_USER_UI_DATA, *PCUR_USER_UI_DATA;

//�����ϵ��UI���ݣ���Ҫ�����������ͼ
typedef struct _recent_contact_ui_data
{
	char8 uid[256]; //�û�ΨһUId
	char8 sipUri[256]; //�û�sip�˺�
	int32 chatType; //��������
	char8 name[256]; //������������
	//char8 headPath[256]; //��������ͷ��·��
	int32 status; //��������״̬
	char8 msgContent[1024*2]; //��Ϣ����
	int64 msgTime; //��Ϣʱ��
	int32  unreadNum; //δ����Ϣ����
} RECENT_CONTACT_UI_DATA, *PRECENT_CONTACT_UI_DATA;

//���м�¼UI���ݣ���Ҫ�����������ͼ
typedef struct _call_record_ui_data
{
	char8 uid[256]; //�û�ΨһUId
	char8 sipUri[256]; //�û�sip�˺�
	int32 callType; //ͨ������
	char8 name[256]; //������������
	char8 headPath[256]; //��������ͷ��·��
	int32 status; //��������״̬	
	char8 numCallback[256]; //�غ�����
	char8 displayNum[256]; //��ʾ����
	char8 callTime[256]; //����ʱ��

} CALL_RECORD_UI_DATA, *PCALL_RECORD_UI_DATA;

//��Ӧ�����ݽṹ
typedef struct _lapp_data
{
	char8 uid[256]; //�û�id
	char8 name[256]; //����
	char8 desc[256]; //����
	int32 show_way; //չ�ַ�ʽ
}LAPP_UI_DATA, *PLAPP_UI_DATA;

//�����ṹ��
typedef struct _upgrade_ui_data
{
	char8 updateLink[256]; //������ַ
	BOOL  bCompeletedUpdate; //����������ȫ������ TRUE:ȫ�� FALSE������
	char8 version[32]; //�����汾	
	char8 forceVersion[32]; //ǿ�������汾	
}UPGRADE_UI_DATA, *PUPGRADE_UI_DATA;
// interface of account info

//add by zhangpeng on 20160530 begin
//ȷ��Ψһ�û��Ľṹ
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

//���½ڵ�ṹ
typedef struct _update_node
{
	int nType; //�ڵ����ͣ�0�����ţ�1���û�
	DWORD dwLastUpdateTime; //�ϴθ���ʱ��
	CString strVersion; //��ǰ���صİ汾��

}UPDATE_NODE, *PUPDATE_NODE;

//�û��ڵ���½ṹ
typedef struct _puid_update
{
	PUID puid;
	UPDATE_NODE un;

}PUID_UPDATE, *PPUID_UPDATE;

//��¼���ݽṹ
typedef struct _login_data
{
public:
	bool bLogin[3]; //��¼ʱ������
	bool bTokenExpire; //��¼�Ƿ���� //û�õ���������
	bool bEntPBVerChange; //��ҵͨѶ¼�İ汾���Ƿ����仯
	bool bPersonalPBVerChange; //����ͨѶ¼�İ汾���Ƿ����仯
	bool bUpdateLinkChange; //���µ�ַ�Ƿ����仯
	volatile bool bNeedUpdate; //�Ƿ���Ҫ���µı��

	bool bCheckedIP;
	CCriticalSection  csIPCheckcs;

	volatile __time64_t tokenExpireTime; // token����ʱ��

	void* hLoadLAPPThread;  //��Ӧ�û�ȡ�߳�
	void* hLoadPBThread; //ͨѶ¼����ҵ�͸��ˣ����ݼ����߳�
	void* hLoadGroupThread; //Ⱥ�����ݼ����߳�
	void* hLoadRecentContactThread; //�����ϵ�����ݼ����߳�
	void* hLoadCallRecordThread; //ͨ����¼���ݼ����߳�
	//20170603 gavin add
	void* hLoadPersonalPBThread; //����ͨѶ¼���ݼ����߳�
	//20170603 gavin end
	void* hUpdateCheckThread; //��������߳�
	void* hGetUserInfoThread; //��ȡ�û���Ϣ�߳�

	CCriticalSection csLoginState; //��¼״̬�ٽ���
	LOGIN_STATE loginState; //��¼״̬

	PHONEBOOKITEM pbItemSelf; //��ǰ��¼�û���ͨѶ¼����

	PGList listCallRecord; //ͨ����¼����

	//���°汾�Žṹ
	LAST_DATA_VER stLDV;

	map<CString, UPDATE_NODE> mapUid2UpdateNode1; //�ṹ1������ڵ�ĸ���
	map<PUID, UPDATE_NODE> mapUid2UpdateNode2; //�ṹ2�����û��ڵ�ĸ���
	queue<PUID_UPDATE> queUid2UpdateNode3; //�ṹ3�����û����ӹ�ϵ�Ľڵ����
	queue<PUID_UPDATE> queUid2UpdateNode3s; //�ṹ3�����û����ӹ�ϵ�Ľڵ����
	queue<PUID> queUid2UpdateNode4; //�ṹ4������Ϣ���������û��ĸ���
	//20170612 gavin add
	queue<PUID> queUid2UpdateNode5; //�ṹ5������Ϣ�������ĸ���ͨѶ¼���û����ݵĸ���
	//20170612 gavin end
	queue<UPDATE_PHBK_UI> queUpdatePhBkUI; //ͨѶ¼������½ṹ
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

	void* hUpdateThread1; //ִ����ڵ�ĸ���
	void* hUpdateThread2; //ִ���û��ڵ�ĸ���
	void* hUpdateThread3; //�Ե�ǰ�û����ӹ��ĵĽڵ�ִ�и���
	void* hUpdateThread4; //����Ϣ���������û����к�̨����
	void* hUpdateThread5; //����Ϣ�������ĸ���ͨ��¼�û����к�̨����
	void* hUpdateThread3s; //�Ե�ǰ�û����ӹ��ĵĽڵ�ִ�и���

	UI_PARA2 uiPara[2]; //��������ṹ, 0����¼��1���ǳ�

	CUserAgentCore *pUserAgentCore; //ua����

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

//��¼����
extern LOGIN_DATA g_loginData;
//��¼����
extern APP_CONFIG Man_appConfig;
void getconfig(APP_CONFIG& config);
extern BOOL boIsAutoLogin;//�Ƿ�elgg��mqtt��sipһ���½/TRUE =  ֱ�ӵ�¼/FALSE = ��¼elgg��mqtt��qsip������¼ for sdk
//add by zhangpeng on 20160530 end

//��������߳�����
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
*  @remarks     ���µ�½״̬�����濪����ֻ��Ҫ���½���״̬��
usage example��
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
*  @remarks     �����߸����ͷ����list
usage example��
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
*  @remarks     ֻ����δ��¼������¸ýӿڲŻ���ȷ����
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
					usage example��
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
*  @remarks     ֻ�ܶ��Ѿ���¼���˺Ž��������޸ģ��������ע״̬���أ�cb��userdata����Ϊ��
*  @see          
*******************************************************************************/
int32 UpdatePassword(char8* newpwd,void* userdata = NULL,pWSCallback cb = NULL);

//20170617 gavin 
//�����ϴε�¼�û������û��˺���Ϣ
int32 AM_ConfigUser();
int32 AM_ConfigUser(PGList* pacclist);
//�����û��˺���Ϣ
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
*  @brief       		��ȡ�����ַ
*  @param[in/out]		clist: ��Ӧ���б� PGList��data�ṹ��Ϊ PLAPP_UI_DATA
*  @return       		suc return zero, otherwise none zero  	
*  @remarks			    �ڸ������б���ʾ��ͷ����LAPPĿ¼����uid��ʹ�ú�ע���ͷ�
*******************************************************************************/
int32 GetLAPPList(PGList* clist);

/*******************************************************************************
*  @fun         		GetLAPPUrl
*  @brief       		��ȡ�����ַ
*  @param[in]		    uid: ��Ӧ��id ����GetLAPPList
*  @param[in/out]		url: ����url //��������С����8192
*  @return       		suc return zero, otherwise none zero  	
*  @remarks			    urlֱ����WEB�ؼ���չʾ
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
*  @brief       		������Ӧ�õ�ַ��ͼƬ
*  @param[in/out]		cb: �ص� ��������ͨ��GetLAPPUrl��ȡ
*  @return       		suc return zero, otherwise none zero  	
*  @remarks			    urlֱ����WEB�ؼ���չʾ
*******************************************************************************/
int32 DownloadLAPP(pWSCallback cb);

//************************************
// Method:    GetPrefix
// FullName:  ��ȡ������
// Access:    public 
// Returns:   int32 suc return zero, otherwise none zero  	
// Qualifier: 
// Parameter[in/out]: char8 * prefix char prex[256]
//************************************
int32 GetPrefix(char8* prefix);

//************************************
// Method:    GetCurPsersonalDir
// FullName:  ��ȡ��ǰ�û�Ŀ¼
// Access:    public 
// Returns:   int32 suc return zero, otherwise none zero  	
// Qualifier: 
// Parameter[in/out]: char8 * dir Ŀ¼·��
//************************************
int32 GetCurPsersonalDir(char8* dir);

/*******************************************************************************
*  @fun         		UpdateVisibleUserInfo
*  @brief       		���¿ɼ���ϵ����Ϣ
*  @param[in]			groupIdLists: ����չ���Ĳ��ŵ�ID�б��ɱ������߸����ͷţ�GList�е�data�ֶ�����Ϊchar*
*  @return       		suc return zero, otherwise none zero  	
*  @remarks			    ����Ҫ���¿ɼ���ϵ����Ϣʱ�����ô˽ӿڣ�������չ���Ĳ��ŵ�ID�б���
*******************************************************************************/
int32 UpdateVisibleUserInfo(PGList groupIdLists);
/*******************************************************************************
*  @fun         		FastUpdateUserInfo
*  @brief       		��������ĳ���˵���Ϣ
*  @param[in]			uid: �û�uid
*  @return       		suc return zero, otherwise none zero  	
*  @remarks			    ����Ҫ��������ĳ���˵���Ϣʱ�����Ե��ô˽ӿڡ���������촰�ڣ����Ե��ô˽ӿڣ������û���uid����
*******************************************************************************/
int32 FastUpdateUserInfo(char8* uid);

/*******************************************************************************
*  @fun         		DoFullPhBkUpdate
*  @brief       		ͨѶ¼ȫ�����·���
*  @remarks			    ����Ҫ�ֶ�����ͨѶ¼��ȫ������ʱ�����ô˽ӿڼ���
*******************************************************************************/
void DoFullPhBkUpdate();

//����Token
void UpdateAuthToken(BOOL bForceUpdate = FALSE);
//��ȡ�����Ϣ  chattype = CT_GroupʱֻȡȺ����Ϣ
void GetLastIM(ChatType chattype, PGList* plst);


void SetGroupHeadPortraitLoadingCallback(pGroupHeadPortraitLoading  cb);//�ⲿ����
#endif

//ɾ����Ϣ�б��ĳ����¼
int32 DelRecord(char8* uid);
int32 SetTargetDeccriptionDeleteFlag(char8* uid);//���ô��б�ɾ���ĶԻ�target_description��־Ϊ1
//����target_id��ֵΪ2˵������Ϣ�Ѿ�����Ϣ�б���ɾ��
int32 SetTargetDescriptionDelete(char8* uid);
//����target_id����target_description��ֵ  1���ö� 0:��ͨ
int32 SetTargetDescriptionTop(char8* uid);