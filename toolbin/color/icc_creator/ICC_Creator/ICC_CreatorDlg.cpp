/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


#include "stdafx.h"
#include "ICC_Creator.h"
#include "ICC_CreatorDlg.h"
#include "icc_create.h"
#include <math.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CICC_CreatorDlg dialog




CICC_CreatorDlg::CICC_CreatorDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CICC_CreatorDlg::IDD, pParent)
        , m_effect_desc(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CICC_CreatorDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDITTHRESH, m_graythreshold);
    DDX_Text(pDX, IDC_EDIT1, m_effect_desc);
    DDX_Control(pDX, IDC_EDIT1, m_desc_effect_str);
}

BEGIN_MESSAGE_MAP(CICC_CreatorDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
        ON_BN_CLICKED(IDC_CIELAB, &CICC_CreatorDlg::OnBnClickedCielab)
        ON_BN_CLICKED(IDC_NAMES, &CICC_CreatorDlg::OnBnClickedNames)
        ON_BN_CLICKED(IDC_ICC_PROFILE, &CICC_CreatorDlg::OnBnClickedIccProfile)
        ON_BN_CLICKED(IDC_ICC_HELP, &CICC_CreatorDlg::OnBnClickedIccHelp)
        ON_BN_CLICKED(IDC_GRAY2CMYK, &CICC_CreatorDlg::OnBnClickedGray2cmyk)
        ON_BN_CLICKED(IDC_CMYK2RGB, &CICC_CreatorDlg::OnBnClickedCmyk2rgb)
        ON_BN_CLICKED(IDC_RGB2CMYK, &CICC_CreatorDlg::OnBnClickedRgb2cmyk)
        ON_BN_CLICKED(IDC_CMYK2GRAY2, &CICC_CreatorDlg::OnBnClickedCmyk2gray2)
        ON_BN_CLICKED(IDC_PSICC, &CICC_CreatorDlg::OnBnClickedPsicc)
        ON_BN_CLICKED(IDC_GRAYTHRESH, &CICC_CreatorDlg::OnBnClickedGraythresh)
        ON_EN_CHANGE(IDC_EDITTHRESH, &CICC_CreatorDlg::OnEnChangeEditthresh)
        ON_BN_CLICKED(IDC_PSTABLES, &CICC_CreatorDlg::OnBnClickedPstables)
        ON_BN_CLICKED(IDC_CHECK1, &CICC_CreatorDlg::OnBnClickedCheck1)
        ON_BN_CLICKED(IDC_EFFECTTABLES2, &CICC_CreatorDlg::OnBnClickedEffecttables2)
        ON_BN_CLICKED(IDC_EFFECTICC3, &CICC_CreatorDlg::OnBnClickedEffecticc3)
        ON_EN_CHANGE(IDC_EDIT1, &CICC_CreatorDlg::OnEnChangeEdit1)
END_MESSAGE_MAP()


// CICC_CreatorDlg message handlers

BOOL CICC_CreatorDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here

        this->m_num_icc_colorants = 0;
        this->m_num_colorant_names = 0;
        this->m_sample_rate = 0;
        this->m_cielab = NULL;
        this->m_colorant_names = NULL;  
        this->m_cpsi_mode = false;
        this->m_ucr_bg_data = NULL;
        this->m_effect_data = NULL;
        this->SetDlgItemText(IDC_STATUS,_T("Ready."));
        this->m_floatthreshold_gray = 50;
        this->m_effect_desc.Preallocate(0);
        this->m_graythreshold.SetWindowText(_T("50"));

        return TRUE;  // return TRUE  unless you set the focus to a control
}

void CICC_CreatorDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CICC_CreatorDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CICC_CreatorDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CICC_CreatorDlg::OnBnClickedCielab()
{
    /* Load the CIELAB data.  Data is preceded
       by a two line header.  First line is the
       number of colorants.  Second line is the
       number of samples in each colorant 
       direction (a single number since they
       must be sampled with the same number for 
       now).  Data is then after that.  */

    int code;
    TCHAR szFile[MAX_PATH];
    ZeroMemory(szFile, MAX_PATH);
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize	= sizeof(OPENFILENAME);
    ofn.Flags		= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.hwndOwner	= this->m_hWnd;

    ofn.lpstrFilter	= _T("Supported Files Types(*.txt)\0*.txt;*.TXT\0\0");

    ofn.lpstrTitle	= _T("Load CIELAB File");
    ofn.lpstrFile	= szFile;
    ofn.nMaxFile	= MAX_PATH;

    if (IDOK == GetOpenFileName(&ofn)){

        code = this->GetCIELAB(szFile);

        if (code == 0)
            this->SetDlgItemText(IDC_STATUS,_T("CIELAB Data Loaded"));


    } else {

        this->SetDlgItemText(IDC_STATUS,_T("CIELAB file failed to open!"));

    }







}

void CICC_CreatorDlg::OnBnClickedNames()
{
    /* Load the Names data.  Data is preceded
       by a one line header.  First line is the
       number of colorants.  Data is then after that.  */

    int code;
    TCHAR szFile[MAX_PATH];
    ZeroMemory(szFile, MAX_PATH);
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize	= sizeof(OPENFILENAME);
    ofn.Flags		= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.hwndOwner	= this->m_hWnd;

    ofn.lpstrFilter	= _T("Supported Files Types(*.txt)\0*.txt;*.TXT\0\0");

    ofn.lpstrTitle	= _T("Load Names File");
    ofn.lpstrFile	= szFile;
    ofn.nMaxFile	= MAX_PATH;

    if (IDOK == GetOpenFileName(&ofn)){

        code = this->GetNames(szFile);
        if (code == 0)
            this->SetDlgItemText(IDC_STATUS,_T("Name Data Loaded"));

    } else {

        this->SetDlgItemText(IDC_STATUS,_T("Names file failed to open!"));

    }

}

void CICC_CreatorDlg::OnBnClickedIccProfile()
{

    /* Check that the data is OK */

    int ok;

    if (this->m_num_colorant_names != this->m_num_icc_colorants || this->m_num_colorant_names < 2) {

        this->SetDlgItemText(IDC_STATUS,_T("Number colorants wrong!"));
        return;

    }

    /* Should be good.  Create the ICC profile */

    TCHAR szFile[MAX_PATH];
    ZeroMemory(szFile, MAX_PATH);
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize	= sizeof(OPENFILENAME);
    ofn.Flags		= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.hwndOwner	= this->m_hWnd;

    ofn.lpstrFilter	= _T("Supported Files Types(*.icc)\0*.icc;*.ICC\0\0");

    ofn.lpstrTitle	= _T("Load Names File");
    ofn.lpstrFile	= szFile;
    ofn.nMaxFile	= MAX_PATH;

    if (IDOK == GetSaveFileName(&ofn)){

         ok = create_devicen_profile(m_cielab, m_colorant_names, m_num_icc_colorants, m_sample_rate, szFile);
        
        if (ok == 0)
            this->SetDlgItemText(IDC_STATUS,_T("ICC Profile Created"));

    } 

  
}

int CICC_CreatorDlg::GetCIELAB(LPCTSTR lpszPathName)
{
    FILE *fid;	
    short k;
    int val;
    long num_samps;

    // open the file to read

    fid=fopen((const char*) lpszPathName,"r");

    /* First read in the header information */

    val = fscanf(fid,"%d",&(this->m_num_icc_colorants));
    
    if (m_num_icc_colorants < 2 || m_num_icc_colorants > 15 ) {

        this->SetDlgItemText(IDC_STATUS,_T("Number colorants out of range"));
        return(-1);
    }

    val = fscanf(fid,"%d",&(this->m_sample_rate));
    
    if (m_sample_rate < 2 || m_sample_rate > 256 ) {

        this->SetDlgItemText(IDC_STATUS,_T("Sample rate out of range"));
        return(-1);
    }

    /* Allocate the space */

    if (m_cielab)
        free(m_cielab);

    num_samps = (long) pow((double) m_sample_rate, (long) m_num_icc_colorants);

    m_cielab = (cielab_t*) malloc( sizeof(cielab_t) * num_samps );

    if (m_cielab == NULL) {

        this->SetDlgItemText(IDC_STATUS,_T("CIELAB malloc failed"));
        return(-1);

    }

    /* Read in the CIELAB data */

    for( k = 0; k < (num_samps) ; k++ ){

        val=fscanf(fid,"%f",&(m_cielab[k].lstar));
        val=fscanf(fid,"%f",&(m_cielab[k].astar));
        val=fscanf(fid,"%f",&(m_cielab[k].bstar));

    }

    fclose(fid);
    return(0);

}




int CICC_CreatorDlg::GetNames(LPCTSTR lpszPathName)
{

    FILE *fid;	
    short k;
    int val;
    char *ptr;
    int done;

    /* open the file to read */

    fid=fopen((const char*) lpszPathName,"r");

    /* First read in the header information */

    val = fscanf(fid,"%d",&(this->m_num_colorant_names));
    
    if (m_num_colorant_names < 2 || m_num_colorant_names > 15 ) {

        this->SetDlgItemText(IDC_STATUS,_T("Number colorants out of range"));
        return(-1);
    }

    /* Allocate the space */

    if (m_colorant_names)
        free(m_colorant_names);

    m_colorant_names = (colornames_t*) malloc( sizeof(colornames_t) * m_num_colorant_names );

    if (m_colorant_names == NULL) {

        this->SetDlgItemText(IDC_STATUS,_T("Names malloc failed"));
        return(-1);

    }

    /* Read in the Names data.  Have to worry about spurious */

    done = 0;
    k = 0;
    while(!done)
    {

        ptr = &((m_colorant_names[k].name)[0]);

        fgets(ptr, MAX_NAME_SIZE-2, fid);
        m_colorant_names[k].length = strlen(ptr);

        if (m_colorant_names[k].length > 1) {

            /* Got one */

            k++;
            
        }

        if (k == m_num_colorant_names)
            done = 1;

    }

    fclose(fid);
    return(0);

}

void CICC_CreatorDlg::OnBnClickedIccHelp()
{

    /* Throw out the help/read me window */

}

void CICC_CreatorDlg::OnBnClickedCmyk2gray()
{
    // Create a device link profile that uses
    // default PS methods to map CMYK values
    // into gray

    int ok;
    TCHAR szFile[MAX_PATH];
    ZeroMemory(szFile, MAX_PATH);
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize	= sizeof(OPENFILENAME);
    ofn.Flags		= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |OFN_HIDEREADONLY;
    ofn.hwndOwner	= this->m_hWnd;

    ofn.lpstrFilter	= _T("Supported Files Types(*.icc)\0*.icc;*.ICC\0\0");

    ofn.lpstrTitle	= _T("Load Names File");
    ofn.lpstrFile	= szFile;
    ofn.nMaxFile	= MAX_PATH;

    if (IDOK == GetSaveFileName(&ofn)) {
        ok = create_devicelink_profile(szFile,CMYK2GRAY);
        if (ok == 0)
            this->SetDlgItemText(IDC_STATUS,_T("Link Profile Created"));
    } 
}

 

void CICC_CreatorDlg::OnBnClickedGray2cmyk()
    {
    // Create a device link profile that uses
    // default PS methods to map gray values
    // into CMYK

    int ok;
    TCHAR szFile[MAX_PATH];
    ZeroMemory(szFile, MAX_PATH);
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize	= sizeof(OPENFILENAME);
    ofn.Flags		= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.hwndOwner	= this->m_hWnd;

    ofn.lpstrFilter	= _T("Supported Files Types(*.icc)\0*.icc;*.ICC\0\0");

    ofn.lpstrTitle	= _T("Load Names File");
    ofn.lpstrFile	= szFile;
    ofn.nMaxFile	= MAX_PATH;

    if (IDOK == GetSaveFileName(&ofn)) {
         ok = create_devicelink_profile(szFile,GRAY2CMYK);
        if (ok == 0)
            this->SetDlgItemText(IDC_STATUS,_T("Link Profile Created"));
    } 
}
void CICC_CreatorDlg::OnBnClickedCmyk2rgb()
    {
    // Create a device link profile that uses
    // default PS methods to map gray values
    // into CMYK

    int ok;
    TCHAR szFile[MAX_PATH];
    ZeroMemory(szFile, MAX_PATH);
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize	= sizeof(OPENFILENAME);
    ofn.Flags		= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |OFN_HIDEREADONLY;
    ofn.hwndOwner	= this->m_hWnd;

    ofn.lpstrFilter	= _T("Supported Files Types(*.icc)\0*.icc;*.ICC\0\0");

    ofn.lpstrTitle	= _T("Load Names File");
    ofn.lpstrFile	= szFile;
    ofn.nMaxFile	= MAX_PATH;

    if (IDOK == GetSaveFileName(&ofn)) {
         ok = create_devicelink_profile(szFile,CMYK2RGB);
        if (ok == 0)
            this->SetDlgItemText(IDC_STATUS,_T("Link Profile Created"));
    } 
}
void CICC_CreatorDlg::OnBnClickedRgb2cmyk()
    {
    // Create a device link profile that uses
    // default PS methods to map gray values
    // into CMYK

    int ok;
    TCHAR szFile[MAX_PATH];
    ZeroMemory(szFile, MAX_PATH);
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize	= sizeof(OPENFILENAME);
    ofn.Flags		= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |OFN_HIDEREADONLY;
    ofn.hwndOwner	= this->m_hWnd;

    ofn.lpstrFilter	= _T("Supported Files Types(*.icc)\0*.icc;*.ICC\0\0");

    ofn.lpstrTitle	= _T("Load Names File");
    ofn.lpstrFile	= szFile;
    ofn.nMaxFile	= MAX_PATH;

    if (IDOK == GetSaveFileName(&ofn)) {
         ok = create_devicelink_profile(szFile,RGB2CMYK);
        if (ok == 0)
            this->SetDlgItemText(IDC_STATUS,_T("Link Profile Created"));
    } 
} 
void CICC_CreatorDlg::OnBnClickedCmyk2gray2()
{
    int ok;
    TCHAR szFile[MAX_PATH];
    ZeroMemory(szFile, MAX_PATH);
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize	= sizeof(OPENFILENAME);
    ofn.Flags		= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |OFN_HIDEREADONLY;
    ofn.hwndOwner	= this->m_hWnd;

    ofn.lpstrFilter	= _T("Supported Files Types(*.icc)\0*.icc;*.ICC\0\0");

    ofn.lpstrTitle	= _T("Load Names File");
    ofn.lpstrFile	= szFile;
    ofn.nMaxFile	= MAX_PATH;

    if (IDOK == GetSaveFileName(&ofn)) {
         ok = create_devicelink_profile(szFile,CMYK2GRAY);
        if (ok == 0)
            this->SetDlgItemText(IDC_STATUS,_T("Link Profile Created"));
    } 
}

/* Here we go ahead and create the RGB, CMYK and Gray default profiles
   that when used together will have behaviours very similar to the
   standard PS defined color mappings.  These are used for the GS code
   in the creation of soft masks in transparency. */

void CICC_CreatorDlg::OnBnClickedPsicc()
{
    int ok;
    TCHAR szFile[MAX_PATH];
    ZeroMemory(szFile, MAX_PATH);
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize	= sizeof(OPENFILENAME);
    ofn.Flags		= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |OFN_HIDEREADONLY;
    ofn.hwndOwner	= this->m_hWnd;
    ofn.lpstrFilter	= _T("Supported Files Types(*.icc)\0*.icc;*.ICC\0\0");
    ofn.lpstrTitle	= _T("Save PS Gray Profile");
    ofn.lpstrFile	= szFile;
    ofn.nMaxFile	= MAX_PATH;

    if (IDOK == GetSaveFileName(&ofn)) {
         ok = create_psgray_profile(szFile);
        if (ok == 0)
            this->SetDlgItemText(IDC_STATUS,_T("Created PS Gray Profile"));
    } 
    ofn.lpstrTitle	= _T("Save PS RGB Profile");
    if (IDOK == GetSaveFileName(&ofn)) {
         ok = create_psrgb_profile(szFile);
        if (ok == 0)
            this->SetDlgItemText(IDC_STATUS,_T("Created PS RGB Profile"));
    } 
    ofn.lpstrTitle	= _T("Save PS CMYK Profile");
    if (IDOK == GetSaveFileName(&ofn)) {
        ok = create_pscmyk_profile(szFile, false, this->m_cpsi_mode, this->m_ucr_bg_data);
        if (ok == 0)
            this->SetDlgItemText(IDC_STATUS,_T("Created PS CMYK Profile"));
    } 
}

void CICC_CreatorDlg::OnBnClickedGraythresh()
{

    int ok;
    TCHAR szFile[MAX_PATH];
    ZeroMemory(szFile, MAX_PATH);
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize	= sizeof(OPENFILENAME);
    ofn.Flags		= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |OFN_HIDEREADONLY;
    ofn.hwndOwner	= this->m_hWnd;
    ofn.lpstrFilter	= _T("Supported Files Types(*.icc)\0*.icc;*.ICC\0\0");
    ofn.lpstrTitle	= _T("Save Gray ICC Profile");
    ofn.lpstrFile	= szFile;
    ofn.nMaxFile	= MAX_PATH;

    if (IDOK == GetSaveFileName(&ofn)) {
         ok = create_gray_threshold_profile(szFile, this->m_floatthreshold_gray);
        if (ok == 0)
            this->SetDlgItemText(IDC_STATUS,_T("Created Gray Threshhold Profile"));
    } 
}

void CICC_CreatorDlg::OnEnChangeEditthresh()
{
    // TODO:  If this is a RICHEDIT control, the control will not
    // send this notification unless you override the CDialog::OnInitDialog()
    // function and call CRichEditCtrl().SetEventMask()
    // with the ENM_CHANGE flag ORed into the mask.

    // TODO:  Add your control notification handler code here
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here

	char str[25];
	int data;

	this->m_graythreshold.GetWindowText(str,24);
        sscanf(str,"%f",&(this->m_floatthreshold_gray));
        if (this->m_floatthreshold_gray < 0) {
            this->m_floatthreshold_gray = 0;
            this->m_graythreshold.SetWindowText(_T("0"));
        }
        if (this->m_floatthreshold_gray > 100) {
            this->m_floatthreshold_gray = 100;
            this->m_graythreshold.SetWindowText(_T("100"));
        }
}

int CICC_CreatorDlg::ParseData(char pszInFile[], bool is_ucr) {

    FILE *fid = NULL;
    unsigned char num_read;
    char header[256];
    int r, g, b, c, m, y, k;
    int j;
    ucrbg_t *data;

    
    /* Allocate space for the data */
    if (is_ucr) {
        this->m_ucr_bg_data = (ucrbg_t*) malloc(sizeof(ucrbg_t));
        data = this->m_ucr_bg_data;
    } else {
        this->m_effect_data = (ucrbg_t*) malloc(sizeof(ucrbg_t));
        data = this->m_effect_data;
    }
    if (data == NULL) {
        return -1;
    }
    data->cyan = (unsigned char*) malloc(256);
    data->magenta = (unsigned char*) malloc(256);
    data->yellow = (unsigned char*) malloc(256);
    data->black = (unsigned char*) malloc(256);
    if (data->cyan == NULL ||
        data->magenta == NULL ||
        data->yellow == NULL ||
        data->black == NULL) {
        return -1;
    }
    fid = fopen(pszInFile, "r");
    if (!fid) {
        return -1;
    }
    /* First line */
    fgets(&(header[0]),255,fid);
    for (j = 0; j < 256; j++) {
        num_read = fscanf(fid,"%d %d %d %d %d %d %d",&r, &g, &b, &c, &m, &y, &k);
        if(num_read != 7) 
            return -1;  
        if (c > 255) c = 255;
        if (c < 0) c = 0;
        if (m > 255) m = 255;
        if (m < 0) m = 0;
        if (y > 255) y = 255;
        if (y < 0) y = 0;
        if (k > 255) k = 255;
        if (k < 0) k = 0;
        data->cyan[j] = (unsigned char) c;
        data->magenta[j] = (unsigned char) m;
        data->yellow[j] = (unsigned char) y;
        data->black[j] = (unsigned char) k;
    }
    fclose(fid);
    return(0);
}

/* Load the table that can be used to define the relationships between 
   RGB, Gray, CMYK */
void CICC_CreatorDlg::OnBnClickedPstables()
{

    int returnval;
    TCHAR szFile[MAX_PATH];
    ZeroMemory(szFile, MAX_PATH);
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.hwndOwner = this->m_hWnd;

    ofn.lpstrFilter = _T("Supported Files Types(*.txt)\0*.txt\0TXT Files (*.)\0*.TXT\0\0");

    ofn.lpstrTitle = _T("Load Table Data");
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    if (IDOK == GetOpenFileName(&ofn)) {
        returnval = ParseData(szFile, true);
        if (returnval == 0) {
            this->SetDlgItemText(IDC_STATUS,"Data Loaded OK");
        } else {
            this->SetDlgItemText(IDC_STATUS,"Data Load Failed!");
            free(this->m_ucr_bg_data->cyan);
            free(this->m_ucr_bg_data->magenta);
            free(this->m_ucr_bg_data->yellow);
            free(this->m_ucr_bg_data->black);
            free(this->m_ucr_bg_data);
            this->m_ucr_bg_data = NULL;
        }
    }
}
void CICC_CreatorDlg::OnBnClickedCheck1()
{
    if (this->m_cpsi_mode == true) {
        this->m_cpsi_mode = false;
    } else {
        this->m_cpsi_mode = true;
    }
}

void CICC_CreatorDlg::OnBnClickedEffecttables2()
{
    int returnval;
    TCHAR szFile[MAX_PATH];
    ZeroMemory(szFile, MAX_PATH);
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.hwndOwner = this->m_hWnd;

    ofn.lpstrFilter = _T("Supported Files Types(*.txt)\0*.txt\0TXT Files (*.)\0*.TXT\0\0");

    ofn.lpstrTitle = _T("Load Table Data");
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    if (IDOK == GetOpenFileName(&ofn)) {
        returnval = ParseData(szFile, false);
        if (returnval == 0) {
            this->SetDlgItemText(IDC_STATUS,"Data Loaded OK");
        } else {
            this->SetDlgItemText(IDC_STATUS,"Data Load Failed!");
            free(this->m_effect_data->cyan);
            free(this->m_effect_data->magenta);
            free(this->m_effect_data->yellow);
            free(this->m_effect_data->black);
            free(this->m_effect_data);
            this->m_effect_data = NULL;
        }
    }
}

void CICC_CreatorDlg::OnBnClickedEffecticc3()
{
    int ok;
    TCHAR szFile[MAX_PATH];
    ZeroMemory(szFile, MAX_PATH);
    OPENFILENAME ofn;
    char des_ptr[25];
    int data;

    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize	= sizeof(OPENFILENAME);
    ofn.Flags		= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |OFN_HIDEREADONLY;
    ofn.hwndOwner	= this->m_hWnd;
    ofn.lpstrFilter	= _T("Supported Files Types(*.icc)\0*.icc;*.ICC\0\0");
    ofn.lpstrTitle	= _T("Save Effect Profile");
    ofn.lpstrFile	= szFile;
    ofn.nMaxFile	= MAX_PATH;

    if (IDOK == GetSaveFileName(&ofn)) {
	this->m_desc_effect_str.GetWindowTextA(des_ptr,24);
        /* Get the description string */
        ok = create_effect_profile(szFile, this->m_effect_data, des_ptr);
        if (ok == 0)
            this->SetDlgItemText(IDC_STATUS,_T("Created Effect Profile"));
    } 
}

void CICC_CreatorDlg::OnEnChangeEdit1()
{
    // TODO:  If this is a RICHEDIT control, the control will not
    // send this notification unless you override the CDialog::OnInitDialog()
    // function and call CRichEditCtrl().SetEventMask()
    // with the ENM_CHANGE flag ORed into the mask.

    // TODO:  Add your control notification handler code here

    int zz;

    zz = 1;

    return;
}
