class CItemDropInfo
{
public:
	CItemDropInfo(int iLevelStart, int iLevelEnd, int iPercent, DWORD dwVnumStart, DWORD dwVnumEnd, BYTE bCount = 1, BYTE bJob = 0) :
		m_iLevelStart(iLevelStart), m_iLevelEnd(iLevelEnd), m_iPercent(iPercent), m_dwVnumStart(dwVnumStart), m_dwVnumEnd(dwVnumEnd), m_bCount(bCount), m_bJob(bJob)
	{
	}

	int	m_iLevelStart;
	int	m_iLevelEnd;
	int	m_iPercent; // 1 ~ 1000
	DWORD	m_dwVnumStart;
	DWORD	m_dwVnumEnd;
	BYTE	m_bCount;
	BYTE	m_bJob;

	friend bool operator < (const CItemDropInfo& l, const CItemDropInfo& r)
	{
		return l.m_iLevelEnd < r.m_iLevelEnd;
	}
};

extern std::vector<CItemDropInfo> g_vec_pkCommonDropItem[MOB_RANK_MAX_NUM];

typedef struct SDropItem
{
	int		iLvStart;
	int		iLvEnd;
	float	fPercent;
	char	szItemName[ITEM_NAME_MAX_LEN + 1];
	int		iCount;
	int		iJob;
} TDropItem;

